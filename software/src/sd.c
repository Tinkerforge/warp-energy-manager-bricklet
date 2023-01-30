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
#include "bricklib2/os/coop_task.h"

#include "sdmmc.h"

#include "xmc_rtc.h"
#include "xmc_wdt.h"
#include "lfs.h"

SD sd;
CoopTask sd_task;

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
	if(err != LFS_ERR_OK) {
		lfs_file_close(&sd.lfs, &file);
		return false;
	}
	lfs_ssize_t size = lfs_file_write(&sd.lfs, &file, &df, sizeof(Wallbox5MinDataFile));
	if(size != sizeof(Wallbox5MinDataFile)) {
		logd("nf lfs_file_write %d\n\r", size);
	}
	err = lfs_file_close(&sd.lfs, &file);
	logd("nf lfs_file_close %d\n\r", err);
	if(err != LFS_ERR_OK) {
		err = lfs_file_close(&sd.lfs, &file);
		logd("nf lfs_file_close again %d\n\r", err);
		if(err != LFS_ERR_OK) {
			return false;
		}
	}

	return true;
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
		err = lfs_file_close(&sd.lfs, &file);
		logd("sd_write lfs_file_close after err %d\n\r", err);
		sd_make_path(year, month, day);
		bool ret = sd_write_wallbox_data_point_new_file(f);
		if(!ret) {
			return false;
		}
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

bool sd_read_wallbox_data_point(uint8_t wallbox_id, uint8_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t *data, uint16_t amount, uint16_t offset) {
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
	if((err == LFS_ERR_EXIST) || (err == LFS_ERR_NOENT)) {
		logd("file not found, creating no data values\n\r");
		for(uint16_t i = 0; i < amount; i++) {
			sd.sd_wallbox_data_points_cb_data[i*sizeof(Wallbox5MinData)]   = SD_5MIN_FLAG_NO_DATA;
			sd.sd_wallbox_data_points_cb_data[i*sizeof(Wallbox5MinData)+1] = 0;
			sd.sd_wallbox_data_points_cb_data[i*sizeof(Wallbox5MinData)+2] = 0;
		}
		return true;
	} else if(err != LFS_ERR_OK) {	
		logd("sd_read lfs_file_opencfg %s: %d\n\r", f, err);
		return false;
	}

	const uint16_t pos = 8 + (hour*12 + minute/5) * sizeof(Wallbox5MinData) + offset*sizeof(Wallbox5MinData);
	lfs_ssize_t size = lfs_file_seek(&sd.lfs, &file, pos, LFS_SEEK_SET);
	//logd("sd_read lfs_file_seek %d -> %d\n\r", pos, size);
	if(size != pos) {
		err = lfs_file_close(&sd.lfs, &file);
		logd("sd_read lfs_file_close %d\n\r", err);
	}

	size = lfs_file_read(&sd.lfs, &file, data, amount*sizeof(Wallbox5MinData));
	//logd("sd_read lfs_file_read %d %d -> %d\n\r", data5m->flags, data5m->power, size);
	if(size != amount*sizeof(Wallbox5MinData)) {
		err = lfs_file_close(&sd.lfs, &file);
		logd("sd_read lfs_file_close %d\n\r", err);
		return false;
	}

	err = lfs_file_close(&sd.lfs, &file);
	//logd("sd_read lfs_file_close %d\n\r", err);
	return true;
}

void sd_init_task(void) {
	memset(&sd, 0, sizeof(SD));

	// Status 0xFFFFFFFF = not yet initialized
	sd.sd_status  = 0xFFFFFFFF;
	sd.lfs_status = 0xFFFFFFFF;

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

	coop_task_yield();
	int err = lfs_mount(&sd.lfs, &sd.lfs_config);
	sd.lfs_status = ABS(err);
	coop_task_yield();

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

void sd_tick_task(void) {
	sd.sdmmc_init_last = system_timer_get_ms();

	while(true) {
		// We retry to initialize SD card once per second
		if((sd.sdmmc_init_last != 0) && system_timer_is_time_elapsed_ms(sd.sdmmc_init_last, 1000)) {
			sd_init_task();
		}

		if((sd.sd_status == SDMMC_ERROR_OK) && (sd.lfs_status == LFS_ERR_OK)) {
			// handle setter
			if(sd.wallbox_data_point_end > 0) {
				// Make copy and decrease the end pointer
				// The sd write function might yield, so the data needs to be copied beforehand (because it might be overwritten)
				sd.wallbox_data_point_end--;
				WallboxDataPoint wdp = sd.wallbox_data_point[sd.wallbox_data_point_end];

				Wallbox5MinData wb_5min_data = {
					.flags = wdp.flags,
					.power = wdp.power
				};

				bool ret = sd_write_wallbox_data_point(wdp.wallbox_id, wdp.year, wdp.month, wdp.day, wdp.hour, wdp.minute, &wb_5min_data);
				if(!ret) {
					logd("sd_write_wallbox_data_point failed\n\r");
					bool ret = sd_write_wallbox_data_point(wdp.wallbox_id, wdp.year, wdp.month, wdp.day, wdp.hour, wdp.minute, &wb_5min_data);
					if(!ret) {
						logd("sd_write_wallbox_data_point failed again %d\n\r", sd.wallbox_data_point_end);

						if(sd.wallbox_data_point_end < SD_WALLBOX_DATA_POINT_LENGTH) {
							sd.wallbox_data_point[sd.wallbox_data_point_end] = wdp;
							sd.wallbox_data_point_end++;
						}
					}
				}
			}

			// handle getter
			if(sd.new_sd_wallbox_data_points) {
				for(uint16_t offset = 0; offset < sd.get_sd_wallbox_data_points.amount*sizeof(Wallbox5MinData); offset += 60) {
					logd("handle getter %d\n\r", offset);	
					uint16_t amount = MIN(20, sd.get_sd_wallbox_data_points.amount - offset/sizeof(Wallbox5MinData));
					if(!sd_read_wallbox_data_point(sd.get_sd_wallbox_data_points.wallbox_id, sd.get_sd_wallbox_data_points.year, sd.get_sd_wallbox_data_points.month, sd.get_sd_wallbox_data_points.day, sd.get_sd_wallbox_data_points.hour, sd.get_sd_wallbox_data_points.minute, sd.sd_wallbox_data_points_cb_data, amount, offset/sizeof(Wallbox5MinData))) {

					}
					sd.sd_wallbox_data_points_cb_offset = offset;
					sd.sd_wallbox_data_points_cb_data_length = sd.get_sd_wallbox_data_points.amount*sizeof(Wallbox5MinData);
					sd.new_sd_wallbox_data_points_cb = true;

					while(sd.new_sd_wallbox_data_points_cb) {
						coop_task_yield();
					}
					logd("handle done len %d, offset %d\n\r", sd.sd_wallbox_data_points_cb_data_length, offset);
				}

				sd.new_sd_wallbox_data_points = false;
			}
		}

		coop_task_yield();
	}
}

void sd_init(void) {
	coop_task_init(&sd_task, sd_tick_task);

}

void sd_tick(void) {
	coop_task_tick(&sd_task);
}



int sd_lfs_erase(const struct lfs_config *c, lfs_block_t block) {
	//logd("[lfs erase] block %d\n\r", (uint32_t)block);
	return LFS_ERR_OK;
}

int sd_lfs_read(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, void *buffer, lfs_size_t size) {
	XMC_WDT_Service();
	coop_task_yield(); // TODO: Use interrupts for SPI communication and yield there instead of here
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
	coop_task_yield(); // TODO: Use interrupts for SPI communication and yield there instead of here
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