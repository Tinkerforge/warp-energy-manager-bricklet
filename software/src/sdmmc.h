#ifndef SDMMC_H_
#define SDMMC_H

#include <stdint.h>
#include "bricklib2/hal/spi_fifo/spi_fifo.h"

#define SDMMC_SECTOR_SIZE 512

typedef struct {
	uint16_t pos;
	uint32_t sector;
	uint8_t  type;

	SPIFifo spi_fifo;
} SDMMC;

extern SDMMC sdmmc;

// SPI Command Set
#define SDMMC_CMD0		0x40	// software reset
#define SDMMC_CMD1		0x41	// initiate initialisation process
#define SDMMC_CMD8		0x48	// check voltage range (SDC v2 only)
#define SDMMC_CMD9		0x49	// read CSD register
#define SDMMC_CMD12		0x4C	// force stop transmission
#define SDMMC_CMD16		0x50	// set read/write block size
#define SDMMC_CMD17		0x51	// read a block
#define SDMMC_CMD18		0x52	// read multiple blocks
#define SDMMC_CMD24		0x58	// write a block
#define SDMMC_CMD25		0x59	// write multiple blocks
#define SDMMC_CMD41		0x69
#define SDMMC_CMD55		0x77	// leading command for ACMD commands
#define SDMMC_CMD58		0x7A	// read OCR
#define SDMMC_ACMD41	0xE9	// ACMDx = CMDx + 0x80
// (note: all commands are ORed with 64 [e.g. CMD16 = 16+64 = 0x50])

// SDMMC Card Type Definitions
#define SDMMC_TYPE_MMC		0x01
#define SDMMC_TYPE_SD1		0x02								// SD v1
#define SDMMC_TYPE_SD2		0x04								// SD v2
#define SDMMC_TYPE_SDC		(SDMMC_TYPE_SD1|SDMMC_TYPE_SD2)		// SD
#define SDMMC_TYPE_BLOCK	0x08								// block addressing

// Error Codes
#define SDMMC_ERR_NONE								+0
#define SDMMC_ERR_INIT								-1
#define SDMMC_ERR_READ_BLOCK_DATA_TOKEN_MISSING		-5
#define SDMMC_ERR_BAD_REPLY							-6
#define SDMMC_ERR_ACCESS_DENIED						-7
#define SDMMC_ERR_IDLE_STATE_TIMEOUT				-10
#define SDMMC_ERR_OP_COND_TIMEOUT					-11
#define SDMMC_ERR_SET_BLOCKLEN_TIMEOUT				-12
#define SDMMC_ERR_READ_BLOCK_TIMEOUT				-13
#define SDMMC_ERR_APP_CMD_TIMEOUT					-14
#define SDMMC_ERR_APP_SEND_IF_COND_TIMEOUT			-15

#define SDMMC_WAIT_MS								1
#define SDMMC_CMD8_ACCEPTED							2
#define SDMMC_CMD8_REJECTED							3

#define SDMMC_ARG_STUFF                             0x00000000
#define SDMMC_ARG_CMD8                              0x000001AA
#define SDMMC_ARG_ACMD41                            0x40000000

#define SDMMC_DATA_TOKEN_TIMEOUT                    100 // ms
#define SDMMC_RESPONSE_TIMEOUT                      100 // ms
#define SDMMC_READ_RETRY_COUNT                      10

int16_t sdmmc_response(uint8_t response);
uint8_t sdmmc_send_command(uint8_t cmd, uint32_t arg);
uint8_t sdmmc_wait_until_ready(void);
int16_t sdmmc_response_r7(void);
int16_t sdmmc_response_r3(void);
int16_t sdmmc_init(void);
void sdmmc_send_spi_command(uint8_t cmd, uint32_t arg, uint8_t crc, uint8_t read_bytes);
int8_t sdmmc_read_block(uint32_t sector, uint8_t *data);
int16_t sdmmc_write_block(uint32_t sector, const uint8_t* data);

#endif
