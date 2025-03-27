#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"
#include <pico/stdio_usb.h> // Make sure USB stdio is used
#include <iostream>
#include <cstdio> // For printf if needed, and getchar_timeout_us

// --- Configuration ---
// Stepper Driver Pins
const uint STEP_PIN = 0;
const uint DIR_PIN = 1;
const uint ENABLE_PIN = 2; // Active LOW enable often used

// Motor Speed & Acceleration Parameters
const int TARGET_PPS = 1200;       // Target speed in Pulses Per Second
const int START_PPS = 100;         // Starting speed for acceleration
const int ACCEL_STEP_PPS = 20;     // How much to increase PPS per update during acceleration
const int DECEL_STEP_PPS = 50;     // How much to decrease PPS per update during deceleration
const uint UPDATE_INTERVAL_MS = 20; // How often to update speed (adjust if needed)

// --- Global Variables ---
volatile bool stepPinState = false;
struct repeating_timer stepTimer;
volatile bool timerActive = false;

// Motor State Machine
enum MotorState {
    STOPPED,
    ACCELERATING,
    RUNNING,
    DECELERATING
};
volatile MotorState currentState = STOPPED;
volatile int currentPPS = 0;

// === Removed Switch Debounce Variables ===
// volatile bool lastStableSwitchState = true;
// volatile bool integratingSwitchState = true;
// volatile int consecutiveCount = 0;

// --- Function Declarations ---
void init_gpio();
void init_motor();
bool step_timer_callback(struct repeating_timer *t);
int64_t pps_to_delay_us(int pps);
void start_step_timer(int pps);
void stop_step_timer();
// void handle_switch(); // Removed
void update_motor_state(); // Modified
void setup();
void display_menu();
void handle_serial_input(char cmd);
void load_simulation_placeholder();
void run_simulation_placeholder();
void start_test_motor(); // New function to start the test sequence
void stop_test_motor();  // New function to explicitly stop the test motor

// --- Function Implementations ---

int64_t pps_to_delay_us(int pps) {
    if (pps <= 0) {
        return -1;
    }
    return (int64_t)(1000000.0 / (2.0 * pps));
}

bool step_timer_callback(struct repeating_timer *t) {
    stepPinState = !stepPinState;
    gpio_put(STEP_PIN, stepPinState);
    return true;
}

void start_step_timer(int pps) {
    if (timerActive) {
        cancel_repeating_timer(&stepTimer);
        timerActive = false;
    }
    int64_t delay_us = pps_to_delay_us(pps);
    if (delay_us > 0) {
        if (add_repeating_timer_us(delay_us, step_timer_callback, NULL, &stepTimer)) {
            timerActive = true;
        } else {
            std::cout << "Error: Failed to add repeating timer!" << std::endl;
            currentState = STOPPED;
            currentPPS = 0;
            gpio_put(ENABLE_PIN, 1);
        }
    } else {
         if (timerActive) {
             cancel_repeating_timer(&stepTimer);
             timerActive = false;
         }
         gpio_put(STEP_PIN, 0); // Ensure step pin is low if pps is 0 or less
    }
}

void stop_step_timer() {
    if (timerActive) {
        cancel_repeating_timer(&stepTimer);
        timerActive = false;
    }
    stepPinState = false;
    gpio_put(STEP_PIN, 0);
}

void init_gpio() {
    // Stepper pins
    gpio_init(STEP_PIN);
    gpio_init(DIR_PIN);
    gpio_init(ENABLE_PIN);
    gpio_set_dir(STEP_PIN, GPIO_OUT);
    gpio_set_dir(DIR_PIN, GPIO_OUT);
    gpio_set_dir(ENABLE_PIN, GPIO_OUT);

    // === Removed Switch Pin Init ===
    // gpio_init(SWITCH_PIN);
    // gpio_set_dir(SWITCH_PIN, GPIO_IN);
    // gpio_pull_up(SWITCH_PIN);

    // Onboard LED
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    gpio_put(PICO_DEFAULT_LED_PIN, 0);
}

