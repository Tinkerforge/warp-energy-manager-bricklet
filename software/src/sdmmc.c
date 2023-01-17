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

// The rough layout of this code is inspired by https://github.com/spidru/sdmmc (originally MIT License)
//   This repository has unfortunately not been updated for many years and it contains some bugs,
//   but it was a good starting point. I would not recommend to use it directly for a project.
// I also found the elm-chan SD/MMC documentation very helpful: http://elm-chan.org/docs/mmc/mmc_e.html
// Additionally i looked at the code from the generated MMC driver from Infineon/DAVE.

#include "sdmmc.h"
#include "configs/config_sdmmc.h"

#include "bricklib2/logging/logging.h"

#include <stdint.h>

SDMMC sdmmc;

#define SPI_FIFO_TIMEOUT 1000

bool sdmmc_spi_write(const uint8_t *data, uint32_t length) {
	XMC_USIC_CH_RXFIFO_Flush(sdmmc.spi_fifo.channel);

	uint32_t start = system_timer_get_ms();
	uint16_t mosi_count = 0;
	uint16_t miso_count = 0;
	while((mosi_count < length) || (miso_count < length)) {
		if(system_timer_is_time_elapsed_ms(start, SPI_FIFO_TIMEOUT)) {
			return false;
		}
		
		if(!XMC_USIC_CH_TXFIFO_IsFull(sdmmc.spi_fifo.channel) && (mosi_count < length)) {
			sdmmc.spi_fifo.channel->IN[0] = data[mosi_count];
			mosi_count++;
		}

		if(!XMC_USIC_CH_RXFIFO_IsEmpty(sdmmc.spi_fifo.channel) && (miso_count < length)) {
			volatile uint8_t __attribute__((unused)) _ = sdmmc.spi_fifo.channel->OUTR;
			miso_count++;
		}
	}

	return true;
}

bool sdmmc_spi_read(uint8_t *data, uint32_t length) {
	XMC_USIC_CH_RXFIFO_Flush(sdmmc.spi_fifo.channel);

	uint32_t start = system_timer_get_ms();
	uint16_t mosi_count = 0;
	uint16_t miso_count = 0;
	while((mosi_count < length) || (miso_count < length)) {
		if(system_timer_is_time_elapsed_ms(start, SPI_FIFO_TIMEOUT)) {
			return false;
		}
		
		if(!XMC_USIC_CH_TXFIFO_IsFull(sdmmc.spi_fifo.channel) && (mosi_count < length)) {
			sdmmc.spi_fifo.channel->IN[0] = 0xFF;
			mosi_count++;
		}

		if(!XMC_USIC_CH_RXFIFO_IsEmpty(sdmmc.spi_fifo.channel) && (miso_count < length)) {
			data[miso_count] = sdmmc.spi_fifo.channel->OUTR;
			miso_count++;
		}
	}

	return true;
}

void sdmmc_spi_select(void) {
	XMC_GPIO_SetOutputLow(SDMMC_SELECT_PORT, SDMMC_SELECT_PIN);
}

void sdmmc_spi_deselect(void) {
	XMC_GPIO_SetOutputHigh(SDMMC_SELECT_PORT, SDMMC_SELECT_PIN);
}

