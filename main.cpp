#include <iostream>
#include <cstring>

#ifdef PICO_BUILD
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "hardware/adc.h"
#include "hardware/sync.h"
#include "pico/util/queue.h"
#endif

#include "radlib/rtty/BaudotEncoder.h"
#include "radlib/lcd/HD44780_PCF8574.h"
#include "radlib/tests/TestI2CInterface.h"
#include "radlib/tests/TestClockInterface.h"

#ifdef PICO_BUILD
#include "radlib/hw/pico/PICOI2CInterface.h"
#include "radlib/hw/pico/PICOClockInterface.h"
#include "hello-ps2keyboard/PS2Keyboard.h"
#endif

#include "hello-ps2keyboard/KeyboardListener.h"
#include "hello-scamp/Demodulator.h"
#include "hello-scamp/Util.h"
#include "hello-scamp/Frame30.h"
#include "hello-scamp/TestDemodulatorListener.h"
#include "hello-pico-si5351/si5351.h"

#include "StationDemodulatorListener.h"
#include "Si5351Modulator.h"
#include "Si5351FSKModulator.h"
#include "EditorState.h"

#define KBD_DATA_PIN  (2)
#define KBD_CLOCK_PIN (3)
#define I2C0_SDA_PIN  (4)      // Pin 6: I2C channel 0 - data
#define I2C0_SCL_PIN  (5)      // Pin 7: I2C channel 0 - clock
#define I2C1_SDA_PIN  (6)      // Pin 9  - SI5351 data, yellow
#define I2C1_SCL_PIN  (7)      // Pin 10 - SI5351 clock, green
 
// This is an active high signal that deals with the heavy
// TR relay movements. 
//
// IMPORTANT: It is essential that the antenna load be attached
// to the PA output BEFORE we transmit any power to avoid blowing
// the finals during hot-switching transients.
//
#define TR_PHASE_0_PIN (8)

// This is an active low signal that deals with the "normal" TR
// switching.
#define TR_PHASE_1_PIN (9)

#define LED_PIN (25)

using namespace std;
using namespace radlib;
using namespace scamp;

/**
 * @param msg Must be null-terminated!
 * @param speed In WPM (assuming PARIS convention)
 */
void send_morse(const char* msg, Modulator& mod, uint16_t speed);

static const uint16_t sampleFreq = 2000;
static const uint32_t adcClockHz = 48000000;
static const uint16_t lowFreq = 100;
static const unsigned int samplesPerSymbol = 60;
static const unsigned int usPerSymbol = (1000000 / sampleFreq) * samplesPerSymbol;
static const unsigned int markFreq = 667;
static const unsigned int spaceFreq = 600;
static const uint32_t rfFreq = 7042000;

// The size of the FFT used for frequency acquisition
static const uint16_t log2fftN = 9;
static const uint16_t fftN = 1 << log2fftN;
// Space for the demodulator to work in (no dynamic memory allocation!)
static q15 trigTable[fftN];
static q15 window[fftN];
static q15 buffer[fftN];
static cq15 fftResult[fftN];
// The demodulator 
static Demodulator demod(sampleFreq, lowFreq, log2fftN,
    trigTable, window, fftResult, buffer);

// Diagnostic area
static TestDemodulatorListener::Sample samples[2000];

enum StationMode { IDLE_MODE, RX_MODE, TX_MODE };

static StationMode stationMode = StationMode::IDLE_MODE;

static uint32_t maxUs = 0;

enum DisplayPage { PAGE_LOGO, PAGE_STATUS, PAGE_RX, PAGE_TX };

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

// This is the queue ussed to pass keyboard activity from the ISR into the main
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
static queue_t adcSampleQueue;

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

uint32_t get_us() {
    absolute_time_t at = get_absolute_time();
    return to_us_since_boot(at);
}

/**
 * Formats the tuning frequency into "7.042 5" format.
*/
static void fmtFreq(Si5351Modulator& mod, char* buffer) {
    uint32_t f = mod.getBaseFreq();
    uint32_t m = f / 1000000;
    uint32_t k = (f / 1000) % 1000;
    uint32_t h = (f % 1000) / 100;
    sprintf(buffer, "%2d.%03d %d", m, k, h);
    buffer[8] = 32;
}