void init_motor() {
    gpio_put(DIR_PIN, 1);
    gpio_put(ENABLE_PIN, 1); // Start disabled
    gpio_put(STEP_PIN, 0);
    sleep_ms(10);
    currentState = STOPPED;
    currentPPS = 0;
}

// === Removed handle_switch() ===

// Updates motor speed based on the current state (acceleration/deceleration)
// Now only acts based on currentState, not switch input.
void update_motor_state() {
    bool speedChanged = false;

    switch (currentState) {
        case ACCELERATING:
            // Continue accelerating until target speed is reached
            if (currentPPS < TARGET_PPS) {
                currentPPS += ACCEL_STEP_PPS;
                if (currentPPS > TARGET_PPS) {
                    currentPPS = TARGET_PPS;
                }
                speedChanged = true;
            } else {
                currentPPS = TARGET_PPS; // Ensure it's exactly target
                currentState = RUNNING;
                speedChanged = true; // Update timer to exact target freq
                std::cout << "State: RUNNING at " << currentPPS << " PPS" << std::endl;
            }
            break;

        case DECELERATING:
            // Continue decelerating until stopped
            if (currentPPS > 0) {
                currentPPS -= DECEL_STEP_PPS;
                 if (currentPPS <= 0) {
                    currentPPS = 0; // Clamp to zero
                 }
                speedChanged = true;

                if (currentPPS == 0) {
                    currentState = STOPPED;
                    stop_step_timer();
                    gpio_put(ENABLE_PIN, 1); // Disable motor driver
                    std::cout << "State: STOPPED" << std::endl;
                    speedChanged = false; // No timer update needed when stopped
                }
            } else {
                 // Already at 0 PPS, ensure stopped state
                 if (currentState != STOPPED) { // Avoid redundant messages/actions
                     currentState = STOPPED;
                     stop_step_timer();
                     gpio_put(ENABLE_PIN, 1);
                     std::cout << "State: STOPPED (from zero check)" << std::endl;
                 }
                 // Safety check
                 if (timerActive) stop_step_timer();
                 if (gpio_get(ENABLE_PIN) == 0) gpio_put(ENABLE_PIN, 1);
            }
            break;

        case RUNNING:
            // Motor runs at TARGET_PPS. State change initiated by commands.
            // Do nothing here regarding speed change.
            break;

        case STOPPED:
            // Motor is stopped. State change initiated by commands.
            // Ensure motor remains disabled and timer stopped
            if (gpio_get(ENABLE_PIN) == 0 || timerActive) {
                gpio_put(ENABLE_PIN, 1);
                stop_step_timer();
            }
            break;
    }

    // If speed changed (and not stopped), update the timer
    if (speedChanged && currentState != STOPPED) {
        start_step_timer(currentPPS);
    }
}

// Displays the menu options
void display_menu() {
    std::cout << "\n--- Serial Control Menu ---" << std::endl;
    std::cout << "l: Load Simulation (Placeholder)" << std::endl;
    std::cout << "r: Run Simulation (Placeholder)" << std::endl;
    std::cout << "t: Run Motor Test (Accel to " << TARGET_PPS << " PPS)" << std::endl;
    std::cout << "s: Stop Motor Test (Decel to 0 PPS)" << std::endl;
    std::cout << "m: Show this Menu" << std::endl;
    std::cout << "Enter command: ";
    // Use flush to ensure the prompt appears immediately over serial
    std::cout.flush();
}

// Placeholder function for loading a simulation
void load_simulation_placeholder() {
    std::cout << "[Placeholder] Loading simulation..." << std::endl;
    // Add simulation loading logic here in the future
    display_menu(); // Show menu again after action
}

// Placeholder function for running a simulation
void run_simulation_placeholder() {
    std::cout << "[Placeholder] Running simulation..." << std::endl;
    // Add simulation running logic here in the future
    display_menu(); // Show menu again after action
}

// Starts the motor test sequence (acceleration)
void start_test_motor() {
    if (currentState == STOPPED) {
        std::cout << "Starting Motor Test..." << std::endl;
        currentState = ACCELERATING;
        currentPPS = START_PPS;
        gpio_put(ENABLE_PIN, 0); // Enable motor driver
        start_step_timer(currentPPS);
        std::cout << "State: ACCELERATING" << std::endl;
    } else {
        std::cout << "Motor is already running or changing state. Use 's' to stop first." << std::endl;
    }
    // Don't redisplay menu immediately, let user see state changes
}

