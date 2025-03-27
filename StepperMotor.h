#ifndef STEPPER_MOTOR_H
#define STEPPER_MOTOR_H

#include "pico/stdlib.h"

// --- Public Types ---
enum MotorState {
    MOTOR_STOPPED,
    MOTOR_ACCELERATING,
    MOTOR_RUNNING,
    MOTOR_DECELERATING
};

// --- Public Function Declarations ---

/**
 * @brief Initializes GPIO pins and internal state for the stepper motor.
 * Call this once during setup.
 */
void motor_init();

/**
 * @brief Updates the motor's speed and state (handles acceleration/deceleration).
 * Call this periodically in the main loop when the motor is not stopped.
 */
void motor_update_state();

/**
 * @brief Starts the motor test sequence (initiates acceleration).
 */
void motor_start_test();

/**
 * @brief Stops the motor test sequence (initiates deceleration).
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


#endif // STEPPER_MOTOR_H