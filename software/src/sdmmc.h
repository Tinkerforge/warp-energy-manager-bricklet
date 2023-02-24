/* warp-energy-manager-bricklet
 * Copyright (C) 2023 Olaf Lüke <olaf@tinkerforge.com>
 *
 * sdmmc.c: SD MMC SPI implementation
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

#ifndef SDMMC_H_
#define SDMMC_H

#include <stdint.h>

#define SDMMC_SECTOR_SIZE 512

// SCR register content
typedef struct {
	uint32_t sd_spec         : 4; // Physical Layer Specification Version supported by the card
	uint32_t scr_structure   : 4; // SCR version
	uint32_t sd_bus_width    : 4; // DAT bus widths that are supported by card
	uint32_t sd_security     : 3; // CPRM security specification version for each capacity card
	uint32_t data_erase_stat : 1; // Data status after erase (0 or 1)
	uint32_t                 : 3; // Reserved bits
	uint32_t ext_security    : 4; // Extended security
	uint32_t sd_spec_3       : 1; // SD specification version
	uint32_t cmd_support     : 2; // Support bit of new commands are defined to bit 33-32 of SCR
	uint32_t                 : 6; // Reserved bits
	uint32_t manuf_resvd     : 32; // Reserved: Manufacturer's use
} SDMMC_SCR;

// CID register content
typedef struct {
	uint16_t manufacturing_date; // 12 bit Manufacturing date (MDT)
	uint32_t product_serial_num; // 32 bit serial number (PSN)
	uint8_t  product_rev;        // 32 bit Product revision (PRV)
	uint8_t  product_name[5];    // 40 bit product name (PNM)
	uint8_t  app_oem_id[2];      // 16 bit OEM/Application ID (OID)
	uint8_t  manufacturer_id;    // 8 bit Manufacturer ID (MID)
} SDMMC_CID;

// CSD register content (v1)
typedef struct {
	uint32_t fixed               : 1;  // Always fixed to 1
	uint32_t crc                 : 7;  // CRC bits
	uint32_t                     : 2;  // Reserved bits
	uint32_t file_fmt            : 2;  // Indicates file format on the card
	uint32_t temp_write_prot     : 1;  // Temporarily protects card content from being overwritten or erased
	uint32_t perm_write_prot     : 1;  // Permanently protects card content against overwriting or erasing
	uint32_t copy                : 1;  // Defines if content is original (= 0) or has been copied (= 1)
	uint32_t file_fmt_grp        : 1;  // Indicates selected group of file formats
	uint32_t                     : 5;  // Reserved bits
	uint32_t write_blk_partial   : 1;  // Defines if partial block sizes can be used in block write commands
	uint32_t write_blk_len       : 4;  // Maximum write data block length is computed as 2WRITE_BL_LEN
	uint32_t write_speed_factor  : 3;  // Defines typical block program time as a multiple of the read access time
	uint32_t                     : 2;  // Reserved bits
	uint32_t write_prot_grp_en   : 1;  // A value of 0 means no group write protection possible
	uint32_t write_prot_grp_size : 7;  // Size of a write protected group; Indexed from 0
	uint32_t erase_sector_size   : 7;  // Size of an erasable sector
	uint32_t erase_blk_en        : 1;  // ERASE_BLK_EN defines granularity of unit size of erasable data
	uint32_t dev_size_mult       : 3;  // Multiplier for total size of device (Check C_SIZE)
	uint32_t max_write_current   : 3;  // Maximum write current at maximum VDD
	uint32_t min_write_current   : 3;  // Maximum write current at minimum VDD
	uint32_t max_read_current    : 3;  // Maximum read current at maximum VDD
	uint32_t min_read_current    : 3;  // Maximum read current at minimum VDD
	uint32_t dev_size_low        : 2;  // Total size of data card
	uint32_t dev_size_high       : 10; // Total size of data card
	uint32_t                     : 2;  // Reserved bits
	uint32_t dsr_imp             : 1;  // Is configurable driver stage is integrated with card?
	uint32_t read_blk_misalign   : 1;  // Can read block occupy more than 1 physical block on card?
	uint32_t write_blk_misalign  : 1;  // Can written block occupy more than 1 physical block on card?
	uint32_t read_blk_partial    : 1;  // Can a block be read partially? : Always valid in SD
	uint32_t read_blk_len        : 4;  // Maximum read data block length is computed as 2READ_BL_LEN
	uint32_t ccc                 : 12; // Card command class register specifies supported command classes
	uint32_t tran_speed          : 8;  // Defines maximum data transfer rate per one data line
	uint32_t nsac                : 8;  // Defines worst case for clock-dependent factor of the data access time
	uint32_t taac                : 8;  // Defines asynchronous part of data access time
	uint32_t                     : 6;  // Reserved bits
	uint32_t csd_struct          : 2;  // Describes version of the CSD structure
} SDMMC_CSD_V1;

// CSD register content (v2)
typedef struct {
	uint32_t fixed               : 1;  // Always fixed to 1
	uint32_t crc                 : 7;  // CRC bits
	uint32_t                     : 2;  // Reserved bits
	uint32_t file_fmt            : 2;  // Indicates file format on the card
	uint32_t temp_write_prot     : 1;  // Temporarily protects card content from being overwritten or erased
	uint32_t perm_write_prot     : 1;  // Permanently protects card content against overwriting or erasing
	uint32_t copy                : 1;  // Defines if content is original (= 0) or has been copied (= 1)
	uint32_t file_fmt_grp        : 1;  // Indicates selected group of file formats
	uint32_t                     : 5;  // Reserved bits
	uint32_t write_blk_partial   : 1;  // Defines if partial block sizes can be used in block write commands
	uint32_t write_blk_len       : 4;  // Maximum write data block length is computed as 2WRITE_BL_LEN
	uint32_t write_speed_factor  : 3;  // Defines typical block program time as a multiple of the read access time
	uint32_t                     : 2;  // Reserved bits
	uint32_t write_prot_grp_en   : 1;  // A value of 0 means no group write protection possible
	uint32_t write_prot_grp_size : 7;  // Size of a write protected group; Indexed from 0
	uint32_t erase_sector_size   : 7;  // Size of an erasable sector
	uint32_t erase_blk_en        : 1;  // If 1, host can erase one or multiple units of 512 bytes
	uint32_t                     : 1;  // Reserved bits
	uint32_t dev_size_low        : 16; // Total size of data card
	uint32_t dev_size_high       : 6;  // Total size of data card
	uint32_t                     : 6;  // Reserved bits
	uint32_t dsr_imp             : 1;  // Is configurable driver stage is integrated with card?
	uint32_t read_blk_misalign   : 1;  // When 0, read crossing block boundaries is disabled in high capacity SD cards
	uint32_t write_blk_misalign  : 1;  // When 0, write crossing block boundaries is disabled in high capacity SD cards
	uint32_t read_blk_partial    : 1;  // Fixed to 0; Only unit of block access is permitted
	uint32_t read_blk_len        : 4;  // Fixed at 0x09; Indicates READ_BL_LEN = 512 bytes
	uint32_t ccc                 : 12; // Card command class register specifies supported command classes
	uint32_t tran_speed          : 8;  // Defines maximum data transfer rate per one data line
	uint32_t nsac                : 8;  // Fixed at 0x00
	uint32_t taac                : 8;  // Fixed at 0x0E; Indicates asynchronous part of data access time as 1 ms
	uint32_t                     : 6;  // Reserved bits
	uint32_t csd_struct          : 2;  // Describes version of the CSD structure
} SDMMC_CSD_V2;

typedef struct {
	uint16_t pos;
	uint32_t sector;
	uint8_t  type;

	uint8_t ocr[4];
	SDMMC_CID cid;
	SDMMC_CSD_V2 csd_v2;
} SDMMC;

extern SDMMC sdmmc;

// SPI Command Set
#define SDMMC_CMD0   0x40 // software reset
#define SDMMC_CMD1   0x41 // initiate initialisation process
#define SDMMC_CMD8   0x48 // check voltage range (SDC v2 only)
#define SDMMC_CMD9   0x49 // read CSD register
#define SDMMC_CMD10  0x4A // read CID register
#define SDMMC_CMD12  0x4C // force stop transmission
#define SDMMC_CMD16  0x50 // set read/write block size
#define SDMMC_CMD17  0x51 // read a block
#define SDMMC_CMD18  0x52 // read multiple blocks
#define SDMMC_CMD24  0x58 // write a block
#define SDMMC_CMD25  0x59 // write multiple blocks
#define SDMMC_CMD41  0x69
#define SDMMC_CMD55  0x77 // leading command for ACMD commands
#define SDMMC_CMD58  0x7A // read OCR
#define SDMMC_ACMD41 0xE9 // ACMDx = CMDx + 0x80
// (note: all commands are ORed with 64 [e.g. CMD16 = 16+64 = 0x50])

// SDMMC Card Type Definitions
#define SDMMC_TYPE_MMC   0x01
#define SDMMC_TYPE_SD1   0x02 // SD v1
#define SDMMC_TYPE_SD2   0x04 // SD v2
#define SDMMC_TYPE_SDC   (SDMMC_TYPE_SD1|SDMMC_TYPE_SD2) // SD
#define SDMMC_TYPE_BLOCK 0x08 // block addressing

#define SDMMC_CMD8_ACCEPTED                         2
#define SDMMC_CMD8_REJECTED                         3

#define SDMMC_ARG_STUFF                             0x00000000
#define SDMMC_ARG_CMD8                              0x000001AA
#define SDMMC_ARG_ACMD41                            0x40000000

#define SDMMC_DATA_TOKEN_TIMEOUT                    100 // ms
#define SDMMC_RESPONSE_TIMEOUT                      100 // ms
#define SDMMC_READ_RETRY_COUNT                      10

#define SDMMC_BLOCK_SPI_CSD_CID_LENGTH              16
#define SDMMC_BLOCK_SPI_START_BLOCK_TOKEN           0xFE
#define SDMMC_BLOCK_SPI_INIT_SPEED                  300000

typedef enum {
	SDMMC_ERROR_OK                  = 0,
	SDMMC_ERROR_READ_BLOCK_TIMEOUT  = 1,
	SDMMC_ERROR_WRITE_BLOCK_TIMEOUT = 2,
	SDMMC_ERROR_RESPONSE_TIMEOUT    = 3,
	SDMMC_ERROR_INIT_TYPE           = 11,
	SDMMC_ERROR_INIT_VER_OR_VOLTAGE = 12,
	SDMMC_ERROR_INIT_ACMD41         = 13,
	SDMMC_ERROR_INIT_CMD58          = 14,
	SDMMC_ERROR_INIT_CMD0           = 15, // Expect this error when no SD card is inserted
	SDMMC_ERROR_CID_START           = 21,
	SDMMC_ERROR_CID_CMD10           = 22,
	SDMMC_ERROR_CSD_START           = 31,
	SDMMC_ERROR_CSD_CMD9            = 32,
	SDMMC_ERROR_COUNT_TO_HIGH       = 41,
	SDMMC_ERROR_NO_CARD             = 51,
} SDMMCError;

SDMMCError sdmmc_response(uint8_t response);
uint8_t sdmmc_send_command(uint8_t cmd, uint32_t arg);
uint8_t sdmmc_wait_until_ready(void);
SDMMCError sdmmc_init(void);
void sdmmc_send_spi_command(uint8_t cmd, uint32_t arg, uint8_t crc, uint8_t read_bytes);
SDMMCError sdmmc_read_block(uint32_t sector, uint8_t *data);
SDMMCError sdmmc_write_block(uint32_t sector, const uint8_t* data);
void sdmmc_spi_deinit(void);

#endif
