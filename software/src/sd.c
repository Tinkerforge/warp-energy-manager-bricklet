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

#include "configs/config_sdmmc.h"
#include "sdmmc.h"

#include "xmc_rtc.h"
#include "xmc_wdt.h"
#include "lfs.h"
#include "sd_new_file_objects.h"
#include "data_storage.h"

SD sd;
CoopTask sd_task;

bool sd_lfs_format = false;

#define SD_POSTFIX_WB 0
#define SD_POSTFIX_EM 1
static const char SD_POSTFIXES[2][4] = {
	".wb",
	".em"
};

static const char BASE58_ALPHABET[] = "123456789abcdefghijkmnopqrstuvwxyzABCDEFGHJKLMNPQRSTUVWXYZ";
#define BASE58_MAX_STR_SIZE 7 // for uint32_t

char* base58_encode(uint32_t value, char *str) {
	uint32_t mod;
	char reverse_str[BASE58_MAX_STR_SIZE] = {'\0'};
	int i = 0;
	int k = 0;

	while (value >= 58) {
		mod = value % 58;
		reverse_str[i] = BASE58_ALPHABET[mod];
		value = value / 58;
		++i;
	}

	reverse_str[i] = BASE58_ALPHABET[value];

	for (k = 0; k <= i; k++) {
		str[k] = reverse_str[i - k];
	}

	return str+i+1;
}

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

char* sd_get_path_with_filename(uint32_t wallbox_id, uint8_t year, uint8_t month, uint8_t day, uint8_t postfix) {
	static char p[SD_PATH_LENGTH] = {'\0'};
	memset(p, '\0', SD_PATH_LENGTH);

	char *np = p;
	np = sd_itoa(year, np);
	*np++ = '/';
	np = sd_itoa(month, np);
	*np++ = '/';
	if(day != SD_FILE_NO_DAY_IN_PATH) { // Use 0xFF to not add day to path
		np = sd_itoa(day, np);
		*np++ = '/';
	}
	np = base58_encode(wallbox_id, np);
	strncat(np, SD_POSTFIXES[postfix], 3);

	return p;
}

void sd_make_path(uint8_t year, uint8_t month, uint8_t day) {
	char path[SD_PATH_LENGTH] = {'\0'};
	sd_itoa(year, &path[0]);
	lfs_mkdir(&sd.lfs, path);

	strcat(path, "/");
	sd_itoa(month, &path[strlen(path)]);
	lfs_mkdir(&sd.lfs, path);

	if(day == SD_FILE_NO_DAY_IN_PATH) {
		return;
	}

	strcat(path, "/");
	sd_itoa(day, &path[strlen(path)]);
	lfs_mkdir(&sd.lfs, path);
}

bool sd_write_wallbox_data_point_new_file(char *f) {
	lfs_file_t file;

	memset(sd.lfs_file_config.buffer, 0, 512);
	sd.lfs_file_config.attrs = NULL;
	sd.lfs_file_config.attr_count = 0;
	int err = lfs_file_opencfg(&sd.lfs, &file, f, LFS_O_CREAT | LFS_O_RDWR, &sd.lfs_file_config);
	if(err != LFS_ERR_OK) {
		logw("lfs_file_opencfg %d\n\r", err);
		lfs_file_close(&sd.lfs, &file);
		return false;
	}

	lfs_ssize_t size = lfs_file_write(&sd.lfs, &file, &sd_new_file_wb_5min, sizeof(Wallbox5MinDataFile));
	if(size != sizeof(Wallbox5MinDataFile)) {
		logw("lfs_file_write %d vs %d\n\r", size, sizeof(Wallbox5MinDataFile));
		err = lfs_file_close(&sd.lfs, &file);
		logw("lfs_file_close %d\n\r", err);
		return false;
	}

	err = lfs_file_close(&sd.lfs, &file);
	if(err != LFS_ERR_OK) {
		logw("lfs_file_close %d\n\r", err);
		return false;
	}

	return true;
}

bool sd_write_wallbox_data_point(uint32_t wallbox_id, uint8_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, Wallbox5MinData *data5m) {
	char *f = sd_get_path_with_filename(wallbox_id, year, month, day, SD_POSTFIX_WB);

	memset(sd.lfs_file_config.buffer, 0, 512);
	sd.lfs_file_config.attrs = NULL;
	sd.lfs_file_config.attr_count = 0;

	lfs_file_t file;
	int err = lfs_file_opencfg(&sd.lfs, &file, f, LFS_O_RDWR, &sd.lfs_file_config);
	if((err == LFS_ERR_EXIST) || (err == LFS_ERR_NOENT)) {
		err = lfs_file_close(&sd.lfs, &file);

		sd_make_path(year, month, day);
		bool ret = sd_write_wallbox_data_point_new_file(f);
		if(!ret) {
			logw("file not found and could not create new file %s\n\r", f);
			return false;
		}

		err = lfs_file_opencfg(&sd.lfs, &file, f, LFS_O_RDWR, &sd.lfs_file_config);
		if(err != LFS_ERR_OK) {
			logw("lfs_file_opencfg %s: %d\n\r", f, err);
			return false;
		}
	}

	const uint16_t pos = sizeof(SDMetadata) + (hour*12 + minute/5) * sizeof(Wallbox5MinData);
	lfs_ssize_t size   = lfs_file_seek(&sd.lfs, &file, pos, LFS_SEEK_SET);
	if(size != pos) {
		logw("lfs_file_seek %d vs %d\n\r", pos, size);
		err = lfs_file_close(&sd.lfs, &file);
		logw("lfs_file_close %d\n\r", err);
		return false;
	}

	size = lfs_file_write(&sd.lfs, &file, data5m, sizeof(Wallbox5MinData));
	if(size != sizeof(Wallbox5MinData)) {
		logw("lfs_file_write flags %d, power %d, size %d vs %d\n\r", data5m->flags, data5m->power, size, sizeof(Wallbox5MinData));
		err = lfs_file_close(&sd.lfs, &file);
		logw("lfs_file_close %d\n\r", err);
		return false;
	}

	err = lfs_file_close(&sd.lfs, &file);
	if(err != LFS_ERR_OK) {
		logd("lfs_file_close %d\n\r", err);
		return false;
	}
	return true;
}

