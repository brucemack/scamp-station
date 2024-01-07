#include <iostream>
#include <cstring>

#ifdef PICO_BUILD
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "hardware/adc.h"
#include "hardware/sync.h"
#include "pico/util/queue.h"
#include "pico/multicore.h"
#endif

#include "radlib/rtty/BaudotEncoder.h"
#include "radlib/lcd/HD44780_PCF8574.h"
#include "radlib/tests/TestI2CInterface.h"
#include "radlib/tests/TestClockInterface.h"
#include "radlib/morse/MorseEncoder.h"

#ifdef PICO_BUILD
#include "radlib/hw/pico/PICOI2CInterface.h"
#include "radlib/hw/pico/PICOClockInterface.h"
#include "hello-ps2keyboard/PS2Keyboard.h"
#endif

#include "hello-ps2keyboard/KeyboardListener.h"
#include "hello-pico-si5351/si5351.h"
#include "radlib/scamp/Util.h"
#include "radlib/scamp/Frame30.h"

#include "main2.h"
#include "Si5351FSKModulator.h"
#include "EditorState.h"
#include "DemodulatorUtil.h"

#define KBD_DATA_PIN  (2)
#define KBD_CLOCK_PIN (3)
#define I2C0_SDA_PIN  (4)      // Pin 6: I2C channel 0 - data
#define I2C0_SCL_PIN  (5)      // Pin 7: I2C channel 0 - clock
#define I2C1_SDA_PIN  (6)      // Pin 9  - SI5351 data, yellow
#define I2C1_SCL_PIN  (7)      // Pin 10 - SI5351 clock, green
 
// This is an active low signal that deals with the heavy
// TR relay movements. 
//
// IMPORTANT: It is essential that the antenna load be attached
// to the PA output BEFORE we transmit any power to avoid blowing
// the finals during hot-switching transients.
//
#define TR_PHASE_0_PIN (8)

// This is an active high signal that deals with the "normal" TR
// switching.
#define TR_PHASE_1_PIN (9)

#define LED_PIN (25)

using namespace std;
using namespace radlib;

uint16_t sampleFreq = 2000;
uint16_t lowFreq = 100;

static const uint32_t adcClockHz = 48000000;
static const unsigned int samplesPerSymbol = 60;
static const unsigned int usPerSymbol = (1000000 / sampleFreq) * samplesPerSymbol;
// Adjustable mark frequency
unsigned int markFreq = 667;
//static const unsigned int spaceFreq = 600;
static const unsigned int bwFreq = 67;

// This is the "tuned" frequency.  For SCAMP FSK this is the 
// center frequency.
//static uint32_t rfFreq = 7078000;
static uint32_t rfFreq = 7108000;

enum StationMode { IDLE_MODE, RX_MODE, TX_MODE };

static StationMode stationMode = StationMode::IDLE_MODE;

enum DisplayPage { PAGE_LOGO, PAGE_STATUS, PAGE_RX, PAGE_TX };

// This should be added to the desired frequency
static int32_t correctionHz = 500;
// This is amount the RX LO is shifted *DOWN*
static int32_t rxShift = 700;

// ----- KEYBOARD RELATED -------------------------------------------------

struct KeyEvent {

    uint16_t scanCode;
    bool shiftState;
    bool ctlState;
    bool altState;

    char getAscii() {
        return keyboard_code_to_ascii(scanCode, shiftState);
    }
};

// This is the queue used to pass keyboard activity from the ISR into the main
// event loop
static queue_t keyEventQueue;

// Keyboard listener
class KeyListener : public KeyboardListener {
public:
    void onKeyDown(uint16_t scanCode, bool shiftState, bool ctlState, bool altState) { 
        KeyEvent ev;
        ev.scanCode = scanCode;
        ev.shiftState = shiftState;
        ev.ctlState = ctlState;
        ev.altState = altState;
        queue_try_add(&keyEventQueue, &ev);
    };
};

