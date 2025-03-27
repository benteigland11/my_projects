#ifndef SDCARD_MANAGER_H
#define SDCARD_MANAGER_H

#include <stdbool.h> // For bool type

// Maximum number of data points we can store from the profile
#define MAX_FLIGHT_DATA_POINTS 1000 // Adjust as needed based on expected file size and Pico RAM

// Structure to hold one data point
typedef struct {
    double time_s;
    double acceleration_g;
} FlightDataPoint;

/**
 * @brief Initializes the SPI peripheral and attempts to mount the SD card filesystem.
 *
 * @return true if initialization and mounting were successful, false otherwise.
 */
bool sd_init();

/**
 * @brief Loads flight profile data from a specified CSV file on the SD card.
 * Parses OpenRocket format: ignores lines until "# Event Launch", reads Time,G data,
 * stops reading data after "# Event APOGEE".
 *
 * @param filename The path to the CSV file (e.g., "/profile.csv").
 * @return The number of data points successfully loaded, or -1 on critical error (e.g., file not found).
 */
int sd_load_flight_profile(const char* filename);

/**
 * @brief Gets the total number of data points loaded from the last successful load.
 *
 * @return The number of data points.
 */
int sd_get_data_count();

/**
 * @brief Gets a specific data point by its index.
 *
 * @param index The index of the data point (0 to sd_get_data_count() - 1).
 * @param data_point Pointer to a FlightDataPoint struct where the data will be copied.
 * @return true if the index was valid and data was copied, false otherwise.
 */
bool sd_get_data_point(int index, FlightDataPoint* data_point);

#endif // SDCARD_MANAGER_H