lfs_file_t* sd_lfs_open_buffered_read(uint32_t wallbox_id, uint8_t year, uint8_t month, uint8_t day, uint8_t postfix, int *err) {
	// Check if file is already open
	if(sd.buffered_read_is_open) {
		// Check if open file is same as requested file
		if((sd.buffered_read_current_wallbox_id == wallbox_id) && (sd.buffered_read_current_year == year) && (sd.buffered_read_current_month == month) && (sd.buffered_read_current_day == day) && (sd.buffered_read_current_postfix == postfix)) {
			*err = sd.buffered_read_current_err;
			return &sd.buffered_read_file;
		} else {
			// File is open but not correct -> close file
			int err = lfs_file_close(&sd.lfs, &sd.buffered_read_file);
			if(err != LFS_ERR_OK) {
				logd("lfs_file_close %d\n\r", err);
			}
			sd.buffered_read_is_open = false;
		}
	// If file is not open but current wallbox fits, and error was non-existing file, return error
	} else if((sd.buffered_read_current_err == LFS_ERR_EXIST) || (sd.buffered_read_current_err == LFS_ERR_NOENT)) {
		if((sd.buffered_read_current_wallbox_id == wallbox_id) && (sd.buffered_read_current_year == year) && (sd.buffered_read_current_month == month) && (sd.buffered_read_current_day == day) && (sd.buffered_read_current_postfix == postfix)) {
			*err = sd.buffered_read_current_err;
			return &sd.buffered_read_file;
		}
	}

	sd.buffered_read_current_wallbox_id = wallbox_id;
	sd.buffered_read_current_year       = year;
	sd.buffered_read_current_month      = month;
	sd.buffered_read_current_day        = day;
	sd.buffered_read_current_postfix    = postfix;

	char *f = sd_get_path_with_filename(wallbox_id, year, month, day, postfix);
	memset(sd.lfs_file_config.buffer, 0, 512);
	sd.lfs_file_config.attrs = NULL;
	sd.lfs_file_config.attr_count = 0;
	sd.buffered_read_current_err = lfs_file_opencfg(&sd.lfs, &sd.buffered_read_file, f, LFS_O_RDONLY, &sd.lfs_file_config);

	if(sd.buffered_read_current_err != LFS_ERR_OK) {
		logw("lfs_file_opencfg %s: %d\n\r", f, sd.buffered_read_current_err);
		sd.buffered_read_is_open = false;
	} else {
		sd.buffered_read_is_open =  true;
	}

	*err = sd.buffered_read_current_err;
	return &sd.buffered_read_file;
}

int sd_lfs_close_buffered_read(void) {
	sd.buffered_read_is_open = false;
	return lfs_file_close(&sd.lfs, &sd.buffered_read_file);
}

bool sd_read_wallbox_data_point(uint32_t wallbox_id, uint8_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t *data, uint16_t amount, uint16_t offset) {
	int err = LFS_ERR_OK;
	lfs_file_t *file = sd_lfs_open_buffered_read(wallbox_id, year, month, day, SD_POSTFIX_WB, &err);
	if((err == LFS_ERR_EXIST) || (err == LFS_ERR_NOENT)) {
		for(uint16_t i = 0; i < amount; i++) {
			data[i*sizeof(Wallbox5MinData)]   = SD_5MIN_FLAG_NO_DATA;
			data[i*sizeof(Wallbox5MinData)+1] = 0;
			data[i*sizeof(Wallbox5MinData)+2] = 0;
		}
		return true;
	} else if(err != LFS_ERR_OK) {
		logw("lfs_file_opencfg %d\n\r", err);
		return false;
	}

	const uint16_t pos = 8 + (hour*12 + minute/5) * sizeof(Wallbox5MinData) + offset*sizeof(Wallbox5MinData);
	lfs_ssize_t size   = lfs_file_seek(&sd.lfs, file, pos, LFS_SEEK_SET);
	if(size != pos) {
		logw("lfs_file_seek %d vs %d\n\r", pos, size);
		err = sd_lfs_close_buffered_read();
		logw("lfs_file_close %d\n\r", err);
		return false;
	}

	size = lfs_file_read(&sd.lfs, file, data, amount*sizeof(Wallbox5MinData));
	if(size != amount*sizeof(Wallbox5MinData)) {
		logw("lfs_file_read flags size %d vs %d\n\r", size, amount*sizeof(Wallbox5MinData));
		err = sd_lfs_close_buffered_read();
		logw("lfs_file_close %d\n\r", err);
		return false;
	}

	return true;
}

bool sd_write_wallbox_daily_data_point_new_file(char *f) {
	lfs_file_t file;

	memset(sd.lfs_file_config.buffer, 0, 512);
	sd.lfs_file_config.attrs = NULL;
	sd.lfs_file_config.attr_count = 0;
	int err = lfs_file_opencfg(&sd.lfs, &file, f, LFS_O_CREAT | LFS_O_RDWR, &sd.lfs_file_config);
	if(err != LFS_ERR_OK) {
		logw("lfs_file_opencfg %d\n\r", err);
		lfs_file_close(&sd.lfs, &file);
		return false;
	}
	lfs_ssize_t size = lfs_file_write(&sd.lfs, &file, &sd_new_file_wb_1day, sizeof(Wallbox1DayDataFile));
	if(size != sizeof(Wallbox1DayDataFile)) {
		logw("lfs_file_write %d\n\r", size);
		err = lfs_file_close(&sd.lfs, &file);
		logw("lfs_file_close %d\n\r", err);
		return false;
	}
	err = lfs_file_close(&sd.lfs, &file);
	if(err != LFS_ERR_OK) {
		logd("lfs_file_close %d\n\r", err);
		return false;
	}

	return true;
}

bool sd_write_wallbox_daily_data_point(uint32_t wallbox_id, uint8_t year, uint8_t month, uint8_t day, Wallbox1DayData *data1d) {
	char *f = sd_get_path_with_filename(wallbox_id, year, month, SD_FILE_NO_DAY_IN_PATH, SD_POSTFIX_WB);

	lfs_file_t file;
	memset(sd.lfs_file_config.buffer, 0, 512);
	sd.lfs_file_config.attrs = NULL;
	sd.lfs_file_config.attr_count = 0;

	int err = lfs_file_opencfg(&sd.lfs, &file, f, LFS_O_RDWR, &sd.lfs_file_config);
	if((err == LFS_ERR_EXIST) || (err == LFS_ERR_NOENT)) {
		err = lfs_file_close(&sd.lfs, &file);
		sd_make_path(year, month, SD_FILE_NO_DAY_IN_PATH);
		bool ret = sd_write_wallbox_daily_data_point_new_file(f);
		if(!ret) {
			logw("file not found and could not create new file %s\n\r", f);
			return false;
		}
		err = lfs_file_opencfg(&sd.lfs, &file, f, LFS_O_RDWR, &sd.lfs_file_config);
		if(err != LFS_ERR_OK) {
			logw("lfs_file_opencfg %s: %d\n\r", f, err);
			return false;
		}
	}

	const uint16_t pos = sizeof(SDMetadata) + (day-1) * sizeof(Wallbox1DayData);
	lfs_ssize_t size   = lfs_file_seek(&sd.lfs, &file, pos, LFS_SEEK_SET);
	if(size != pos) {
		logw("lfs_file_seek %d vs %d\n\r", pos, size);
		err = lfs_file_close(&sd.lfs, &file);
		logw("lfs_file_close %d\n\r", err);
	}

	size = lfs_file_write(&sd.lfs, &file, data1d, sizeof(Wallbox1DayData));
	if(size != sizeof(Wallbox1DayData)) {
		logw("lfs_file_write energy %d, size %d vs %d\n\r", data1d->energy, size, sizeof(Wallbox1DayData));
		err = lfs_file_close(&sd.lfs, &file);
		logw("lfs_file_close %d\n\r", err);
		return false;
	}

	err = lfs_file_close(&sd.lfs, &file);
	if(err != LFS_ERR_OK) {
		logw("lfs_file_close %d\n\r", err);
		return false;
	}

	return true;
}

