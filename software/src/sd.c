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
	if(day != SD_FILE_NO_DAY_IN_PATH) { // Use 0xFF to not add day to path
		np = sd_itoa(day, np);
		*np++ = '/';
	}
	np = sd_itoa(wallbox_id, np);
	strncat(np, postfix, 3);

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
	Wallbox5MinDataFile df;
	memset(&df, 0, sizeof(Wallbox5MinDataFile));
	df.metadata.magic   = SD_METADATA_MAGIC;
	df.metadata.version = SD_METADATA_VERSION;
	df.metadata.type    = SD_METADATA_TYPE_WB_5MIN;
	for(uint16_t i = 0; i < SD_5MIN_PER_DAY; i++) {
		df.data[i].flags = SD_5MIN_FLAG_NO_DATA;
	}

	lfs_file_t file;
	int err = lfs_file_opencfg(&sd.lfs, &file, f, LFS_O_CREAT | LFS_O_RDWR, &sd.lfs_file_config);
	if(err != LFS_ERR_OK) {
		logw("lfs_file_opencfg %d\n\r", err);
		lfs_file_close(&sd.lfs, &file);
		return false;
	}

	lfs_ssize_t size = lfs_file_write(&sd.lfs, &file, &df, sizeof(Wallbox5MinDataFile));
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

bool sd_write_wallbox_data_point(uint8_t wallbox_id, uint8_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, Wallbox5MinData *data5m) {
	char *f = sd_get_path_with_filename(wallbox_id, year, month, day, ".wb");

	lfs_file_t file;
	memset(sd.lfs_file_config.buffer, 0, 512);
	sd.lfs_file_config.attrs = NULL;
	sd.lfs_file_config.attr_count = 0;

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
		logw("lfs_file_close %d\n\r", err);
		return false;
	}

	return true;
}

bool sd_read_wallbox_data_point(uint8_t wallbox_id, uint8_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t *data, uint16_t amount, uint16_t offset) {
	char *f = sd_get_path_with_filename(wallbox_id, year, month, day, ".wb");

	lfs_file_t file;
	memset(sd.lfs_file_config.buffer, 0, 512);
	sd.lfs_file_config.attrs = NULL;
	sd.lfs_file_config.attr_count = 0;

	int err = lfs_file_opencfg(&sd.lfs, &file, f, LFS_O_RDWR, &sd.lfs_file_config);
	if((err == LFS_ERR_EXIST) || (err == LFS_ERR_NOENT)) {
		for(uint16_t i = 0; i < amount; i++) {
			data[i*sizeof(Wallbox5MinData)]   = SD_5MIN_FLAG_NO_DATA;
			data[i*sizeof(Wallbox5MinData)+1] = 0;
			data[i*sizeof(Wallbox5MinData)+2] = 0;
		}
		return true;
	} else if(err != LFS_ERR_OK) {	
		logw("lfs_file_opencfg %s: %d\n\r", f, err);
		return false;
	}

	const uint16_t pos = 8 + (hour*12 + minute/5) * sizeof(Wallbox5MinData) + offset*sizeof(Wallbox5MinData);
	lfs_ssize_t size   = lfs_file_seek(&sd.lfs, &file, pos, LFS_SEEK_SET);
	if(size != pos) {
		logw("lfs_file_seek %d vs %d\n\r", pos, size);
		err = lfs_file_close(&sd.lfs, &file);
		logw("lfs_file_close %d\n\r", err);
		return false;
	}

	size = lfs_file_read(&sd.lfs, &file, data, amount*sizeof(Wallbox5MinData));
	if(size != amount*sizeof(Wallbox5MinData)) {
		logw("lfs_file_read flags size %d vs %d\n\r", size, amount*sizeof(Wallbox5MinData));
		err = lfs_file_close(&sd.lfs, &file);
		logw("lfs_file_close %d\n\r", err);
		return false;
	}

	err = lfs_file_close(&sd.lfs, &file);
	if(err != LFS_ERR_OK) {
		logw("lfs_file_close %d\n\r", err);
	}
	return true;
}

