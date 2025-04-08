#include "openrocket_parser.h" // Include the header file we defined
#include "sd_card_manager.h"   // For SD card functions [cite: uploaded:my_projects/sd_card_manager.h]

#include "hardware/flash.h"    // For flash operations
#include "hardware/sync.h"     // For disabling interrupts

#include <vector>              // For std::vector to store parsed data
#include <string>              // For std::string operations (optional, could use C strings)
#include <cstring>             // For memcpy, memset, strstr, strtok_r, strlen
#include <cstdio>              // For printf (debugging)
#include <cstdlib>             // For malloc, free, strtof (or sscanf)
#include <cctype>              // For isprint (used in display, but good include)
#include <cmath> // For sqrt, M_PI


// --- Static Storage for Parsed Data ---
// This vector will hold the data points after parsing
static std::vector<FlightDataPoint> parsed_flight_data;

// --- Helper Function ---
// Calculates size padded up to the nearest flash page boundary
static inline size_t get_padded_size(size_t size) {
    return (size + FLASH_PAGE_SIZE - 1) & ~(FLASH_PAGE_SIZE - 1);
}

// --- Flash Storage Functions ---

bool store_openrocket_to_flash(const char* sd_filename) {
    printf("Storing '%s' to flash...\n", sd_filename);

    if (!sd_is_mounted()) { // [cite: uploaded:my_projects/sd_card_manager.cpp]
        printf("Error: SD card not mounted.\n");
        return false;
    }

    // 1. Get file size
    long file_size_long = sd_get_file_size(sd_filename); // [cite: uploaded:my_projects/sd_card_manager.h]
    if (file_size_long < 0) {
        printf("Error: Failed to get size of '%s'\n", sd_filename);
        return false;
    }
    if (file_size_long == 0) {
         printf("Error: File '%s' is empty.\n", sd_filename);
         return false;
    }
    size_t file_size = (size_t)file_size_long;

    // 2. Check if file fits (including our header)
    size_t total_size_needed = sizeof(FlashDataHeader) + file_size;
    if (total_size_needed > FLASH_STORAGE_MAX_SIZE) {
        printf("Error: File size (%u bytes) + header (%u bytes) exceeds flash storage limit (%u bytes).\n",
               (unsigned int)file_size, (unsigned int)sizeof(FlashDataHeader), (unsigned int)FLASH_STORAGE_MAX_SIZE);
        return false;
    }

    // 3. Allocate RAM buffer
    size_t buffer_alloc_size = get_padded_size(total_size_needed);
    uint8_t* ram_buffer = (uint8_t*)malloc(buffer_alloc_size);
    if (!ram_buffer) {
        printf("Error: Failed to allocate %u bytes for RAM buffer.\n", (unsigned int)buffer_alloc_size);
        return false;
    }
    memset(ram_buffer, 0xFF, buffer_alloc_size); // Flash needs 0xFF

    // 4. Read file into RAM buffer (after the header space)
    int bytes_read = sd_read_file(sd_filename, ram_buffer + sizeof(FlashDataHeader), file_size); // [cite: uploaded:my_projects/sd_card_manager.h]
    if (bytes_read < 0 || (size_t)bytes_read != file_size) {
        printf("Error: Failed to read full file '%s' (%d bytes read).\n", sd_filename, bytes_read);
        free(ram_buffer);
        return false;
    }

    // 5. Prepare the header
    FlashDataHeader* header = (FlashDataHeader*)ram_buffer;
    header->magic = FLASH_DATA_MAGIC;
    header->data_size = file_size;

    // 6. Flash Operations (Interrupts Disabled)
    printf("Preparing to write %u bytes (padded) to flash offset 0x%X...\n", (unsigned int)buffer_alloc_size, FLASH_TARGET_OFFSET);

    uint32_t ints = save_and_disable_interrupts();

    // Erase necessary sectors
    size_t erase_size = get_padded_size(total_size_needed);
    printf("Erasing %u bytes...\n", (unsigned int)erase_size);
    flash_range_erase(FLASH_TARGET_OFFSET, erase_size);

    // Program the data
    printf("Programming %u bytes...\n", (unsigned int)buffer_alloc_size);
    flash_range_program(FLASH_TARGET_OFFSET, ram_buffer, buffer_alloc_size);

    restore_interrupts(ints);

    // 7. Verification (Optional but Recommended)
    const FlashDataHeader* readback_header = (const FlashDataHeader*)FLASH_STORAGE_ADDRESS;
    bool verified = (memcmp(header, readback_header, sizeof(FlashDataHeader)) == 0);
    // Could add full data verification here if needed

    // 8. Cleanup
    free(ram_buffer);

    if (verified) {
        printf("Flash write successful and verified.\n");
        return true;
    } else {
        printf("Error: Flash write verification FAILED.\n");
        return false;
    }
}

