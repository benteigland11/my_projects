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

// Placeholder function declarations (could be moved elsewhere later)
void load_simulation_placeholder();
void run_simulation_placeholder();


#endif // SERIAL_MENU_H