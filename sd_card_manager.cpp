#include "sd_card_manager.h"
#include "tf_card.h" // Includes hardware specifics and FatFs diskio mapping
#include "ff.h"      // FatFs main header
#include "pico/stdlib.h" // For printf, sleep_ms etc.
#include <stdio.h>
#include <string.h> // For memcpy

static FATFS fs;
static bool is_mounted = false;

// --- Initialization ---

bool sd_init() {
    printf("Initializing SD card...\n");
    is_mounted = false;

    // Configure SPI pins (uses defaults from tf_card.c/pico_sdk_init.cmake unless overridden)
    // You can customize pins using pico_fatfs_set_config() like in mainex.cpp if needed
 // Define the GPIO pins you want to use for SPI0
const uint32_t SD_MISO_PIN = 16;
const uint32_t SD_CS_PIN   = 17;
const uint32_t SD_SCK_PIN  = 18;
const uint32_t SD_MOSI_PIN = 19;

pico_fatfs_spi_config_t config = {
    spi0, // Still using the SPI0 peripheral
    CLK_SLOW_DEFAULT,
    CLK_FAST_DEFAULT,
    SD_MISO_PIN,
    SD_CS_PIN,
    SD_SCK_PIN,
    SD_MOSI_PIN,
    true  // use internal pullup for MISO (adjust as needed)
};
    pico_fatfs_set_config(&config);
    

    // Attempt to mount the filesystem
    // The drive path "" means default drive 0.
    // The '1' forces a mount now.
    FRESULT fr = f_mount(&fs, "", 1);
    if (fr != FR_OK) {
        printf("ERROR: Failed to mount SD card (%d)\n", fr);
        // Optionally add retry logic like in mainex.cpp
        return false;
    }

    printf("SD card mounted successfully.\n");

    // Optional: Print card info like in mainex.cpp
    switch (fs.fs_type) {
        case FS_FAT12: printf("  Type: FAT12\n"); break;
        case FS_FAT16: printf("  Type: FAT16\n"); break;
        case FS_FAT32: printf("  Type: FAT32\n"); break;
        case FS_EXFAT: printf("  Type: EXFAT\n"); break;
        default: printf("  Type: Unknown\n"); break;
    }
    printf("  Card size: %llu MB\n", (uint64_t)fs.csize * fs.n_fatent * fs.csize / (1024 * 1024));


    is_mounted = true;
    return true;
}

bool sd_is_mounted() {
    return is_mounted;
}

// --- File Operations ---

bool sd_write_file(const char* filename, const void* data, size_t size) {
    if (!is_mounted) {
        printf("ERROR: SD card not mounted.\n");
        return false;
    }

    FIL fil;
    FRESULT fr;
    UINT bytes_written;

    // Open file with write access. Create if it doesn't exist. Truncate if it does.
    fr = f_open(&fil, filename, FA_WRITE | FA_CREATE_ALWAYS);
    if (fr != FR_OK) {
        printf("ERROR: Failed to open file '%s' for writing (%d)\n", filename, fr);
        return false;
    }

    // Write the data
    fr = f_write(&fil, data, size, &bytes_written);
    if (fr != FR_OK) {
        printf("ERROR: Failed to write to file '%s' (%d)\n", filename, fr);
        f_close(&fil); // Close file even on error
        return false;
    }
    if (bytes_written < size) {
         printf("WARNING: Wrote only %u out of %u bytes to '%s'\n", bytes_written, size, filename);
         // Decide if partial write is an error for your application
    }

    // Close the file (important to flush buffers)
    fr = f_close(&fil);
    if (fr != FR_OK) {
        printf("ERROR: Failed to close file '%s' after writing (%d)\n", filename, fr);
        return false; // Or maybe just warn? Depends on severity.
    }

    printf("Successfully wrote %u bytes to '%s'\n", bytes_written, filename);
    return true;
}

int sd_read_file(const char* filename, void* buffer, size_t max_size) {
    if (!is_mounted) {
        printf("ERROR: SD card not mounted.\n");
        return -1;
    }

    FIL fil;
    FRESULT fr;
    UINT bytes_read;

    // Open file for reading
    fr = f_open(&fil, filename, FA_READ);
    if (fr != FR_OK) {
        printf("ERROR: Failed to open file '%s' for reading (%d)\n", filename, fr);
        return -1;
    }

    // Read data
    fr = f_read(&fil, buffer, max_size, &bytes_read);
    if (fr != FR_OK) {
        printf("ERROR: Failed to read from file '%s' (%d)\n", filename, fr);
        f_close(&fil);
        return -1;
    }

    // Close the file
    fr = f_close(&fil);
    if (fr != FR_OK) {
        printf("ERROR: Failed to close file '%s' after reading (%d)\n", filename, fr);
        // Still return bytes read, but maybe log the error
    }

    printf("Successfully read %u bytes from '%s'\n", bytes_read, filename);
    return (int)bytes_read;
}


long sd_get_file_size(const char* filename) {
     if (!is_mounted) {
        printf("ERROR: SD card not mounted.\n");
        return -1;
    }

    FILINFO fno;
    FRESULT fr;

    fr = f_stat(filename, &fno);
    if (fr != FR_OK) {
         printf("ERROR: Failed to get info for file '%s' (%d) - might not exist.\n", filename, fr);
         return -1;
    }

    return (long)fno.fsize;
}