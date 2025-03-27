#include "SDCardManager.h"
#include "hardware/spi.h"
#include "pico/stdlib.h"
#include "ff.h"
#include "diskio.h"
#include <stdio.h>
#include <string.h>

// --- SPI/SD Pin Configuration ---
#define SPI_PORT spi0
#define SPI_SCK_PIN 18
#define SPI_MOSI_PIN 19
#define SPI_MISO_PIN 16
#define SD_CS_PIN 17

// Add near the top, outside any function
DWORD get_fattime(void) {
    // Return a fixed timestamp (e.g., 2025-03-27 00:00:00)
    return ((DWORD)(2025 - 1980) << 25) // Year (since 1980)
         | ((DWORD)3 << 21)             // Month
         | ((DWORD)27 << 16)            // Day
         | ((DWORD)0 << 11)             // Hour
         | ((DWORD)0 << 5)              // Minute
         | ((DWORD)0 >> 1);             // Second / 2
}

// --- Module-Internal State ---
static FATFS s_fs;
static bool s_sd_mounted = false;
static FlightDataPoint s_flight_data[MAX_FLIGHT_DATA_POINTS];
static int s_data_point_count = 0;

// --- SD Card SPI Functions ---
static void sd_select() { gpio_put(SD_CS_PIN, 0); sleep_us(1); }
static void sd_deselect() { gpio_put(SD_CS_PIN, 1); sleep_us(1); }

static uint8_t sd_spi_transfer(uint8_t byte) {
    uint8_t received;
    spi_write_read_blocking(SPI_PORT, &byte, &received, 1);
    return received;
}

static void sd_send_cmd(uint8_t cmd, uint32_t arg) {
    uint8_t buffer[6];
    buffer[0] = 0x40 | cmd;
    buffer[1] = (arg >> 24) & 0xFF;
    buffer[2] = (arg >> 16) & 0xFF;
    buffer[3] = (arg >> 8) & 0xFF;
    buffer[4] = arg & 0xFF;
    buffer[5] = (cmd == 0) ? 0x95 : (cmd == 8 ? 0x87 : 0xFF); // CRC for CMD0, CMD8

    sd_select();
    for (int i = 0; i < 6; i++) sd_spi_transfer(buffer[i]);
}

static uint8_t sd_read_r1() {
    uint8_t r1;
    for (int i = 0; i < 8; i++) {
        r1 = sd_spi_transfer(0xFF);
        if (!(r1 & 0x80)) return r1;
    }
    return r1;
}

static bool sd_init_card() {
    spi_init(SPI_PORT, 400 * 1000);
    gpio_set_function(SPI_SCK_PIN, GPIO_FUNC_SPI);
    gpio_set_function(SPI_MOSI_PIN, GPIO_FUNC_SPI);
    gpio_set_function(SPI_MISO_PIN, GPIO_FUNC_SPI);
    gpio_init(SD_CS_PIN);
    gpio_set_dir(SD_CS_PIN, GPIO_OUT);
    sd_deselect();

    for (int i = 0; i < 10; i++) sd_spi_transfer(0xFF);

    sd_send_cmd(0, 0);
    if (sd_read_r1() != 0x01) {
        printf("ERROR: SD CMD0 failed\n");
        return false;
    }

    sd_send_cmd(8, 0x1AA);
    uint8_t r1 = sd_read_r1();
    if (r1 == 0x01) {
        for (int i = 0; i < 4; i++) sd_spi_transfer(0xFF);
    } else if (r1 != 0x05) {
        printf("ERROR: SD CMD8 failed (%d)\n", r1);
        return false;
    }

    for (int i = 0; i < 1000; i++) {
        sd_send_cmd(55, 0);
        sd_read_r1();
        sd_send_cmd(41, 0x40000000);
        r1 = sd_read_r1();
        if (r1 == 0x00) break;
        sleep_ms(1);
    }
    if (r1 != 0x00) {
        printf("ERROR: SD ACMD41 failed\n");
        return false;
    }

    sd_deselect();
    spi_set_baudrate(SPI_PORT, 12500 * 1000);
    return true;
}

