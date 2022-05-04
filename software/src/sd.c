/* sd-card-bricklet
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
#include "bricklib2/logging/logging.h"

#include "fatfs/ff.h"
#include "SDMMC_BLOCK/sdmmc_block.h"

#include "xmc_rtc.h"

SD sd;

void sd_init(void) {
	// Init SDMMC block
	SDMMC_BLOCK_STATUS_t sdmmc_status = SDMMC_BLOCK_Init(&SDMMC_BLOCK_0);
	if(sdmmc_status != SDMMC_BLOCK_STATUS_SUCCESS) {
		logd("SDMMC_BLOCK_Init() failed, status = %d\r\n", sdmmc_status);
		return;
	}

	// Init RTC
	const XMC_RTC_CONFIG_t rtc_config = {
		{
			.seconds = 0U,
			.minutes = 0U,
			.hours = 0U,
			.days = 0U,
			.daysofweek = XMC_RTC_WEEKDAY_THURSDAY,
			.month = XMC_RTC_MONTH_JANUARY,
			.year = 1970U
		}, {
			.seconds = 0U,
			.minutes = 1U,
			.hours = 0U,
			.days = 0U,
			.month = XMC_RTC_MONTH_JANUARY,
			.year = 1970U
		},
		.prescaler = 0x7FFFU
	};
	XMC_RTC_STATUS_t rtc_status = XMC_RTC_Init(&rtc_config);
	if(rtc_status == XMC_RTC_STATUS_OK) {
		XMC_RTC_Start();
	} else {
		logd("XMC_RTC_Init() failed, status = %d\r\n", rtc_status);
	}

	FATFS fs;
	FRESULT res = f_mount(&fs, "", 0);

	// mount the default drive
	res = f_mount(&fs, "", 0);
	if(res != FR_OK) {
		logd("f_mount() failed, res = %d\r\n", res);
		return;
	}

	logd("f_mount() done!\r\n");

	uint32_t freeClust;
	logd("1\n\r");
	FATFS* fs_ptr = &fs;
	logd("2\n\r");
	res = f_getfree("", &freeClust, &fs_ptr); // Warning! This fills fs.n_fatent and fs.csize!
	logd("3\n\r");
	if(res != FR_OK) {
		logd("f_getfree() failed, res = %d\r\n", res);
		return;
	}

	logd("f_getfree() done!\r\n");

	uint32_t totalBlocks = (fs.n_fatent - 2) * fs.csize;
	uint32_t freeBlocks = freeClust * fs.csize;

	logd("Total blocks: %d (%d Mb)\r\n", totalBlocks, totalBlocks / 2000);
	logd("Free blocks: %d (%d Mb)\r\n", freeBlocks, freeBlocks / 2000);

	DIR dir;
	res = f_opendir(&dir, "/");
	if(res != FR_OK) {
		logd("f_opendir() failed, res = %d\r\n", res);
		return;
	}

	FILINFO fileInfo;
	uint32_t totalFiles = 0;
	uint32_t totalDirs = 0;
	logd("--------\r\nRoot directory:\r\n");
	for(;;) {
		res = f_readdir(&dir, &fileInfo);
		if((res != FR_OK) || (fileInfo.fname[0] == '\0')) {
			break;
		}
		
		if(fileInfo.fattrib & AM_DIR) {
			logd("  DIR  %s\r\n", fileInfo.fname);
			totalDirs++;
		} else {
			logd("  FILE %s\r\n", fileInfo.fname);
			totalFiles++;
		}
	}

	logd("(total: %d dirs, %d files)\r\n--------\r\n", totalDirs, totalFiles);

	res = f_closedir(&dir);
	if(res != FR_OK) {
		logd("f_closedir() failed, res = %d\r\n", res);
		return;
	}

	logd("Writing to log.txt...\r\n");

	char writeBuff[128] = "lol";
	logd("Total blocks: %d (%d Mb); Free blocks: %d (%d Mb)\r\n",
		totalBlocks, totalBlocks / 2000,
		freeBlocks, freeBlocks / 2000);

	FIL logFile;
	res = f_open(&logFile, "log.txt", FA_OPEN_APPEND | FA_WRITE);
	if(res != FR_OK) {
		logd("f_open() failed, res = %d\r\n", res);
		return;
	}

	unsigned int bytesToWrite = strlen(writeBuff);
	unsigned int bytesWritten;
	res = f_write(&logFile, writeBuff, bytesToWrite, &bytesWritten);
	if(res != FR_OK) {
		logd("f_write() failed, res = %d\r\n", res);
		return;
	}

	if(bytesWritten < bytesToWrite) {
		logd("WARNING! Disk is full, bytesToWrite = %d, bytesWritten = %d\r\n", bytesToWrite, bytesWritten);
	}

	res = f_close(&logFile);
	if(res != FR_OK) {
		logd("f_close() failed, res = %d\r\n", res);
		return;
	}

	logd("Reading file...\r\n");
	FIL msgFile;
	res = f_open(&msgFile, "log.txt", FA_READ);
	if(res != FR_OK) {
		logd("f_open() failed, res = %d\r\n", res);
		return;
	}

	char readBuff[128];
	unsigned int bytesRead;
	res = f_read(&msgFile, readBuff, sizeof(readBuff)-1, &bytesRead);
	if(res != FR_OK) {
		logd("f_read() failed, res = %d\r\n", res);
		return;
	}

	readBuff[bytesRead] = '\0';
	logd("```\r\n%s\r\n```\r\n", readBuff);

	res = f_close(&msgFile);
	if(res != FR_OK) {
		logd("f_close() failed, res = %d\r\n", res);
		return;
	}

	// Unmount
	res = f_mount(NULL, "", 0);
	if(res != FR_OK) {
		logd("Unmount failed, res = %d\r\n", res);
		return;
	}

	logd("Done!\r\n");
}

void sd_tick(void) {

}