void sdmmc_spi_init(void) {
	// Initialise SPI FIFO
	sdmmc.spi_fifo.baudrate = SDMMC_SPI_BAUDRATE;
	sdmmc.spi_fifo.channel = SDMMC_USIC_SPI;

	sdmmc.spi_fifo.rx_fifo_size = SDMMC_RX_FIFO_SIZE;
	sdmmc.spi_fifo.rx_fifo_pointer = SDMMC_RX_FIFO_DATA_POINTER;
	sdmmc.spi_fifo.tx_fifo_size = SDMMC_TX_FIFO_SIZE;
	sdmmc.spi_fifo.tx_fifo_pointer = SDMMC_TX_FIFO_DATA_POINTER;

	sdmmc.spi_fifo.clock_passive_level = SDMMC_CLOCK_PASSIVE_LEVEL;
	sdmmc.spi_fifo.clock_output = SDMMC_CLOCK_OUTPUT;
	sdmmc.spi_fifo.slave = SDMMC_SLAVE;

	sdmmc.spi_fifo.sclk_port = SDMMC_SCLK_PORT;
	sdmmc.spi_fifo.sclk_pin = SDMMC_SCLK_PIN;
	sdmmc.spi_fifo.sclk_pin_mode = SDMMC_SCLK_PIN_MODE;

	sdmmc.spi_fifo.select_port = SDMMC_SELECT_PORT;
	sdmmc.spi_fifo.select_pin = SDMMC_SELECT_PIN;
	sdmmc.spi_fifo.select_pin_mode = SDMMC_SELECT_PIN_MODE;

	sdmmc.spi_fifo.mosi_port = SDMMC_MOSI_PORT;
	sdmmc.spi_fifo.mosi_pin = SDMMC_MOSI_PIN;
	sdmmc.spi_fifo.mosi_pin_mode = SDMMC_MOSI_PIN_MODE;

	sdmmc.spi_fifo.miso_port = SDMMC_MISO_PORT;
	sdmmc.spi_fifo.miso_pin = SDMMC_MISO_PIN;
	sdmmc.spi_fifo.miso_input = SDMMC_MISO_INPUT;
	sdmmc.spi_fifo.miso_source = SDMMC_MISO_SOURCE;

	spi_fifo_init(&sdmmc.spi_fifo);
}

// Continuously checks reply from card until expected reply is given or until timeout occurs.
SDMMCError sdmmc_response(uint8_t response) {
	uint8_t result = ~response;

	uint32_t start = system_timer_get_ms();
	while(result != response) {
		sdmmc_spi_read(&result, 1);
		if(system_timer_is_time_elapsed_ms(start, SDMMC_RESPONSE_TIMEOUT)) {
			return SDMMC_ERROR_RESPONSE_TIMEOUT;
		}
	}

	return SDMMC_ERROR_OK;
}

uint8_t sdmmc_send_command(uint8_t cmd, uint32_t arg) {
	uint8_t data = 0;

	if(sdmmc_wait_until_ready() != 0xFF) {
		logd("sdmmc_send_command timeout\n\r");
		return 0xFF; // TODO: Maybe use SDMCError as return and uint8_t *response as argument?
	}

	if(cmd & 0x80) {
		// Send CMD55 packet
		uint8_t data_cmd55[6] = {SDMMC_CMD55, 0, 0, 0, 0, 1}; // trailing byte (7-bit CRC + stop bit)
		sdmmc_spi_write(data_cmd55, 6);

		for(uint8_t retry = 0; retry < SDMMC_READ_RETRY_COUNT; retry++) {
			sdmmc_spi_read(&data, 1);

			// when the card is busy, MSB in R1 is 1
			if(!(data & 0x80)) {
				break;
			}
		}

		if(data != 1) {
			return data;
		}
		cmd = cmd & 0x7F; // prepare next command (below)
	}

	uint8_t data_cmd[6] = {cmd, (arg >> 24) & 0xFF, (arg >> 16) & 0xFF, (arg >> 8) & 0xFF, arg & 0xFF, 1}; // trailing byte (7-bit CRC + stop bit)
	if(cmd == SDMMC_CMD0) {
		data_cmd[5] = 0x95;
	} else if(cmd == SDMMC_CMD8) {
		data_cmd[5] = 0x87;
	}
	sdmmc_spi_write(data_cmd, 6);

	if(cmd == SDMMC_CMD12) {
		// skip a stuff byte when stop reading (?)
		sdmmc_spi_read(&data, 1);
	}

	for(uint8_t retry = 0; retry < SDMMC_READ_RETRY_COUNT; retry++) {
		sdmmc_spi_read(&data, 1);

		// when the card is busy, MSB in R1 is 1
		if(!(data & 0x80)) {
			break;
		}
	}

	return data;
}

