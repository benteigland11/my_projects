#include "servo_controller.h"
#include "hardware/pwm.h"
#include "hardware/gpio.h"
#include "hardware/clocks.h"
#include "pico/time.h"
#include <cmath>
#include <cstdio>
#include <iostream> // Needed for calibration output/input
#include <string>   // Needed for reading input
#include <cstring>  // Needed for strchr in input reading
#include <cstdlib>  // Needed for atof in input reading
#include <cctype>   // Needed for isdigit, isprint in input reading

// --- Constants ---
const float PWM_FREQUENCY = 50.0f; // Hz (20ms period) [cite: uploaded:my_projects/servo_controller.cpp]

// --- Internal Variables ---
static uint s_pwm_slice_num;
static pwm_config s_pwm_config;
static uint32_t s_pwm_wrap_value;

// Current servo pulse settings (microseconds) - Initialized with typical values
static float s_min_pulse_us = 600.0f;  // Default min [cite: uploaded:my_projects/servo_controller.cpp]
static float s_max_pulse_us = 2400.0f; // Default max [cite: uploaded:my_projects/servo_controller.cpp]

// --- Helper Functions ---

// Calculates the PWM level (duty cycle count) for a given pulse width in microseconds
static uint16_t calculate_pwm_level(float pulse_us) { // [cite: uploaded:my_projects/servo_controller.cpp]
    // Clamp pulse width to prevent going beyond theoretical limits if desired
    // (e.g., pulse_us = std::max(400.0f, std::min(2600.0f, pulse_us)); )

    float duty_cycle = pulse_us / (1000000.0f / PWM_FREQUENCY); // fraction of the period [cite: uploaded:my_projects/servo_controller.cpp]
    uint16_t level = static_cast<uint16_t>(roundf(duty_cycle * (s_pwm_wrap_value + 1))); // [cite: uploaded:my_projects/servo_controller.cpp]
    // Clamp level just in case
    if (level > s_pwm_wrap_value) level = s_pwm_wrap_value; // [cite: uploaded:my_projects/servo_controller.cpp]
    return level;
}

// Helper to read a float from serial (adapted from SerialMenu.cpp)
static float read_float_from_serial(const char* prompt) {
     char buffer[32];
     int index = 0;
     buffer[0] = '\0';

     std::cout << std::endl << prompt;
     std::cout.flush();

     while (index < sizeof(buffer) - 1) {
         int c = getchar(); // Blocking read

         if (c == PICO_ERROR_TIMEOUT || c == PICO_ERROR_NONE) continue;

         char ch = (char)c;

         if (ch == '\r' || ch == '\n') { // Handle Enter
             std::cout << std::endl;
             break;
         }
         else if ((ch == '\b' || ch == 127) && index > 0) { // Handle Backspace [cite: uploaded:my_projects/SerialMenu.cpp]
             index--;
             buffer[index] = '\0';
             std::cout << "\b \b";
             std::cout.flush();
         }
         // Handle valid number characters [cite: uploaded:my_projects/SerialMenu.cpp]
         else if (isdigit(ch) || (ch == '.' && strchr(buffer, '.') == nullptr) || (ch == '-' && index == 0)) {
              if (isprint(ch)) {
                  buffer[index++] = ch;
                  buffer[index] = '\0';
                  std::cout << ch;
                  std::cout.flush();
              }
         }
     }
     float value = atof(buffer); // [cite: uploaded:my_projects/SerialMenu.cpp]
     printf("Input converted to: %.1f\n", value);
     return value;
}


// --- Public Function Implementations ---

void servo_init() {
    printf("Initializing Servo PWM on GPIO %u...\n", SERVO_PIN); // [cite: uploaded:my_projects/servo_controller.cpp]

    gpio_set_function(SERVO_PIN, GPIO_FUNC_PWM); // [cite: uploaded:my_projects/servo_controller.cpp]
    s_pwm_slice_num = pwm_gpio_to_slice_num(SERVO_PIN); // [cite: uploaded:my_projects/servo_controller.cpp]

    uint32_t sys_clk_hz = clock_get_hz(clk_sys); // [cite: uploaded:my_projects/servo_controller.cpp]
    float divider = static_cast<float>(sys_clk_hz) / 1000000.0f; // [cite: uploaded:my_projects/servo_controller.cpp]
    if (divider < 1.0f) divider = 1.0f; // [cite: uploaded:my_projects/servo_controller.cpp]
    if (divider >= 256.0f) divider = 255.0f + 255.0f/256.0f; // [cite: uploaded:my_projects/servo_controller.cpp]
    s_pwm_wrap_value = static_cast<uint32_t>(roundf( (float)sys_clk_hz / (divider * PWM_FREQUENCY) )) - 1; // [cite: uploaded:my_projects/servo_controller.cpp]

    s_pwm_config = pwm_get_default_config(); // [cite: uploaded:my_projects/servo_controller.cpp]
    pwm_config_set_clkdiv(&s_pwm_config, divider); // [cite: uploaded:my_projects/servo_controller.cpp]
    pwm_config_set_wrap(&s_pwm_config, s_pwm_wrap_value); // [cite: uploaded:my_projects/servo_controller.cpp]
    pwm_init(s_pwm_slice_num, &s_pwm_config, true); // [cite: uploaded:my_projects/servo_controller.cpp]

    float effective_freq = (float)sys_clk_hz / (divider * (s_pwm_wrap_value + 1)); // [cite: uploaded:my_projects/servo_controller.cpp]
    printf("  PWM Slice: %u, Sys Clock: %lu Hz, Divider: %.2f, Wrap Val: %lu\n",
           s_pwm_slice_num, sys_clk_hz, divider, s_pwm_wrap_value); // [cite: uploaded:my_projects/servo_controller.cpp]
    printf("  Target Freq: %.1f Hz, Effective Freq: %.2f Hz\n", PWM_FREQUENCY, effective_freq); // [cite: uploaded:my_projects/servo_controller.cpp]

    // Set initial position using the *current* (default) max pulse width for position 0.0
    // This matches the behavior in the original main.cpp where it set position 1.0 initially
    // which used MAX_PULSE_US. Let's keep it consistent.
    printf("Setting initial servo position to 0.0 (using MAX pulse width %.1f us)...\n", s_max_pulse_us); // [cite: uploaded:my_projects/servo_controller.cpp]
    servo_set_position(0.0f); // Use the public function which now uses the variables


    printf("Servo Initialized (PWM started, set to position 0.0).\n");
}