static bool sd_read_sector(uint32_t sector, uint8_t* buffer) {
    sd_select();
    sd_send_cmd(17, sector);
    uint8_t r1 = sd_read_r1();
    if (r1 != 0x00) {
        sd_deselect();
        return false;
    }

    uint8_t token;
    for (int i = 0; i < 1000; i++) {
        token = sd_spi_transfer(0xFF);
        if (token == 0xFE) break;
    }
    if (token != 0xFE) {
        sd_deselect();
        return false;
    }

    for (int i = 0; i < 512; i++) buffer[i] = sd_spi_transfer(0xFF);
    sd_spi_transfer(0xFF); // CRC
    sd_spi_transfer(0xFF); // CRC
    sd_deselect();
    return true;
}

// --- FatFs DiskIO Functions ---
DSTATUS disk_initialize(BYTE pdrv) {
    return sd_init_card() ? 0 : STA_NOINIT;
}

DSTATUS disk_status(BYTE pdrv) {
    return s_sd_mounted ? 0 : STA_NOINIT;
}

DRESULT disk_read(BYTE pdrv, BYTE* buff, LBA_t sector, UINT count) {
    for (UINT i = 0; i < count; i++) {
        if (!sd_read_sector(sector + i, buff + (i * 512))) {
            return RES_ERROR;
        }
    }
    return RES_OK;
}

DRESULT disk_write(BYTE pdrv, const BYTE* buff, LBA_t sector, UINT count) {
    return RES_NOTRDY; // Read-only for now
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void* buff) {
    switch (cmd) {
        case CTRL_SYNC: return RES_OK;
        case GET_SECTOR_COUNT: *(DWORD*)buff = 0xFFFFFFFF; return RES_OK;
        case GET_SECTOR_SIZE: *(WORD*)buff = 512; return RES_OK;
        case GET_BLOCK_SIZE: *(DWORD*)buff = 1; return RES_OK;
        default: return RES_PARERR;
    }
}

// --- SDCardManager Functions ---
bool sd_init() {
    printf("Initializing SD card...\n");
    FRESULT fr = f_mount(&s_fs, "", 1);
    if (fr != FR_OK) {
        printf("ERROR: Failed to mount SD card (%d)\n", fr);
        s_sd_mounted = false;
        return false;
    }
    printf("SD card mounted successfully.\n");
    s_sd_mounted = true;
    return true;
}

int sd_load_flight_profile(const char* filename) {
    if (!s_sd_mounted) {
        printf("ERROR: SD card not mounted.\n");
        return -1;
    }

    printf("Loading flight profile: %s\n", filename);
    s_data_point_count = 0;

    FIL fil;
    FRESULT fr = f_open(&fil, filename, FA_READ);
    if (fr != FR_OK) {
        printf("ERROR: Failed to open file '%s' (%d)\n", filename, fr);
        return -1;
    }

    printf("File opened. Reading data...\n");

    char line_buffer[128];
    bool launch_detected = false;
    bool apogee_detected = false;

    while (f_gets(line_buffer, sizeof(line_buffer), &fil)) {
        if (line_buffer[0] == '#') {
            if (!launch_detected) {
                if (strstr(line_buffer, " Event Launch") != NULL) {
                    printf("--- Launch Detected ---\n");
                    launch_detected = true;
                }
            } else if (!apogee_detected) {
                if (strstr(line_buffer, " Event APOGEE") != NULL) {
                    printf("--- Apogee Detected ---\n");
                    apogee_detected = true;
                    break;
                }
            }
            continue;
        }

        if (launch_detected && !apogee_detected) {
            double time_val, g_val;
            if (sscanf(line_buffer, "%lf,%lf", &time_val, &g_val) == 2) {
                if (s_data_point_count < MAX_FLIGHT_DATA_POINTS) {
                    s_flight_data[s_data_point_count].time_s = time_val;
                    s_flight_data[s_data_point_count].acceleration_g = g_val;
                    s_data_point_count++;
                } else {
                    printf("WARNING: Max data points (%d) reached.\n", MAX_FLIGHT_DATA_POINTS);
                    apogee_detected = true;
                    break;
                }
            }
        }
    }

    f_close(&fil);
    printf("Finished processing file. Loaded %d data points.\n", s_data_point_count);

    if (!launch_detected) printf("WARNING: Launch event not found.\n");
    if (!apogee_detected && s_data_point_count < MAX_FLIGHT_DATA_POINTS) {
        printf("WARNING: Apogee event not found.\n");
    }

    return s_data_point_count;
}

int sd_get_data_count() { return s_data_point_count; }

bool sd_get_data_point(int index, FlightDataPoint* data_point) {
    if (data_point != NULL && index >= 0 && index < s_data_point_count) {
        *data_point = s_flight_data[index];
        return true;
    }
    return false;
}