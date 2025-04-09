/**
 * @file SerialMenu.cpp
 * @brief Handles user interaction via serial console for motor control, SD card, and configuration.
 */

 #include "SerialMenu.h"
 #include "StepperMotor.h"    // To call motor control functions [cite: uploaded:my_projects/SerialMenu.cpp]
 #include "sd_card_manager.h" // To call SD card functions [cite: uploaded:my_projects/SerialMenu.cpp]
 #include "openrocket_parser.h" // To call parsing/calculation functions [cite: uploaded:my_projects/SerialMenu.cpp]
 #include "servo_controller.h" // << ADDED: To call servo functions

 #include <iostream>          // For cout [cite: uploaded:my_projects/SerialMenu.cpp]
 #include <cstdio>            // For printf, getchar [cite: uploaded:my_projects/SerialMenu.cpp]
 #include <string>            // For std::string [cite: uploaded:my_projects/SerialMenu.cpp]
 #include <vector>            // For std::vector [cite: uploaded:my_projects/SerialMenu.cpp]
 #include <cstring>           // For memset, strchr [cite: uploaded:my_projects/SerialMenu.cpp]
 #include <cstdlib>           // For atof, atoi [cite: uploaded:my_projects/SerialMenu.cpp]
 #include <cctype>            // For isdigit, isprint [cite: uploaded:my_projects/SerialMenu.cpp]
 #include "pico/time.h"       // For simulation timing [cite: uploaded:my_projects/SerialMenu.cpp]


 // --- Static Module Variables ---

 // Current state of the menu system
 static MenuSystemState s_currentMenuState = MENU_STATE_MAIN; // [cite: uploaded:my_projects/SerialMenu.cpp]

 // Configuration Variables (with defaults)
 static float s_configured_radius_cm = 15.0f; // Default radius [cite: uploaded:my_projects/SerialMenu.cpp]


 // --- Helper Functions for Input Reading ---

 /**
  * @brief Reads a floating-point number from the serial input. (BLOCKING)
  */
 static float menu_read_float(const char* prompt) { // [cite: uploaded:my_projects/SerialMenu.cpp]
     char buffer[32];
     int index = 0;
     buffer[0] = '\0';

     std::cout << std::endl << prompt;
     std::cout.flush();

     while (index < sizeof(buffer) - 1) {
         int c = getchar(); // Blocking read [cite: uploaded:my_projects/SerialMenu.cpp]

         if (c == PICO_ERROR_TIMEOUT || c == PICO_ERROR_NONE) continue;

         char ch = (char)c;

         if (ch == '\r' || ch == '\n') { // Handle Enter [cite: uploaded:my_projects/SerialMenu.cpp]
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
     printf("Input converted to: %.3f\n", value); // [cite: uploaded:my_projects/SerialMenu.cpp]
     return value;
 }

 /**
  * @brief Reads an integer from the serial input. (BLOCKING)
  */
 static int menu_read_int(const char* prompt) { // [cite: uploaded:my_projects/SerialMenu.cpp]
     char buffer[16];
     int index = 0;
     buffer[0] = '\0';

     std::cout << std::endl << prompt;
     std::cout.flush();

     while (index < sizeof(buffer) - 1) {
         int c = getchar(); // Blocking read [cite: uploaded:my_projects/SerialMenu.cpp]

         if (c == PICO_ERROR_TIMEOUT || c == PICO_ERROR_NONE) continue;

         char ch = (char)c;

         if (ch == '\r' || ch == '\n') { // Handle Enter [cite: uploaded:my_projects/SerialMenu.cpp]
             std::cout << std::endl;
             break;
         }
         else if ((ch == '\b' || ch == 127) && index > 0) { // Handle Backspace [cite: uploaded:my_projects/SerialMenu.cpp]
             index--;
             buffer[index] = '\0';
             std::cout << "\b \b";
             std::cout.flush();
         }
         else if (isdigit(ch)) { // Only allow digits for integer input [cite: uploaded:my_projects/SerialMenu.cpp]
              if (isprint(ch)) {
                 buffer[index++] = ch;
                 buffer[index] = '\0';
                 std::cout << ch;
                 std::cout.flush();
              }
         }
     }
     if (index == 0) return -1; // No input entered [cite: uploaded:my_projects/SerialMenu.cpp]
     int value = atoi(buffer); // [cite: uploaded:my_projects/SerialMenu.cpp]
     printf("Input converted to: %d\n", value); // [cite: uploaded:my_projects/SerialMenu.cpp]
     return value;
 }


 // --- Configuration Accessor ---
 float get_configured_radius_cm() { // [cite: uploaded:my_projects/SerialMenu.cpp]
     return s_configured_radius_cm;
 }

 // --- Menu Display Functions ---

 /**
  * @brief Displays the main command menu.
  */
 void menu_display_main() { // [cite: uploaded:my_projects/SerialMenu.cpp]
     std::cout << "\n--- Serial Control Menu ---" << std::endl;
     std::cout << "t: Run Motor Test" << std::endl; // [cite: uploaded:my_projects/SerialMenu.cpp]
     std::cout << "v: Calibrate Servo" << std::endl; // <<< UPDATED
     std::cout << "s: Stop Motor Test/Simulation" << std::endl; // [cite: uploaded:my_projects/SerialMenu.cpp]
     std::cout << "l: Load Simulation File" << std::endl; // [cite: uploaded:my_projects/SerialMenu.cpp]
     std::cout << "r: Run Loaded Simulation" << std::endl; // [cite: uploaded:my_projects/SerialMenu.cpp]
     std::cout << "i: Initialize SD Card" << std::endl; // [cite: uploaded:my_projects/SerialMenu.cpp]
     std::cout << "k: Check SD Card Status" << std::endl; // [cite: uploaded:my_projects/SerialMenu.cpp]
     std::cout << "c: Configure Apparatus" << std::endl; // [cite: uploaded:my_projects/SerialMenu.cpp]
     std::cout << "m: Show this Menu" << std::endl; // [cite: uploaded:my_projects/SerialMenu.cpp]
     std::cout << "Enter command: ";
     std::cout.flush();
 }

 /**
  * @brief Displays the configuration menu.
  */
 void menu_display_config() { // [cite: uploaded:my_projects/SerialMenu.cpp]
      std::cout << "\n--- Apparatus Configuration ---" << std::endl;
      printf("  1: Radius: %.2f cm\n", s_configured_radius_cm); // [cite: uploaded:my_projects/SerialMenu.cpp]
      // Add other settings display here...
      std::cout << "\nEnter number to change, or B to go back: "; // [cite: uploaded:my_projects/SerialMenu.cpp]
      std::cout.flush();
 }


 // --- SD Card Menu Action Implementations ---

 void menu_sd_init() {                                      // [cite: uploaded:my_projects/SerialMenu.cpp]
     std::cout << "\nAttempting SD Card Initialization..." << std::endl;
     if (sd_init()) { // [cite: uploaded:my_projects/SerialMenu.cpp]
         std::cout << "SD Card Initialized Successfully." << std::endl;
     } else {
         std::cout << "SD Card Initialization FAILED." << std::endl;
     }
     menu_display_main(); // Show main menu again [cite: uploaded:my_projects/SerialMenu.cpp]
 }

 void menu_sd_show_status() {                               // [cite: uploaded:my_projects/SerialMenu.cpp]
      std::cout << "\n--- SD Card Status ---" << std::endl;
      if (sd_is_mounted()) { // [cite: uploaded:my_projects/SerialMenu.cpp]
          std::cout << "Status: Mounted and Ready" << std::endl;
      } else {
          std::cout << "Status: NOT Mounted (Initialize with 'i')" << std::endl;
      }
      menu_display_main(); // Show main menu again [cite: uploaded:my_projects/SerialMenu.cpp]
 }

 // NOTE: Test write/read functions omitted for brevity, assuming they exist as before [cite: uploaded:my_projects/SerialMenu.cpp]
 // void menu_sd_write_test() { ... }
 // void menu_sd_read_test() { ... }


 // --- Servo Action Implementations ---

 // REMOVED: menu_servo_test() function implementation [cite: uploaded:my_projects/SerialMenu.cpp]
 // void menu_servo_test() {
 //    servo_test_sweep(); // [cite: uploaded:my_projects/SerialMenu.cpp]
 //    menu_display_main(); // [cite: uploaded:my_projects/SerialMenu.cpp]
 //}

 /**
  * @brief Calls the servo calibration routine.
  */
 void menu_servo_calibrate() { // <<< ADDED
     servo_calibrate(); // Call the function in servo_controller
     // Show menu again after calibration completes
     menu_display_main();
 }


 // --- Simulation Action Implementations ---

 /**
  * @brief Loads simulation data from a selected SD file to flash storage.
  * Uses the currently configured radius for calculations.
  */
 void menu_load_simulation_from_sd_to_flash() { // [cite: uploaded:my_projects/SerialMenu.cpp]
     std::cout << "\n--- Load Simulation File ---" << std::endl;

     if (!sd_is_mounted()) { std::cout << "Error: SD Card not mounted...\n"; menu_display_main(); return; } // [cite: uploaded:my_projects/SerialMenu.cpp]

     // List CSV Files
     std::vector<std::string> csv_files = sd_list_files("", ".csv"); // [cite: uploaded:my_projects/SerialMenu.cpp]
     if (csv_files.empty()) { std::cout << "Error: No .csv files found...\n"; menu_display_main(); return; } // [cite: uploaded:my_projects/SerialMenu.cpp]

     std::cout << "Available CSV files:" << std::endl;
     for (size_t i = 0; i < csv_files.size(); ++i) { // [cite: uploaded:my_projects/SerialMenu.cpp]
         printf("  %d: %s\n", (int)(i + 1), csv_files[i].c_str()); // [cite: uploaded:my_projects/SerialMenu.cpp]
     }

     // Get User Selection
     int choice = -1;
     std::string selected_filename;
     while(true) { // [cite: uploaded:my_projects/SerialMenu.cpp]
         choice = menu_read_int("Enter the number of the file to load: "); // [cite: uploaded:my_projects/SerialMenu.cpp]
         if (choice > 0 && (size_t)choice <= csv_files.size()) {
             selected_filename = csv_files[choice - 1]; // [cite: uploaded:my_projects/SerialMenu.cpp]
             printf("Selected file: %s\n", selected_filename.c_str()); // [cite: uploaded:my_projects/SerialMenu.cpp]
             break;
         } else { std::cout << "Invalid choice...\n"; } // [cite: uploaded:my_projects/SerialMenu.cpp]
     }

     // Store selected file to Flash
     std::cout << "Storing '" << selected_filename << "' to Flash..." << std::endl; // [cite: uploaded:my_projects/SerialMenu.cpp]
     if (!store_openrocket_to_flash(selected_filename.c_str())) { std::cout << "FAILED to store to flash...\n"; menu_display_main(); return; } // [cite: uploaded:my_projects/SerialMenu.cpp]
     std::cout << "Successfully stored to flash." << std::endl; // [cite: uploaded:my_projects/SerialMenu.cpp]

     // Read back from Flash
     size_t stored_size = get_stored_data_size_from_flash(); // [cite: uploaded:my_projects/SerialMenu.cpp]
     if (stored_size == 0) { std::cout << "Error: Stored size is 0...\n"; menu_display_main(); return; } // [cite: uploaded:my_projects/SerialMenu.cpp]
     char* data_buffer = (char*)malloc(stored_size); // [cite: uploaded:my_projects/SerialMenu.cpp]
     if (!data_buffer) { std::cout << "Error: Failed to allocate buffer...\n"; menu_display_main(); return; } // [cite: uploaded:my_projects/SerialMenu.cpp]
     int bytes_read = read_openrocket_from_flash(data_buffer, stored_size); // [cite: uploaded:my_projects/SerialMenu.cpp]
     if (bytes_read <= 0 || (size_t)bytes_read != stored_size) { std::cout << "Error reading back from flash...\n"; free(data_buffer); menu_display_main(); return; } // [cite: uploaded:my_projects/SerialMenu.cpp]

     // Parse the data
     bool parse_success = parse_openrocket_data(data_buffer, bytes_read); // [cite: uploaded:my_projects/SerialMenu.cpp]
     size_t point_count_after_parse = get_parsed_data_count(); // [cite: uploaded:my_projects/SerialMenu.cpp]
     free(data_buffer); // Free buffer now [cite: uploaded:my_projects/SerialMenu.cpp]
     if (!parse_success || point_count_after_parse == 0) { std::cout << "Warning: Parsing failed or yielded zero points...\n"; menu_display_main(); return; } // [cite: uploaded:my_projects/SerialMenu.cpp]

     // Get Configured Radius
     float radius_cm = get_configured_radius_cm(); // Use stored value [cite: uploaded:my_projects/SerialMenu.cpp]
     float radius_m = radius_cm / 100.0f; // [cite: uploaded:my_projects/SerialMenu.cpp]
     printf("Using configured radius: %.2f cm (%.4f m)\n", radius_cm, radius_m); // [cite: uploaded:my_projects/SerialMenu.cpp]

     // Calculate PPS
     bool calc_success = calculate_pps_for_parsed_data(radius_m); // [cite: uploaded:my_projects/SerialMenu.cpp]
     if (!calc_success) { printf("Warning: Failed to calculate target PPS values.\n"); } // [cite: uploaded:my_projects/SerialMenu.cpp]

     size_t final_point_count = get_parsed_data_count(); // [cite: uploaded:my_projects/SerialMenu.cpp]
     std::cout << "Load process complete for '" << selected_filename << "'. Points: " << final_point_count << std::endl; // [cite: uploaded:my_projects/SerialMenu.cpp]

     menu_display_main(); // Show main menu again [cite: uploaded:my_projects/SerialMenu.cpp]
 }

 /**
  * @brief Runs the loaded simulation profile, including servo control.
  */
 void menu_run_simulation() { // [cite: uploaded:my_projects/SerialMenu.cpp]
    printf("\n--- Initializing Simulation Run ---\n");

    // 1. Check if data is loaded
    size_t point_count = get_parsed_data_count(); // [cite: uploaded:my_projects/SerialMenu.cpp]
    if (point_count == 0) { // [cite: uploaded:my_projects/SerialMenu.cpp]
        printf("Error: No parsed simulation data available. Load data first ('l').\n");
        menu_display_main(); // Display main menu [cite: uploaded:my_projects/SerialMenu.cpp]
        return;
    }

    // --- 2. Initialize Servo State Tracking ---
    float target_servo_state_position = 0.0f; // Servo starts (and represents) position 0.0 [cite: uploaded:my_projects/SerialMenu.cpp]
    int previous_acceleration_sign = 1;  // Assume initially corresponds to positive G (matching 0.0 state) [cite: uploaded:my_projects/SerialMenu.cpp]
    int flip_cooldown_counter = 0;      // Cooldown after flipping [cite: uploaded:my_projects/SerialMenu.cpp]

    FlightDataPoint first_point = get_parsed_data_point(0); // Need first point info [cite: uploaded:my_projects/SerialMenu.cpp]
    if (first_point.acceleration >= 0) { // [cite: uploaded:my_projects/SerialMenu.cpp]
         previous_acceleration_sign = 1; // [cite: uploaded:my_projects/SerialMenu.cpp]
    } else {
         previous_acceleration_sign = -1; // [cite: uploaded:my_projects/SerialMenu.cpp]
    }
    printf("Servo starting at position 0.0 (set by init). Primed initial sign state. Starting simulation...\n"); // [cite: uploaded:my_projects/SerialMenu.cpp]


    // --- 3. Start Simulation Timing ---
    absolute_time_t start_time = get_absolute_time(); // [cite: uploaded:my_projects/SerialMenu.cpp]
    bool stopped = false; // [cite: uploaded:my_projects/SerialMenu.cpp]

    // --- 4. Main Simulation Loop ---
    printf("Timestamp (s), Target PPS (Hz), Servo State (0/1)\n"); // Header for runtime data [cite: uploaded:my_projects/SerialMenu.cpp]
    for (size_t i = 0; i < point_count && !stopped; ++i) { // [cite: uploaded:my_projects/SerialMenu.cpp]
        FlightDataPoint point = get_parsed_data_point(i); // [cite: uploaded:my_projects/SerialMenu.cpp]

        // --- 4a. Servo Flip Logic ---
        int current_acceleration_sign = (point.acceleration > 0.001f) ? 1 : ((point.acceleration < -0.001f) ? -1 : 0); // Check sign with tolerance [cite: uploaded:my_projects/SerialMenu.cpp]

        if (flip_cooldown_counter > 0) { // [cite: uploaded:my_projects/SerialMenu.cpp]
            flip_cooldown_counter--; // Decrement cooldown [cite: uploaded:my_projects/SerialMenu.cpp]
        } else {
            // Check for sign change [cite: uploaded:my_projects/SerialMenu.cpp]
            if (current_acceleration_sign != 0 && previous_acceleration_sign != 0 && current_acceleration_sign != previous_acceleration_sign) {
                printf("\nSIGN CHANGE DETECTED at t=%.3f (%.3f G -> %.3f G)! Flipping servo.\n", // [cite: uploaded:my_projects/SerialMenu.cpp]
                       point.timestamp, (i > 0 ? get_parsed_data_point(i-1).acceleration : first_point.acceleration), point.acceleration);

                // Flip target position state [cite: uploaded:my_projects/SerialMenu.cpp]
                target_servo_state_position = (target_servo_state_position == 0.0f) ? 1.0f : 0.0f;
                servo_set_position(target_servo_state_position); // Command the move [cite: uploaded:my_projects/SerialMenu.cpp]
                flip_cooldown_counter = 3; // Start cooldown [cite: uploaded:my_projects/SerialMenu.cpp]
            }
        }
        // Update previous sign [cite: uploaded:my_projects/SerialMenu.cpp]
        if (current_acceleration_sign != 0) {
            previous_acceleration_sign = current_acceleration_sign; // [cite: uploaded:my_projects/SerialMenu.cpp]
        }


        // --- 4b. Timing Logic & Wait ---
        absolute_time_t current_time = get_absolute_time(); // [cite: uploaded:my_projects/SerialMenu.cpp]
        int64_t elapsed_us = absolute_time_diff_us(start_time, current_time); // [cite: uploaded:my_projects/SerialMenu.cpp]
        int64_t target_us = (int64_t)(point.timestamp * 1000000.0f); // [cite: uploaded:my_projects/SerialMenu.cpp]
        int64_t delay_us = target_us - elapsed_us; // [cite: uploaded:my_projects/SerialMenu.cpp]

        // Wait if needed, checking for stop command [cite: uploaded:my_projects/SerialMenu.cpp]
        if (delay_us > 1000) { // [cite: uploaded:my_projects/SerialMenu.cpp]
            absolute_time_t wait_until_time = delayed_by_us(current_time, delay_us); // [cite: uploaded:my_projects/SerialMenu.cpp]
            while (absolute_time_diff_us(get_absolute_time(), wait_until_time) > 0 && !stopped) { // [cite: uploaded:my_projects/SerialMenu.cpp]
                int c = getchar_timeout_us(100); // Check for stop input non-blockingly [cite: uploaded:my_projects/SerialMenu.cpp]
                if (c == 's' || c == 'S') { // [cite: uploaded:my_projects/SerialMenu.cpp]
                    printf("\nStop requested by user.\n");
                    motor_stop_test(); // Initiate motor stop [cite: uploaded:my_projects/SerialMenu.cpp]
                    stopped = true;    // Set flag to exit simulation loop [cite: uploaded:my_projects/SerialMenu.cpp]
                    break;             // Exit inner wait loop [cite: uploaded:my_projects/SerialMenu.cpp]
                }
                tight_loop_contents(); // Yield for background tasks [cite: uploaded:my_projects/SerialMenu.cpp]
            }
            if (stopped) break; // Exit outer simulation loop if stopped during wait [cite: uploaded:my_projects/SerialMenu.cpp]
        } else if (delay_us < -15000) { // Check if lagging [cite: uploaded:my_projects/SerialMenu.cpp]
            printf("Warning: Simulation lagging at point %u (Target Time %.3f s)\n", (unsigned int)i, point.timestamp); // [cite: uploaded:my_projects/SerialMenu.cpp]
        }


        // --- 4c. Command the Motor ---
        // Print current state for this timestamp
        printf("%.3f, %.3f, %.1f\n", // [cite: uploaded:my_projects/SerialMenu.cpp]
               point.timestamp, point.target_pps, target_servo_state_position); // Use state tracking variable [cite: uploaded:my_projects/SerialMenu.cpp]
        motor_set_target_frequency(point.target_pps); // Set motor speed [cite: uploaded:my_projects/SerialMenu.cpp]

    } // End main simulation loop [cite: uploaded:my_projects/SerialMenu.cpp]


    // --- 5. Simulation End ---
    if (!stopped) { // [cite: uploaded:my_projects/SerialMenu.cpp]
         printf("\nSimulation finished normally.\n");
    }
    motor_set_target_frequency(0.0f); // Ensure motor is stopped [cite: uploaded:my_projects/SerialMenu.cpp]

    // Return servo to default position 0.0
    printf("Returning servo to default position 0.0...\n"); // [cite: uploaded:my_projects/SerialMenu.cpp]
    // Note: Previous version set to 1.0, but 0.0 seems more standard 'default'
    servo_set_position(0.0f); // <<< Changed to 0.0
    sleep_ms(1000); // Wait long enough for potential full travel [cite: uploaded:my_projects/SerialMenu.cpp]

    menu_display_main(); // Display main menu [cite: uploaded:my_projects/SerialMenu.cpp]
}


 // --- Input Handling ---

 /**
  * @brief Handles input commands specific to the Configuration menu.
  */
 static void menu_handle_config_input(char cmd) { // [cite: uploaded:my_projects/SerialMenu.cpp]
      switch (cmd) {
         case '1': { // Set Radius [cite: uploaded:my_projects/SerialMenu.cpp]
             float new_radius = -1.0f;
              while(new_radius <= 0.0f) { // [cite: uploaded:my_projects/SerialMenu.cpp]
                  new_radius = menu_read_float("Enter new radius (cm, > 0): "); // [cite: uploaded:my_projects/SerialMenu.cpp]
                  if (new_radius <= 0.0f) { std::cout << "Invalid radius...\n"; } // [cite: uploaded:my_projects/SerialMenu.cpp]
              }
              s_configured_radius_cm = new_radius; // Store [cite: uploaded:my_projects/SerialMenu.cpp]
              printf("Radius set to %.2f cm\n", s_configured_radius_cm); // [cite: uploaded:my_projects/SerialMenu.cpp]
              menu_display_config(); // Show updated config menu [cite: uploaded:my_projects/SerialMenu.cpp]
             break;
         }
         // Add cases '2', '3' etc. for future settings

         case 'b': case 'B': case 'q': case 'Q': // Back/Quit [cite: uploaded:my_projects/SerialMenu.cpp]
              s_currentMenuState = MENU_STATE_MAIN; // Change state back [cite: uploaded:my_projects/SerialMenu.cpp]
              menu_display_main(); // Show main menu [cite: uploaded:my_projects/SerialMenu.cpp]
              break;


        case '\n': case '\r': break; // Ignore Enter [cite: uploaded:my_projects/SerialMenu.cpp]

         default: // [cite: uploaded:my_projects/SerialMenu.cpp]
             std::cout << "\nUnknown config command: '" << cmd << "'" << std::endl;
             menu_display_config(); // Show config menu again [cite: uploaded:my_projects/SerialMenu.cpp]
             break;
      }
       std::cout.flush();
 }

 /**
  * @brief Processes a single character command based on the current menu state.
  */
 void menu_handle_input(char cmd) { // [cite: uploaded:my_projects/SerialMenu.cpp]
     // Route command based on current state
     if (s_currentMenuState == MENU_STATE_CONFIG) { // [cite: uploaded:my_projects/SerialMenu.cpp]
         menu_handle_config_input(cmd);
         return; // Config handler does its own redisplay [cite: uploaded:my_projects/SerialMenu.cpp]
     }

     // --- Handle MAIN MENU commands ---
     switch (cmd) {
         // Motor
         case 't': case 'T': motor_start_test(); break; // [cite: uploaded:my_projects/SerialMenu.cpp]
         case 's': case 'S': // [cite: uploaded:my_projects/SerialMenu.cpp]
             motor_stop_test(); // [cite: uploaded:my_projects/SerialMenu.cpp]
             // The main loop will implicitly redisplay menu when motor stops
             // if (motor_get_state() == MOTOR_STOPPED) menu_display_main();
             break;
         // Simulation
         case 'l': case 'L': menu_load_simulation_from_sd_to_flash(); break; // [cite: uploaded:my_projects/SerialMenu.cpp]
         case 'r': case 'R': menu_run_simulation(); break; // [cite: uploaded:my_projects/SerialMenu.cpp]
         // SD Card
         case 'i': case 'I': menu_sd_init(); break; // [cite: uploaded:my_projects/SerialMenu.cpp]
         case 'k': case 'K': menu_sd_show_status(); break; // [cite: uploaded:my_projects/SerialMenu.cpp]
         // Configuration
         case 'c': case 'C': // [cite: uploaded:my_projects/SerialMenu.cpp]
              s_currentMenuState = MENU_STATE_CONFIG; // [cite: uploaded:my_projects/SerialMenu.cpp]
              menu_display_config(); // [cite: uploaded:my_projects/SerialMenu.cpp]
              break;
        //Servo
         case 'v': case 'V': // <<< UPDATED CASE
             menu_servo_calibrate(); // <<< CALL NEW FUNCTION
             break; // Servo calibrate handles redisplaying menu
         // General
         case 'm': case 'M': case '?': menu_display_main(); break; // [cite: uploaded:my_projects/SerialMenu.cpp]
         // Ignore Enter
         case '\n': case '\r': break; // [cite: uploaded:my_projects/SerialMenu.cpp]
         // Unknown
         default: // [cite: uploaded:my_projects/SerialMenu.cpp]
             std::cout << "\nUnknown command: '" << cmd << "'" << std::endl;
             menu_display_main(); // [cite: uploaded:my_projects/SerialMenu.cpp]
             break;
     }
      std::cout.flush();
 }