void servo_set_position(float position) { // [cite: uploaded:my_projects/servo_controller.cpp]
    // Clamp position input
    if (position < 0.0f) position = 0.0f; // [cite: uploaded:my_projects/servo_controller.cpp]
    if (position > 1.0f) position = 1.0f; // [cite: uploaded:my_projects/servo_controller.cpp]

    // Interpolate pulse width using the *current* min/max settings
    float pulse_us = s_min_pulse_us + (s_max_pulse_us - s_min_pulse_us) * position; // Uses variables now
    uint16_t level = calculate_pwm_level(pulse_us); // [cite: uploaded:my_projects/servo_controller.cpp]

    // Debug Print
    printf("DEBUG SERVO: set_position(%.2f) -> pulse=%.0fus (min=%.0f, max=%.0f), level=%u (wrap=%lu)\n",
           position, pulse_us, s_min_pulse_us, s_max_pulse_us, level, s_pwm_wrap_value); // [cite: uploaded:my_projects/servo_controller.cpp]

    // Set the PWM duty cycle
    pwm_set_gpio_level(SERVO_PIN, level); // [cite: uploaded:my_projects/servo_controller.cpp]
}

// --- Getter/Setter Implementations ---
float servo_get_min_pulse_us() {
    return s_min_pulse_us;
}

void servo_set_min_pulse_us(float us) {
    // Add validation if desired (e.g., check > 0, check < max_pulse)
    printf("Setting min pulse width to: %.1f us\n", us);
    s_min_pulse_us = us;
}

float servo_get_max_pulse_us() {
    return s_max_pulse_us;
}

void servo_set_max_pulse_us(float us) {
    // Add validation if desired (e.g., check > 0, check > min_pulse)
    printf("Setting max pulse width to: %.1f us\n", us);
    s_max_pulse_us = us;
}


// --- Calibration Function Implementation ---
void servo_calibrate() {
    printf("\n--- Servo Calibration ---");
    printf("\nCurrent Min Pulse: %.1f us", servo_get_min_pulse_us());
    printf("\nCurrent Max Pulse: %.1f us", servo_get_max_pulse_us());

    // Get New Min Pulse
    float new_min = read_float_from_serial("\nEnter new MIN pulse width (us): ");
    if (new_min > 0) { // Basic validation: must be positive
        servo_set_min_pulse_us(new_min);
        printf("Moving servo to new MIN (position 0.0)...");
        servo_set_position(0.0f);
        sleep_ms(1000); // Pause to observe
        printf(" Done.\n");
    } else {
        printf("Invalid input for min pulse. Keeping current value.\n");
    }

    // Get New Max Pulse
    float new_max = read_float_from_serial("\nEnter new MAX pulse width (us): ");
     if (new_max > 0 && new_max > servo_get_min_pulse_us()) { // Basic validation: positive and > min
        servo_set_max_pulse_us(new_max);
        printf("Moving servo to new MAX (position 1.0)...");
        servo_set_position(1.0f);
        sleep_ms(1000); // Pause to observe
        printf(" Done.\n");
    } else {
        printf("Invalid input for max pulse (must be > 0 and > min pulse). Keeping current value.\n");
    }

    // Optional: Move back to center or min?
    printf("Returning servo to start (position 1)...");
    servo_set_position(1.0f);
    sleep_ms(500);
    printf(" Done.\n");

    printf("--- Servo Calibration Complete ---\n");
    // NOTE: These settings are NOT saved persistently. They will reset on reboot.
    // Saving requires extra code (e.g., writing to flash).
}


// Removed: void servo_test_sweep() implementation [cite: uploaded:my_projects/servo_controller.cpp]