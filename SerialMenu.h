#ifndef SERIAL_MENU_H
#define SERIAL_MENU_H

/**
 * @brief Displays the command menu to the serial console.
 */
void menu_display();

/**
 * @brief Processes a single character command received from the serial console.
 * @param cmd The character command received.
 */
void menu_handle_input(char cmd);

// --- SD Card Menu Actions ---
/**
 * @brief Attempts to initialize and mount the SD card.
 */
void menu_sd_init();

/**
 * @brief Performs a simple test write to the SD card ("test_write.txt").
 */
void menu_sd_write_test();

/**
 * @brief Performs a simple test read from the SD card ("test_write.txt").
 */
void menu_sd_read_test();

/**
 * @brief Shows the current mount status of the SD card.
 */
void menu_sd_show_status();


// --- Stepper Motor Menu Actions (Placeholders or Existing) ---
// Placeholder function declarations (could be moved elsewhere later)
void load_simulation_placeholder(); // Keep placeholder for now
void run_simulation_placeholder();  // Keep placeholder for now
static float menu_read_float(const char* prompt);
static int menu_read_int(const char* prompt);

#endif // SERIAL_MENU_H