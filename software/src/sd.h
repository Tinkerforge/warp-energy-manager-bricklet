/* warp-energy-manager-bricklet
 * Copyright (C) 2022 Olaf Lüke <olaf@tinkerforge.com>
 *
 * sd.h: SD card handling
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef SD_H
#define SD_H

#define LFS_NO_MALLOC

#include "lfs.h"
#include "communication.h"
#include "xmc_gpio.h"

#define SD_PATH_LENGTH 32
#define SD_FILE_LENGTH 32

#define SD_METADATA_MAGIC        0x4243
#define SD_METADATA_VERSION      0
#define SD_METADATA_TYPE_WB_5MIN 0
#define SD_METADATA_TYPE_WB_1DAY 1
#define SD_METADATA_TYPE_EM_5MIN 2
#define SD_METADATA_TYPE_EM_1DAY 3

#define SD_5MIN_PER_DAY (12*24)
#define SD_1DAY_PER_MONTH (31)
#define SD_5MIN_FLAG_NO_DATA (1 << 7)
#define SD_FILE_NO_DAY_IN_PATH 0xFF

#define SD_WALLBOX_DATA_POINT_LENGTH 8
#define SD_WALLBOX_DAILY_DATA_POINT_LENGTH 2
#define SD_ENERGY_MANAGER_DATA_POINT_LENGTH 8
#define SD_ENERGY_MANAGER_DAILY_DATA_POINT_LENGTH 2

#define SD_CALLBACK_TIMEOUT 1000 // ms

#define SD_DETECTED_PIN P2_1

typedef struct {
    uint16_t magic;
    uint8_t version;
    uint8_t type;
    uint8_t future_use[4];
} __attribute__((__packed__)) SDMetadata;


typedef struct {
    uint8_t flags; // IEC_STATE (bit 0-2) + future use
    uint16_t power; // W
} __attribute__((__packed__)) Wallbox5MinData;

typedef struct {
    SDMetadata metadata; 
    Wallbox5MinData data[SD_5MIN_PER_DAY];
} __attribute__((__packed__)) Wallbox5MinDataFile;

typedef struct {
    uint32_t energy; // kWh
} __attribute__((__packed__)) Wallbox1DayData;

typedef struct {
    SDMetadata metadata; 
    Wallbox1DayData data[SD_1DAY_PER_MONTH]; // 31 days (one month)
} __attribute__((__packed__)) Wallbox1DayDataFile;


typedef struct {
	uint8_t flags; // bit 0 = 1p/3p, bit 1-2 = input, bit 3-4 = output
	int32_t power_grid; // W
	int32_t power_general[6]; // W
} __attribute__((__packed__)) EnergyManager5MinData;

typedef struct {
    SDMetadata metadata; 
    EnergyManager5MinData data[SD_5MIN_PER_DAY];
} __attribute__((__packed__)) EnergyManager5MinDataFile;

typedef struct {
	uint32_t energy_grid_in_; // generated in kWh 
	uint32_t energy_grid_out; // consumed in kWh
	uint32_t energy_general_in[6]; // generated in kWh
	uint32_t energy_general_out[6]; // consumed in kWh
} __attribute__((__packed__)) EnergyManager1DayData;

typedef struct {
    SDMetadata metadata; 
    EnergyManager1DayData data[SD_1DAY_PER_MONTH]; // 31 days (one month)
} __attribute__((__packed__)) EnergyManager1DayDataFile;


typedef struct {
	uint32_t wallbox_id;
	uint8_t year;
	uint8_t month;
	uint8_t day;
	uint8_t hour;
	uint8_t minute;
	uint8_t flags;
	uint16_t power;
} __attribute__((__packed__)) WallboxDataPoint;

typedef struct {
	uint32_t wallbox_id;
	uint8_t year;
	uint8_t month;
	uint8_t day;
	uint32_t energy;
} __attribute__((__packed__)) WallboxDailyDataPoint;

typedef struct {
	uint8_t year;
	uint8_t month;
	uint8_t day;
	uint8_t hour;
	uint8_t minute;
	uint8_t flags;
	int32_t power_grid;
	int32_t power_general[6];
} __attribute__((__packed__)) EnergyManagerDataPoint;

typedef struct {
	uint8_t year;
	uint8_t month;
	uint8_t day;
	uint32_t energy_grid_in;
	uint32_t energy_grid_out;
	uint32_t energy_general_in[6];
	uint32_t energy_general_out[6];
} __attribute__((__packed__)) EnergyManagerDailyDataPoint;

typedef struct {
	uint32_t sd_status;
	uint32_t lfs_status;
	uint16_t sector_size;
	uint32_t sector_count;
	uint32_t card_type;
	uint8_t product_rev;
	char product_name[5];
	uint8_t manufacturer_id;

    lfs_t lfs;
    struct lfs_config lfs_config;
    struct lfs_file_config lfs_file_config;
    uint8_t lfs_file_buffer[512];
    uint8_t lfs_read_buffer[512];
    uint8_t lfs_prog_buffer[512];
    uint8_t lfs_lookahead_buffer[512];

    uint32_t sd_rw_error_count;

    uint32_t sdmmc_init_last;

    WallboxDataPoint wallbox_data_point[SD_WALLBOX_DATA_POINT_LENGTH];
    uint8_t wallbox_data_point_end;
    WallboxDailyDataPoint wallbox_daily_data_point[SD_WALLBOX_DAILY_DATA_POINT_LENGTH];
    uint8_t wallbox_daily_data_point_end;
    EnergyManagerDataPoint energy_manager_data_point[SD_ENERGY_MANAGER_DATA_POINT_LENGTH];
    uint8_t energy_manager_data_point_end;
    EnergyManagerDailyDataPoint energy_manager_daily_data_point[SD_ENERGY_MANAGER_DAILY_DATA_POINT_LENGTH];
    uint8_t energy_manager_daily_data_point_end;


    GetSDWallboxDataPoints get_sd_wallbox_data_points;
    volatile bool new_sd_wallbox_data_points;
    GetSDWallboxDailyDataPoints get_sd_wallbox_daily_data_points;
    volatile bool new_sd_wallbox_daily_data_points;
    GetSDEnergyManagerDataPoints get_sd_energy_manager_data_points;
    volatile bool new_sd_energy_manager_data_points;
    GetSDEnergyManagerDailyDataPoints get_sd_energy_manager_daily_data_points;
    volatile bool new_sd_energy_manager_daily_data_points;


    uint16_t sd_wallbox_data_points_cb_data_length;
    uint16_t sd_wallbox_data_points_cb_offset;
    uint8_t sd_wallbox_data_points_cb_data[60];
    volatile bool new_sd_wallbox_data_points_cb;

    uint16_t sd_wallbox_daily_data_points_cb_data_length;
    uint16_t sd_wallbox_daily_data_points_cb_offset;
    uint32_t sd_wallbox_daily_data_points_cb_data[15];
    volatile bool new_sd_wallbox_daily_data_points_cb;

    uint16_t sd_energy_manager_data_points_cb_data_length;
    uint16_t sd_energy_manager_data_points_cb_offset;
    uint8_t sd_energy_manager_data_points_cb_data[58];
    volatile bool new_sd_energy_manager_data_points_cb;

    uint16_t sd_energy_manager_daily_data_points_cb_data_length;
    uint16_t sd_energy_manager_daily_data_points_cb_offset;
    uint8_t sd_energy_manager_daily_data_points_cb_data[56];
    volatile bool new_sd_energy_manager_daily_data_points_cb;

    bool buffered_read_is_open;
    int buffered_read_current_err;
    uint32_t buffered_read_current_wallbox_id;
    uint8_t buffered_read_current_year;
    uint8_t buffered_read_current_month;
    uint8_t buffered_read_current_day;
    uint8_t buffered_read_current_postfix;
    lfs_file_t buffered_read_file;
} SD;

extern SD sd;

bool sd_read_wallbox_data_point(uint32_t wallbox_id, uint8_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t *data, uint16_t amount, uint16_t offset);
bool sd_write_wallbox_data_point(uint32_t wallbox_id, uint8_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, Wallbox5MinData *data5m);

int sd_lfs_erase(const struct lfs_config *c, lfs_block_t block);
int sd_lfs_read(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, void *buffer, lfs_size_t size);
int sd_lfs_prog(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, const void *buffer, lfs_size_t size);
int sd_lfs_sync(const struct lfs_config *c);

void sd_init(void);
void sd_tick(void);

#endif