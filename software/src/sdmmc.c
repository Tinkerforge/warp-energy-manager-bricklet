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
int16_t sdmmc_response(uint8_t response) {
	uint8_t result = ~response;

	uint32_t start = system_timer_get_ms();
	while(result != response) {
		sdmmc_spi_read(&result, 1);
		if(system_timer_is_time_elapsed_ms(start, SDMMC_RESPONSE_TIMEOUT)) {
			return SDMMC_ERR_IDLE_STATE_TIMEOUT;
		}
	}

	return SDMMC_ERR_NONE;
}

uint8_t sdmmc_send_command(uint8_t cmd, uint32_t arg) {
	uint8_t data = 0;

	if(sdmmc_wait_until_ready() != 0xFF) {
		logd("sdmmc_send_command timeout\n\r");
		return SDMMC_ERR_IDLE_STATE_TIMEOUT;
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

// Checks for a R7-type response
int16_t sdmmc_response_r7(void) {
	int16_t ret = sdmmc_response(0x01);
	if(ret != SDMMC_ERR_NONE) {
		return ret;
	}

	uint8_t data[4] = {0};
	sdmmc_spi_read(data, 4);
	uint32_t reply = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
	if((reply & 0xFFF) == 0x1AA) {
		return SDMMC_CMD8_ACCEPTED;
	}

	return SDMMC_CMD8_REJECTED;
}

int16_t sdmmc_response_r3(void) {
	int16_t ret = sdmmc_response(0x01);
	if(ret != SDMMC_ERR_NONE) {
		return ret;
	}

	uint8_t data[4] = {0};
	sdmmc_spi_read(data, 4);
	uint32_t reply = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
	if(reply & 0x40000000) { // check if CCS (bit 30) = 1
		return 2;
	}

	return 1;
}

// Initialize SD card.
int16_t sdmmc_init(void) {
	memset(&sdmmc, 0, sizeof(SDMMC));
	sdmmc_spi_init();

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
				for(uint8_t retry = 0; retry < SDMMC_READ_RETRY_COUNT; retry++) {
					uint8_t ret = sdmmc_send_command(SDMMC_ACMD41, SDMMC_ARG_ACMD41);
					if(ret == 0) {
						break;
					}
				}

				// CMD58
				if(sdmmc_send_command(SDMMC_CMD58, 0) == 0) {
					// Read OCR
					sdmmc_spi_read(buffer, 4);

					// Check CCS bit
					sdmmc.type = (buffer[0] & 0x40) ? (SDMMC_TYPE_SD2 | SDMMC_TYPE_BLOCK) : SDMMC_TYPE_SD2;	// SDv2 (HC or SC)
				} else {
					logd("CMD58 failed\n\r");
					// TODO: Error code for this
				}
			} else {
				logd("SD card version blow 2.0 or voltage not supported\n\r");
				logd("buffer: %d %d %d %d\n\r", buffer[0], buffer[1], buffer[2], buffer[3]);
				// TODO: Error code for this
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
			}
		}
	} else {
		logd("CMD0 failed\n\r");
		// TODO: Error code for this
	}

	sdmmc_spi_deselect();

	if(sdmmc.type == 0) {
		return SDMMC_ERR_INIT;
	}

	sdmmc.sector = 0;
	sdmmc.pos = 0;
	return SDMMC_ERR_NONE;
}

// Sends one of the commands from the SPI command set.
void sdmmc_send_spi_command(uint8_t cmd, uint32_t arg, uint8_t crc, uint8_t read_bytes) {
	sdmmc_response(0xFF);

	uint8_t buffer[6] = {cmd, arg >> 24, arg >> 16, arg >> 8, arg, crc};
	sdmmc_spi_write(buffer, 6);
}

// Reads one block
int8_t sdmmc_read_block(uint32_t sector, uint8_t *data) {
	uint8_t ret = SDMMC_ERR_READ_BLOCK_TIMEOUT;
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

			ret = SDMMC_ERR_NONE;
		}
	}

	sdmmc_spi_deselect();
	return ret;
}

// Write one block
int16_t sdmmc_write_block(uint32_t sector, const uint8_t* data) {
	sdmmc_spi_select();

	// different addressing for SDSC and SDHC
	uint32_t address = sdmmc.type & SDMMC_TYPE_BLOCK ? sector : sector << 9;
	sdmmc_send_spi_command(SDMMC_CMD24, address, 0xFF, 8);
	if(sdmmc_response(0x00) != SDMMC_ERR_NONE) {
		sdmmc_spi_deselect();
		return SDMMC_ERR_READ_BLOCK_TIMEOUT;
	}

	// send token
	uint8_t tmp = 0xFE;
	sdmmc_spi_write(&tmp, 1);

	sdmmc_spi_write(data, SDMMC_SECTOR_SIZE);

	// write checksum
	tmp = 0xFF;
	sdmmc_spi_write(&tmp, 1);
	sdmmc_spi_write(&tmp, 1);

	if(sdmmc_response(0xE5) != SDMMC_ERR_NONE) {
		sdmmc_spi_deselect();
		return SDMMC_ERR_READ_BLOCK_TIMEOUT;
	}

	do {
		sdmmc_spi_read(&tmp, 1);
	} while(tmp == 0x00); // wait for write to finish

	sdmmc_spi_deselect();
	return 0;
}