uint8_t sdmmc_wait_until_ready(void) {
	uint32_t start = system_timer_get_ms();

	uint8_t data = 0;
	while(data != 0xFF) {
		sdmmc_spi_read(&data, 1);
		if(system_timer_is_time_elapsed_ms(start, SDMMC_RESPONSE_TIMEOUT)) {
			break;
		}
	}

	return data;
}

int16_t sdmmc_init_csd(void) {
	sdmmc_spi_select();
	if(sdmmc_send_command(SDMMC_CMD9, SDMMC_ARG_STUFF) == 0) {
		if(sdmmc_response(SDMMC_BLOCK_SPI_START_BLOCK_TOKEN) == SDMMC_ERROR_OK) {
			uint8_t buffer[SDMMC_BLOCK_SPI_CSD_CID_LENGTH+2] = {0};
			sdmmc_spi_read(buffer, SDMMC_BLOCK_SPI_CSD_CID_LENGTH+2); // +2 = unused CRC

			// We assume that it is a high capicity card that supports CSV v2 here
			uint8_t *buffer_csd = (uint8_t*)&(sdmmc.csd_v2);
			// Reverse order
			for(uint8_t i = 0; i < SDMMC_BLOCK_SPI_CSD_CID_LENGTH; i++) {
				buffer_csd[SDMMC_BLOCK_SPI_CSD_CID_LENGTH-i-1] = buffer[i];
			}
		} else {
			sdmmc_spi_deselect();
			return SDMMC_ERROR_CSD_START;
		}
	} else {
		sdmmc_spi_deselect();
		return SDMMC_ERROR_CSD_CMD9;
	}
		
	sdmmc_spi_deselect();

	logd("CSD v2:\n\r");
	logd("  crc                 %d\n\r", sdmmc.csd_v2.crc);
	logd("  fixed               %d\n\r", sdmmc.csd_v2.fixed);
	logd("  file_fmt            %d\n\r", sdmmc.csd_v2.file_fmt);
	logd("  temp_write_prot     %d\n\r", sdmmc.csd_v2.temp_write_prot);
	logd("  perm_write_prot     %d\n\r", sdmmc.csd_v2.perm_write_prot);
	logd("  copy                %d\n\r", sdmmc.csd_v2.copy);
	logd("  file_fmt_grp        %d\n\r", sdmmc.csd_v2.file_fmt_grp);
	logd("  write_blk_partial   %d\n\r", sdmmc.csd_v2.write_blk_partial);
	logd("  write_blk_len       %d\n\r", sdmmc.csd_v2.write_blk_len);
	logd("  write_speed_factor  %d\n\r", sdmmc.csd_v2.write_speed_factor);
	logd("  write_prot_grp_en   %d\n\r", sdmmc.csd_v2.write_prot_grp_en);
	logd("  write_prot_grp_size %d\n\r", sdmmc.csd_v2.write_prot_grp_size);
	logd("  erase_sector_size   %d\n\r", sdmmc.csd_v2.erase_sector_size);
	logd("  erase_blk_en        %d\n\r", sdmmc.csd_v2.erase_blk_en);
	logd("  dev_size_low        %d\n\r", sdmmc.csd_v2.dev_size_low);
	logd("  dev_size_high       %d\n\r", sdmmc.csd_v2.dev_size_high);
	logd("  dsr_imp             %d\n\r", sdmmc.csd_v2.dsr_imp);
	logd("  read_blk_misalign   %d\n\r", sdmmc.csd_v2.read_blk_misalign);
	logd("  write_blk_misalign  %d\n\r", sdmmc.csd_v2.write_blk_misalign);
	logd("  read_blk_partial    %d\n\r", sdmmc.csd_v2.read_blk_partial);
	logd("  read_blk_len        %d\n\r", sdmmc.csd_v2.read_blk_len);
	logd("  ccc                 %d\n\r", sdmmc.csd_v2.ccc);
	logd("  tran_speed          %d\n\r", sdmmc.csd_v2.tran_speed);
	logd("  nsac                %d\n\r", sdmmc.csd_v2.nsac);
	logd("  taac                %d\n\r", sdmmc.csd_v2.taac);
	logd("  csd_struct          %d\n\r", sdmmc.csd_v2.csd_struct);

	return SDMMC_ERROR_OK;
}

