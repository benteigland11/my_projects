#ifndef OPENROCKET_PARSER_H
#define OPENROCKET_PARSER_H

#include <cstddef> // For size_t
#include <cstdint> // For uint types like uint32_t
#include "pico/stdlib.h" // Includes basic types and potentially XIP_BASE, PICO_FLASH_SIZE_BYTES

// --- Configuration: Flash Storage ---

// Example: Reserve 64KB near the end of 2MB flash.
// Adjust PICO_FLASH_SIZE_BYTES if your board has a different flash size.
// Ensure FLASH_TARGET_OFFSET is aligned to FLASH_SECTOR_SIZE (4096).
#define FLASH_STORAGE_MAX_SIZE (64 * 1024)
// Default to PICO_FLASH_SIZE_BYTES if available (usually 2MB), otherwise define manually
#ifndef PICO_FLASH_SIZE_BYTES
    #define PICO_FLASH_SIZE_BYTES (2 * 1024 * 1024) // Default assumption if not defined by SDK
#endif
#define FLASH_TARGET_OFFSET    (PICO_FLASH_SIZE_BYTES - FLASH_STORAGE_MAX_SIZE)
#define FLASH_STORAGE_ADDRESS  (XIP_BASE + FLASH_TARGET_OFFSET)

// Header stored at the beginning of the flash region
struct FlashDataHeader {
    uint32_t magic;      // To verify data presence/version
    uint32_t data_size;  // Actual size of the stored OpenRocket data in bytes
};
#define FLASH_DATA_MAGIC 0xFDEDBEEF // Example magic number ("FEED BEEF" sort of)

// --- Data Structure for Parsed Flight Data ---
struct FlightDataPoint {
    float timestamp;
    float acceleration; // Original G value
    float target_pps;   // Calculated Pulses Per Second (Hz) for motor
};

// --- Function Declarations: Flash Handling ---

/**
 * @brief Reads an OpenRocket file from the SD card and writes it to the defined flash region.
 * @param sd_filename The full path to the file on the SD card.
 * @return True on success, false on failure.
 */
bool store_openrocket_to_flash(const char* sd_filename);

/**
 * @brief Reads the previously stored OpenRocket data from flash into a RAM buffer.
 * @param buffer Pointer to the RAM buffer where data will be copied.
 * @param buffer_size The maximum size of the provided RAM buffer.
 * @return The number of bytes read (actual data size), or -1 on error.
 */
int read_openrocket_from_flash(void* buffer, size_t buffer_size);

/**
 * @brief Gets the size of the data currently stored in flash, based on the header.
 * @return The size of the stored data in bytes, or 0 if no valid header is found.
 */
size_t get_stored_data_size_from_flash();

// --- Function Declarations: CSV Parsing ---

/**
 * @brief Parses the flight data buffer (read from flash).
 * Stores valid timestamp,acceleration pairs between "# Event IGNITION" and "# Event APOGEE".
 * @param data_buffer Pointer to the character buffer holding the CSV data.
 * @param data_size The size of the data in the buffer.
 * @return True if parsing finished successfully (IGNITION found), false otherwise.
 */
bool parse_openrocket_data(const char* data_buffer, size_t data_size);

// --- Function Declarations: Accessors for Parsed Data ---

/**
 * @brief Gets the number of valid data points parsed.
 * @return The count of stored FlightDataPoints.
 */
size_t get_parsed_data_count();

/**
 * @brief Gets a specific parsed data point by index.
 * @param index The index of the data point (0 to count-1).
 * @return The FlightDataPoint at the specified index. Returns {0, 0} if index is out of bounds.
 */
FlightDataPoint get_parsed_data_point(size_t index);

/**
 * @brief Calculates target PPS for all previously parsed data points based on radius.
 * Populates the target_pps field in the stored FlightDataPoints.
 * @param radius_m The radius of the centrifuge arm in meters.
 * @return True on success, false if no data was parsed or radius is invalid.
 */
bool calculate_pps_for_parsed_data(float radius_m);

#endif // OPENROCKET_PARSER_H