bool sd_read_wallbox_daily_data_point(uint32_t wallbox_id, uint8_t year, uint8_t month, uint8_t day, uint32_t *data, uint16_t amount, uint16_t offset) {
	int err = LFS_ERR_OK;
	lfs_file_t *file = sd_lfs_open_buffered_read(wallbox_id, year, month, SD_FILE_NO_DAY_IN_PATH, SD_POSTFIX_WB, &err);
	if((err == LFS_ERR_EXIST) || (err == LFS_ERR_NOENT)) {
		for(uint16_t i = 0; i < amount; i++) {
			data[i] = UINT32_MAX;
		}
		return true;
	} else if(err != LFS_ERR_OK) {
		logw("lfs_file_opencfg: %d\n\r", err);
		return false;
	}

	const uint16_t pos = sizeof(SDMetadata) + (day-1) * sizeof(Wallbox1DayData) + offset*sizeof(Wallbox1DayData);
	lfs_ssize_t size = lfs_file_seek(&sd.lfs, file, pos, LFS_SEEK_SET);
	if(size != pos) {
		logw("lfs_file_seek %d vs %d\n\r", pos, size);
		err = sd_lfs_close_buffered_read();
		logw("lfs_file_close %d\n\r", err);
	}

	size = lfs_file_read(&sd.lfs, file, data, amount*sizeof(Wallbox1DayData));
	if(size != amount*sizeof(Wallbox1DayData)) {
		logw("lfs_file_read flags size %d vs %d\n\r", size, amount*sizeof(Wallbox1DayData));
		err = sd_lfs_close_buffered_read();
		logd("lfs_file_close %d\n\r", err);
		return false;
	}

	return true;
}

bool sd_write_energy_manager_data_point_new_file(char *f) {
	lfs_file_t file;

	memset(sd.lfs_file_config.buffer, 0, 512);
	sd.lfs_file_config.attrs = NULL;
	sd.lfs_file_config.attr_count = 0;
	int err = lfs_file_opencfg(&sd.lfs, &file, f, LFS_O_CREAT | LFS_O_RDWR, &sd.lfs_file_config);
	if(err != LFS_ERR_OK) {
		logw("lfs_file_opencfg %d\n\r", err);
		lfs_file_close(&sd.lfs, &file);
		return false;
	}

	lfs_ssize_t size = lfs_file_write(&sd.lfs, &file, &sd_new_file_em_5min, sizeof(EnergyManager5MinDataFile));
	if(size != sizeof(EnergyManager5MinDataFile)) {
		logw("lfs_file_write %d vs %d\n\r", size, sizeof(EnergyManager5MinDataFile));
		err = lfs_file_close(&sd.lfs, &file);
		logw("lfs_file_close %d\n\r", err);
		return false;
	}

#if 0
	// Write metadata and first 36 data points
	lfs_ssize_t size = lfs_file_write(&sd.lfs, &file, &sd_new_file_em_5min, sizeof(buffer));
	if(size != sizeof(buffer)) {
		logw("lfs_file_write %d vs %d\n\r", size, sizeof(buffer));
		err = lfs_file_close(&sd.lfs, &file);
		logw("lfs_file_close %d\n\r", err);
		return false;
	}

	// Write rest in 36 data points chunks
	for(uint8_t i = 1; i < SD_5MIN_PER_DAY/36; i++) {
		lfs_ssize_t size = lfs_file_write(&sd.lfs, &file, df->data, 36*sizeof(EnergyManager5MinData));
		if(size != 36*sizeof(EnergyManager5MinData)) {
			logw("lfs_file_write %d vs %d\n\r", size, 36*sizeof(EnergyManager5MinData));
			err = lfs_file_close(&sd.lfs, &file);
			logw("lfs_file_close %d\n\r", err);
			return false;
		}
	}
#endif

	err = lfs_file_close(&sd.lfs, &file);
	if(err != LFS_ERR_OK) {
		logw("lfs_file_close %d\n\r", err);
		return false;
	}

	return true;
}

bool sd_write_energy_manager_data_point(uint8_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, EnergyManager5MinData *data5m) {
	char *f = sd_get_path_with_filename(0, year, month, day, SD_POSTFIX_EM);

	lfs_file_t file;
	memset(sd.lfs_file_config.buffer, 0, 512);
	sd.lfs_file_config.attrs = NULL;
	sd.lfs_file_config.attr_count = 0;

	int err = lfs_file_opencfg(&sd.lfs, &file, f, LFS_O_RDWR, &sd.lfs_file_config);
	if((err == LFS_ERR_EXIST) || (err == LFS_ERR_NOENT)) {
		err = lfs_file_close(&sd.lfs, &file);

		sd_make_path(year, month, day);
		bool ret = sd_write_energy_manager_data_point_new_file(f);
		if(!ret) {
			logw("file not found and could not create new file %s\n\r", f);
			return false;
		}

		err = lfs_file_opencfg(&sd.lfs, &file, f, LFS_O_RDWR, &sd.lfs_file_config);
		if(err != LFS_ERR_OK) {
			logw("lfs_file_opencfg %s: %d\n\r", f, err);
			return false;
		}
	}

	const uint16_t pos = sizeof(SDMetadata) + (hour*12 + minute/5) * sizeof(EnergyManager5MinData);
	lfs_ssize_t size   = lfs_file_seek(&sd.lfs, &file, pos, LFS_SEEK_SET);
	if(size != pos) {
		logw("lfs_file_seek %d vs %d\n\r", pos, size);
		err = lfs_file_close(&sd.lfs, &file);
		logw("lfs_file_close %d\n\r", err);
		return false;
	}

	size = lfs_file_write(&sd.lfs, &file, data5m, sizeof(EnergyManager5MinData));
	if(size != sizeof(EnergyManager5MinData)) {
		logw("lfs_file_write flags %d, power %d, size %d vs %d\n\r", data5m->flags, data5m->power_grid, size, sizeof(EnergyManager5MinData));
		err = lfs_file_close(&sd.lfs, &file);
		logw("lfs_file_close %d\n\r", err);
		return false;
	}

	err = lfs_file_close(&sd.lfs, &file);
	if(err != LFS_ERR_OK) {
		logw("lfs_file_close %d\n\r", err);
		return false;
	}

	return true;
}