int read_openrocket_from_flash(void* buffer, size_t buffer_size) {
    const FlashDataHeader* header = (const FlashDataHeader*)FLASH_STORAGE_ADDRESS;

    // 1. Check magic number
    if (header->magic != FLASH_DATA_MAGIC) {
        printf("Error: Flash data magic number mismatch.\n");
        return -1;
    }

    // 2. Get stored data size
    size_t stored_size = header->data_size;
     if (stored_size == 0 || stored_size > (FLASH_STORAGE_MAX_SIZE - sizeof(FlashDataHeader))) {
        printf("Error: Invalid stored data size (%u bytes) in flash header.\n", (unsigned int)stored_size);
         return -1;
    }


    // 3. Check if user buffer is large enough
    if (stored_size > buffer_size) {
        printf("Error: Provided buffer (%u bytes) is too small for stored data (%u bytes).\n", (unsigned int)buffer_size, (unsigned int)stored_size);
        return -1;
    }

    // 4. Copy data from flash (after the header) to the user buffer
    const uint8_t* flash_data_start = (const uint8_t*)FLASH_STORAGE_ADDRESS + sizeof(FlashDataHeader);
    memcpy(buffer, flash_data_start, stored_size);

    printf("Read %u bytes from flash.\n", (unsigned int)stored_size);
    return (int)stored_size;
}

size_t get_stored_data_size_from_flash() {
     const FlashDataHeader* header = (const FlashDataHeader*)FLASH_STORAGE_ADDRESS;
     if (header->magic == FLASH_DATA_MAGIC) {
         if (header->data_size < (FLASH_STORAGE_MAX_SIZE - sizeof(FlashDataHeader))) {
            return header->data_size;
         }
     }
     return 0; // Indicate no valid data or header found
}


// --- CSV Parsing Function (Corrected Logic) ---

bool parse_openrocket_data(const char* data_buffer, size_t data_size) {
    printf("Parsing flight data (%u bytes)...\n", (unsigned int)data_size);
    parsed_flight_data.clear();

    bool found_ignition = false;
    bool found_apogee = false;

    char* buffer_copy = (char*)malloc(data_size + 1);
    if (!buffer_copy) {
        printf("Error: Failed to allocate buffer for parsing.\n");
        return false;
    }
    memcpy(buffer_copy, data_buffer, data_size);
    buffer_copy[data_size] = '\0';

    char* saveptr;
    char* line = strtok_r(buffer_copy, "\n\r", &saveptr);

    while (line != nullptr && !found_apogee) {
        // --- Logic for finding the start ---
        if (!found_ignition) {
            if (strstr(line, "# Event IGNITION") != nullptr) {
                printf("Found IGNITION event.\n");
                found_ignition = true;
                // Continue to the next line immediately
                line = strtok_r(nullptr, "\n\r", &saveptr);
                continue;
            }
            // Skip lines before IGNITION is found
            line = strtok_r(nullptr, "\n\r", &saveptr);
            continue;
        }

        // --- Logic after IGNITION is found ---

        // Check for APOGEE first (stops parsing)
        if (strstr(line, "# Event APOGEE") != nullptr) {
            printf("Found APOGEE event. Stopping parse.\n");
            found_apogee = true;
            break; // Exit the while loop
        }

        // Check if the line starts with *any* "# Event" and skip it
        if (strstr(line, "# Event") == line) { // Check prefix
             printf("Skipping event line: %s\n", line);
             line = strtok_r(nullptr, "\n\r", &saveptr);
             continue; // Skip to next line
        }

        // Attempt to parse as data
        float timestamp = 0.0f;
        float acceleration = 0.0f;
        // Use sscanf. Add error checking as needed.
        if (sscanf(line, "%f,%f", &timestamp, &acceleration) == 2) {
            parsed_flight_data.push_back({timestamp, acceleration});
        } else {
            if (strlen(line) > 0) { // Avoid warning on blank lines
                 printf("Warning: Failed to parse data line: %s\n", line);
            }
        }

        // Get the next line
        line = strtok_r(nullptr, "\n\r", &saveptr);
    } // End while loop
    

    free(buffer_copy);

    printf("Parsing finished. Found %u data points.\n", (unsigned int)parsed_flight_data.size());
    return found_ignition; // Success if we at least found ignition
}


