#ifndef SERVO_CONTROLLER_H
#define SERVO_CONTROLLER_H

#include "stdlib.h"

// --- Configuration ---
// Define the GPIO pin connected to the servo signal line
// Ensure this pin is not used by other peripherals (stepper, SD card)
const int SERVO_PIN = 15; // Example pin, change if needed [cite: uploaded:my_projects/servo_controller.h]

// --- Public Function Declarations ---

/**
 * @brief Initializes the PWM hardware for the servo pin.
 * Must be called once during setup before using other servo functions.
 */
void servo_init(); // [cite: uploaded:my_projects/servo_controller.h]

/**
 * @brief Sets the servo position based on current min/max pulse settings.
 * @param position A value from 0.0 (min pulse) to 1.0 (max pulse).
 * Values outside this range will be clamped.
 */
void servo_set_position(float position); // [cite: uploaded:my_projects/servo_controller.h]

/**
 * @brief Gets the current minimum servo pulse width in microseconds.
 * @return Minimum pulse width (us).
 */
float servo_get_min_pulse_us();

/**
 * @brief Sets the minimum servo pulse width.
 * @param us New minimum pulse width in microseconds.
 */
void servo_set_min_pulse_us(float us);

/**
 * @brief Gets the current maximum servo pulse width in microseconds.
 * @return Maximum pulse width (us).
 */
float servo_get_max_pulse_us();

/**
 * @brief Sets the maximum servo pulse width.
 * @param us New maximum pulse width in microseconds.
 */
void servo_set_max_pulse_us(float us);

/**
 * @brief Enters a blocking serial routine to calibrate servo min/max pulse widths.
 */
void servo_calibrate();

// Removed: void servo_test_sweep();

#endif // SERVO_CONTROLLER_H