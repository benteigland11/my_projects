#include "pico/stdlib.h"  // Pico SDK core library
#include "hardware/timer.h"
#include <iostream>
#include <pico/stdio_usb.h>

volatile bool pinState = false;
uint8_t ctlPin = 0;
uint8_t dirPin = 1;
uint8_t enablePin = 2;

int ppsValues[] = { 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000, 1100, 1200, 1300, 1400, 1500, 1600, 1700, 1800, 1900, 2000 };
int numSteps = sizeof(ppsValues) / sizeof(ppsValues[0]);

void togglePin() {
    pinState = !pinState;
    gpio_put(ctlPin, pinState);
}

void init_gpio() {
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_init(ctlPin);
    gpio_init(dirPin);
    gpio_init(enablePin);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    gpio_set_dir(ctlPin, GPIO_OUT);
    gpio_set_dir(dirPin, GPIO_OUT);
    gpio_set_dir(enablePin, GPIO_OUT);
}

void init_motor() {
    gpio_put(enablePin, 0);
    sleep_ms(500); // Consider if this delay is truly needed
    gpio_put(dirPin, 1);
    sleep_ms(100); // Consider if this delay is truly needed
    gpio_put(ctlPin, 0);
}

bool repeating_timer_callback(struct repeating_timer *t) {
    // printf("Timer fired!\n"); // REMOVE THIS FOR HIGH FREQUENCY
    togglePin();
    return true;
}

void stopTimer(struct repeating_timer timerSig) {
    gpio_put(ctlPin, 0);
    cancel_repeating_timer(&timerSig);
}

void setup() {
    stdio_init_all(); // Initializes stdio AND waits for USB connection
    stdio_usb_init(); // Initializes USB serial
    init_gpio();
    init_motor();

    while (!stdio_usb_connected()) {
        sleep_ms(100);
    }
}

int main() {
    setup();
    // Wait for USB connection is now handled in setup.

    struct repeating_timer timerSig;

    for(int step = 0; step < numSteps; step++) {
        int pps = ppsValues[step];

        std::cout << "Setting PPS to " << pps << std::endl;

        int64_t delay_us = (int64_t)(1000000.0 / (2.0 * pps));
        add_repeating_timer_us(delay_us, repeating_timer_callback, NULL, &timerSig);

        unsigned long duration = 10000; // 10 seconds
        unsigned long starTime = to_ms_since_boot(get_absolute_time());
        while (to_ms_since_boot(get_absolute_time()) - starTime < duration) {
            // Wait for the duration
        }
        
        stopTimer(timerSig);
        std::cout << "Motor stopped. Enter 'n' to proceed to next PPS or 'r' to repeat" << std::endl;

        while (true)
        {
            // check if user input a letter
            if (stdio_usb_connected()) {
                char c = getchar_timeout_us(0);
                if (c == 'n') {
                    break;
                } else if (c == 'r') {
                    step--;
                    break;
                }
            }
        }
        
    }
    std::cout << "All steps completed." << std::endl;

    while(true) {
        gpio_put(PICO_DEFAULT_LED_PIN, 1);
        sleep_ms(500);
        gpio_put(PICO_DEFAULT_LED_PIN, 0);
        sleep_ms(500);
    }

    return 0;
}
