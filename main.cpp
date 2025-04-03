#include "pico/stdlib.h"
#include "hardware/gpio.h"    // For general GPIO init (like LED)
#include <pico/stdio_usb.h> // For USB stdio init/check
#include <iostream>         // For initial messages
#include <cstdio>           // For getchar_timeout_us, PICO_ERROR_TIMEOUT

// Include our module headers
#include "StepperMotor.h"
#include "SerialMenu.h"

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
    // Useful if you want to see the very first messages.
    #if !PICO_STDIO_USB_WAIT_FOR_CONNECTION
    sleep_ms(2000); // Wait 2 seconds if not automatically waiting
    #endif
    std::cout << "\n--- System Initializing ---" << std::endl;

    // Initialize basic board hardware (like the LED)
    init_board_gpio();
    gpio_put(PICO_DEFAULT_LED_PIN, 1); // Turn LED on during init

    // Initialize our modules
    motor_init(); // Initializes motor GPIO and state

    // SerialMenu doesn't require explicit init currently

    gpio_put(PICO_DEFAULT_LED_PIN, 0); // Turn LED off after successful init
    std::cout << "--- System Initialization Complete ---" << std::endl;
}

/**
 * @brief Main application: Initializes system and runs the main loop.
 */
int main() {
    system_setup();   // Initialize everything
    menu_display();   // Show the menu initially

    uint64_t last_motor_update_time = time_us_64();

    // --- Main Execution Loop ---
    // This structure is common in embedded systems:
    // check inputs, update state machines/logic, update outputs.
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
             menu_handle_input((char)c);
        }

        // 2. Update Motor State Periodically (Time-Based Task)
        // This runs regardless of user input to ensure smooth motor ramps.
        uint64_t now = time_us_64();
        MotorState currentMotorState = motor_get_state(); // Get state from module

        // Only call update function if motor is supposed to be moving/changing state
        if ( currentMotorState != MOTOR_STOPPED ) {
             // Check if enough time has passed since the last motor speed update
             if (now - last_motor_update_time >= (uint64_t)MAIN_LOOP_UPDATE_INTERVAL_MS * 1000) {
                 motor_update_state(); // Tell motor module to update its speed/state
                 last_motor_update_time = now; // Reset the timer for the next update
            }
        } else {
            // If motor is stopped, keep resetting the timer
            // Prevents a large time difference accumulating while stopped.
            last_motor_update_time = now;
        }

        // 3. Update Onboard LED (Simple Output/Status Indicator)
        // LED is ON if motor is doing anything (Accelerating, Running, Decelerating)
        gpio_put(PICO_DEFAULT_LED_PIN, (currentMotorState != MOTOR_STOPPED));

        // 4. Small Delay (Yield CPU Time)
        // Prevent the loop from consuming 100% CPU when idle.
        // Keep this short as motor step timing relies on its hardware timer.
        sleep_us(100); // Sleep for 100 microseconds
    }

    return 0; // Should never reach here in an embedded loop
}