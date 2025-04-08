#include "SerialMenu.h"   // Our own header
#include "StepperMotor.h" // To call motor control functions
#include "sd_card_manager.h" // To call SD card functions
#include "openrocket_parser.h"
#include "pico/time.h"   // For get_absolute_time, absolute_time_diff_us

#include <iostream>
#include <cstdio>
#include <string>       // For std::string
#include <vector>       // For std::vector if reading into dynamic buffer later
#include <cstring>
#include <cstdlib> // For atof
#include <cctype>  // For isdigit, isprint


// --- Public Function Implementations ---

/**
 * @brief Displays the command menu to the serial console.
 */
void menu_display() {
    std::cout << "\n--- Serial Control Menu ---" << std::endl;
    // Motor Controls
    std::cout << "t: Run Motor Test (Simple Accel/Decel)" << std::endl;
    std::cout << "s: Stop Motor Test/Simulation" << std::endl;
    // Simulation (Placeholders)
    std::cout << "l: Load Simulation (Placeholder)" << std::endl; // Placeholder
    std::cout << "r: Run Simulation (Placeholder)" << std::endl;   // Placeholder
    // SD Card Controls
    std::cout << "i: Initialize SD Card" << std::endl;
    std::cout << "k: Check SD Card Status" << std::endl;
    std::cout << "w: Write Test File (test_write.txt)" << std::endl;
    std::cout << "d: Read Test File (test_write.txt)" << std::endl; // Changed char from 'r' to 'd' to avoid conflict
    // General
    std::cout << "m: Show this Menu" << std::endl;
    std::cout << "Enter command: ";
    std::cout.flush(); // Ensure prompt appears
}

// --- SD Card Menu Action Implementations ---

void menu_sd_init() {
    std::cout << "\nAttempting SD Card Initialization..." << std::endl;
    if (sd_init()) {
        std::cout << "SD Card Initialized Successfully." << std::endl;
    } else {
        std::cout << "SD Card Initialization FAILED." << std::endl;
    }
    menu_display(); // Show menu again
}

void menu_sd_show_status() {
     std::cout << "\n--- SD Card Status ---" << std::endl;
     if (sd_is_mounted()) {
         std::cout << "Status: Mounted and Ready" << std::endl;
         // Could add f_getfree() here later for free space
     } else {
         std::cout << "Status: NOT Mounted (Initialize with 'i')" << std::endl;
     }
     menu_display(); // Show menu again
}

void menu_sd_write_test() {
    std::cout << "\nAttempting SD Card Test Write..." << std::endl;
    if (!sd_is_mounted()) {
        std::cout << "Error: SD Card not mounted. Initialize with 'i' first." << std::endl;
        menu_display();
        return;
    }

    const char* filename = "test_write.txt";
    std::string test_data = "This is a test write from the Pico!\nTimestamp: "
                           + std::to_string(to_ms_since_boot(get_absolute_time())) + "\n";

    if (sd_write_file(filename, test_data.c_str(), test_data.length())) {
        std::cout << "Successfully wrote test data to " << filename << std::endl;
    } else {
        std::cout << "FAILED to write test data to " << filename << std::endl;
    }
    menu_display(); // Show menu again
}

void menu_sd_read_test() {
    std::cout << "\nAttempting SD Card Test Read..." << std::endl;
     if (!sd_is_mounted()) {
        std::cout << "Error: SD Card not mounted. Initialize with 'i' first." << std::endl;
        menu_display();
        return;
    }

    const char* filename = "test_write.txt";
    char read_buffer[256]; // Allocate a buffer on the stack
    memset(read_buffer, 0, sizeof(read_buffer)); // Clear buffer

    long file_size = sd_get_file_size(filename);
    if (file_size == -1) {
         std::cout << "Error: Could not get size of " << filename << " (may not exist)." << std::endl;
         menu_display();
         return;
    }
    if (file_size == 0) {
         std::cout << "File " << filename << " is empty." << std::endl;
         menu_display();
         return;
    }
    if (file_size >= sizeof(read_buffer)) {
         std::cout << "Warning: File size (" << file_size << ") >= buffer size (" << sizeof(read_buffer) << "). Reading truncated data." << std::endl;
    }

    int bytes_read = sd_read_file(filename, read_buffer, sizeof(read_buffer) - 1); // Read up to buffer size - 1 for null terminator

    if (bytes_read > 0) {
        std::cout << "Successfully read " << bytes_read << " bytes from " << filename << ":" << std::endl;
        std::cout << "--------------------" << std::endl;
        std::cout << read_buffer << std::endl; // Print the content
        std::cout << "--------------------" << std::endl;
    } else if (bytes_read == 0) {
         std::cout << "Read 0 bytes from " << filename << "." << std::endl;
    }
    else { // bytes_read == -1
        std::cout << "FAILED to read from " << filename << "." << std::endl;
    }
    menu_display(); // Show menu again
}