bool sd_read_energy_manager_data_point(uint8_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t *data, uint16_t amount, uint16_t offset) {
	int err = LFS_ERR_OK;
	lfs_file_t *file = sd_lfs_open_buffered_read(0, year, month, day, SD_POSTFIX_EM, &err);
	if((err == LFS_ERR_EXIST) || (err == LFS_ERR_NOENT)) {
		for(uint16_t i = 0; i < amount; i++) {
			data[i*sizeof(EnergyManager5MinData)] = SD_5MIN_FLAG_NO_DATA;
			// 4 byte grid and 6*4 byte general
			for(uint8_t j = 0; j < 4*7; j++) {
				data[i*sizeof(EnergyManager5MinData)+1+j] = 0;
			}
		}
		return true;
	} else if(err != LFS_ERR_OK) {
		logw("lfs_file_opencfg: %d\n\r", err);
		return false;
	}

	const uint16_t pos = 8 + (hour*12 + minute/5) * sizeof(EnergyManager5MinData) + offset*sizeof(EnergyManager5MinData);
	lfs_ssize_t size   = lfs_file_seek(&sd.lfs, file, pos, LFS_SEEK_SET);
	if(size != pos) {
		logw("lfs_file_seek %d vs %d\n\r", pos, size);
		err = sd_lfs_close_buffered_read();
		logw("lfs_file_close %d\n\r", err);
		return false;
	}

	size = lfs_file_read(&sd.lfs, file, data, amount*sizeof(EnergyManager5MinData));
	if(size != amount*sizeof(EnergyManager5MinData)) {
		logw("lfs_file_read flags size %d vs %d\n\r", size, amount*sizeof(EnergyManager5MinData));
		err = sd_lfs_close_buffered_read();
		logw("lfs_file_close %d\n\r", err);
		return false;
	}

	return true;
}

bool sd_write_energy_manager_daily_data_point_new_file(char *f) {
	lfs_file_t file;

	memset(sd.lfs_file_config.buffer, 0, 512);
	sd.lfs_file_config.attrs = NULL;
	sd.lfs_file_config.attr_count = 0;
	int err = lfs_file_opencfg(&sd.lfs, &file, f, LFS_O_CREAT | LFS_O_RDWR, &sd.lfs_file_config);
	if(err != LFS_ERR_OK) {
		logw("lfs_file_opencfg %d\n\r", err);
		lfs_file_close(&sd.lfs, &file);
		return false;
	}
	lfs_ssize_t size = lfs_file_write(&sd.lfs, &file, &sd_new_file_em_1day, sizeof(EnergyManager1DayDataFile));
	if(size != sizeof(EnergyManager1DayDataFile)) {
		logw("lfs_file_write %d\n\r", size);
		err = lfs_file_close(&sd.lfs, &file);
		logw("lfs_file_close %d\n\r", err);
		return false;
	}
	err = lfs_file_close(&sd.lfs, &file);
	if(err != LFS_ERR_OK) {
		logd("lfs_file_close %d\n\r", err);
		return false;
	}

	return true;
}

bool sd_write_energy_manager_daily_data_point(uint8_t year, uint8_t month, uint8_t day, EnergyManager1DayData *data1d) {
	char *f = sd_get_path_with_filename(0, year, month, SD_FILE_NO_DAY_IN_PATH, SD_POSTFIX_EM);

	lfs_file_t file;
	memset(sd.lfs_file_config.buffer, 0, 512);
	sd.lfs_file_config.attrs = NULL;
	sd.lfs_file_config.attr_count = 0;

	int err = lfs_file_opencfg(&sd.lfs, &file, f, LFS_O_RDWR, &sd.lfs_file_config);
	if((err == LFS_ERR_EXIST) || (err == LFS_ERR_NOENT)) {
		err = lfs_file_close(&sd.lfs, &file);
		sd_make_path(year, month, SD_FILE_NO_DAY_IN_PATH);
		bool ret = sd_write_energy_manager_daily_data_point_new_file(f);
		if(!ret) {
			logw("file not found and could not create new file %s\n\r", f);
			return false;
		}
		err = lfs_file_opencfg(&sd.lfs, &file, f, LFS_O_RDWR, &sd.lfs_file_config);
		if(err != LFS_ERR_OK) {
			logw("lfs_file_opencfg %s: %d\n\r", f, err);
			return false;
		}
	}

	const uint16_t pos = sizeof(SDMetadata) + (day-1) * sizeof(EnergyManager1DayData);
	lfs_ssize_t size   = lfs_file_seek(&sd.lfs, &file, pos, LFS_SEEK_SET);
	if(size != pos) {
		logw("lfs_file_seek %d vs %d\n\r", pos, size);
		err = lfs_file_close(&sd.lfs, &file);
		logw("lfs_file_close %d\n\r", err);
	}

	size = lfs_file_write(&sd.lfs, &file, data1d, sizeof(EnergyManager1DayData));
	if(size != sizeof(EnergyManager1DayData)) {
		logw("lfs_file_write energy grid in/out %d %d, size %d vs %d\n\r", data1d->energy_grid_in_, data1d->energy_grid_out, size, sizeof(EnergyManager1DayData));
		err = lfs_file_close(&sd.lfs, &file);
		logw("lfs_file_close %d\n\r", err);
		return false;
	}

	err = lfs_file_close(&sd.lfs, &file);
	if(err != LFS_ERR_OK) {
		logw("lfs_file_close %d\n\r", err);
		return false;
	}

	return true;
}

