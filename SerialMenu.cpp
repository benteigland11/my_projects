#include "SerialMenu.h"   // Our own header
#include "StepperMotor.h" // To call motor control functions

#include <iostream>
#include <cstdio>         // For PICO_ERROR_TIMEOUT if checking input here

// --- Public Function Implementations ---

/**
 * @brief Displays the command menu to the serial console.
 */
void menu_display() {
    std::cout << "\n--- Serial Control Menu ---" << std::endl;
    std::cout << "l: Load Simulation from SD Card (/flight_profile.csv)" << std::endl; // Updated description
    std::cout << "r: Run Simulation (Placeholder - uses loaded data later)" << std::endl;
    std::cout << "t: Run Motor Test (Simple Accel/Decel)" << std::endl;
    std::cout << "s: Stop Motor Test/Simulation" << std::endl;
    std::cout << "m: Show this Menu" << std::endl;
    std::cout << "Enter command: ";
    std::cout.flush(); // Ensure prompt appears
}

/**
 * @brief Placeholder function that now triggers SD card loading.
 */
void load_simulation_placeholder() { // Renaming this later might be clearer, e.g., menu_action_load_sim
    
}

/**
 * @brief Placeholder function for running a simulation (will use loaded data later).
 */
void run_simulation_placeholder() {
    // Future implementation: Check if data is loaded (sd_get_data_count() > 0)
    // and then use the data points from SDCardManager (sd_get_data_point)
    // to control the motor over time.
    menu_display(); // Show menu again
}


/**
 * @brief Processes a single character command received from the serial console.
 * @param cmd The character command received.
 */
void menu_handle_input(char cmd) {
    // Note: Echoing is handled in main.cpp now

    switch (cmd) {
        case 'l':
        case 'L':
            load_simulation_placeholder(); // Call the SD loading function
            break;
        case 'r':
        case 'R':
            run_simulation_placeholder();
            break;
        case 't':
        case 'T':
            // Call motor module function for simple test run
            motor_start_test();
            // Menu isn't redisplayed immediately to allow user to see motor status messages
            break;
        case 's':
        case 'S':
            // Call motor module function to stop test/simulation
            motor_stop_test();
            // Display menu again only if motor was already stopped (or finished stopping)
            // Checking state immediately might show DECELERATING, might need adjustment later
            if (motor_get_state() == MOTOR_STOPPED) {
                 menu_display();
            }
            break;
        case 'm':
        case 'M':
        case '?':
            menu_display();
            break;
        case '\n': // Ignore newline characters
        case '\r': // Ignore carriage return characters
            // Don't redisplay menu on just Enter key
            break;
        default:
            std::cout << "\nUnknown command: '" << cmd << "'" << std::endl;
            menu_display(); // Show menu again on error
            break;
    }
     std::cout.flush(); // Ensure output buffer is flushed
}