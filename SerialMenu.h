#ifndef SERIAL_MENU_H
#define SERIAL_MENU_H

#include "openrocket_parser.h" // [cite: uploaded:my_projects/SerialMenu.h]

// --- Public Types ---

// Define different states for the menu system
typedef enum {
    MENU_STATE_MAIN,
    MENU_STATE_CONFIG
} MenuSystemState; // [cite: uploaded:my_projects/SerialMenu.h]


// --- Public Function Declarations ---

/**
 * @brief Displays the main command menu.
 */
void menu_display_main(); // [cite: uploaded:my_projects/SerialMenu.h]

/**
 * @brief Displays the configuration menu.
 */
void menu_display_config(); // [cite: uploaded:my_projects/SerialMenu.h]

/**
 * @brief Processes a single character command based on the current menu state.
 * @param cmd The character command received.
 */
void menu_handle_input(char cmd); // [cite: uploaded:my_projects/SerialMenu.h]


// --- SD Card Menu Actions ---
void menu_sd_init(); // [cite: uploaded:my_projects/SerialMenu.h]
// void menu_sd_write_test(); // Declaration likely exists [cite: uploaded:my_projects/SerialMenu.h]
// void menu_sd_read_test(); // Declaration likely exists [cite: uploaded:my_projects/SerialMenu.h]
void menu_sd_show_status(); // [cite: uploaded:my_projects/SerialMenu.h]


// --- Simulation Actions ---
void menu_load_simulation_from_sd_to_flash(); // [cite: uploaded:my_projects/SerialMenu.h]
void menu_run_simulation(); // [cite: uploaded:my_projects/SerialMenu.h]

// --- Servo Actions ---
// Removed: void menu_servo_test(); [cite: uploaded:my_projects/SerialMenu.h]
/**
 * @brief Initiates the servo calibration routine.
 */
void menu_servo_calibrate();


// --- Configuration Accessor ---
/**
 * @brief Gets the currently configured radius.
 * @return The radius in centimeters (cm).
 */
float get_configured_radius_cm(); // [cite: uploaded:my_projects/SerialMenu.h]


#endif // SERIAL_MENU_H