bool sd_read_energy_manager_daily_data_point(uint8_t year, uint8_t month, uint8_t day, uint8_t *data, uint16_t amount, uint16_t offset) {
	int err = LFS_ERR_OK;
	lfs_file_t *file = sd_lfs_open_buffered_read(0, year, month, SD_FILE_NO_DAY_IN_PATH, SD_POSTFIX_EM, &err);
	if((err == LFS_ERR_EXIST) || (err == LFS_ERR_NOENT)) {
		memset(data, 0xFF, amount*sizeof(EnergyManager1DayData));
		return true;
	} else if(err != LFS_ERR_OK) {
		logw("lfs_file_opencfg: %d\n\r", err);
		return false;
	}

	const uint16_t pos = sizeof(SDMetadata) + (day-1) * sizeof(EnergyManager1DayData) + offset*sizeof(EnergyManager1DayData);
	lfs_ssize_t size = lfs_file_seek(&sd.lfs, file, pos, LFS_SEEK_SET);
	if(size != pos) {
		logw("lfs_file_seek %d vs %d\n\r", pos, size);
		err = sd_lfs_close_buffered_read();
		logw("lfs_file_close %d\n\r", err);
	}

	size = lfs_file_read(&sd.lfs, file, data, amount*sizeof(EnergyManager1DayData));
	if(size != amount*sizeof(EnergyManager1DayData)) {
		logw("lfs_file_read flags size %d vs %d\n\r", size, amount*sizeof(EnergyManager1DayData));
		err = sd_lfs_close_buffered_read();
		logd("lfs_file_close %d\n\r", err);
		return false;
	}

	return true;
}

bool sd_write_storage(uint8_t page) {
	char f[SD_PATH_LENGTH] = "storage/X.sp";
	f[8] = '0' + page;

	lfs_file_t file;
	memset(sd.lfs_file_config.buffer, 0, 512);
	sd.lfs_file_config.attrs = NULL;
	sd.lfs_file_config.attr_count = 0;

	int err = lfs_file_opencfg(&sd.lfs, &file, f, LFS_O_RDWR, &sd.lfs_file_config);
	if((err == LFS_ERR_EXIST) || (err == LFS_ERR_NOENT)) {
		err = lfs_file_close(&sd.lfs, &file);
		lfs_mkdir(&sd.lfs, "storage");
		err = lfs_file_opencfg(&sd.lfs, &file, f, LFS_O_CREAT | LFS_O_RDWR, &sd.lfs_file_config);
		if(err != LFS_ERR_OK) {
			logw("lfs_file_opencfg %s: %d\n\r", f, err);
			return false;
		}
	}

	lfs_ssize_t size = lfs_file_write(&sd.lfs, &file, data_storage.storage[page], DATA_STORAGE_SIZE);
	if(size != DATA_STORAGE_SIZE) {
		logw("lfs_file_write size %d vs %d\n\r", size, DATA_STORAGE_SIZE);
		err = lfs_file_close(&sd.lfs, &file);
		logw("lfs_file_close %d\n\r", err);
		return false;
	}

	err = lfs_file_close(&sd.lfs, &file);
	if(err != LFS_ERR_OK) {
		logw("lfs_file_close %d\n\r", err);
		return false;
	}

	return true;
}

bool sd_read_storage(uint8_t page) {
	char f[SD_PATH_LENGTH] = "storage/X.sp";
	f[8] = '0' + page;

	lfs_file_t file;
	memset(sd.lfs_file_config.buffer, 0, 512);
	sd.lfs_file_config.attrs = NULL;
	sd.lfs_file_config.attr_count = 0;

	int err = lfs_file_opencfg(&sd.lfs, &file, f, LFS_O_RDONLY, &sd.lfs_file_config);
	if((err == LFS_ERR_EXIST) || (err == LFS_ERR_NOENT)) {
		return true;
	}

	lfs_ssize_t size = lfs_file_read(&sd.lfs, &file, data_storage.storage[page], DATA_STORAGE_SIZE);
	if(size != DATA_STORAGE_SIZE) {
		logw("lfs_file_read size %d vs %d\n\r", size, DATA_STORAGE_SIZE);
		err = lfs_file_close(&sd.lfs, &file);
		logd("lfs_file_close %d\n\r", err);
		return false;
	}
	err = lfs_file_close(&sd.lfs, &file);
	if(err != LFS_ERR_OK) {
		logw("lfs_file_close %d\n\r", err);
		return false;
	}

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
	if(sdmmc_error != SDMMC_ERROR_OK) {
		sd.sd_status = sdmmc_error;
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

	int err = 0;
	if(sd_lfs_format) {
		logd("Starting lfs format...\n\r");
		coop_task_yield();
		err = lfs_format(&sd.lfs, &sd.lfs_config);
		sd_lfs_format = false;
		logd("... done\n\r", err);
		if(err != LFS_ERR_OK) {
			logw("lfs_format %d\n\r", err);
			sd.lfs_status = ABS(err);
			sd.sd_status = sdmmc_error;
			return;
		}
	}

	coop_task_yield();
	err = lfs_mount(&sd.lfs, &sd.lfs_config);
	sd.lfs_status = ABS(err);
	coop_task_yield();

	if(err != LFS_ERR_OK) {
		logw("lfs_mount %d\n\r", err);
		err = lfs_format(&sd.lfs, &sd.lfs_config);
		if(err != LFS_ERR_OK) {
			logw("lfs_format %d\n\r", err);
			sd.lfs_status = ABS(err);
			sd.sd_status = sdmmc_error;
			return;
		}
		err = lfs_mount(&sd.lfs, &sd.lfs_config);
		if(err != LFS_ERR_OK) {
			logw("lfs_mount 2nd try %d\n\r", err);
			sd.lfs_status = ABS(err);
			sd.sd_status = sdmmc_error;
			return;
		}
	}

	// Open/create boot_count file, increment boot count and write it back
	// This is a good initial sd card sanity check
	lfs_file_t file;

	// read boot count
	uint32_t boot_count = 0;
	err = lfs_file_opencfg(&sd.lfs, &file, "boot_count", LFS_O_RDWR | LFS_O_CREAT, &sd.lfs_file_config);
	if(err != LFS_ERR_OK) {
		logd("boot_count lfs_file_opencfg %d\n\r", err);
	}

	lfs_ssize_t size = lfs_file_read(&sd.lfs, &file, &boot_count, sizeof(boot_count));
	if(size != sizeof(boot_count)) {
		logd("boot_count lfs_file_read size %d vs %d\n\r", size, sizeof(boot_count));
	}

 	// update boot count
	boot_count += 1;
	err = lfs_file_rewind(&sd.lfs, &file);
	if(err != LFS_ERR_OK) {
		logd("boot_count lfs_file_rewind %d\n\r", err);
	}

	size = lfs_file_write(&sd.lfs, &file, &boot_count, sizeof(boot_count));
	if(size != sizeof(boot_count)) {
		logd("boot_count lfs_file_write size %s vs %d\n\r", size, sizeof(boot_count));
	}

	err = lfs_file_close(&sd.lfs, &file);
	if(err != LFS_ERR_OK) {
		logd("boot_count lfs_file_close %d\n\r", err);
	}

	logd("boot_count: %d\n\r", boot_count);

	// Set sd status at the end, to make sure that everything is completely initialized
	// before any other code tries to access the sd card
	sd.sd_status = sdmmc_error;

	for(uint8_t i = 0; i < DATA_STORAGE_PAGES; i++) {
		data_storage.read_from_sd[i] = true;
	}
}

void sd_tick_task_handle_wallbox_data(void) {
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
			logw("sd_write_wallbox_data_point failed wb %d, date %d %d %d %d %d\n\r", wdp.wallbox_id, wdp.year, wdp.month, wdp.day, wdp.hour, wdp.minute);

			// Put data point back into buffer and increase error counter
			if(sd.wallbox_data_point_end < SD_WALLBOX_DATA_POINT_LENGTH) {
				sd.wallbox_data_point[sd.wallbox_data_point_end] = wdp;
				sd.wallbox_data_point_end++;
			}

			sd.sd_rw_error_count++;
		} else {
			sd.sd_rw_error_count = 0;
		}
	}

	// handle getter
	if(sd.new_sd_wallbox_data_points) {
		for(uint16_t offset = 0; offset < sd.get_sd_wallbox_data_points.amount*sizeof(Wallbox5MinData); offset += 60) {
			uint16_t amount = MIN(20, sd.get_sd_wallbox_data_points.amount - offset/sizeof(Wallbox5MinData));
			if(!sd_read_wallbox_data_point(sd.get_sd_wallbox_data_points.wallbox_id, sd.get_sd_wallbox_data_points.year, sd.get_sd_wallbox_data_points.month, sd.get_sd_wallbox_data_points.day, sd.get_sd_wallbox_data_points.hour, sd.get_sd_wallbox_data_points.minute, sd.sd_wallbox_data_points_cb_data, amount, offset/sizeof(Wallbox5MinData))) {
				logw("sd_read_wallbox_data_point failed wb %d, date %d %d %d %d %d, amount %d, offset %d\n\r", sd.get_sd_wallbox_data_points.wallbox_id, sd.get_sd_wallbox_data_points.year, sd.get_sd_wallbox_data_points.month, sd.get_sd_wallbox_data_points.day, sd.get_sd_wallbox_data_points.hour, sd.get_sd_wallbox_data_points.minute, amount, offset);

				// Increase error counter and return (try again in next tick)
				sd.sd_rw_error_count++;
				return;
			} else {
				sd.sd_rw_error_count = 0;
			}
			sd.sd_wallbox_data_points_cb_offset = offset;
			sd.sd_wallbox_data_points_cb_data_length = sd.get_sd_wallbox_data_points.amount*sizeof(Wallbox5MinData);
			sd.new_sd_wallbox_data_points_cb = true;

			uint32_t start = system_timer_get_ms();
			while(sd.new_sd_wallbox_data_points_cb) {
				coop_task_yield();
				if(system_timer_is_time_elapsed_ms(start, SD_CALLBACK_TIMEOUT)) { // try for 1 second at most
					logw("sd_read_wallbox_data_point timeout wb %d, date %d %d %d %d %d, amount %d, offset %d\n\r", sd.get_sd_wallbox_data_points.wallbox_id, sd.get_sd_wallbox_data_points.year, sd.get_sd_wallbox_data_points.month, sd.get_sd_wallbox_data_points.day, sd.get_sd_wallbox_data_points.hour, sd.get_sd_wallbox_data_points.minute, amount, offset);

					// Increase error counter and return (try again in next tick)
					sd.sd_rw_error_count++;
					return;
				} else {
					sd.sd_rw_error_count = 0;
				}
			}
		}

		// Close buffered read after getter stream is finished
		sd_lfs_close_buffered_read();

		sd.new_sd_wallbox_data_points = false;
	}
}