// --- Accessor Functions for Parsed Data ---

size_t get_parsed_data_count() {
    return parsed_flight_data.size();
}

FlightDataPoint get_parsed_data_point(size_t index) {
    if (index < parsed_flight_data.size()) {
        return parsed_flight_data[index];
    }
    // Return a default/invalid point if index is out of bounds
    printf("Warning: Requested parsed data index %u out of bounds (size %u).\n",
           (unsigned int)index, (unsigned int)parsed_flight_data.size());
    return {0.0f, 0.0f};
}

bool calculate_pps_for_parsed_data(float radius_m) {
    const float G_ACCEL = 9.80665f; // m/s^2
    const float RPM_TO_PPS_FACTOR = 0.3f; // From user: RPM = PPS * 0.3 => PPS = RPM / 0.3

    if (radius_m <= 0.0f) {
        printf("Error: Invalid radius (%.3f m) for PPS calculation.\n", radius_m);
        return false;
    }
    if (parsed_flight_data.empty()) {
        printf("Warning: No parsed data available to calculate PPS.\n");
        return false;
    }

    printf("Calculating Target PPS for %u points with radius %.3f m (Map Gs->RPM->PPS)...\n", // Updated message
           (unsigned int)parsed_flight_data.size(), radius_m);

    for (FlightDataPoint& point : parsed_flight_data) { // Use reference to modify
        float target_rpm = 0.0f;
        float target_pps = 0.0f;

        // Step 1: Calculate target physical angular velocity (omega) in rad/s from Gs
        float accel_g_abs = fabsf(point.acceleration);
        float accel_mps2 = accel_g_abs * G_ACCEL;
        float omega_squared = 0.0f;
        if (radius_m > 0.0f && accel_mps2 > 0.0f) {
             omega_squared = accel_mps2 / radius_m;
        }
        float omega = 0.0f; // rad/s
        if (omega_squared > 0.0f) {
             omega = sqrt(omega_squared);
        }

        // Step 2: Convert omega (rad/s) to target physical RPM
        if (omega > 0.0f) {
            // omega (rad/s) * (60 s / min) / (2*pi rad / rev) = RPM
            target_rpm = omega * 60.0f / (2.0f * M_PI);
        } else {
            target_rpm = 0.0f;
        }

        // Step 3: Convert target physical RPM to required input PPS using the provided factor
        if (target_rpm > 0.0f && RPM_TO_PPS_FACTOR != 0.0f) {
             target_pps = target_rpm / RPM_TO_PPS_FACTOR;
        } else {
             target_pps = 0.0f;
        }

        // Store the final calculated PPS value needed by the motor driver
        point.target_pps = target_pps;

        // Optional: Print intermediate and final values during debug
        // printf("  t=%.3f, G=%.3f -> omega=%.3f rad/s -> RPM=%.3f -> PPS=%.3f Hz\n",
        //        point.timestamp, point.acceleration, omega, target_rpm, point.target_pps);
    }
    printf("PPS calculation complete.\n");
    return true;
}