// --- Placeholder Simulation Actions ---

/**
 * @brief Placeholder function - will load data from SD card later.
 */
void menu_load_simulation_from_sd_to_flash() {
    std::cout << "\n--- Load Simulation File ---" << std::endl;

    if (!sd_is_mounted()) {
         std::cout << "Error: SD Card not mounted. Initialize with 'i' first." << std::endl;
         menu_display();
         return;
    }

    // --- Step 1: List CSV Files ---
    std::vector<std::string> csv_files = sd_list_files("", ".csv"); // Scan root for .csv

    if (csv_files.empty()) {
        std::cout << "Error: No .csv files found in the root directory." << std::endl;
        menu_display();
        return;
    }

    std::cout << "Available CSV files:" << std::endl;
    for (size_t i = 0; i < csv_files.size(); ++i) {
        // Print numbered list (starting from 1 for user)
        printf("  %d: %s\n", (int)(i + 1), csv_files[i].c_str());
    }

    // --- Step 2: Get User Selection ---
    int choice = -1;
    std::string selected_filename;
    while(true) {
        choice = menu_read_int("Enter the number of the file to load: ");
        // Validate choice (adjust for 0-based index)
        if (choice > 0 && (size_t)choice <= csv_files.size()) {
            selected_filename = csv_files[choice - 1]; // Get filename from vector
            printf("Selected file: %s\n", selected_filename.c_str());
            break; // Valid choice, exit loop
        } else {
            std::cout << "Invalid choice. Please enter a number between 1 and " << csv_files.size() << "." << std::endl;
        }
    }

    // --- Step 3: Store selected file to Flash ---
     std::cout << "Attempting to load '" << selected_filename << "' from SD to Flash..." << std::endl;
    if (!store_openrocket_to_flash(selected_filename.c_str())) { // Use selected filename
        std::cout << "FAILED to store '" << selected_filename << "' to flash. Aborting load." << std::endl;
        menu_display();
        return;
    }
     std::cout << "Successfully stored '" << selected_filename << "' to flash." << std::endl;


    // --- Step 4: Read back from Flash ---
    size_t stored_size = get_stored_data_size_from_flash();
    if (stored_size == 0) { /* ... error handling ... */ menu_display(); return; }
    std::cout << "DEBUG: Stored size reported by header: " << stored_size << " bytes." << std::endl;

    char* data_buffer = (char*)malloc(stored_size);
    if (!data_buffer) { /* ... error handling ... */ menu_display(); return; }

    int bytes_read = read_openrocket_from_flash(data_buffer, stored_size);
    std::cout << "DEBUG: Bytes actually read from flash: " << bytes_read << std::endl;
    if (bytes_read <= 0 || (size_t)bytes_read != stored_size) { /* ... error handling ... */ free(data_buffer); menu_display(); return; }


    // --- Step 5: Parse the data ---
    std::cout << "DEBUG: Calling parser..." << std::endl;
    bool parse_success = parse_openrocket_data(data_buffer, bytes_read);
    std::cout << "DEBUG: Parser returned: " << (parse_success ? "true" : "false") << std::endl;
    size_t point_count_after_parse = get_parsed_data_count();
    std::cout << "DEBUG: Point count *after* parsing: " << point_count_after_parse << std::endl;
    free(data_buffer); // Free buffer now

    if (!parse_success || point_count_after_parse == 0) { /* ... parse error/warning ... */ menu_display(); return; }


    // --- Step 6: Get Radius ---
    float radius_cm = 0.0f;
    while(radius_cm <= 0.0f) {
        radius_cm = menu_read_float("Enter radius (cm, must be > 0): ");
        if (radius_cm <= 0.0f) { std::cout << "Invalid radius. Please try again." << std::endl; }
    }
    float radius_m = radius_cm / 100.0f;
    printf("DEBUG: Using radius: %.4f m\n", radius_m);


    // --- Step 7: Calculate PPS ---
    std::cout << "DEBUG: Calling PPS calculator..." << std::endl;
    bool calc_success = calculate_pps_for_parsed_data(radius_m);
    std::cout << "DEBUG: PPS calculator returned: " << (calc_success ? "true" : "false") << std::endl;
    if (!calc_success) { printf("Warning: Failed to calculate target PPS values.\n"); }

    size_t final_point_count = get_parsed_data_count();
    std::cout << "Load process complete for '" << selected_filename << "'. Final parsed point count: " << final_point_count << std::endl;

    menu_display(); // Show menu again
}