bool sd_write_wallbox_daily_data_point_new_file(char *f) {
	Wallbox1DayDataFile df;
	memset(&df, 0, sizeof(Wallbox1DayDataFile));
	df.metadata.magic   = SD_METADATA_MAGIC;
	df.metadata.version = SD_METADATA_VERSION;
	df.metadata.type    = SD_METADATA_TYPE_WB_5MIN;
	for(uint16_t i = 0; i < SD_1DAY_PER_MONTH; i++) {
		df.data[i].energy = UINT32_MAX;
	}

	lfs_file_t file;
	int err = lfs_file_opencfg(&sd.lfs, &file, f, LFS_O_CREAT | LFS_O_RDWR, &sd.lfs_file_config);
	if(err != LFS_ERR_OK) {
		logw("lfs_file_opencfg %d\n\r", err);
		lfs_file_close(&sd.lfs, &file);
		return false;
	}
	lfs_ssize_t size = lfs_file_write(&sd.lfs, &file, &df, sizeof(Wallbox1DayDataFile));
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

bool sd_write_wallbox_daily_data_point(uint8_t wallbox_id, uint8_t year, uint8_t month, uint8_t day, Wallbox1DayData *data1d) {
	char *f = sd_get_path_with_filename(wallbox_id, year, month, SD_FILE_NO_DAY_IN_PATH, ".wb");

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
			logw("lfs_file_opencfg %s: %d\nr\r", f, err);
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

bool sd_read_wallbox_daily_data_point(uint8_t wallbox_id, uint8_t year, uint8_t month, uint8_t day, uint32_t *data, uint16_t amount, uint16_t offset) {
	char *f = sd_get_path_with_filename(wallbox_id, year, month, SD_FILE_NO_DAY_IN_PATH, ".wb");

	lfs_file_t file;
	memset(sd.lfs_file_config.buffer, 0, 512);
	sd.lfs_file_config.attrs = NULL;
	sd.lfs_file_config.attr_count = 0;

	int err = lfs_file_opencfg(&sd.lfs, &file, f, LFS_O_RDWR, &sd.lfs_file_config);
	if((err == LFS_ERR_EXIST) || (err == LFS_ERR_NOENT)) {
		for(uint16_t i = 0; i < amount; i++) {
			data[i] = UINT32_MAX;
		}
		return true;
	} else if(err != LFS_ERR_OK) {	
		logw("lfs_file_opencfg %s: %d\n\r", f, err);
		return false;
	}

	const uint16_t pos = sizeof(SDMetadata) + (day-1) * sizeof(Wallbox1DayData) + offset*sizeof(Wallbox1DayData);
	lfs_ssize_t size = lfs_file_seek(&sd.lfs, &file, pos, LFS_SEEK_SET);
	if(size != pos) {
		logw("lfs_file_seek %d vs %d\n\r", pos, size);
		err = lfs_file_close(&sd.lfs, &file);
		logw("lfs_file_close %d\n\r", err);
	}

	size = lfs_file_read(&sd.lfs, &file, data, amount*sizeof(Wallbox1DayData));
	if(size != amount*sizeof(Wallbox1DayData)) {
		logw("lfs_file_read flags size %d vs %d\n\r", size, amount*sizeof(Wallbox1DayData));
		err = lfs_file_close(&sd.lfs, &file);
		logd("lfs_file_close %d\n\r", err);
		return false;
	}

	err = lfs_file_close(&sd.lfs, &file);
	if(err != LFS_ERR_OK) {
		logw("lfs_file_close %d\n\r", err);
	}

	return true;
}

bool sd_write_energy_manager_data_point_new_file(char *f) {
	// Full file to large, we write it in chunks
	uint8_t buffer[36*sizeof(EnergyManager5MinData) + sizeof(SDMetadata)] = {0};
	EnergyManager5MinDataFile *df = (EnergyManager5MinDataFile *)buffer;
	df->metadata.magic   = SD_METADATA_MAGIC;
	df->metadata.version = SD_METADATA_VERSION;
	df->metadata.type    = SD_METADATA_TYPE_WB_5MIN;
	for(uint8_t i = 0; i < 36; i++) {
		df->data[i].flags = SD_5MIN_FLAG_NO_DATA;
	}

	lfs_file_t file;
	int err = lfs_file_opencfg(&sd.lfs, &file, f, LFS_O_CREAT | LFS_O_RDWR, &sd.lfs_file_config);
	if(err != LFS_ERR_OK) {
		logw("lfs_file_opencfg %d\n\r", err);
		lfs_file_close(&sd.lfs, &file);
		return false;
	}

	// Write metadata and first 36 data points
	lfs_ssize_t size = lfs_file_write(&sd.lfs, &file, df, sizeof(buffer));
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

	err = lfs_file_close(&sd.lfs, &file);
	if(err != LFS_ERR_OK) {
		logw("lfs_file_close %d\n\r", err);
		return false;
	}

	return true;
}

bool sd_write_energy_manager_data_point(uint8_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, EnergyManager5MinData *data5m) {
	char *f = sd_get_path_with_filename(0, year, month, day, ".em");

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
	char *f = sd_get_path_with_filename(0, year, month, day, ".em");

	lfs_file_t file;
	memset(sd.lfs_file_config.buffer, 0, 512);
	sd.lfs_file_config.attrs = NULL;
	sd.lfs_file_config.attr_count = 0;

	int err = lfs_file_opencfg(&sd.lfs, &file, f, LFS_O_RDWR, &sd.lfs_file_config);
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
		logw("lfs_file_opencfg %s: %d\n\r", f, err);
		return false;
	}

	const uint16_t pos = 8 + (hour*12 + minute/5) * sizeof(EnergyManager5MinData) + offset*sizeof(EnergyManager5MinData);
	lfs_ssize_t size   = lfs_file_seek(&sd.lfs, &file, pos, LFS_SEEK_SET);
	if(size != pos) {
		logw("lfs_file_seek %d vs %d\n\r", pos, size);
		err = lfs_file_close(&sd.lfs, &file);
		logw("lfs_file_close %d\n\r", err);
		return false;
	}

	size = lfs_file_read(&sd.lfs, &file, data, amount*sizeof(EnergyManager5MinData));
	if(size != amount*sizeof(EnergyManager5MinData)) {
		logw("lfs_file_read flags size %d vs %d\n\r", size, amount*sizeof(EnergyManager5MinData));
		err = lfs_file_close(&sd.lfs, &file);
		logw("lfs_file_close %d\n\r", err);
		return false;
	}

	err = lfs_file_close(&sd.lfs, &file);
	if(err != LFS_ERR_OK) {
		logw("lfs_file_close %d\n\r", err);
	}
	return true;
}

bool sd_write_energy_manager_daily_data_point_new_file(char *f) {
	EnergyManager1DayDataFile df;
	memset(&df, 0, sizeof(EnergyManager1DayDataFile));
	df.metadata.magic   = SD_METADATA_MAGIC;
	df.metadata.version = SD_METADATA_VERSION;
	df.metadata.type    = SD_METADATA_TYPE_EM_5MIN;
	for(uint8_t i = 0; i < SD_1DAY_PER_MONTH; i++) {
		df.data[i].energy_grid_in_ = UINT32_MAX;
		df.data[i].energy_grid_out = UINT32_MAX;
		for(uint8_t j = 0; j < 6; j++) {
			df.data[i].energy_general_in[j]  = UINT32_MAX;
			df.data[i].energy_general_out[j] = UINT32_MAX;
		}
	}

	lfs_file_t file;
	int err = lfs_file_opencfg(&sd.lfs, &file, f, LFS_O_CREAT | LFS_O_RDWR, &sd.lfs_file_config);
	if(err != LFS_ERR_OK) {
		logw("lfs_file_opencfg %d\n\r", err);
		lfs_file_close(&sd.lfs, &file);
		return false;
	}
	lfs_ssize_t size = lfs_file_write(&sd.lfs, &file, &df, sizeof(EnergyManager1DayDataFile));
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
	char *f = sd_get_path_with_filename(0, year, month, SD_FILE_NO_DAY_IN_PATH, ".em");

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
			logw("lfs_file_opencfg %s: %d\nr\r", f, err);
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
	char *f = sd_get_path_with_filename(0, year, month, SD_FILE_NO_DAY_IN_PATH, ".em");

	lfs_file_t file;
	memset(sd.lfs_file_config.buffer, 0, 512);
	sd.lfs_file_config.attrs = NULL;
	sd.lfs_file_config.attr_count = 0;

	int err = lfs_file_opencfg(&sd.lfs, &file, f, LFS_O_RDWR, &sd.lfs_file_config);
	if((err == LFS_ERR_EXIST) || (err == LFS_ERR_NOENT)) {
		memset(data, 0xFF, amount*sizeof(EnergyManager1DayData));
		return true;
	} else if(err != LFS_ERR_OK) {	
		logw("lfs_file_opencfg %s: %d\n\r", f, err);
		return false;
	}

	const uint16_t pos = sizeof(SDMetadata) + (day-1) * sizeof(EnergyManager1DayData) + offset*sizeof(EnergyManager1DayData);
	lfs_ssize_t size = lfs_file_seek(&sd.lfs, &file, pos, LFS_SEEK_SET);
	if(size != pos) {
		logw("lfs_file_seek %d vs %d\n\r", pos, size);
		err = lfs_file_close(&sd.lfs, &file);
		logw("lfs_file_close %d\n\r", err);
	}

	size = lfs_file_read(&sd.lfs, &file, data, amount*sizeof(EnergyManager1DayData));
	if(size != amount*sizeof(EnergyManager1DayData)) {
		logw("lfs_file_read flags size %d vs %d\n\r", size, amount*sizeof(EnergyManager1DayData));
		err = lfs_file_close(&sd.lfs, &file);
		logd("lfs_file_close %d\n\r", err);
		return false;
	}

	err = lfs_file_close(&sd.lfs, &file);
	if(err != LFS_ERR_OK) {
		logw("lfs_file_close %d\n\r", err);
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

	if(err != LFS_ERR_OK) {
		logw("lfs_mount %d\n\r", err);
		err = lfs_format(&sd.lfs, &sd.lfs_config);
		if(err != LFS_ERR_OK) {
			logw("lfs_format %d\n\r", err);
			sd.lfs_status = ABS(err);
			return;
		}
		err = lfs_mount(&sd.lfs, &sd.lfs_config);
		if(err != LFS_ERR_OK) {
			logw("lfs_mount 2nd try %d\n\r", err);
			sd.lfs_status = ABS(err);
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
		logd("boot_count lfs_file_read size %s vs %d\n\r", size, sizeof(boot_count));
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
				}
			}
		}

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
				}
			}
		}

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
				}
			}
		}

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
				}
			}
		}

		sd.new_sd_energy_manager_daily_data_points = false;
	}
}

void sd_tick_task(void) {
	sd.sdmmc_init_last = system_timer_get_ms();

	while(true) {
		// TODO: Handle sd.sd_rw_error_count > X here

		// We retry to initialize SD card once per second
		if((sd.sdmmc_init_last != 0) && system_timer_is_time_elapsed_ms(sd.sdmmc_init_last, 1000)) {
			sd_init_task();
		}

		if((sd.sd_status == SDMMC_ERROR_OK) && (sd.lfs_status == LFS_ERR_OK)) {
			sd_tick_task_handle_wallbox_data();
			sd_tick_task_handle_wallbox_daily_data();
			sd_tick_task_handle_energy_manager_data();
			sd_tick_task_handle_energy_manager_daily_data();
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