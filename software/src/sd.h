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

#define SD_PATH_LENGTH 32
#define SD_FILE_LENGTH 32

#define SD_METADATA_MAGIC0  0x42
#define SD_METADATA_MAGIC1  0x43
#define SD_METADATA_VERSION 0

#define SD_METADATA_MAGIC0_POS  0
#define SD_METADATA_MAGIC1_POS  1
#define SD_METADATA_VERSION_POS 2

#define SD_5MIN_PER_DAY (12*24)
#define SD_5MIN_FLAG_NO_DATA (1 << 7)


typedef struct {
    uint8_t flags; // IEC_STATE (bit 0-2) + future use
    uint16_t power; // W
} __attribute__((__packed__)) Wallbox5MinData;

typedef struct {
    uint8_t metadata[8]; // future use (maybe byte 0+1 = magic, byte 2 = version)
    Wallbox5MinData data[SD_5MIN_PER_DAY]; // 12*5*24 minutes (one day)
} __attribute__((__packed__)) Wallbox5MinDataFile;

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

    uint32_t sdmmc_init_last;
} SD;

extern SD sd;

bool sd_read_wallbox_data_point(uint8_t wallbox_id, uint8_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, Wallbox5MinData *data5m);
bool sd_write_wallbox_data_point(uint8_t wallbox_id, uint8_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, Wallbox5MinData *data5m);

int sd_lfs_erase(const struct lfs_config *c, lfs_block_t block);
int sd_lfs_read(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, void *buffer, lfs_size_t size);
int sd_lfs_prog(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, const void *buffer, lfs_size_t size);
int sd_lfs_sync(const struct lfs_config *c);

void sd_init(void);
void sd_tick(void);

#endif