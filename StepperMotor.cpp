#include "StepperMotor.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"
#include <iostream> // For error messages/state changes
#include <cmath>    // For floorf

// --- Module-Internal Configuration ---
const uint STEP_PIN = 0;  // From original file [cite: uploaded:my_projects/StepperMotor.cpp]
const uint DIR_PIN = 1;   // From original file [cite: uploaded:my_projects/StepperMotor.cpp]
const uint ENABLE_PIN = 2; // Active LOW enable [cite: uploaded:my_projects/StepperMotor.cpp]

// Configuration for the test run ('t' command)
const int TARGET_PPS = 1200;       // From original file [cite: uploaded:my_projects/StepperMotor.cpp]
const int START_PPS = 100;         // From original file [cite: uploaded:my_projects/StepperMotor.cpp]
const int ACCEL_STEP_PPS = 20;     // From original file [cite: uploaded:my_projects/StepperMotor.cpp]
const int DECEL_STEP_PPS = 50;     // From original file [cite: uploaded:my_projects/StepperMotor.cpp]

// --- Module-Internal State Variables (Encapsulated) ---
static volatile MotorState s_currentState = MOTOR_STOPPED;     // From original file [cite: uploaded:my_projects/StepperMotor.cpp]
static volatile int s_currentPPS = 0;                          // From original file [cite: uploaded:my_projects/StepperMotor.cpp]
static volatile bool s_stepPinState = false;                   // From original file [cite: uploaded:my_projects/StepperMotor.cpp]
static struct repeating_timer s_stepTimer;                     // From original file [cite: uploaded:my_projects/StepperMotor.cpp]
static volatile bool s_timerActive = false;                    // From original file [cite: uploaded:my_projects/StepperMotor.cpp]

// --- Module-Internal Helper Function Declarations ---
static bool step_timer_callback(struct repeating_timer *t);    // From original file [cite: uploaded:my_projects/StepperMotor.cpp]
static int64_t pps_to_delay_us(int pps);                       // From original file [cite: uploaded:my_projects/StepperMotor.cpp]
static void start_step_timer(int pps);                         // From original file [cite: uploaded:my_projects/StepperMotor.cpp]
static void stop_step_timer();                                 // From original file [cite: uploaded:my_projects/StepperMotor.cpp]

// --- Public Function Implementations ---

