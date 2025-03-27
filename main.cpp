#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"
#include <pico/stdio_usb.h>
#include <iostream>

// --- Configuration ---
// Stepper Driver Pins
const uint STEP_PIN = 0;
const uint DIR_PIN = 1;
const uint ENABLE_PIN = 2; // Active LOW enable often used

// Switch Pin (Connect switch between this pin and GND)
const uint SWITCH_PIN = 14; // <<< Use this pin for your toggle switch

// Motor Speed & Acceleration Parameters
const int TARGET_PPS = 1200;      // Target speed in Pulses Per Second
const int START_PPS = 100;        // Starting speed for acceleration
const int ACCEL_STEP_PPS = 20;    // How much to increase PPS per update during acceleration
const int DECEL_STEP_PPS = 50;    // How much to decrease PPS per update during deceleration
const uint UPDATE_INTERVAL_MS = 20; // How often to check switch and update speed (adjust if needed)

// --- Debounce Configuration ---
// How many consecutive readings of the same state are needed to confirm it.
// Total debounce time = REQUIRED_CONSECUTIVE_SAMPLES * UPDATE_INTERVAL_MS
// Example: 4 * 20ms = 80ms debounce time. Adjust as needed for your switch.
const int REQUIRED_CONSECUTIVE_SAMPLES = 4;

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

// --- Switch Debounce State Variables ---
// volatile uint64_t lastSwitchChangeTime = 0; // No longer needed
volatile bool lastStableSwitchState = true;   // The confirmed, debounced state (true = OFF/HIGH)
volatile bool integratingSwitchState = true;  // The state currently being sampled/counted
volatile int consecutiveCount = 0;            // Counter for consecutive samples

// --- Function Declarations ---
void init_gpio();
void init_motor();
bool step_timer_callback(struct repeating_timer *t);
int64_t pps_to_delay_us(int pps);
void start_step_timer(int pps);
void stop_step_timer();
void handle_switch(); // Uses new debounce logic
void update_motor_state();
void setup();

// --- Function Implementations ---

// Calculates the required timer delay (in microseconds) between STEP pin toggles
int64_t pps_to_delay_us(int pps) {
    if (pps <= 0) {
        return -1;
    }
    return (int64_t)(1000000.0 / (2.0 * pps));
}

// Timer callback - simply toggles the step pin
bool step_timer_callback(struct repeating_timer *t) {
    stepPinState = !stepPinState;
    gpio_put(STEP_PIN, stepPinState);
    return true;
}

// Starts or updates the repeating timer for step pulses
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
         gpio_put(STEP_PIN, 0);
    }
}

// Stops the repeating timer and ensures the step pin is low
void stop_step_timer() {
    if (timerActive) {
        cancel_repeating_timer(&stepTimer);
        timerActive = false;
    }
    stepPinState = false;
    gpio_put(STEP_PIN, 0);
}

// Initialize GPIO pins
void init_gpio() {
    // Stepper pins
    gpio_init(STEP_PIN);
    gpio_init(DIR_PIN);
    gpio_init(ENABLE_PIN);
    gpio_set_dir(STEP_PIN, GPIO_OUT);
    gpio_set_dir(DIR_PIN, GPIO_OUT);
    gpio_set_dir(ENABLE_PIN, GPIO_OUT);

    // Switch pin
    gpio_init(SWITCH_PIN);
    gpio_set_dir(SWITCH_PIN, GPIO_IN);
    gpio_pull_up(SWITCH_PIN); // Use internal pull-up resistor

    // Onboard LED
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    gpio_put(PICO_DEFAULT_LED_PIN, 0);
}

// Set initial motor direction and disable state
void init_motor() {
    gpio_put(DIR_PIN, 1);
    gpio_put(ENABLE_PIN, 1); // Start disabled
    gpio_put(STEP_PIN, 0);
    sleep_ms(10);
}

// Reads switch state with counter-based debouncing and triggers state changes
void handle_switch() {
    bool currentPhysicalSwitchState = gpio_get(SWITCH_PIN); // Read the raw pin state

    // --- Counter-Based Debounce Logic ---
    if (currentPhysicalSwitchState == integratingSwitchState) {
        // Physical state matches the state we are currently counting
        if (consecutiveCount < REQUIRED_CONSECUTIVE_SAMPLES) {
             consecutiveCount++; // Increment count towards stability threshold
        }
    } else {
        // Physical state has changed from the one we were counting
        integratingSwitchState = currentPhysicalSwitchState; // Start counting the new state
        consecutiveCount = 1; // Reset count for this new state
    }

    // Check if the stability threshold has been reached
    if (consecutiveCount >= REQUIRED_CONSECUTIVE_SAMPLES) {
        // We have enough consecutive samples of 'integratingSwitchState'
        if (integratingSwitchState != lastStableSwitchState) {
            // And this stable state is DIFFERENT from the previously known stable state
            lastStableSwitchState = integratingSwitchState; // Update the stable state
            std::cout << "Switch Stable State Changed To: "
                      << (lastStableSwitchState ? "OFF (HIGH)" : "ON (LOW)")
                      << std::endl;
            // Note: We don't set stableSwitchStateChanged here, the main logic below uses lastStableSwitchState directly
        }
        // If integratingSwitchState == lastStableSwitchState, it just means the state remains stable.
    }
    // --- End Debounce Logic ---


    // --- Determine Desired Motor Action based on the *Debounced* Stable Switch State ---
    // Remember: With pull-up, LOW (false) means switch is ON/Closed, HIGH (true) means OFF/Open.
    bool shouldMotorRun = !lastStableSwitchState; // Motor should run if switch is ON (debounced LOW)

    // --- Update State Machine Based on Desired Action ---
    // This logic only acts based on the confirmed 'lastStableSwitchState'
    if (shouldMotorRun) {
        // Switch is ON - motor should be running or accelerating
        if (currentState == STOPPED) {
            currentState = ACCELERATING;
            currentPPS = START_PPS;
            gpio_put(ENABLE_PIN, 0); // Enable motor driver
            start_step_timer(currentPPS);
            std::cout << "State: ACCELERATING" << std::endl;
        }
        // If already ACCELERATING or RUNNING, do nothing here (update_motor_state handles it)
    } else {
        // Switch is OFF - motor should be stopping or stopped
        if (currentState == RUNNING || currentState == ACCELERATING) {
            currentState = DECELERATING;
            std::cout << "State: DECELERATING" << std::endl;
        }
        // If already DECELERATING or STOPPED, do nothing here (update_motor_state handles it)
    }
}