/**
 * @brief Placeholder function for running a simulation.
 */
void menu_run_simulation() { // Renamed function
    printf("\n--- Running Simulation ---\n");
    printf("Press 's' to stop.\n");

    size_t point_count = get_parsed_data_count();
    if (point_count == 0) {
        printf("Error: No parsed simulation data available. Load data first ('l').\n");
        menu_display();
        return;
    }

    absolute_time_t start_time = get_absolute_time();
    bool stopped = false;

    for (size_t i = 0; i < point_count && !stopped; ++i) {
        FlightDataPoint point = get_parsed_data_point(i);
        printf("DEBUG RUN: i=%u, Read t=%.3f, Read pps=%.3f\n",
               (unsigned int)i, point.timestamp, point.target_pps); // Verify the
        // Calculate time elapsed since start
        absolute_time_t current_time = get_absolute_time();
        int64_t elapsed_us = absolute_time_diff_us(start_time, current_time);
        float elapsed_s = elapsed_us / 1000000.0f;

        // Calculate delay needed until this data point's timestamp
        int64_t target_us = (int64_t)(point.timestamp * 1000000.0f);
        int64_t delay_us = target_us - elapsed_us;

        if (delay_us > 1000) { // Add a small threshold
            // Wait until it's time for this data point
            // Need a non-blocking wait that checks for stop command
            absolute_time_t wait_until_time = delayed_by_us(current_time, delay_us);
            while (absolute_time_diff_us(get_absolute_time(), wait_until_time) > 0 && !stopped) {
                int c = getchar_timeout_us(100); // Check for input every 100us
                if (c == 's' || c == 'S') {
                    printf("\nStop requested.\n");
                    motor_stop_test(); // Call your motor stop function [cite: uploaded:my_projects/SerialMenu.cpp calls motor_stop_test]
                    stopped = true;
                    break;
                }
                // Allow other background tasks if needed
                tight_loop_contents();
            }
            if (stopped) break; // Exit outer loop if stopped during wait
        } else if (delay_us < -10000) { // If we are significantly behind schedule
            printf("Warning: Simulation lagging at point %u (Time %.3f s)\n", (unsigned int)i, point.timestamp);
        }

        // --- COMMAND THE MOTOR ---
        // You need a function like this in your StepperMotor module
        printf("Time: %.3f s, Setting Target PPS: %.3f Hz\n", point.timestamp, point.target_pps);
        motor_set_target_frequency(point.target_pps); // Needs implementation in StepperMotor.cpp/.h
    }

    if (!stopped) {
         printf("\nSimulation finished.\n");
         // Optionally set motor speed back to 0
          motor_set_target_frequency(0.0f); // Or call motor_stop_test()
    }

    menu_display();
}


/**
 * @brief Processes a single character command received from the serial console.
 */