int16_t sdmmc_init_cid(void) {
	sdmmc_spi_select();
	if(sdmmc_send_command(SDMMC_CMD10, SDMMC_ARG_STUFF) == 0) {
		if(sdmmc_response(SDMMC_BLOCK_SPI_START_BLOCK_TOKEN) == SDMMC_ERROR_OK) {
			uint8_t buffer[SDMMC_BLOCK_SPI_CSD_CID_LENGTH+2] = {0};
			sdmmc_spi_read(buffer, SDMMC_BLOCK_SPI_CSD_CID_LENGTH+2); // +2 = unused CRC

			// Reverse order and fix endians in one go
			sdmmc.cid.manufacturing_date  = (uint16_t)((uint16_t)((uint16_t)buffer[SDMMC_BLOCK_SPI_CSD_CID_LENGTH-1-2] & 0x0FU) << 8);
			sdmmc.cid.manufacturing_date |= ((uint16_t)buffer[SDMMC_BLOCK_SPI_CSD_CID_LENGTH-1-1] & 0xFFU);
			sdmmc.cid.product_serial_num  = buffer[SDMMC_BLOCK_SPI_CSD_CID_LENGTH-1-3];
			sdmmc.cid.product_serial_num |= (((uint32_t)buffer[SDMMC_BLOCK_SPI_CSD_CID_LENGTH-1-4] << 8) | ((uint32_t)buffer[SDMMC_BLOCK_SPI_CSD_CID_LENGTH-1-5] << 16) | ((uint32_t)buffer[SDMMC_BLOCK_SPI_CSD_CID_LENGTH-1-6] << 24));
			sdmmc.cid.product_rev         = (buffer[SDMMC_BLOCK_SPI_CSD_CID_LENGTH-1-7] & 0xFFU);
			sdmmc.cid.product_name[0]     = buffer[SDMMC_BLOCK_SPI_CSD_CID_LENGTH-1-12];
			sdmmc.cid.product_name[1]     = buffer[SDMMC_BLOCK_SPI_CSD_CID_LENGTH-1-11];
			sdmmc.cid.product_name[2]     = buffer[SDMMC_BLOCK_SPI_CSD_CID_LENGTH-1-10];
			sdmmc.cid.product_name[3]     = buffer[SDMMC_BLOCK_SPI_CSD_CID_LENGTH-1-9];
			sdmmc.cid.product_name[4]     = buffer[SDMMC_BLOCK_SPI_CSD_CID_LENGTH-1-8];
			sdmmc.cid.app_oem_id[0]       = buffer[SDMMC_BLOCK_SPI_CSD_CID_LENGTH-1-14];
			sdmmc.cid.app_oem_id[1]       = buffer[SDMMC_BLOCK_SPI_CSD_CID_LENGTH-1-13];
			sdmmc.cid.manufacturer_id     = buffer[SDMMC_BLOCK_SPI_CSD_CID_LENGTH-1-15];
		} else {
			sdmmc_spi_deselect();
			return SDMMC_ERROR_CID_START;
		}
	} else {
		sdmmc_spi_deselect();
		return SDMMC_ERROR_CID_CMD10;
	}
		
	sdmmc_spi_deselect();

	logd("CID:\n\r");
	logd("  manufacturing_date %d\n\r", sdmmc.cid.manufacturing_date);
	logd("  product_serial_num %d\n\r", sdmmc.cid.product_serial_num);
	logd("  product_rev %d\n\r", sdmmc.cid.product_rev);
	logd("  product_name %c %c %c %c %c\n\r", sdmmc.cid.product_name[0], sdmmc.cid.product_name[1], sdmmc.cid.product_name[2], sdmmc.cid.product_name[3], sdmmc.cid.product_name[4]);
	logd("  app_oem_id %d %d\n\r", sdmmc.cid.app_oem_id[0], sdmmc.cid.app_oem_id[1]);
	logd("  manufacturer_id %d\n\r", sdmmc.cid.manufacturer_id);

	return SDMMC_ERROR_OK;
}

