#include "pico/stdlib.h"
#include "hardware/gpio.h"    // For general GPIO init (like LED)
#include <pico/stdio_usb.h> // For USB stdio init/check
#include <iostream>         // For initial messages
#include <cstdio>           // For getchar_timeout_us, PICO_ERROR_TIMEOUT
#include "servo_controller.h" 

// Include our module headers
#include "StepperMotor.h"
#include "SerialMenu.h"
#include "sd_card_manager.h" // Include SD manager header (though init is now manual)

// --- Configuration ---
// How often the main loop checks motor state and input (milliseconds)
const uint MAIN_LOOP_UPDATE_INTERVAL_MS = 20;

// --- Function Declarations (main scope) ---
void system_setup();
void init_board_gpio(); // For non-motor GPIO like LED

// --- Function Implementations ---

/**
 * @brief Initializes non-motor specific GPIO like the onboard LED.
 */
void init_board_gpio() {
    // Onboard LED
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    gpio_put(PICO_DEFAULT_LED_PIN, 0); // Start with LED off
}

/**
 * @brief Initial system setup function: Initializes hardware and software modules.
 */
void system_setup() {
    stdio_init_all(); // Initialize chosen stdio (USB in this case)

    // Optional: Wait a moment for serial connection to establish
    #if !PICO_STDIO_USB_WAIT_FOR_CONNECTION
    sleep_ms(2000); // Wait 2 seconds if not automatically waiting
    #endif
    std::cout << "\n--- System Initializing ---" << std::endl;

    // Initialize basic board hardware (like the LED)
    init_board_gpio();
    gpio_put(PICO_DEFAULT_LED_PIN, 1); // Turn LED on during init

    // Initialize our modules
    motor_init(); // Initializes motor GPIO and state
    servo_init(); 

    // SerialMenu doesn't require explicit init currently

    // *** SD Card initialization is now handled manually via the SerialMenu ***
    // *** Do NOT call sd_init() here anymore ***
    std::cout << "SD Card: Use Serial Menu ('i' command) to initialize." << std::endl;


    gpio_put(PICO_DEFAULT_LED_PIN, 0); // Turn LED off after successful init
    std::cout << "--- System Initialization Complete ---" << std::endl;
}

/**
 * @brief Main application: Initializes system and runs the main loop.
 */
int main() {
    system_setup();   // Initialize everything (except SD card now)
    menu_display_main();   // Show the menu initially

    uint64_t last_motor_update_time = time_us_64();

    // --- Main Execution Loop ---
    while (true) {
        // 1. Check for Serial Input (Non-Blocking Event Handling)
        int c = getchar_timeout_us(0); // Check for character, return PICO_ERROR_TIMEOUT if none
        if (c != PICO_ERROR_TIMEOUT) {
             // Echo character immediately for user feedback
             std::cout << (char)c;
             // Add newline after CR or LF for better terminal display
             if ((char)c == '\r' || (char)c == '\n') {
                std::cout << std::endl;
             }
             std::cout.flush();

             // Handle the command using the menu module's logic
             menu_handle_input((char)c); // This now handles SD commands too
        }

        // 2. Update Motor State Periodically (Time-Based Task)
        uint64_t now = time_us_64();
        MotorState currentMotorState = motor_get_state(); // Get state from module

        if ( currentMotorState != MOTOR_STOPPED ) {
             if (now - last_motor_update_time >= (uint64_t)MAIN_LOOP_UPDATE_INTERVAL_MS * 1000) {
                 motor_update_state();
                 last_motor_update_time = now;
            }
        } else {
            last_motor_update_time = now;
        }

        // 3. Update Onboard LED (Simple Output/Status Indicator)
        // LED is ON if motor is doing anything
        gpio_put(PICO_DEFAULT_LED_PIN, (currentMotorState != MOTOR_STOPPED));

        // 4. Small Delay (Yield CPU Time)
        sleep_us(100);
    }

    return 0; // Should never reach here
}