// Updates motor speed based on the current state (acceleration/deceleration)
// This function implicitly uses the 'lastStableSwitchState' updated by handle_switch()
// to check if the switch was flipped during ramps.
void update_motor_state() {
    bool speedChanged = false;

    switch (currentState) {
        case ACCELERATING:
            // Check if switch turned OFF during acceleration
            if (lastStableSwitchState == true) { // Switch is OFF (debounced HIGH)
                 currentState = DECELERATING;
                 std::cout << "State: DECELERATING (Switch OFF during Accel)" << std::endl;
                 // Fallthrough to start decelerating immediately
            } else {
                // Switch is still ON, continue accelerating
                if (currentPPS < TARGET_PPS) {
                    currentPPS += ACCEL_STEP_PPS;
                    if (currentPPS > TARGET_PPS) {
                        currentPPS = TARGET_PPS;
                    }
                    speedChanged = true;
                } else {
                    currentPPS = TARGET_PPS;
                    currentState = RUNNING;
                    speedChanged = true;
                    std::cout << "State: RUNNING at " << currentPPS << " PPS" << std::endl;
                }
                break; // Only break if still accelerating
            }
            // Fallthrough to DECELERATING if switch was turned off

        case DECELERATING:
             // Check if switch turned back ON during deceleration
             if (lastStableSwitchState == false) { // Switch is ON (debounced LOW)
                 currentState = ACCELERATING; // Re-accelerate
                 std::cout << "State: ACCELERATING (Switch ON during Decel)" << std::endl;
                 speedChanged = true; // Need to update timer based on current (lower) PPS
             } else {
                 // Switch is still OFF, continue decelerating
                 if (currentPPS > 0) {
                     currentPPS -= DECEL_STEP_PPS;
                     // Optional: Add a floor slightly above 0 for smoother stop before disabling?
                     // if (currentPPS < START_PPS / 2 && currentPPS > 0) {
                     //     currentPPS = START_PPS / 2;
                     // }
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
             }
            break;

        case RUNNING:
            // Check if switch turned OFF while running
             if (lastStableSwitchState == true) { // Switch is OFF (debounced HIGH)
                 currentState = DECELERATING;
                 std::cout << "State: DECELERATING (Switch OFF during Run)" << std::endl;
             }
            // No speed change needed if switch remains ON
            break;

        case STOPPED:
             // State change from STOPPED is handled in handle_switch()
             // Ensure motor remains disabled and timer stopped
             if (gpio_get(ENABLE_PIN) == 0 || timerActive) {
                 gpio_put(ENABLE_PIN, 1);
                 stop_step_timer();
                 // std::cout << "Corrected state in STOPPED" << std::endl; // Can be noisy
             }
            break;
    }

    // If speed changed (and not stopped), update the timer
    if (speedChanged && currentState != STOPPED) {
        start_step_timer(currentPPS);
    }
}


// Initial setup function
void setup() {
    stdio_init_all();
    // Reducing wait time as USB connection is optional for core functionality
    uint usb_wait_count = 0;
    while (usb_wait_count < 15) { // Wait up to 1.5 seconds
        sleep_ms(100);
        usb_wait_count++;
    }
    std::cout << "--- Stepper Control Initialized (Single Switch - Counter Debounce) ---" << std::endl;
    std::cout << "Toggle Switch (GPIO" << SWITCH_PIN << ") ON to run, OFF to stop." << std::endl;
    std::cout << "Debounce requires " << REQUIRED_CONSECUTIVE_SAMPLES << " samples ("
              << REQUIRED_CONSECUTIVE_SAMPLES * UPDATE_INTERVAL_MS << "ms)." << std::endl;

    init_gpio();
    init_motor();

    // Initialize debounce state based on initial reading after setup
    sleep_ms(50); // Allow pull-up to settle well
    lastStableSwitchState = gpio_get(SWITCH_PIN);
    integratingSwitchState = lastStableSwitchState;
    consecutiveCount = REQUIRED_CONSECUTIVE_SAMPLES; // Assume initial state is stable
    std::cout << "Initial Switch State: " << (lastStableSwitchState ? "OFF (HIGH)" : "ON (LOW)") << std::endl;
}

// Main loop
int main() {
    setup();

    while (true) {
        handle_switch();       // Check switch state & perform debouncing
        update_motor_state();  // Update speed/state based on debounced switch state

        // Optional LED feedback
        gpio_put(PICO_DEFAULT_LED_PIN, (currentState != STOPPED));

        sleep_ms(UPDATE_INTERVAL_MS); // Control loop rate & debounce sample interval
    }

    return 0;
}