// Initialize SD card.
SDMMCError sdmmc_init(void) {
	SDMMCError sdmmc_error = SDMMC_ERROR_OK;

	memset(&sdmmc, 0, sizeof(SDMMC));
	sdmmc_spi_init();

	// Start initialization with slow speed (300kHz)
	XMC_USIC_CH_SetBaudrate(sdmmc.spi_fifo.channel, SDMMC_BLOCK_SPI_INIT_SPEED, 2); 

	uint8_t buffer[10];

	// allowing 80 clock cycles for initialisation
	sdmmc_spi_deselect();
	sdmmc_spi_read(buffer, 10);
	sdmmc_spi_select();

	// CMD0
	if(sdmmc_send_command(SDMMC_CMD0, SDMMC_ARG_STUFF) == 1) {
		// CMD8
		if(sdmmc_send_command(SDMMC_CMD8, SDMMC_ARG_CMD8) == 1) {
			// SDv2
			sdmmc_spi_read(buffer, 4);
			if(buffer[2] == 0x01 && buffer[3] == 0xAA) {
				// ACMD41
				uint8_t retry;
				for(retry = 0; retry < SDMMC_READ_RETRY_COUNT; retry++) {
					uint8_t ret = sdmmc_send_command(SDMMC_ACMD41, SDMMC_ARG_ACMD41);
					if(ret == 0) {
						break;
					}
				}
				if(retry != SDMMC_READ_RETRY_COUNT) {
					// CMD58
					if(sdmmc_send_command(SDMMC_CMD58, 0) == 0) {
						// Read OCR
						sdmmc_spi_read(sdmmc.ocr, 4);

						// Check CCS bit
						sdmmc.type = (sdmmc.ocr[0] & 0x40) ? (SDMMC_TYPE_SD2 | SDMMC_TYPE_BLOCK) : SDMMC_TYPE_SD2;	// SDv2 (HC or SC)
					} else {
						logd("CMD58 failed\n\r");
						sdmmc_error = SDMMC_ERROR_INIT_CMD58;
					}
				} else {
					logd("ACMD41 failed\n\r");
					sdmmc_error = SDMMC_ERROR_INIT_ACMD41;
				}
			} else {
				logd("SD card version below 2.0 or voltage not supported\n\r");
				sdmmc_error = SDMMC_ERROR_INIT_VER_OR_VOLTAGE;
			}
		} else {
			uint8_t cmd = 0;
			// SDv1 or MMCv3
			if(sdmmc_send_command(SDMMC_ACMD41, SDMMC_ARG_STUFF) <= 1) {
				// SDv1
				sdmmc.type = SDMMC_TYPE_SD1;
				cmd = SDMMC_ACMD41;
			} else {
				// MMCv3
				sdmmc.type = SDMMC_TYPE_MMC;
				cmd = SDMMC_CMD1;
			}

			uint8_t retry;
			for(retry = 0; retry < SDMMC_READ_RETRY_COUNT; retry++) {
				if(sdmmc_send_command(cmd, SDMMC_ARG_STUFF) == 0) {
					break;
				}
			}

			if((retry == SDMMC_READ_RETRY_COUNT) || (sdmmc_send_command(SDMMC_CMD16, SDMMC_SECTOR_SIZE) != SDMMC_ARG_STUFF)) {
				// invalid card type
				sdmmc.type = 0;
				sdmmc_error = SDMMC_ERROR_INIT_TYPE;
			}
		}
	} else {
		logd("CMD0 failed\n\r");
		sdmmc_error = SDMMC_ERROR_INIT_CMD0;
	}

	sdmmc_spi_deselect();

	// Use normal speed after initialization
	XMC_USIC_CH_SetBaudrate(sdmmc.spi_fifo.channel, sdmmc.spi_fifo.baudrate, 2); 

	if(sdmmc.type == 0) {
		return sdmmc_error;
	}

	sdmmc_error = sdmmc_init_csd();
	if(sdmmc_error != SDMMC_ERROR_OK) {
		return sdmmc_error;
	}
	sdmmc_error = sdmmc_init_cid();
	if(sdmmc_error != SDMMC_ERROR_OK) {
		return sdmmc_error;
	}

	sdmmc.sector = 0;
	sdmmc.pos = 0;
	return SDMMC_ERROR_OK;
}