void sd_tick_task_handle_wallbox_daily_data(void) {
	// handle setter
	if(sd.wallbox_daily_data_point_end > 0) {
		// Make copy and decrease the end pointer
		// The sd write function might yield, so the data needs to be copied beforehand (because it might be overwritten)
		sd.wallbox_daily_data_point_end--;
		WallboxDailyDataPoint wddp = sd.wallbox_daily_data_point[sd.wallbox_daily_data_point_end];

		Wallbox1DayData wb_1day_data = {
			.energy = wddp.energy
		};

		bool ret = sd_write_wallbox_daily_data_point(wddp.wallbox_id, wddp.year, wddp.month, wddp.day, &wb_1day_data);
		if(!ret) {
			logw("sd_write_wallbox_daily_data_point failed wb %d, date %d %d %d\n\r", wddp.wallbox_id, wddp.year, wddp.month, wddp.day);

			// Put data point back into buffer and increase error counter
			if(sd.wallbox_daily_data_point_end < SD_WALLBOX_DAILY_DATA_POINT_LENGTH) {
				sd.wallbox_daily_data_point[sd.wallbox_daily_data_point_end] = wddp;
				sd.wallbox_daily_data_point_end++;
			}

			sd.sd_rw_error_count++;
		} else {
			sd.sd_rw_error_count = 0;
		}
	}

	// handle getter
	if(sd.new_sd_wallbox_daily_data_points) {
		for(uint16_t offset = 0; offset < sd.get_sd_wallbox_daily_data_points.amount*sizeof(Wallbox1DayData); offset += 60) {
			uint16_t amount = MIN(15, sd.get_sd_wallbox_daily_data_points.amount - offset/sizeof(Wallbox1DayData));
			if(!sd_read_wallbox_daily_data_point(sd.get_sd_wallbox_daily_data_points.wallbox_id, sd.get_sd_wallbox_daily_data_points.year, sd.get_sd_wallbox_daily_data_points.month, sd.get_sd_wallbox_daily_data_points.day, sd.sd_wallbox_daily_data_points_cb_data, amount, offset/sizeof(Wallbox1DayData))) {
				logw("sd_read_wallbox_daily_data_point failed wb %d, date %d %d %d, amount %d, offset %d\n\r", sd.get_sd_wallbox_data_points.wallbox_id, sd.get_sd_wallbox_data_points.year, sd.get_sd_wallbox_data_points.month, sd.get_sd_wallbox_data_points.day, amount, offset/sizeof(uint32_t));

				// Increase error counter and return (try again in next tick)
				sd.sd_rw_error_count++;
				return;
			} else {
				sd.sd_rw_error_count = 0;
			}
			sd.sd_wallbox_daily_data_points_cb_offset = offset/sizeof(uint32_t);
			sd.sd_wallbox_daily_data_points_cb_data_length = sd.get_sd_wallbox_daily_data_points.amount;
			sd.new_sd_wallbox_daily_data_points_cb = true;

			uint32_t start = system_timer_get_ms();
			while(sd.new_sd_wallbox_daily_data_points_cb) {
				coop_task_yield();
				if(system_timer_is_time_elapsed_ms(start, SD_CALLBACK_TIMEOUT)) { // try for 1 second at most
					logw("sd_read_wallbox_data_point timeout wb %d, date %d %d %d, amount %d, offset %d\n\r", sd.get_sd_wallbox_data_points.wallbox_id, sd.get_sd_wallbox_data_points.year, sd.get_sd_wallbox_data_points.month, sd.get_sd_wallbox_data_points.day, amount, offset);

					// Increase error counter and return (try again in next tick)
					sd.sd_rw_error_count++;
					return;
				} else {
					sd.sd_rw_error_count = 0;
				}
			}
		}

		// Close buffered read after getter stream is finished
		sd_lfs_close_buffered_read();

		sd.new_sd_wallbox_daily_data_points = false;
	}
}