void motor_init() {                                           // From original file [cite: uploaded:my_projects/StepperMotor.cpp]
    // Initialize Stepper GPIO pins
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

// *** MODIFIED motor_update_state ***
void motor_update_state() {
    bool speedChanged = false;

    switch (s_currentState) {
        case MOTOR_ACCELERATING:                                // Logic from original file [cite: uploaded:my_projects/StepperMotor.cpp]
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

        case MOTOR_DECELERATING:                                // Logic modified slightly from original [cite: uploaded:my_projects/StepperMotor.cpp]
            if (s_currentPPS > 0) {
                s_currentPPS -= DECEL_STEP_PPS;
                 if (s_currentPPS <= 0) {
                    s_currentPPS = 0;
                    // State change now handled below
                 }
                speedChanged = true; // Speed potentially changed

                // Check if deceleration resulted in zero speed
                if (s_currentPPS == 0 && s_currentState != MOTOR_STOPPED) {
                    s_currentState = MOTOR_STOPPED;
                    stop_step_timer();
                    gpio_put(ENABLE_PIN, 1); // Disable motor driver
                    std::cout << "State: STOPPED" << std::endl;
                    speedChanged = false; // Speed is now 0, no timer update needed
                }
            } else { // Should already be 0 PPS if decelerating
                 if (s_currentState != MOTOR_STOPPED) {
                     s_currentState = MOTOR_STOPPED;
                     stop_step_timer();
                     gpio_put(ENABLE_PIN, 1);
                     std::cout << "State: STOPPED (from zero check)" << std::endl;
                 }
                 // Ensure timer/enable are correct if somehow missed
                 if (s_timerActive) stop_step_timer();
                 if (gpio_get(ENABLE_PIN) == 0) gpio_put(ENABLE_PIN, 1);
            }
            break;

        case MOTOR_RUNNING:                                     // Logic from original file [cite: uploaded:my_projects/StepperMotor.cpp]
            // State change initiated externally by motor_stop_test()
            // Or maybe by motor_set_target_frequency(0)?
            break;

        // *** ADDED Case for MOTOR_SIMULATING ***
        case MOTOR_SIMULATING:
            // Speed is controlled directly by motor_set_target_frequency.
            // motor_update_state does nothing in this state regarding speed.
            speedChanged = false;
            break;

        case MOTOR_STOPPED:                                     // Logic from original file [cite: uploaded:my_projects/StepperMotor.cpp]
            // State change initiated externally
            // Ensure motor remains disabled and timer stopped
            if (gpio_get(ENABLE_PIN) == 0 || s_timerActive) {
                gpio_put(ENABLE_PIN, 1);
                stop_step_timer();
            }
            break;
    }

    // Only update timer if speed changed during ACCEL/DECEL phases
    if (speedChanged && (s_currentState == MOTOR_ACCELERATING || s_currentState == MOTOR_DECELERATING)) {
         start_step_timer(s_currentPPS);
    }
}

void motor_start_test() {                                      // From original file [cite: uploaded:my_projects/StepperMotor.cpp]
    if (s_currentState == MOTOR_STOPPED) {
        std::cout << "Starting Motor Test..." << std::endl;
        s_currentState = MOTOR_ACCELERATING;
        s_currentPPS = START_PPS;
        gpio_put(ENABLE_PIN, 0); // Enable motor driver
        start_step_timer(s_currentPPS);
        std::cout << "State: ACCELERATING" << std::endl;
    } else {
        std::cout << "Motor is not stopped. Use 's' to stop first." << std::endl;
    }
}

// *** MODIFIED motor_stop_test ***
void motor_stop_test() {                                      // Modified from original [cite: uploaded:my_projects/StepperMotor.cpp]
    // Now also handles stopping from SIMULATING state
    if (s_currentState == MOTOR_RUNNING || s_currentState == MOTOR_ACCELERATING || s_currentState == MOTOR_SIMULATING) {
        std::cout << "Stopping Motor..." << std::endl;
        // If simulating, just set speed to 0 directly. If test run, decelerate.
        if (s_currentState == MOTOR_SIMULATING) {
            motor_set_target_frequency(0.0f); // Go directly to 0 PPS and STOPPED state
            // The state change message happens inside motor_set_target_frequency
        } else {
             s_currentState = MOTOR_DECELERATING; // Initiate test run deceleration
             std::cout << "State: DECELERATING" << std::endl;
        }
    } else if (s_currentState == MOTOR_STOPPED) {
         std::cout << "Motor is already stopped." << std::endl;
    } else { // Already decelerating
        std::cout << "Motor is already stopping." << std::endl;
    }
}

MotorState motor_get_state() {                                // From original file [cite: uploaded:my_projects/StepperMotor.cpp]
    return s_currentState;
}

int motor_get_current_pps() {                                 // From original file [cite: uploaded:my_projects/StepperMotor.cpp]
    return s_currentPPS;
}

// *** ADDED Implementation for motor_set_target_frequency ***
/**
 * @brief Sets the target rotational frequency for the motor directly.
 */
void motor_set_target_frequency(float pps) {
    int target_pps_int = static_cast<int>(floorf(pps + 0.5f)); // Round to nearest int

    if (target_pps_int < 0) {
        target_pps_int = 0;
    }

    // Check if the target integer PPS requires a state/timer change
    bool needs_update = false;
    if (target_pps_int != s_currentPPS) needs_update = true;
    if (target_pps_int > 0 && s_currentState == MOTOR_STOPPED) needs_update = true;
    if (target_pps_int == 0 && s_currentState != MOTOR_STOPPED) needs_update = true;

    if (needs_update) {
        s_currentPPS = target_pps_int; // Update internal PPS tracking

        if (s_currentPPS > 0) {
            // If stopped, enable motor and set state before starting timer
            if (s_currentState == MOTOR_STOPPED) {
                 std::cout << "Simulation enabling motor." << std::endl;
                 gpio_put(ENABLE_PIN, 0); // Enable driver
                 s_currentState = MOTOR_SIMULATING; // Set new state
                 std::cout << "State: SIMULATING" << std::endl;
            } else if (s_currentState != MOTOR_SIMULATING) {
                 // If coming from test run states, force into simulating mode
                 s_currentState = MOTOR_SIMULATING;
                 std::cout << "State: SIMULATING (override)" << std::endl;
            }
            // Start or update the timer with the new frequency
            start_step_timer(s_currentPPS);
        } else { // target_pps_int is 0
            if (s_currentState != MOTOR_STOPPED) {
                stop_step_timer();
                gpio_put(ENABLE_PIN, 1); // Disable driver
                s_currentState = MOTOR_STOPPED;
                std::cout << "State: STOPPED (via set_target_frequency(0))" << std::endl;
            }
            // If already stopped, do nothing further
        }
    }
}


// --- Module-Internal Helper Function Implementations ---

static int64_t pps_to_delay_us(int pps) {                     // From original file [cite: uploaded:my_projects/StepperMotor.cpp]
    if (pps <= 0) {
        return -1; // Indicate invalid/stop
    }
    // Delay is for half a pulse period (timer toggles pin)
    return (int64_t)(1000000.0 / (2.0 * pps));
}

static bool step_timer_callback(struct repeating_timer *t) {  // From original file [cite: uploaded:my_projects/StepperMotor.cpp]
    s_stepPinState = !s_stepPinState;
    gpio_put(STEP_PIN, s_stepPinState);
    return true; // Keep timer running
}

static void start_step_timer(int pps) {                       // From original file [cite: uploaded:my_projects/StepperMotor.cpp]
    // Stop existing timer if active
    if (s_timerActive) {
        cancel_repeating_timer(&s_stepTimer);
        s_timerActive = false;
    }
    // Calculate new delay
    int64_t delay_us = pps_to_delay_us(pps);
    if (delay_us > 0) {
        // Add the new timer
        if (add_repeating_timer_us(delay_us, step_timer_callback, NULL, &s_stepTimer)) {
            s_timerActive = true;
        } else {
            std::cout << "Error: Failed to add repeating timer!" << std::endl;
            // Try to recover safely
            s_currentState = MOTOR_STOPPED;
            s_currentPPS = 0;
            gpio_put(ENABLE_PIN, 1);
            s_stepPinState = false;
            gpio_put(STEP_PIN, 0);
        }
    } else {
         // If pps <= 0, ensure timer is stopped and pin is low (handled by caller too, but safe)
         s_stepPinState = false;
         gpio_put(STEP_PIN, 0);
    }
}

static void stop_step_timer() {                              // From original file [cite: uploaded:my_projects/StepperMotor.cpp]
    if (s_timerActive) {
        cancel_repeating_timer(&s_stepTimer);
        s_timerActive = false;
    }
    // Ensure step pin is left in a known low state
    s_stepPinState = false;
    gpio_put(STEP_PIN, 0);
}