void menu_handle_input(char cmd) {
    // Note: Echoing is handled in main.cpp

    switch (cmd) {
        // Motor Controls
        case 't':
        case 'T':
            motor_start_test();
            // Menu isn't redisplayed immediately
            break;
        case 's':
        case 'S':
            motor_stop_test();
            if (motor_get_state() == MOTOR_STOPPED) menu_display();
            break;

        // Simulation Placeholders
        case 'l':
        case 'L':
            menu_load_simulation_from_sd_to_flash();
            break;
        case 'r':
        case 'R':
            menu_run_simulation();
            break;

        // SD Card Controls
        case 'i':
        case 'I':
             menu_sd_init();
             break;
        case 'k':
        case 'K':
             menu_sd_show_status();
             break;
        case 'w':
        case 'W':
             menu_sd_write_test();
             break;
        case 'd': // Changed from 'r'
        case 'D':
             menu_sd_read_test();
             break;

        // General
        case 'm':
        case 'M':
        case '?':
            menu_display();
            break;
        case '\n': // Ignore newline
        case '\r': // Ignore carriage return
            // Don't redisplay menu on just Enter key
            break;
        default:
            std::cout << "\nUnknown command: '" << cmd << "'" << std::endl;
            menu_display(); // Show menu again on error
            break;
    }
     std::cout.flush(); // Ensure output buffer is flushed
}
// --- NEW HELPER FUNCTION for Reading Integer ---
/**
 * @brief Reads an integer from the serial input.
 * NOTE: This is a BLOCKING function.
 * @param prompt The message to display to the user.
 * @return The integer value entered, or -1 on error/non-numeric input.
 */
static int menu_read_int(const char* prompt) {
    char buffer[16];
    int index = 0;
    buffer[0] = '\0';

    std::cout << std::endl << prompt;
    std::cout.flush();

    while (index < sizeof(buffer) - 1) {
        int c = getchar(); // Blocking read

        if (c == PICO_ERROR_TIMEOUT || c == PICO_ERROR_NONE) continue;

        char ch = (char)c;

        if (ch == '\r' || ch == '\n') {
            std::cout << std::endl;
            break;
        }
        else if ((ch == '\b' || ch == 127) && index > 0) {
            index--;
            buffer[index] = '\0';
            std::cout << "\b \b";
            std::cout.flush();
        }
        // Only allow digits (and potentially '-' at the start if needed, but not for menu index)
        else if (isdigit(ch)) {
             if (isprint(ch)) {
                buffer[index++] = ch;
                buffer[index] = '\0';
                std::cout << ch;
                std::cout.flush();
             }
        }
    }

    if (index == 0) return -1; // No input

    int value = atoi(buffer); // Convert string to integer
    printf("Input converted to: %d\n", value); // Debug print
    return value;
}
/**
 * @brief Reads a floating-point number from the serial input.
 * NOTE: This is a BLOCKING function. It waits for user input.
 * @param prompt The message to display to the user.
 * @return The float value entered by the user, or 0.0f on error/empty input.
 */
static float menu_read_float(const char* prompt) {
    char buffer[32]; // Buffer to store input string
    int index = 0;
    buffer[0] = '\0'; // Start with an empty string

    std::cout << std::endl << prompt; // Display the prompt message
    std::cout.flush();

    while (index < sizeof(buffer) - 1) {
        int c = getchar(); // Use blocking getchar() here

        if (c == PICO_ERROR_TIMEOUT || c == PICO_ERROR_NONE) {
            continue; // Should not happen with blocking getchar, but safety first
        }

        char ch = (char)c;

        // Handle Enter key (end of input)
        if (ch == '\r' || ch == '\n') {
            std::cout << std::endl; // Newline after user presses Enter
            break; // Exit loop
        }
        // Handle Backspace/Delete (simple version)
        else if ((ch == '\b' || ch == 127) && index > 0) {
            index--;
            buffer[index] = '\0';
            // Echo backspace, space, backspace to clear character on terminal
            std::cout << "\b \b";
            std::cout.flush();
        }
        // Handle valid number characters (digits, '.', '-')
        else if (isdigit(ch) || (ch == '.' && strchr(buffer, '.') == nullptr) || (ch == '-' && index == 0)) {
             if (isprint(ch)) { // Check if printable before adding
                 buffer[index++] = ch;
                 buffer[index] = '\0'; // Null-terminate
                 std::cout << ch;      // Echo valid character
                 std::cout.flush();
             }
        }
        // Ignore other characters
    }

    // Convert buffer to float
    float value = atof(buffer);
    printf("Input converted to: %.3f\n", value); // Debug print
    return value;
}