void sd_tick_task_handle_energy_manager_data(void) {
	// handle setter
	if(sd.energy_manager_data_point_end > 0) {
		// Make copy and decrease the end pointer
		// The sd write function might yield, so the data needs to be copied beforehand (because it might be overwritten)
		sd.energy_manager_data_point_end--;
		EnergyManagerDataPoint emdp = sd.energy_manager_data_point[sd.energy_manager_data_point_end];

		EnergyManager5MinData em_5min_data;
		memcpy(&em_5min_data, &emdp.flags, sizeof(EnergyManager5MinData));

		bool ret = sd_write_energy_manager_data_point(emdp.year, emdp.month, emdp.day, emdp.hour, emdp.minute, &em_5min_data);
		if(!ret) {
			logw("sd_write_energy_manager_data_point failed date %d %d %d %d %d\n\r", emdp.year, emdp.month, emdp.day, emdp.hour, emdp.minute);

			// Put data point back into buffer and increase error counter
			if(sd.energy_manager_data_point_end < SD_ENERGY_MANAGER_DATA_POINT_LENGTH) {
				sd.energy_manager_data_point[sd.energy_manager_data_point_end] = emdp;
				sd.energy_manager_data_point_end++;
			}

			sd.sd_rw_error_count++;
		} else {
			sd.sd_rw_error_count = 0;
		}
	}

	// handle getter
	if(sd.new_sd_energy_manager_data_points) {
		for(uint16_t offset = 0; offset < sd.get_sd_energy_manager_data_points.amount*sizeof(EnergyManager5MinData); offset += 58) {
			uint16_t amount = MIN(2, sd.get_sd_energy_manager_data_points.amount - offset/sizeof(EnergyManager5MinData));
			if(!sd_read_energy_manager_data_point(sd.get_sd_energy_manager_data_points.year, sd.get_sd_energy_manager_data_points.month, sd.get_sd_energy_manager_data_points.day, sd.get_sd_energy_manager_data_points.hour, sd.get_sd_energy_manager_data_points.minute, sd.sd_energy_manager_data_points_cb_data, amount, offset/sizeof(EnergyManager5MinData))) {
				logw("sd_read_energy_manager_data_point failed date %d %d %d %d %d, amount %d, offset %d\n\r", sd.get_sd_energy_manager_data_points.year, sd.get_sd_energy_manager_data_points.month, sd.get_sd_energy_manager_data_points.day, sd.get_sd_energy_manager_data_points.hour, sd.get_sd_energy_manager_data_points.minute, amount, offset);

				// Increase error counter and return (try again in next tick)
				sd.sd_rw_error_count++;
				return;
			} else {
				sd.sd_rw_error_count = 0;
			}
			sd.sd_energy_manager_data_points_cb_offset = offset;
			sd.sd_energy_manager_data_points_cb_data_length = sd.get_sd_energy_manager_data_points.amount*sizeof(EnergyManager5MinData);
			sd.new_sd_energy_manager_data_points_cb = true;

			uint32_t start = system_timer_get_ms();
			while(sd.new_sd_energy_manager_data_points_cb) {
				coop_task_yield();
				if(system_timer_is_time_elapsed_ms(start, SD_CALLBACK_TIMEOUT)) { // try for 1 second at most
					logw("sd_read_energy_manager_data_point timeout date %d %d %d %d %d, amount %d, offset %d\n\r", sd.get_sd_energy_manager_data_points.year, sd.get_sd_energy_manager_data_points.month, sd.get_sd_energy_manager_data_points.day, sd.get_sd_energy_manager_data_points.hour, sd.get_sd_energy_manager_data_points.minute, amount, offset);

					// Increase error counter and return (try again in next tick)
					sd.sd_rw_error_count++;
					return;
				} else {
					sd.sd_rw_error_count = 0;
				}
			}
		}

		// Close buffered read after getter stream is finished
		sd_lfs_close_buffered_read();

		sd.new_sd_energy_manager_data_points = false;
	}
}