static char upcase(char c) {
    if (c >= 97 && c <= 122) {
        return c - 32;
    } else {
        return c;
    }
}

// ----- ADC RELATED ------------------------------------------------------

// This is the queue used to pass ADC samples from the ISR and into the main 
// event loop.
queue_t adcSampleQueue;

static uint32_t sampleCount = 0;
static uint16_t maxAdcSampleQueue = 0;

// Decorates a function name, such that the function will execute from RAM 
// (assuming it is not inlined into a flash function by the compiler)
#ifdef PICO_BUILD
static void __not_in_flash_func(adc_irq_handler) () {    
    while (!adc_fifo_is_empty()) {
        const int16_t lastSample = adc_fifo_get();
        bool added = queue_try_add(&adcSampleQueue, &lastSample);
        if (!added) {
            return;
        }
        sampleCount++;
        maxAdcSampleQueue = std::max(maxAdcSampleQueue,
            (uint16_t)queue_get_level(&adcSampleQueue));
    }
}
#endif

// ------ DEMODULATOR RELATED -------------------------------------------

// This is the queue used to receive characters back from the 
// demodulator
queue_t demodRxQueue;
// This is the queue used to receive status from the demodulator
queue_t demodStatusQueue;
// This is the queue used to send commands to the demodulator
queue_t demodCommandQueue;

static uint32_t get_us() {
    absolute_time_t at = get_absolute_time();
    return to_us_since_boot(at);
}

/**
 * Formats the tuning frequency into "7.042 5" format.
*/
static void fmtFreq(Si5351FSKModulator& mod, char* buffer) {
    uint32_t f = mod.getBaseFreq();
    uint32_t m = f / 1000000;
    uint32_t k = (f / 1000) % 1000;
    uint32_t h = (f % 1000);
    sprintf(buffer, "%2d.%03d %03d", m, k, h);
    buffer[10] = 32;
}

/**
 * Enter TX mode
*/
static void enter_tx_mode(ClockInterface& clock) {
    if (stationMode != StationMode::TX_MODE) {    
        // Turn off RX LO
        si_enable(1, false);
        // Turn off the ADC since the data is about to become
        // unusable
        adc_run(false);
        // Phase 0 TR switch controls the main TR relay.  We do this first
        // to make sure that the PA is connected to the external load before
        // we power-up the tx.
        gpio_put(TR_PHASE_0_PIN, 0);
        // Make sure the relay is settled
        clock.sleepMs(10);
        // Phase 1 TR switch.  Active high.
        gpio_put(TR_PHASE_1_PIN, 1);

        stationMode = StationMode::TX_MODE;
    }
}

static void enter_rx_mode() {
    if (stationMode != StationMode::RX_MODE) {        
        // Phase 1 TR switch
        gpio_put(TR_PHASE_1_PIN, 0);
        // Phase 0 TR switch
        gpio_put(TR_PHASE_0_PIN, 1);
        // Turn off the ADC back on
        adc_run(true);
        // Turn on RX LO
        si_enable(1, true);

        stationMode = StationMode::RX_MODE;
    }
}

