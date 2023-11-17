#include <iostream>

#ifdef PICO_BUILD
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#endif

#include "hello-lcd/HD44780_PCF8574.h"
#include "hello-lcd/tests/TestI2CInterface.h"
#include "hello-lcd/tests/TestClockInterface.h"

#ifdef PICO_BUILD
#include "hello-lcd/pico/PICOI2CInterface.h"
#include "hello-lcd/pico/PICOClockInterface.h"
#include "hello-ps2keyboard/PS2Keyboard.h"
#endif

#define LED_PIN (25)

#define I2C0_SDA 4 // Pin 6: I2C channel 0 - data
#define I2C0_SCL 5 // Pin 7: I2C channel 0 - clock

#define KBD_DATA_PIN (2)
#define KBD_CLOCK_PIN (3)

class Listener : public KeyboardListener {
public:

    void onKey() { };
};

using namespace std;

int main(int argc, const char** argv) {

#ifdef PICO_BUILD
    stdio_init_all();

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    
    // This example will use I2C0 on the default SDA and SCL pins (GP4, GP5 on a Pico)
    i2c_init(i2c_default, 100 * 1000);
    gpio_set_function(PICO_DEFAULT_I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(PICO_DEFAULT_I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(PICO_DEFAULT_I2C_SDA_PIN);
    gpio_pull_up(PICO_DEFAULT_I2C_SCL_PIN);

    // Keyboard setup
    gpio_init(KBD_DATA_PIN);
    gpio_set_dir(KBD_DATA_PIN, GPIO_IN);   
    gpio_init(KBD_CLOCK_PIN);
    gpio_set_dir(KBD_CLOCK_PIN, GPIO_IN);

    
    gpio_put(LED_PIN, 1);
    sleep_ms(1000);
    gpio_put(LED_PIN, 0);
    sleep_ms(1000);
#endif

#ifdef PICO_BUILD
    PICOI2CInterface i2c(cout);
    PICOClockInterface clk;
#else
    TestI2CInterface i2c(cout);
    TestClockInterface clk;
#endif

    // Use the scanner to find this address
    uint8_t addr = 0x27;
    // 4-bit interface, 2 rows, 5x8 characters
    HD44780_PCF8574 display(2, false, addr, &i2c, &clk);

    printf("SCAMP Station\n");
    
    cout << "----- Init" << endl;
    display.init();
    cout << "----- Set Display" << endl;
    display.setDisplay(false, false, false);
    cout << "----- Clear" << endl;
    display.clearDisplay();
    
    cout << "----- Set Entry Mode" << endl;
    display.setEntryMode(true, false);  // Move sursor, no shift
    cout << "----- Set Display" << endl;
    display.setDisplay(true, true, true);
    cout << "----- Home" << endl;
    display.returnHome();
    cout << "----- Set DDRAM Address" << endl;
    display.setDDRAMAddr(0);
    // This is an important test - it should demonstrate a wrap-around
    cout << "----- Write Message" << endl;
    display.writeLinear(HD44780::Format::FMT_20x4, (uint8_t*)"KC1FSZ SCAMP Station", 20, 0);

    cout << "I2C cycles: " << i2c.getCycleCount() << endl;
    cout << "Busy Count: " << display.getBusyCount() << endl;

    Listener listener;

    keyboard_init(KBD_CLOCK_PIN, KBD_DATA_PIN, &listener);
    gpio_set_irq_enabled_with_callback(KBD_CLOCK_PIN, GPIO_IRQ_EDGE_FALL, 
        true, keyboard_clock_callback);

    // Prevent exit
    while (1) { }

    return 0;
}