// Sends one of the commands from the SPI command set.
void sdmmc_send_spi_command(uint8_t cmd, uint32_t arg, uint8_t crc, uint8_t read_bytes) {
	sdmmc_response(0xFF);

	uint8_t buffer[6] = {cmd, arg >> 24, arg >> 16, arg >> 8, arg, crc};
	sdmmc_spi_write(buffer, 6);
}

// Reads one block
SDMMCError sdmmc_read_block(uint32_t sector, uint8_t *data) {
	uint8_t ret = SDMMC_ERROR_READ_BLOCK_TIMEOUT;
	sdmmc_spi_select();

	// different addressing for SDSC and SDHC
	uint32_t address = sdmmc.type & SDMMC_TYPE_BLOCK ? sector : sector << 9;
	if(sdmmc_send_command(SDMMC_CMD17, address) == 0x00) {
		// wait for start of data token (0xFE)
		uint32_t start = system_timer_get_ms();
		uint8_t data_byte = 0xFF;
		while(data_byte == 0xFF) {
			sdmmc_spi_read(&data_byte, 1);
			if(system_timer_is_time_elapsed_ms(start, SDMMC_DATA_TOKEN_TIMEOUT)) {
				break;
			}
		}

		if(data_byte == 0xFE) {
			// read sector
			sdmmc_spi_read(data, SDMMC_SECTOR_SIZE);

			// skip checksum
			uint8_t tmp[2] = {0, 0};
			sdmmc_spi_read(tmp, 2);

			ret = SDMMC_ERROR_OK;
		}
	}

	sdmmc_spi_deselect();
	return ret;
}

// Write one block
SDMMCError sdmmc_write_block(uint32_t sector, const uint8_t* data) {
	sdmmc_spi_select();

	// different addressing for SDSC and SDHC
	uint32_t address = sdmmc.type & SDMMC_TYPE_BLOCK ? sector : sector << 9;
	sdmmc_send_spi_command(SDMMC_CMD24, address, 0xFF, 8);
	if(sdmmc_response(0x00) != SDMMC_ERROR_OK) {
		sdmmc_spi_deselect();
		return SDMMC_ERROR_WRITE_BLOCK_TIMEOUT;
	}

	// send token
	uint8_t tmp = 0xFE;
	sdmmc_spi_write(&tmp, 1);

	sdmmc_spi_write(data, SDMMC_SECTOR_SIZE);

	// write checksum
	tmp = 0xFF;
	sdmmc_spi_write(&tmp, 1);
	sdmmc_spi_write(&tmp, 1);

	if(sdmmc_response(0xE5) != SDMMC_ERROR_OK) {
		sdmmc_spi_deselect();
		return SDMMC_ERROR_WRITE_BLOCK_TIMEOUT;
	}

	do {
		sdmmc_spi_read(&tmp, 1);
	} while(tmp == 0x00); // wait for write to finish

	sdmmc_spi_deselect();

	return SDMMC_ERROR_OK;
}
