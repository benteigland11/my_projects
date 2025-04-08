#ifndef STEPPER_MOTOR_H
#define STEPPER_MOTOR_H

#include "pico/stdlib.h"

// --- Public Types ---
enum MotorState {
    MOTOR_STOPPED,
    MOTOR_ACCELERATING, // For test run
    MOTOR_RUNNING,      // For test run
    MOTOR_DECELERATING, // For test run
    MOTOR_SIMULATING    // New state for direct frequency control
};

// --- Public Function Declarations ---

/**
 * @brief Initializes GPIO pins and internal state for the stepper motor.
 * Call this once during setup.
 */
void motor_init();

/**
 * @brief Updates the motor's speed and state (handles acceleration/deceleration for test mode).
 * Call this periodically in the main loop. In SIMULATING mode, this function does little.
 */
void motor_update_state();

/**
 * @brief Starts the motor test sequence (initiates acceleration).
 */
void motor_start_test();

/**
 * @brief Stops the motor test sequence (initiates deceleration) or stops simulation immediately.
 */
void motor_stop_test();

/**
 * @brief Gets the current state of the motor.
 * @return The current MotorState.
 */
MotorState motor_get_state();

/**
 * @brief Gets the current target/actual speed of the motor in PPS.
 * @return The current PPS value.
 */
int motor_get_current_pps();

/**
 * @brief Sets the target rotational frequency for the motor, bypassing accel/decel.
 * Used for running the simulation profile directly. Puts motor in SIMULATING state.
 * @param pps Target frequency in pulses per second (Hz). Negative values are treated as 0.
 */
void motor_set_target_frequency(float pps);


#endif // STEPPER_MOTOR_H