// Initiates the motor stopping sequence (deceleration)
void stop_test_motor() {
    if (currentState == RUNNING || currentState == ACCELERATING) {
        std::cout << "Stopping Motor Test..." << std::endl;
        currentState = DECELERATING;
        std::cout << "State: DECELERATING" << std::endl;
        // update_motor_state will handle the ramp down
    } else if (currentState == STOPPED) {
         std::cout << "Motor is already stopped." << std::endl;
         display_menu(); // Show menu again
    } else { // Already decelerating
        std::cout << "Motor is already stopping." << std::endl;
    }
     // Don't redisplay menu immediately
}


// Processes a single character command from serial input
void handle_serial_input(char cmd) {
    std::cout << cmd << std::endl; // Echo command

    switch (cmd) {
        case 'l':
        case 'L':
            load_simulation_placeholder();
            break;
        case 'r':
        case 'R':
            run_simulation_placeholder();
            break;
        case 't':
        case 'T':
            start_test_motor();
            // Menu display handled within start/stop or implicitly by loop
            break;
        case 's':
        case 'S':
            stop_test_motor();
            // Menu display handled within start/stop or implicitly by loop
            break;
        case 'm':
        case 'M':
        case '?':
            display_menu();
            break;
        case '\n': // Ignore newline characters often sent by terminals
        case '\r': // Ignore carriage return characters
            break;
        default:
            std::cout << "Unknown command: '" << cmd << "'" << std::endl;
            display_menu();
            break;
    }
     // Flush output buffer after handling command if needed, especially if not ending with std::endl
     std::cout.flush();
}

// Initial setup function
void setup() {
    stdio_init_all(); // Initialize stdio (including USB CDC by default)

    // Wait for USB connection to be established (optional but good practice)
    // Increased wait time slightly to ensure serial monitor connects
    uint usb_wait_count = 0;
     std::cout << "Waiting for USB Serial connection..." << std::endl;
    while (!stdio_usb_connected() && usb_wait_count < 30) { // Wait up to 3 seconds
        sleep_ms(100);
        usb_wait_count++;
    }
    if (stdio_usb_connected()) {
       std::cout << "USB Serial connected." << std::endl;
    } else {
        std::cout << "Proceeding without USB Serial connection." << std::endl;
    }


    std::cout << "\n--- Stepper Control Initialized (Serial Menu) ---" << std::endl;

    init_gpio();
    init_motor();

    // === Removed Initial Switch State Reading ===
}

// Main loop
int main() {
    setup();
    display_menu(); // Show the menu for the first time

    uint64_t last_update_time = time_us_64();

    while (true) {
        // 1. Check for Serial Input (Non-Blocking)
        int c = getchar_timeout_us(0); // Check for character, return PICO_ERROR_TIMEOUT if none
        if (c != PICO_ERROR_TIMEOUT) {
            handle_serial_input((char)c); // Process the received character
        }

        // 2. Update Motor State Periodically (if not stopped)
        // This ensures acceleration/deceleration ramps happen smoothly
        // regardless of serial input.
        uint64_t now = time_us_64();
        if ( (currentState != STOPPED) && (now - last_update_time >= (uint64_t)UPDATE_INTERVAL_MS * 1000) ) {
             update_motor_state(); // Update speed ramps etc.
             last_update_time = now;
        } else if (currentState == STOPPED) {
             // Reset timer if stopped, so the next update check is accurate when started
             last_update_time = now;
        }


        // 3. Optional LED feedback (indicates motor is active/ramping/running)
        gpio_put(PICO_DEFAULT_LED_PIN, (currentState != STOPPED));

        // 4. Small delay to prevent the loop from consuming 100% CPU
        //    if update_interval is long or motor is stopped.
        //    Adjust if needed, can be very short.
        //    Alternatively, rely on the periodic nature of update_motor_state
        //    when active.
         sleep_us(100);

    }

    return 0; // Should never reach here
}