/**
 * Enter TX mode
*/
static void enter_tx_mode(ClockInterface& clock) {
    if (stationMode != StationMode::TX_MODE) {    
        // Turn off the ADC since the data is about to become
        // unusable
        adc_run(false);
        // Phase 0 TR switch controls the main TR relay.  We do this first
        // to make sure that the PA is connected to the external load before
        // we power-up the tx.
        gpio_put(TR_PHASE_0_PIN, 1);
        // Make sure the relay is settled
        clock.sleepMs(10);
        // Phase 1 TR switch
        gpio_put(TR_PHASE_1_PIN, 0);

        stationMode = StationMode::TX_MODE;
    }
}

static void enter_rx_mode() {
    if (stationMode != StationMode::RX_MODE) {        
        // Phase 1 TR switch
        gpio_put(TR_PHASE_1_PIN, 1);
        // Phase 0 TR switch
        gpio_put(TR_PHASE_0_PIN, 0);
        // Turn off the ADC back on
        adc_run(true);

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

    // TR switch setup
    gpio_init(TR_PHASE_1_PIN);
    gpio_set_dir(TR_PHASE_1_PIN, GPIO_OUT);   
    gpio_put(TR_PHASE_1_PIN, 1);
    gpio_init(TR_PHASE_0_PIN);
    gpio_set_dir(TR_PHASE_0_PIN, GPIO_OUT);   
    gpio_put(TR_PHASE_0_PIN, 0);

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
   
#ifdef PICO_BUILD

    // This is the queue used to collect data from the keyboard
    queue_init(&keyEventQueue, sizeof(KeyEvent), 8);

    keyboard_init(KBD_CLOCK_PIN, KBD_DATA_PIN, &listener);
    gpio_set_irq_enabled_with_callback(KBD_CLOCK_PIN, GPIO_IRQ_EDGE_FALL, 
        true, keyboard_clock_callback);

    // This is the queue used to collect data from the ADC.  Each queue entry
    // is 16 bits (uint16).
    queue_init(&adcSampleQueue, 2, 64);

    // Get the ADC iniialized
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

    // Create the demodulator listner
    //TestDemodulatorListener demodListener(cout, samples, 180);
    //demodListener.setTriggerMode(TestDemodulatorListener::TriggerMode::ON_LOCK);
    //demodListener.setTriggerDelay(0);
    StationDemodulatorListener demodListener(&display);
    demod.setListener(&demodListener);

    // SI5351 setup
    si_init(i2c1);
    cout << "Initialized Si5351" << endl;

    si_enable(0, false);

    Si5351Modulator modulator(clk, markFreq, spaceFreq);
    modulator.setBaseFreq(rfFreq);

    Si5351FSKModulator fskMod(clk, markFreq, markFreq - 170);
    fskMod.setBaseFreq(rfFreq);

    // Here we can inject a tuning error to show that the demodulator will
    // still find the signal.
    const unsigned int tuningErrorHz = 0;    

    // Editor
    char editorSpace[80];  
    EditorState editorState(editorSpace, (uint16_t)80);

    DisplayPage activePage = DisplayPage::PAGE_LOGO;
    bool displayDirty = true;

    // Prevent exit
    while (1) { 

        // Check for ADC activity
        while (!queue_is_empty(&adcSampleQueue)) {
            // Pull off ADC queue
            uint16_t lastSample = 0;
            queue_remove_blocking(&adcSampleQueue, &lastSample);
            // Center around zero
            lastSample -= (2048 + 60);
            // TODO: REMOVE FLOAT
            const float lastSampleF32 = lastSample / 2048.0;
            const q15 lastSampleQ15 = f32_to_q15(lastSampleF32);
            uint32_t start = get_us();
            demod.processSample(lastSampleQ15);
            uint32_t end = get_us();
            maxUs = std::max(maxUs, (end - start));
        }

        // Check for keyboard activity
        if (!queue_is_empty(&keyEventQueue)) {
            // Pull off keyboard event
            KeyEvent ev;
            queue_remove_blocking(&keyEventQueue, &ev);

            if (ev.scanCode == PS2_SCAN_ESC) {
                cout << "UNLOCK" << endl;
                demod.reset();
                displayDirty = true;
            } else  if (ev.scanCode == PS2_SCAN_F1) {
                //demodListener.dumpSamples(cout);
                //demodListener.clearSamples();
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
            } else if (ev.scanCode == PS2_SCAN_ENTER) {
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
            } else if (ev.scanCode == PS2_SCAN_F10) {
                // Switch modes
                enter_tx_mode(clk);
                // Transmission
                fskMod.enable(true);
                transmitBaudot("CQ CQ DE KC1FSZ KC1FSZ KC1FSZ K", fskMod, 22002);
                fskMod.enable(false);
                // Switch modes
                enter_rx_mode();
                // TEMP
                si_enable(0, false);

            } else if (ev.scanCode == PS2_SCAN_F11) {
                // Switch modes
                enter_tx_mode(clk);
                // Transmission
                modulator.enable(true);
                send_morse("CQ CQ DE KC1FSZ KC1FSZ KC1FSZ", modulator, 15);
                modulator.enable(false);
                // Switch modes
                enter_rx_mode();
                // TEMP
                si_enable(0, false);

            } else if (ev.scanCode == PS2_SCAN_F12) {
                // Switch modes
                enter_tx_mode(clk);
                // Transmission
                modulator.enable(true);
                modulator.sendCW();
                modulator.enable(false);
                // Switch modes
                enter_rx_mode();
            } else  if (ev.scanCode == PS2_SCAN_UP) {
                modulator.setBaseFreq(modulator.getBaseFreq() + 1000);
                displayDirty = true;
                // TEMP
                //si_evaluate(0, modulator.getBaseFreq() + 600);
            } else  if (ev.scanCode == PS2_SCAN_DOWN) {
                modulator.setBaseFreq(modulator.getBaseFreq() - 1000);
                displayDirty = true;
                // TEMP
                //si_evaluate(0, modulator.getBaseFreq() + 600);
            } else  if (ev.scanCode == PS2_SCAN_PGUP) {
                modulator.setBaseFreq(modulator.getBaseFreq() + 100);
                displayDirty = true;
                // TEMP
                //si_evaluate(0, modulator.getBaseFreq() + 600);
            } else  if (ev.scanCode == PS2_SCAN_PGDN) {
                modulator.setBaseFreq(modulator.getBaseFreq() - 100);
                displayDirty = true;
                // TEMP
                //si_evaluate(0, modulator.getBaseFreq() + 600);
            } else {
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

        // Display render
        if (displayDirty) {

            if (activePage == DisplayPage::PAGE_LOGO) {
                display.clearDisplay();
                display.writeLinear(HD44780::Format::FMT_20x4, 
                    (uint8_t*)"KC1FSZ SCAMP Station", 20, 0);
                display.writeLinear(HD44780::Format::FMT_20x4, 
                    (uint8_t*)"V0.05", 5, 20);
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
                if (demod.isFrequencyLocked()) {
                    sprintf(text,"LOCKED %03d", demod.getMarkFreq());
                } else {
                    sprintf(text,"NOLOCK", demod.getMarkFreq());
                }
                display.writeLinear(HD44780::Format::FMT_20x4, 
                    (uint8_t*)text, 20, 20);

            } else if (activePage == DisplayPage::PAGE_RX) {
                //demodListener.render(display);
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

// WILL MOVE

static void silence(Modulator& mod, uint16_t dots, uint16_t speed) {
    uint32_t dot_ms = 80;
    uint32_t t = (uint32_t)dots * dot_ms * 1000;
    mod.sendSilence(t);
}

static void dot(Modulator& mod, uint16_t speed, bool last = false) {
    uint32_t dot_ms = 80;
    uint32_t t = dot_ms * 1000;
    mod.sendMark(t);
    if (!last)
        silence(mod, 1, speed);
}

static void dash(Modulator& mod, uint16_t speed, bool last = false) {
    uint32_t dot_ms = 80;
    uint32_t t = 3L * dot_ms * 1000;
    mod.sendMark(t);
    if (!last)
        silence(mod, 1, speed);
}

void send_morse_char(char ch, Modulator& mod, uint16_t speed) {

    switch (ch) 
    {
        case 'A': 
            dot(mod, speed);
            dash(mod, speed, true);
            break;
        case 'B': 
            dash(mod, speed);
            dot(mod, speed);
            dot(mod, speed);
            dot(mod, speed, true);
            break;
        case 'C': 
            dash(mod, speed);
            dot(mod, speed);
            dash(mod, speed);
            dot(mod, speed, true);
            break;
        case 'D': 
            dash(mod, speed);
            dot(mod, speed);
            dot(mod, speed, true);
            break;
        case 'E': 
            dot(mod, speed, true);
            break;
        case 'F': 
            dot(mod, speed);
            dot(mod, speed);
            dash(mod, speed);
            dot(mod, speed, true);
            break;
        case 'G': 
            dash(mod, speed);
            dash(mod, speed);
            dot(mod, speed, true);
            break;
        case 'H': 
            dot(mod, speed);
            dot(mod, speed);
            dot(mod, speed);
            dot(mod, speed, true);
            break;
        case 'I': 
            dot(mod, speed);
            dot(mod, speed, true);
            break;
        case 'J': 
            dot(mod, speed);
            dash(mod, speed);
            dash(mod, speed);
            dash(mod, speed, true);
            break;
        case 'K': 
            dash(mod, speed);
            dot(mod, speed);
            dash(mod, speed, true);
            break;
        case 'L': 
            dot(mod, speed);
            dash(mod, speed);
            dot(mod, speed);
            dot(mod, speed, true);
            break;
        case 'M': 
            dash(mod, speed);
            dash(mod, speed, true);
            break;
        case 'N': 
            dash(mod, speed);
            dot(mod, speed, true);
            break;
        case 'O': 
            dash(mod, speed);
            dash(mod, speed);
            dash(mod, speed, true);
            break;
        case 'P': 
            dot(mod, speed);
            dash(mod, speed);
            dash(mod, speed);
            dot(mod, speed, true);
            break;
        case 'Q': 
            dash(mod, speed);
            dash(mod, speed);
            dot(mod, speed);
            dash(mod, speed, true);
            break;
        case 'R': 
            dot(mod, speed);
            dash(mod, speed);
            dot(mod, speed, true);
            break;
        case 'S': 
            dot(mod, speed);
            dot(mod, speed);
            dot(mod, speed, true);
            break;
        case 'T': 
            dash(mod, speed, true);
            break;
        case 'U': 
            dot(mod, speed);
            dot(mod, speed);
            dash(mod, speed, true);
            break;
        case 'V': 
            dot(mod, speed);
            dot(mod, speed);
            dot(mod, speed);
            dash(mod, speed, true);
            break;
        case 'W': 
            dot(mod, speed);
            dash(mod, speed);
            dash(mod, speed, true);
            break;
        case 'X': 
            dash(mod, speed);
            dot(mod, speed);
            dot(mod, speed);
            dash(mod, speed, true);
            break;
        case 'Y': 
            dash(mod, speed);
            dot(mod, speed);
            dash(mod, speed);
            dash(mod, speed, true);
            break;
        case 'Z': 
            dash(mod, speed);
            dash(mod, speed);
            dot(mod, speed);
            dot(mod, speed, true);
            break;
        case '1': 
            dot(mod, speed);
            dash(mod, speed);
            dash(mod, speed);
            dash(mod, speed);
            dash(mod, speed, true);
            break;
        case '2': 
            dot(mod, speed);
            dot(mod, speed);
            dash(mod, speed);
            dash(mod, speed);
            dash(mod, speed, true);
            break;
        case '3': 
            dot(mod, speed);
            dot(mod, speed);
            dot(mod, speed);
            dash(mod, speed);
            dash(mod, speed, true);
            break;
        case '4': 
            dot(mod, speed);
            dot(mod, speed);
            dot(mod, speed);
            dot(mod, speed);
            dash(mod, speed, true);
            break;
        case '5': 
            dot(mod, speed);
            dot(mod, speed);
            dot(mod, speed);
            dot(mod, speed);
            dot(mod, speed, true);
            break;
        case '6': 
            dash(mod, speed);
            dot(mod, speed);
            dot(mod, speed);
            dot(mod, speed);
            dot(mod, speed, true);
            break;
        case '7': 
            dash(mod, speed);
            dash(mod, speed);
            dot(mod, speed);
            dot(mod, speed);
            dot(mod, speed, true);
            break;
        case '8': 
            dash(mod, speed);
            dash(mod, speed);
            dash(mod, speed);
            dot(mod, speed);
            dot(mod, speed, true);
            break;
        case '9': 
            dash(mod, speed);
            dash(mod, speed);
            dash(mod, speed);
            dash(mod, speed);
            dot(mod, speed, true);
            break;
        case '0': 
            dash(mod, speed);
            dash(mod, speed);
            dash(mod, speed);
            dash(mod, speed);
            dash(mod, speed, true);
            break;
    }
}

void send_morse(const char* s, Modulator& mod, uint16_t speed) {
    cout << "Morse message: " << s << endl;
    uint16_t i = 0;
    for (i = 0; s[i] != 0; i++) {
        if (s[i] == ' ') {
            // NOTE: WE ALREADY SENT 3 AFTER LAST CHAR
            silence(mod, 4, speed);
        } else {
            send_morse_char(s[i], mod, speed);
            silence(mod, 3, speed);
        }
    }
    cout << "DONE" << endl;
}


