#include "StepperMotor.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"
#include <iostream> // For error messages

// --- Module-Internal Configuration ---
const uint STEP_PIN = 0;
const uint DIR_PIN = 1;
const uint ENABLE_PIN = 2; // Active LOW enable

const int TARGET_PPS = 1200;
const int START_PPS = 100;
const int ACCEL_STEP_PPS = 20;
const int DECEL_STEP_PPS = 50;

// --- Module-Internal State Variables (Encapsulated) ---
// Using 'static' limits the scope of these variables to this file.
static volatile MotorState s_currentState = MOTOR_STOPPED;
static volatile int s_currentPPS = 0;
static volatile bool s_stepPinState = false;
static struct repeating_timer s_stepTimer;
static volatile bool s_timerActive = false;

// --- Module-Internal Helper Function Declarations ---
static bool step_timer_callback(struct repeating_timer *t);
static int64_t pps_to_delay_us(int pps);
static void start_step_timer(int pps);
static void stop_step_timer();

// --- Public Function Implementations ---

void motor_init() {
    // Initialize Stepper GPIO pins (moved from general init_gpio)
    gpio_init(STEP_PIN);
    gpio_init(DIR_PIN);
    gpio_init(ENABLE_PIN);
    gpio_set_dir(STEP_PIN, GPIO_OUT);
    gpio_set_dir(DIR_PIN, GPIO_OUT);
    gpio_set_dir(ENABLE_PIN, GPIO_OUT);

    // Set initial motor hardware state
    gpio_put(DIR_PIN, 1);    // Set default direction
    gpio_put(ENABLE_PIN, 1); // Start disabled
    gpio_put(STEP_PIN, 0);   // Ensure step pin is low
    sleep_ms(10); // Allow driver to settle

    // Initialize internal state variables
    s_currentState = MOTOR_STOPPED;
    s_currentPPS = 0;
    s_timerActive = false;
    s_stepPinState = false;
}

void motor_update_state() {
    bool speedChanged = false;

    switch (s_currentState) {
        case MOTOR_ACCELERATING:
            if (s_currentPPS < TARGET_PPS) {
                s_currentPPS += ACCEL_STEP_PPS;
                if (s_currentPPS > TARGET_PPS) {
                    s_currentPPS = TARGET_PPS;
                }
                speedChanged = true;
            } else {
                s_currentPPS = TARGET_PPS;
                s_currentState = MOTOR_RUNNING;
                speedChanged = true; // Update timer to exact target freq
                std::cout << "State: RUNNING at " << s_currentPPS << " PPS" << std::endl;
            }
            break;

        case MOTOR_DECELERATING:
            if (s_currentPPS > 0) {
                s_currentPPS -= DECEL_STEP_PPS;
                 if (s_currentPPS <= 0) {
                    s_currentPPS = 0;
                 }
                speedChanged = true;

                if (s_currentPPS == 0) {
                    s_currentState = MOTOR_STOPPED;
                    stop_step_timer();
                    gpio_put(ENABLE_PIN, 1); // Disable motor driver
                    std::cout << "State: STOPPED" << std::endl;
                    speedChanged = false;
                }
            } else {
                 if (s_currentState != MOTOR_STOPPED) {
                     s_currentState = MOTOR_STOPPED;
                     stop_step_timer();
                     gpio_put(ENABLE_PIN, 1);
                     std::cout << "State: STOPPED (from zero check)" << std::endl;
                 }
                 if (s_timerActive) stop_step_timer();
                 if (gpio_get(ENABLE_PIN) == 0) gpio_put(ENABLE_PIN, 1);
            }
            break;

        case MOTOR_RUNNING:
            // State change initiated externally by motor_stop_test()
            break;

        case MOTOR_STOPPED:
            // State change initiated externally by motor_start_test()
            // Ensure motor remains disabled and timer stopped
            if (gpio_get(ENABLE_PIN) == 0 || s_timerActive) {
                gpio_put(ENABLE_PIN, 1);
                stop_step_timer();
            }
            break;
    }

    if (speedChanged && s_currentState != MOTOR_STOPPED) {
        start_step_timer(s_currentPPS);
    }
}

void motor_start_test() {
    if (s_currentState == MOTOR_STOPPED) {
        std::cout << "Starting Motor Test..." << std::endl;
        s_currentState = MOTOR_ACCELERATING;
        s_currentPPS = START_PPS;
        gpio_put(ENABLE_PIN, 0); // Enable motor driver
        start_step_timer(s_currentPPS);
        std::cout << "State: ACCELERATING" << std::endl;
    } else {
        std::cout << "Motor is already running or changing state. Use 's' to stop first." << std::endl;
    }
}

void motor_stop_test() {
    if (s_currentState == MOTOR_RUNNING || s_currentState == MOTOR_ACCELERATING) {
        std::cout << "Stopping Motor Test..." << std::endl;
        s_currentState = MOTOR_DECELERATING;
        std::cout << "State: DECELERATING" << std::endl;
    } else if (s_currentState == MOTOR_STOPPED) {
         std::cout << "Motor is already stopped." << std::endl;
         // Maybe call menu_display() here or let main loop handle it?
    } else { // Already decelerating
        std::cout << "Motor is already stopping." << std::endl;
    }
}

MotorState motor_get_state() {
    return s_currentState;
}

int motor_get_current_pps() {
    return s_currentPPS;
}


// --- Module-Internal Helper Function Implementations ---

static int64_t pps_to_delay_us(int pps) {
    if (pps <= 0) {
        return -1;
    }
    // Delay is for half a pulse period (toggle on/off)
    return (int64_t)(1000000.0 / (2.0 * pps));
}

// Timer callback - Toggles the step pin
// This function MUST have this exact signature to be used by add_repeating_timer_us
static bool step_timer_callback(struct repeating_timer *t) {
    // Access module-static variable
    s_stepPinState = !s_stepPinState;
    gpio_put(STEP_PIN, s_stepPinState);
    return true; // Keep timer running
}

// Starts or updates the repeating timer for step pulses
static void start_step_timer(int pps) {
    if (s_timerActive) {
        cancel_repeating_timer(&s_stepTimer);
        s_timerActive = false;
    }
    int64_t delay_us = pps_to_delay_us(pps);
    if (delay_us > 0) {
        // Use the module-static callback function
        if (add_repeating_timer_us(delay_us, step_timer_callback, NULL, &s_stepTimer)) {
            s_timerActive = true;
        } else {
            std::cout << "Error: Failed to add repeating timer!" << std::endl;
            s_currentState = MOTOR_STOPPED;
            s_currentPPS = 0;
            gpio_put(ENABLE_PIN, 1); // Ensure motor is disabled on error
        }
    } else {
         // If pps <= 0, ensure timer is stopped and pin is low
         if (s_timerActive) { // Should have been caught by stop logic, but safety first
             cancel_repeating_timer(&s_stepTimer);
             s_timerActive = false;
         }
         s_stepPinState = false;
         gpio_put(STEP_PIN, 0);
    }
}

// Stops the repeating timer and ensures the step pin is low
static void stop_step_timer() {
    if (s_timerActive) {
        cancel_repeating_timer(&s_stepTimer);
        s_timerActive = false;
    }
    s_stepPinState = false;
    gpio_put(STEP_PIN, 0);
}