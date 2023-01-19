/* warp-energy-manager-bricklet
 * Copyright (C) 2022 Olaf Lüke <olaf@tinkerforge.com>
 *
 * sd.c: SD card handling
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

#include "sd.h"

#include <stdlib.h>

#include "bricklib2/logging/logging.h"
#include "bricklib2/utility/util_definitions.h"

#include "sdmmc.h"

#include "xmc_rtc.h"
#include "xmc_wdt.h"
#include "lfs.h"

SD sd;

// Simple base 10 itoa for positive 8 bit numbers
char* sd_itoa(const uint8_t value, char *str) {
	if(value >= 100) {
		str[2] = '0' + (value % 10);
		str[1] = '0' + ((value / 10) % 10);
		str[0] = '0' + (value / 100);
		return str + 3;
	}

	if(value >= 10) {
		str[1] = '0' + (value % 10);
		str[0] = '0' + (value / 10);
		return str + 2;
	}

	str[0] = '0' + value;
	return str + 1;
}

char* sd_get_path_with_filename(uint8_t wallbox_id, uint8_t year, uint8_t month, uint8_t day, char *postfix) {
	static char p[SD_PATH_LENGTH] = {'\0'};
	memset(p, '\0', SD_PATH_LENGTH);

	char *np = p;
	np = sd_itoa(year, np);
	*np++ = '/';
	np = sd_itoa(month, np);
	*np++ = '/';
	np = sd_itoa(day, np);
	*np++ = '/';
	np = sd_itoa(wallbox_id, np);
	strncat(np, postfix, 3);

	return p;
}

void sd_make_path(uint8_t year, uint8_t month, uint8_t day) {
	char path[SD_PATH_LENGTH] = {'\0'};
	sd_itoa(year, &path[0]);
	int err = lfs_mkdir(&sd.lfs, path);
	logd("lfs_mkdir year %s: %d\n\r", path, err);

	strcat(path, "/");
	sd_itoa(month, &path[strlen(path)]);
	err = lfs_mkdir(&sd.lfs, path);
	logd("lfs_mkdir month %s: %d\n\r", path, err);

	strcat(path, "/");
	sd_itoa(day, &path[strlen(path)]);
	err = lfs_mkdir(&sd.lfs, path);
	logd("lfs_mkdir day %s: %d\n\r", path, err);
}

bool sd_write_wallbox_data_point_new_file(char *f) {
	Wallbox5MinDataFile df;
	memset(&df, 0, sizeof(Wallbox5MinDataFile));
	df.metadata[SD_METADATA_MAGIC0_POS]  = SD_METADATA_MAGIC0;
	df.metadata[SD_METADATA_MAGIC1_POS]  = SD_METADATA_MAGIC1;
	df.metadata[SD_METADATA_VERSION_POS] = SD_METADATA_VERSION;
	for(uint16_t i = 0; i < SD_5MIN_PER_DAY; i++) {
		df.data[i].flags = SD_5MIN_FLAG_NO_DATA;
	}

	lfs_file_t file;
	int err = lfs_file_opencfg(&sd.lfs, &file, f, LFS_O_CREAT | LFS_O_RDWR, &sd.lfs_file_config);
	logd("nf lfs_file_opencfg  %d\n\r", err);
	lfs_file_write(&sd.lfs, &file, &df, sizeof(Wallbox5MinDataFile));
	err = lfs_file_close(&sd.lfs, &file);
	logd("nf lfs_file_close %d\n\r", err);
}

bool sd_write_wallbox_data_point(uint8_t wallbox_id, uint8_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, Wallbox5MinData *data5m) {
	if((minute > 55) || (hour > 23) || (day > 31) || (month > 12)) {
		return false;
	}

	char *f = sd_get_path_with_filename(wallbox_id, year, month, day, ".wb");

	lfs_file_t file;
	memset(sd.lfs_file_config.buffer, 0, 512);
	sd.lfs_file_config.attrs = NULL;
	sd.lfs_file_config.attr_count = 0;
	int err = lfs_file_opencfg(&sd.lfs, &file, f, LFS_O_RDWR, &sd.lfs_file_config);
	logd("sd_write lfs_file_opencfg %s: %d\n\r", f, err);
	if((err == LFS_ERR_EXIST) || (err == LFS_ERR_NOENT)) {
		sd_make_path(year, month, day);
		sd_write_wallbox_data_point_new_file(f);
		err = lfs_file_opencfg(&sd.lfs, &file, f, LFS_O_RDWR, &sd.lfs_file_config);
		if(err != LFS_ERR_OK) {
			logd("sd_write lfs_file_opencfg2 %s: %d\nr\r", f, err);
			return false;
		}
	}

	const uint16_t pos = 8 + (hour*12 + minute/5) * sizeof(Wallbox5MinData);
	lfs_ssize_t size = lfs_file_seek(&sd.lfs, &file, pos, LFS_SEEK_SET);
	logd("sd_write lfs_file_seek %d -> %d\n\r", pos, size);
	if(size != pos) {
		err = lfs_file_close(&sd.lfs, &file);
		logd("sd_write lfs_file_close %d\n\r", err);
	}

	size = lfs_file_write(&sd.lfs, &file, data5m, sizeof(Wallbox5MinData));
	logd("sd_write lfs_file_write %d %d -> %d\n\r", data5m->flags, data5m->power, size);
	if(size != sizeof(Wallbox5MinData)) {
		err = lfs_file_close(&sd.lfs, &file);
		logd("sd_write lfs_file_close %d\n\r", err);
		return false;
	}

	err = lfs_file_close(&sd.lfs, &file);
	logd("sd_write lfs_file_close %d\n\r", err);

	return true;
}

bool sd_read_wallbox_data_point(uint8_t wallbox_id, uint8_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, Wallbox5MinData *data5m) {
	if((minute > 55) || (hour > 23) || (day > 31) || (month > 12)) {
		return false;
	}

	//logd("sd_read start\n\r");
	char *f = sd_get_path_with_filename(wallbox_id, year, month, day, ".wb");
	//logd("sd_read sd_get_path_with_filename %s\n\r", f);

	lfs_file_t file;
	memset(sd.lfs_file_config.buffer, 0, 512);
	sd.lfs_file_config.attrs = NULL;
	sd.lfs_file_config.attr_count = 0;
	int err = lfs_file_opencfg(&sd.lfs, &file, f, LFS_O_RDWR, &sd.lfs_file_config);
	//logd("sd_read lfs_file_opencfg %s: %d\n\r", f, err);
	if(err != LFS_ERR_OK) {
		return false;
	}

	const uint16_t pos = 8 + (hour*12 + minute/5) * sizeof(Wallbox5MinData);
	lfs_ssize_t size = lfs_file_seek(&sd.lfs, &file, pos, LFS_SEEK_SET);
	//logd("sd_read lfs_file_seek %d -> %d\n\r", pos, size);
	if(size != pos) {
		err = lfs_file_close(&sd.lfs, &file);
		logd("sd_read lfs_file_close %d\n\r", err);
	}

	size = lfs_file_read(&sd.lfs, &file, data5m, sizeof(Wallbox5MinData));
	//logd("sd_read lfs_file_read %d %d -> %d\n\r", data5m->flags, data5m->power, size);
	if(size != sizeof(Wallbox5MinData)) {
		err = lfs_file_close(&sd.lfs, &file);
		logd("sd_read lfs_file_close %d\n\r", err);
		return false;
	}

	err = lfs_file_close(&sd.lfs, &file);
	//logd("sd_read lfs_file_close %d\n\r", err);
}

void sd_init(void) {
	memset(&sd, 0, sizeof(SD));

	// lfs functions
	sd.lfs_config.read  = sd_lfs_read;
	sd.lfs_config.prog  = sd_lfs_prog;
	sd.lfs_config.erase = sd_lfs_erase;
	sd.lfs_config.sync  = sd_lfs_sync;

	// lfs block device configuration
	sd.lfs_config.read_size      = 512;
	sd.lfs_config.prog_size      = 512;
	sd.lfs_config.block_size     = 512;
	sd.lfs_config.block_count    = 15333376; // sector count for 8gb sd card
	sd.lfs_config.cache_size     = 512;
	sd.lfs_config.lookahead_size = 512;
	sd.lfs_config.block_cycles   = -1; // no wear-leveling (done by sd card itself)

	// lfs buffer
	sd.lfs_config.read_buffer      = sd.lfs_read_buffer;
	sd.lfs_config.prog_buffer      = sd.lfs_prog_buffer;
	sd.lfs_config.lookahead_buffer = sd.lfs_lookahead_buffer;

	// lfs file config
	sd.lfs_file_config.buffer     = sd.lfs_file_buffer;
	sd.lfs_file_config.attr_count = 0;

	SDMMCError sdmmc_error = sdmmc_init();
	sd.sd_status = sdmmc_error;
	if(sdmmc_error != SDMMC_ERROR_OK) {
		logd("sdmmc_init: %d\n\r", sdmmc_error);
		sd.sdmmc_init_last = system_timer_get_ms();
		return;
	}
	sd.sdmmc_init_last = 0;
	
	sd.sector_size     = 512;
	sd.sector_count    = (sdmmc.csd_v2.dev_size_high << 16) | ((sdmmc.csd_v2.dev_size_low + 1) << 10);
	sd.product_rev     = sdmmc.cid.product_rev;
	sd.manufacturer_id = sdmmc.cid.manufacturer_id;
	sd.card_type       = sdmmc.type;
	memcpy(sd.product_name, sdmmc.cid.product_name, 5);

	// Overwrite block count
	sd.lfs_config.block_count = sd.sector_count;

	int err = lfs_mount(&sd.lfs, &sd.lfs_config);
	logd("lfs_mount %d\n\r", err);
	if(err != LFS_ERR_OK) {
		err = lfs_format(&sd.lfs, &sd.lfs_config);
		logd("lfs_format %d\n\r", err);
		if(err != LFS_ERR_OK) {
			sd.lfs_status = ABS(err);
			return;
		}
		err = lfs_mount(&sd.lfs, &sd.lfs_config);
		logd("lfs_mount 2nd try %d\n\r", err);
		if(err != LFS_ERR_OK) {
			sd.lfs_status = ABS(err);
			return;
		}
	}

	lfs_file_t file;

	// read current count
	uint32_t boot_count = 0;
	lfs_file_opencfg(&sd.lfs, &file, "boot_count", LFS_O_RDWR | LFS_O_CREAT, &sd.lfs_file_config);
	lfs_file_read(&sd.lfs, &file, &boot_count, sizeof(boot_count));

 	// update boot count
	boot_count += 1;
	lfs_file_rewind(&sd.lfs, &file);
	logd("lfs_file_write\n\r");
	lfs_file_write(&sd.lfs, &file, &boot_count, sizeof(boot_count));

	// storage is not updated until the file is closed successfully
	logd("lfs_file_close\n\r");
	lfs_file_close(&sd.lfs, &file);


	// release any resources we were using
	// lfs_unmount(&sd.lfs);

	logd("boot_count %d\n\r", boot_count);
}

void sd_tick(void) {
	// We retry to initialize SD card once per second
	if((sd.sdmmc_init_last != 0) && system_timer_is_time_elapsed_ms(sd.sdmmc_init_last, 1000)) {
		sd_init();
	}
}



int sd_lfs_erase(const struct lfs_config *c, lfs_block_t block) {
	//logd("[lfs erase] block %d\n\r", (uint32_t)block);
	return LFS_ERR_OK;
}

int sd_lfs_read(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, void *buffer, lfs_size_t size) {
	XMC_WDT_Service();
	//uint32_t current_tick = xTaskGetTickCount();
	//logd("[lfs read] block addr %d, offset %d, size %d\n\r", block, off, size);
	int8_t ret = sdmmc_read_block(block, buffer);
	//logd("[lfs read] error on read, code: %d\n\r", ret);
	if(ret != SDMMC_ERROR_OK) {
		//logd("[lfs read] error on read, code: %d\n\r", ret);
		return LFS_ERR_IO;
	}
	XMC_WDT_Service();
	return LFS_ERR_OK;
}

int sd_lfs_prog(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, const void *buffer, lfs_size_t size) {
	XMC_WDT_Service();
	//logd("[lfs write] block %d, offset %d, size %d\n\r", block, off, size);
	int16_t ret = sdmmc_write_block(block, buffer);
	//logd("[lfs write] error on write, code: %d\n\r", ret);
	if(ret != SDMMC_ERROR_OK) {
		//logd("[lfs write] error on write, code: %d\n\r", ret);
		return LFS_ERR_IO;
	}
	XMC_WDT_Service();
	return LFS_ERR_OK;
}

int sd_lfs_sync(const struct lfs_config *c) {
	//logd("[lfs sync]\n\r");

	return LFS_ERR_OK;
}