int main(int argc, const char** argv) {

#ifdef PICO_BUILD
    stdio_init_all();

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    
    // This example will use I2C0 on the default SDA and SCL pins (GP4, GP5 on a Pico)
    i2c_init(i2c0, 100 * 1000);
    gpio_set_function(PICO_DEFAULT_I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(PICO_DEFAULT_I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(PICO_DEFAULT_I2C_SDA_PIN);
    gpio_pull_up(PICO_DEFAULT_I2C_SCL_PIN);

    i2c_init(i2c1, 100 * 1000);
    gpio_set_function(I2C1_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C1_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C1_SDA_PIN);
    gpio_pull_up(I2C1_SDA_PIN);

    // Keyboard setup
    gpio_init(KBD_DATA_PIN);
    gpio_set_dir(KBD_DATA_PIN, GPIO_IN);   
    gpio_init(KBD_CLOCK_PIN);
    gpio_set_dir(KBD_CLOCK_PIN, GPIO_IN);

    // TR switch setup (active high -> TX mode)
    gpio_init(TR_PHASE_1_PIN);
    gpio_set_dir(TR_PHASE_1_PIN, GPIO_OUT);   
    gpio_put(TR_PHASE_1_PIN, 0);

    // TR switch setup (active low -> TX mode)
    gpio_init(TR_PHASE_0_PIN);
    gpio_set_dir(TR_PHASE_0_PIN, GPIO_OUT);   
    gpio_put(TR_PHASE_0_PIN, 1);

    stationMode = StationMode::RX_MODE;

    gpio_put(LED_PIN, 1);
    sleep_ms(1000);
    gpio_put(LED_PIN, 0);
    sleep_ms(1000);
#endif

#ifdef PICO_BUILD
    PICOI2CInterface i2c(i2c0, cout);
    PICOClockInterface clk;
#else
    TestI2CInterface i2c(cout);
    TestClockInterface clk;
#endif

    // ------ DISPLAY SETUP ---------------------------------------------------
    // Use the scanner to find this address
    uint8_t addr = 0x27;
    // 4-bit interface, 2 rows, 5x8 characters
    HD44780_PCF8574 display(2, false, addr, &i2c, &clk);

    // Keyboard
    KeyListener listener;

    printf("SCAMP Station\n");
    
    display.init();
    display.setDisplay(false, false, false);
    display.clearDisplay();
    display.setEntryMode(true, false);  // Move sursor, no shift
    display.setDisplay(true, true, true);
    display.returnHome();
    display.setDDRAMAddr(0);

    // Editor
    char editorSpace[80];  
    EditorState editorState(editorSpace, (uint16_t)80);

    DisplayPage activePage = DisplayPage::PAGE_LOGO;
    bool displayDirty = true;
   
    // ------ ADC SETUP -------------------------------------------------------
#ifdef PICO_BUILD
    // This is the queue used to collect data from the keyboard
    queue_init(&keyEventQueue, sizeof(KeyEvent), 8);

    keyboard_init(KBD_CLOCK_PIN, KBD_DATA_PIN, &listener);
    gpio_set_irq_enabled_with_callback(KBD_CLOCK_PIN, GPIO_IRQ_EDGE_FALL, 
        true, keyboard_clock_callback);

    // This is the queue used to collect data from the ADC.  Each queue entry
    // is 16 bits (uint16).
    queue_init(&adcSampleQueue, 2, 64);

    // Get the ADC initialized
    uint8_t adcChannel = 0;
    adc_gpio_init(26 + adcChannel);
    adc_init();
    adc_select_input(adcChannel);
    adc_fifo_setup(
        true,   
        false,
        1,
        false,
        false
    );
    adc_set_clkdiv(adcClockHz / (uint32_t)sampleFreq);
    irq_set_exclusive_handler(ADC_IRQ_FIFO, adc_irq_handler);    
    adc_irq_set_enabled(true);
    irq_set_enabled(ADC_IRQ_FIFO, true);
    adc_run(true);

#endif

    // ------ DEMODULATION SETUP ----------------------------------------------
    queue_init(&demodRxQueue, 1, 16);
    queue_init(&demodCommandQueue, sizeof(DemodulatorCommand), 4);
    queue_init(&demodStatusQueue, sizeof(DemodulatorStatus), 4);

    // Fire off the second thread
    multicore_launch_core1(main2);

    // This is where we keep the current status that was received from
    // the demodulator.
    DemodulatorStatus currentDemodStatus;

    // ------- SI5351 SETUP ---------------------------------------------------
    si_init(i2c1);
    cout << "Initialized Si5351" << endl;

    si_enable(0, false);
    si_enable(1, false);

    int32_t m = (bwFreq / 2);
    int32_t s = m - bwFreq;

    Si5351FSKModulator modulator(clk, m, s, correctionHz);
    modulator.setBaseFreq(rfFreq);

    Si5351FSKModulator rttyMod(clk, m, m - 170, correctionHz);
    rttyMod.setBaseFreq(rfFreq);

    // Receiver frequency setup
    si_enable(1, true);
    si_evaluate(1, rfFreq + correctionHz - rxShift);

    // ----- RX BUFFER SETUP --------------------------------------------------

    const uint16_t rxBufferSize = 80;
    uint8_t rxBuffer[rxBufferSize];
    uint16_t rxBufferUsed = 0;
    for (uint16_t i = 0; i < rxBufferSize; i++) {
        rxBuffer[i] = 0;
    }

    // Here we can inject a tuning error to show that the demodulator will
    // still find the signal.
    const unsigned int tuningErrorHz = 0;    

    // Start in RX mode
    enter_rx_mode();

    // Main event loop. Prevent exit

    while (true) { 

        // Check for RX character activity back from the demodulator
        while (!queue_is_empty(&demodRxQueue)) {
            char c;
            queue_remove_blocking(&demodRxQueue, &c);
            cout << c;
            cout.flush();
            if (rxBufferUsed == 80) {
                // Shift down by 20 characters
                for (uint16_t i = 0; i < 60; i++) {
                    rxBuffer[i] = rxBuffer[i + 20];
                }
                rxBufferUsed = 60;
            }
            rxBuffer[rxBufferUsed++] = c;
            if (activePage == DisplayPage::PAGE_RX) {
                displayDirty = true;
            }
        }

        // Check for demodulator status activity
        while (!queue_is_empty(&demodStatusQueue)) {
            DemodulatorStatus status;
            queue_remove_blocking(&demodStatusQueue, &status);
            if (!(status == currentDemodStatus)) {
                cout << "Demod Status:" << endl;
                cout << "  Locked     " << status.isLocked << endl;
                cout << "  Mark Freq  " << status.lockedMarkFreq << endl;
                cout << endl;
                if (activePage == DisplayPage::PAGE_STATUS) {
                    displayDirty = true;
                }
            }
            currentDemodStatus = status;
            /*
            // Output spectrum for waterfall display
            q15 spectrumMag[256];
            getLiveSpectrum(spectrumMag, 256);
            cout << "[WF]0,0";
            for (uint16_t i = 0; i < 256; i++) {
                cout << ",";
                // Convert the magnitude into a number from 0->255
                float f = q15_to_f32(spectrumMag[i]) * 2550.0f;
                cout << (int)f;
            }
            cout << endl;
            */
        }

        // Check for keyboard activity
        if (!queue_is_empty(&keyEventQueue)) {

            // Pull off keyboard event
            KeyEvent ev;
            queue_remove_blocking(&keyEventQueue, &ev);

            // Look for all of the keys that don't depend on which page we are on

            if (ev.scanCode == PS2_SCAN_ESC) {
                // Send a command to the demodulator
                DemodulatorCommand cmd;
                cmd.cmd = DemodulatorCommand::Command::RESET;
                bool added = queue_try_add(&demodCommandQueue, &cmd);
            } else  if (ev.scanCode == PS2_SCAN_F1) {
                activePage = DisplayPage::PAGE_LOGO;
                displayDirty = true;
            } else  if (ev.scanCode == PS2_SCAN_F2) {
                activePage = DisplayPage::PAGE_STATUS;
                displayDirty = true;
            } else  if (ev.scanCode == PS2_SCAN_F3) {
                activePage = DisplayPage::PAGE_TX;
                displayDirty = true;
            } else  if (ev.scanCode == PS2_SCAN_F4) {
                activePage = DisplayPage::PAGE_RX;
                displayDirty = true;
            } else  if (ev.scanCode == PS2_SCAN_F5) {
                /*
                cout << "ADC Stats:" << endl;
                cout << "AVG " << adcStatSamples.getAvg() << endl;
                cout << "MIN " << adcStatSamples.getMin() << endl;
                cout << "MAX " << adcStatSamples.getMax() << endl;
                cout << "|AVG| " << (float)adcStatSamples.getAvg() / 2048.0f << endl;
                cout << "|MIN| " << (float)adcStatSamples.getMin() / 2048.0f << endl;
                cout << "|MAX| " << (float)adcStatSamples.getMax() / 2048.0f << endl;
                cout << endl;
                */
            // SCAMP CQ
            } else if (ev.scanCode == PS2_SCAN_F9) {
                // Switch modes
                enter_tx_mode(clk);
                // Transmission
                modulator.enable(true);
                // The actual data sending
                Frame30 frames[48];
                modulateMessage("CQ CQ DE KC1FSZ KC1FSZ KC1FSZ K",
                    modulator, usPerSymbol, frames, 48);
                modulator.enable(false);
                // Switch modes
                enter_rx_mode();
                // TEMP
                si_enable(0, false);
            } 
            // RTTY CQ
            else if (ev.scanCode == PS2_SCAN_F10) {
                // Switch modes
                enter_tx_mode(clk);
                // Transmission
                rttyMod.enable(true);
                transmitBaudot("CQ CQ DE KC1FSZ KC1FSZ KC1FSZ K", rttyMod, 22002);
                rttyMod.enable(false);
                // Switch modes
                enter_rx_mode();
                // TEMP
                si_enable(0, false);

            } 
            // CW CQ
            else if (ev.scanCode == PS2_SCAN_F11) {
                // Switch modes
                enter_tx_mode(clk);
                // Transmission
                rttyMod.enable(true);
                send_morse("CQ CQ CQ DE KC1FSZ KC1FSZ KC1FSZ", rttyMod, 15);
                rttyMod.enable(false);
                // Switch modes
                enter_rx_mode();
                // TEMP
                si_enable(0, false);
            } 
            // Test tone
            else if (ev.scanCode == PS2_SCAN_F12) {
                // Switch modes
                enter_tx_mode(clk);
                // Transmission
                modulator.enable(true);
                modulator.sendCW();
                modulator.enable(false);
                // Switch modes
                enter_rx_mode();
            } 
            
            // Everything else depends on the page

            else {
            
                if (activePage == DisplayPage::PAGE_TX) {                

                    if (ev.scanCode == PS2_SCAN_ENTER) {
                        int l = strlen(editorSpace);
                        if (l > 0) {
                            // Set message
                            display.clearDisplay();
                            display.writeLinear(HD44780::Format::FMT_20x4, 
                                (const uint8_t*)"Sending ...", 11, 0);
                            // Switch modes
                            enter_tx_mode(clk);
                            // Radio on
                            modulator.enable(true);
                            // The actual data sending
                            Frame30 frames[48];
                            uint16_t framesSent = modulateMessage(editorSpace, 
                                modulator, usPerSymbol, frames, 48);
                            // Radio off
                            modulator.enable(false);
                            // Switch modes
                            enter_rx_mode();
                            // Display
                            editorState.clear();
                            displayDirty = true;
                        }
                    } 
                    else {
                        char a = ev.getAscii();
                        if (a != 0) {
                            editorState.addChar(upcase(a));
                            displayDirty = true;
                        } else if (ev.scanCode == PS2_SCAN_BSP) {
                            editorState.keyBackspace();
                            displayDirty = true;
                        } else {
                            printf("KBD: %02x %d %d %d %d\n", (int)ev.scanCode, 
                                (int)ev.shiftState, (int)ev.ctlState, (int)ev.altState);
                        }
                    }

                }
                else if (activePage == DisplayPage::PAGE_STATUS) {
                    if (ev.scanCode == PS2_SCAN_UP) {
                        rfFreq += 1000;
                        modulator.setBaseFreq(rfFreq);
                        rttyMod.setBaseFreq(rfFreq);
                        // Receiver frequency setup
                        si_evaluate(1, rfFreq + correctionHz - rxShift);
                        displayDirty = true;
                    } else  if (ev.scanCode == PS2_SCAN_DOWN) {
                        rfFreq -= 1000;
                        modulator.setBaseFreq(rfFreq);
                        rttyMod.setBaseFreq(rfFreq);
                        // Receiver frequency setup
                        si_evaluate(1, rfFreq + correctionHz - rxShift);
                        displayDirty = true;
                    } else  if (ev.scanCode == PS2_SCAN_PGUP) {
                        rfFreq += 50;
                        modulator.setBaseFreq(rfFreq);
                        rttyMod.setBaseFreq(rfFreq);
                        // Receiver frequency setup
                        si_evaluate(1, rfFreq + correctionHz - rxShift);
                        displayDirty = true;
                    } else  if (ev.scanCode == PS2_SCAN_PGDN) {
                        rfFreq -= 50;
                        modulator.setBaseFreq(rfFreq);
                        rttyMod.setBaseFreq(rfFreq);
                        // Receiver frequency setup
                        si_evaluate(1, rfFreq + correctionHz - rxShift);
                        displayDirty = true;
                    } else if (ev.getAscii() == 'q') {
                        if (markFreq > 0) {
                            markFreq--;
                        }
                        // Send a command to the demodulator
                        DemodulatorCommand cmd;
                        cmd.cmd = DemodulatorCommand::Command::SET_MARK;
                        cmd.markFreq = markFreq;
                        queue_try_add(&demodCommandQueue, &cmd);
                    }
                    else if (ev.getAscii() == 'w') {
                        if (markFreq < 1000) {
                            markFreq++;
                        }
                        // Send a command to the demodulator
                        DemodulatorCommand cmd;
                        cmd.cmd = DemodulatorCommand::Command::SET_MARK;
                        cmd.markFreq = markFreq;
                        queue_try_add(&demodCommandQueue, &cmd);
                    }
                }
            }
        }

        // Display render
        if (displayDirty) {

            if (activePage == DisplayPage::PAGE_LOGO) {
                display.clearDisplay();
                display.writeLinear(HD44780::Format::FMT_20x4, 
                    (uint8_t*)"KC1FSZ SCAMP Station", 20, 0);
                display.writeLinear(HD44780::Format::FMT_20x4, 
                    (uint8_t*)"V0.07", 5, 20);
                display.writeLinear(HD44780::Format::FMT_20x4, 
                    (uint8_t*)"Copyright (c) 2023", 18, 40);
            } else if (activePage == DisplayPage::PAGE_STATUS) {
                
                display.clearDisplay();

                char text[20];

                // Frequency
                memset(text, 32, 20);
                fmtFreq(modulator, text);
                display.writeLinear(HD44780::Format::FMT_20x4, 
                    (uint8_t*)text, 20, 0);

                memset(text, 32, 20);
                if (currentDemodStatus.isLocked) {
                    sprintf(text,"LOCKED %03d", currentDemodStatus.lockedMarkFreq);
                } else {
                    sprintf(text,"NOLOCK");
                }
                display.writeLinear(HD44780::Format::FMT_20x4, 
                    (uint8_t*)text, 20, 20);

            } else if (activePage == DisplayPage::PAGE_RX) {
                display.writeLinear(HD44780::Format::FMT_20x4, 
                    (uint8_t*)rxBuffer, 80, 0);
            } else if (activePage == DisplayPage::PAGE_TX) {
                if (editorState.isClear()) {       
                    display.clearDisplay();
                    display.writeLinear(HD44780::Format::FMT_20x4, 
                        (uint8_t*)"Enter TX Text", 13, 60);
                    display.setCursorLinear(HD44780::Format::FMT_20x4, 0);
                } else {
                    editorState.render(display);
                }
            }

            displayDirty = false;
        }

    }

    return 0;
}
