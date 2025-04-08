#ifndef SD_CARD_MANAGER_H
#define SD_CARD_MANAGER_H

#include <stdbool.h>
#include <stddef.h> // For size_t

// Initialize the SD card system and mount the filesystem.
// Returns true on success, false on failure.
bool sd_init();

// Check if the SD card is mounted and ready.
// Returns true if mounted, false otherwise.
bool sd_is_mounted();

// Write data to a file. Creates the file if it doesn't exist, overwrites otherwise.
// Returns true on success, false on failure.
bool sd_write_file(const char* filename, const void* data, size_t size);

// Read data from a file.
// Reads up to 'max_size' bytes into the 'buffer'.
// Returns the number of bytes read, or -1 on error.
int sd_read_file(const char* filename, void* buffer, size_t max_size);

// Get the size of a file.
// Returns the file size in bytes, or -1 if the file doesn't exist or an error occurs.
long sd_get_file_size(const char* filename);

#endif // SD_CARD_MANAGER_H