void sd_tick_task_handle_energy_manager_daily_data(void) {
	// handle setter
	if(sd.energy_manager_daily_data_point_end > 0) {
		// Make copy and decrease the end pointer
		// The sd write function might yield, so the data needs to be copied beforehand (because it might be overwritten)
		sd.energy_manager_daily_data_point_end--;
		EnergyManagerDailyDataPoint emddp = sd.energy_manager_daily_data_point[sd.energy_manager_daily_data_point_end];

		EnergyManager1DayData wb_1day_data;
		memcpy(&wb_1day_data, &emddp.energy_grid_in, sizeof(EnergyManager1DayData));

		bool ret = sd_write_energy_manager_daily_data_point(emddp.year, emddp.month, emddp.day, &wb_1day_data);
		if(!ret) {
			logw("sd_write_energy_manager_daily_data_point failed date %d %d %d\n\r", emddp.year, emddp.month, emddp.day);

			// Put data point back into buffer and increase error counter
			if(sd.energy_manager_daily_data_point_end < SD_ENERGY_MANAGER_DAILY_DATA_POINT_LENGTH) {
				sd.energy_manager_daily_data_point[sd.energy_manager_daily_data_point_end] = emddp;
				sd.energy_manager_daily_data_point_end++;
			}

			sd.sd_rw_error_count++;
		} else {
			sd.sd_rw_error_count = 0;
		}
	}

	// handle getter
	if(sd.new_sd_energy_manager_daily_data_points) {
		for(uint16_t offset = 0; offset < sd.get_sd_energy_manager_daily_data_points.amount*sizeof(EnergyManager1DayData); offset += 56) {
			uint16_t amount = MIN(1, sd.get_sd_energy_manager_daily_data_points.amount - offset/sizeof(EnergyManager1DayData));
			if(!sd_read_energy_manager_daily_data_point(sd.get_sd_energy_manager_daily_data_points.year, sd.get_sd_energy_manager_daily_data_points.month, sd.get_sd_energy_manager_daily_data_points.day, sd.sd_energy_manager_daily_data_points_cb_data, amount, offset/sizeof(EnergyManager1DayData))) {
				logw("sd_read_energy_manager_daily_data_point failed date %d %d %d, amount %d, offset %d\n\r", sd.get_sd_energy_manager_data_points.year, sd.get_sd_energy_manager_data_points.month, sd.get_sd_energy_manager_data_points.day, amount, offset/sizeof(uint32_t));

				// Increase error counter and return (try again in next tick)
				sd.sd_rw_error_count++;
				return;
			} else {
				sd.sd_rw_error_count = 0;
			}
			sd.sd_energy_manager_daily_data_points_cb_offset = offset/sizeof(uint32_t);
			sd.sd_energy_manager_daily_data_points_cb_data_length = sd.get_sd_energy_manager_daily_data_points.amount*(sizeof(EnergyManager1DayData)/sizeof(uint32_t));
			sd.new_sd_energy_manager_daily_data_points_cb = true;

			uint32_t start = system_timer_get_ms();
			while(sd.new_sd_energy_manager_daily_data_points_cb) {
				coop_task_yield();
				if(system_timer_is_time_elapsed_ms(start, SD_CALLBACK_TIMEOUT)) { // try for 1 second at most
					logw("sd_read_energy_manager_data_point timeout date %d %d %d, amount %d, offset %d\n\r", sd.get_sd_energy_manager_data_points.year, sd.get_sd_energy_manager_data_points.month, sd.get_sd_energy_manager_data_points.day, amount, offset);

					// Increase error counter and return (try again in next tick)
					sd.sd_rw_error_count++;
					return;
				} else {
					sd.sd_rw_error_count = 0;
				}
			}
		}

		// Close buffered read after getter stream is finished
		sd_lfs_close_buffered_read();

		sd.new_sd_energy_manager_daily_data_points = false;
	}
}

void sd_tick_task_handle_storage(void) {
	for(uint8_t i = 0; i < DATA_STORAGE_PAGES; i++) {
		if(data_storage.read_from_sd[i]) {
			sd_read_storage(i);
			data_storage.read_from_sd[i] = false;
			data_storage.write_to_sd[i]  = false; // We don't need to write to sd anymore, because we just read it into RAM
		}
		if(data_storage.write_to_sd[i]) {
			sd_write_storage(i);
			data_storage.write_to_sd[i] = false;
		}
	}
}

void sd_tick_task(void) {
	// Pre-initialize sd and lfs status.
	// If no sd card is inserted, the sd_init code is never called and the status would in this case never be set.
	sd.sd_status  = 0xFFFFFFFF;
	sd.lfs_status = 0xFFFFFFFF;

	sd.sdmmc_init_last = system_timer_get_ms();
	const XMC_GPIO_CONFIG_t input_pin_config = {
		.mode             = XMC_GPIO_MODE_INPUT_TRISTATE,
		.input_hysteresis = XMC_GPIO_INPUT_HYSTERESIS_STANDARD
	};

	XMC_GPIO_Init(SD_DETECTED_PIN, &input_pin_config);

	while(true) {
		// If the sd and lfs status are OK and no sd is detected,
		// we assume that the sd card was hot-removed

		const bool sd_detected = !XMC_GPIO_GetInput(SD_DETECTED_PIN);
		if((sd.sd_status == SDMMC_ERROR_OK) && (sd.lfs_status == LFS_ERR_OK) && !sd_detected) {
			sd.sd_rw_error_count = 1000;
			logd("SD card hot-removed? Force error_count=1000 to re-initialize\n\r");
		}

		if(sd_lfs_format) {
			sd.sd_rw_error_count = 1000;
			logd("SD format requested. Force error_count=1000 to re-initialize\n\r");
		}

		if(!sd_detected) {
			sd.sd_status = SDMMC_ERROR_NO_CARD;
		}

		if(sd.sd_rw_error_count > 10) {
			logw("sd.sd_rw_error_count: %d, sd detected: %d, sd format request: %d\n\r", sd.sd_rw_error_count, sd_detected, sd_lfs_format);
			int err = lfs_unmount(&sd.lfs);
			if(err != LFS_ERR_OK) {
				logw("lfs_unmount failed: %d\n\r", err);
			}

			sdmmc_spi_deinit();

			sd.sd_rw_error_count = 0;
			sd.sdmmc_init_last   = system_timer_get_ms() - 1001;
			sd.sd_status         = SDMMC_ERROR_COUNT_TO_HIGH;
			sd.lfs_status        = SDMMC_ERROR_COUNT_TO_HIGH;
		}


		// We retry to initialize SD card once per second
		if((sd.sdmmc_init_last != 0) && system_timer_is_time_elapsed_ms(sd.sdmmc_init_last, 1000)) {
			if(sd_detected) {
				sd_init_task();
			} else {
				logd("No SD card detected\n\r");
				sd.sdmmc_init_last = system_timer_get_ms();
			}
		}

		if((sd.sd_status == SDMMC_ERROR_OK) && (sd.lfs_status == LFS_ERR_OK)) {
			sd_tick_task_handle_wallbox_data();
			sd_tick_task_handle_wallbox_daily_data();
			sd_tick_task_handle_energy_manager_data();
			sd_tick_task_handle_energy_manager_daily_data();
			sd_tick_task_handle_storage();
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
	return LFS_ERR_OK;
}

int sd_lfs_read(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, void *buffer, lfs_size_t size) {
	// Yield once per block read
	coop_task_yield();

	SDMMCError sdmmc_error = sdmmc_read_block(block, buffer);
	if(sdmmc_error != SDMMC_ERROR_OK) {
		logw("sdmmc_read_block error %d, block %d, off %d, size %d\n\r", sdmmc_error, block, off, size);
		return LFS_ERR_IO;
	}
	return LFS_ERR_OK;
}

int sd_lfs_prog(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, const void *buffer, lfs_size_t size) {
	// Yield once per block write
	coop_task_yield();

	SDMMCError sdmmc_error = sdmmc_write_block(block, buffer);
	if(sdmmc_error != SDMMC_ERROR_OK) {
		logw("sdmmc_write_block error %d, block %d, off %d, size %d\n\r", sdmmc_error, block, off, size);
		return LFS_ERR_IO;
	}
	return LFS_ERR_OK;
}

int sd_lfs_sync(const struct lfs_config *c) {
	return LFS_ERR_OK;
}