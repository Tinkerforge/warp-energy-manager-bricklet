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
#include "bricklib2/os/coop_task.h"
#include "bricklib2/utility/util_definitions.h"

#include <stdint.h>

SDMMC sdmmc;

#define SDMMC_SPI_TIMEOUT 1000

#define sdmmc_miso_irq_handler IRQ_Hdlr_11
#define sdmmc_mosi_irq_handler IRQ_Hdlr_12

uint8_t sdmmc_spi_buffer[512] = {0};
volatile uint16_t sdmmc_spi_miso_index  = 0;
volatile uint16_t sdmmc_spi_mosi_index  = 0;
volatile uint16_t sdmmc_spi_data_length = 0;

void __attribute__((optimize("-O3"))) __attribute__ ((section (".ram_code"))) sdmmc_miso_irq_handler(void) {
	const uint8_t amount = XMC_USIC_CH_RXFIFO_GetLevel(SDMMC_USIC);

	switch(amount) {
		case 16: sdmmc_spi_buffer[sdmmc_spi_miso_index++] = SDMMC_USIC->OUTR;
		case 15: sdmmc_spi_buffer[sdmmc_spi_miso_index++] = SDMMC_USIC->OUTR;
		case 14: sdmmc_spi_buffer[sdmmc_spi_miso_index++] = SDMMC_USIC->OUTR;
		case 13: sdmmc_spi_buffer[sdmmc_spi_miso_index++] = SDMMC_USIC->OUTR;
		case 12: sdmmc_spi_buffer[sdmmc_spi_miso_index++] = SDMMC_USIC->OUTR;
		case 11: sdmmc_spi_buffer[sdmmc_spi_miso_index++] = SDMMC_USIC->OUTR;
		case 10: sdmmc_spi_buffer[sdmmc_spi_miso_index++] = SDMMC_USIC->OUTR;
		case  9: sdmmc_spi_buffer[sdmmc_spi_miso_index++] = SDMMC_USIC->OUTR;
		case  8: sdmmc_spi_buffer[sdmmc_spi_miso_index++] = SDMMC_USIC->OUTR;
		case  7: sdmmc_spi_buffer[sdmmc_spi_miso_index++] = SDMMC_USIC->OUTR;
		case  6: sdmmc_spi_buffer[sdmmc_spi_miso_index++] = SDMMC_USIC->OUTR;
		case  5: sdmmc_spi_buffer[sdmmc_spi_miso_index++] = SDMMC_USIC->OUTR;
		case  4: sdmmc_spi_buffer[sdmmc_spi_miso_index++] = SDMMC_USIC->OUTR;
		case  3: sdmmc_spi_buffer[sdmmc_spi_miso_index++] = SDMMC_USIC->OUTR;
		case  2: sdmmc_spi_buffer[sdmmc_spi_miso_index++] = SDMMC_USIC->OUTR;
		case  1: sdmmc_spi_buffer[sdmmc_spi_miso_index++] = SDMMC_USIC->OUTR;
	}
}

void __attribute__((optimize("-O3"))) __attribute__ ((section (".ram_code"))) sdmmc_mosi_irq_handler(void) {
	// Use local pointer to save the time for accessing the struct
	volatile uint32_t *SDMMC_USIC_IN_PTR = SDMMC_USIC->IN;

	const uint16_t to_send    = sdmmc_spi_data_length - sdmmc_spi_mosi_index;
	const uint8_t  fifo_level = MIN(16 - XMC_USIC_CH_TXFIFO_GetLevel(SDMMC_USIC), 16 - XMC_USIC_CH_RXFIFO_GetLevel(SDMMC_USIC));
	const uint8_t  amount     = MIN(to_send, fifo_level);
	switch(amount) {
		case 16: SDMMC_USIC_IN_PTR[0] = sdmmc_spi_buffer[sdmmc_spi_mosi_index++];
		case 15: SDMMC_USIC_IN_PTR[0] = sdmmc_spi_buffer[sdmmc_spi_mosi_index++];
		case 14: SDMMC_USIC_IN_PTR[0] = sdmmc_spi_buffer[sdmmc_spi_mosi_index++];
		case 13: SDMMC_USIC_IN_PTR[0] = sdmmc_spi_buffer[sdmmc_spi_mosi_index++];
		case 12: SDMMC_USIC_IN_PTR[0] = sdmmc_spi_buffer[sdmmc_spi_mosi_index++];
		case 11: SDMMC_USIC_IN_PTR[0] = sdmmc_spi_buffer[sdmmc_spi_mosi_index++];
		case 10: SDMMC_USIC_IN_PTR[0] = sdmmc_spi_buffer[sdmmc_spi_mosi_index++];
		case  9: SDMMC_USIC_IN_PTR[0] = sdmmc_spi_buffer[sdmmc_spi_mosi_index++];
		case  8: SDMMC_USIC_IN_PTR[0] = sdmmc_spi_buffer[sdmmc_spi_mosi_index++];
		case  7: SDMMC_USIC_IN_PTR[0] = sdmmc_spi_buffer[sdmmc_spi_mosi_index++];
		case  6: SDMMC_USIC_IN_PTR[0] = sdmmc_spi_buffer[sdmmc_spi_mosi_index++];
		case  5: SDMMC_USIC_IN_PTR[0] = sdmmc_spi_buffer[sdmmc_spi_mosi_index++];
		case  4: SDMMC_USIC_IN_PTR[0] = sdmmc_spi_buffer[sdmmc_spi_mosi_index++];
		case  3: SDMMC_USIC_IN_PTR[0] = sdmmc_spi_buffer[sdmmc_spi_mosi_index++];
		case  2: SDMMC_USIC_IN_PTR[0] = sdmmc_spi_buffer[sdmmc_spi_mosi_index++];
		case  1: SDMMC_USIC_IN_PTR[0] = sdmmc_spi_buffer[sdmmc_spi_mosi_index++];
	}

	if(sdmmc_spi_mosi_index >= sdmmc_spi_data_length) {
		XMC_USIC_CH_TXFIFO_DisableEvent(SDMMC_USIC, XMC_USIC_CH_TXFIFO_EVENT_CONF_STANDARD);
	}
}

bool sdmmc_spi_transceive_async(const uint8_t *data_mosi, uint8_t *data_miso, uint32_t length) {
	bool ret = true;

	uint32_t start = system_timer_get_ms();
	if(data_mosi) {
		memcpy(sdmmc_spi_buffer, data_mosi, length);
	} else {
		memset(sdmmc_spi_buffer, 0xFF, length);
	}
	sdmmc_spi_miso_index  = 0;
	sdmmc_spi_mosi_index  = 0;
	sdmmc_spi_data_length = length;

	XMC_USIC_CH_RXFIFO_EnableEvent(SDMMC_USIC, XMC_USIC_CH_RXFIFO_EVENT_CONF_STANDARD | XMC_USIC_CH_RXFIFO_EVENT_CONF_ALTERNATE);
	XMC_USIC_CH_TXFIFO_EnableEvent(SDMMC_USIC, XMC_USIC_CH_TXFIFO_EVENT_CONF_STANDARD);
	XMC_USIC_CH_TriggerServiceRequest(SDMMC_USIC, SDMMC_SERVICE_REQUEST_TX);

	uint32_t yield_count = 0;
	while((sdmmc_spi_miso_index < sdmmc_spi_data_length) || (sdmmc_spi_mosi_index < sdmmc_spi_data_length)) {
		coop_task_yield();

		// Read rx FIFO by hand if there is 8 or less bytes left to read (the IRQ is only triggered when level goes from 8 to 9)
		if(sdmmc_spi_data_length - sdmmc_spi_miso_index <= 8) {
			XMC_USIC_CH_RXFIFO_DisableEvent(SDMMC_USIC, XMC_USIC_CH_RXFIFO_EVENT_CONF_STANDARD | XMC_USIC_CH_RXFIFO_EVENT_CONF_ALTERNATE);
			sdmmc_miso_irq_handler();
			XMC_USIC_CH_RXFIFO_EnableEvent(SDMMC_USIC, XMC_USIC_CH_RXFIFO_EVENT_CONF_STANDARD | XMC_USIC_CH_RXFIFO_EVENT_CONF_ALTERNATE);
		}
		
		// Trigger TX IRQ if the FIFO has less than 8 bytes left. If we missed on IRQ trigger it will not trigger otherwise.
		if(XMC_USIC_CH_TXFIFO_GetLevel(SDMMC_USIC) < 8) {
			XMC_USIC_CH_TriggerServiceRequest(SDMMC_USIC, SDMMC_SERVICE_REQUEST_TX);
		}

		yield_count++;
		if(system_timer_is_time_elapsed_ms(start, SDMMC_RESPONSE_TIMEOUT)) {
			logw("sdmmc_spi_transceive_async timeout miso %d, mosi %d, len %d, yield count %d\n\r", sdmmc_spi_miso_index, sdmmc_spi_mosi_index, sdmmc_spi_data_length, yield_count);
			ret = false;
			break;
		}
	}

	XMC_USIC_CH_TXFIFO_DisableEvent(SDMMC_USIC, XMC_USIC_CH_TXFIFO_EVENT_CONF_STANDARD);
	XMC_USIC_CH_RXFIFO_DisableEvent(SDMMC_USIC, XMC_USIC_CH_RXFIFO_EVENT_CONF_STANDARD | XMC_USIC_CH_RXFIFO_EVENT_CONF_ALTERNATE);

	if(data_miso) {
		memcpy(data_miso, sdmmc_spi_buffer, length);
	}

	sdmmc_spi_miso_index  = 0;
	sdmmc_spi_mosi_index  = 0;
	sdmmc_spi_data_length = 0;

	return ret;
}

bool sdmmc_spi_write(const uint8_t *data, uint32_t length) {
	XMC_USIC_CH_RXFIFO_Flush(SDMMC_USIC);
	XMC_USIC_CH_TXFIFO_Flush(SDMMC_USIC);

	if(length < 16) {
		uint16_t mosi_count  = 0;
		uint16_t miso_count  = 0;

		while(mosi_count < length) {
			SDMMC_USIC->IN[0] = data[mosi_count];
			mosi_count++;
		}

		while(miso_count < length) {
			// TODO: Timeout
			while(!XMC_USIC_CH_RXFIFO_IsEmpty(SDMMC_USIC)) {
				volatile uint8_t __attribute__((unused)) _ = SDMMC_USIC->OUTR;
				miso_count++;
			}
		}
	} else {
		bool ret = false;
		while(!ret) {
			// TODO: Timeout
			ret = sdmmc_spi_transceive_async(data, NULL, length);
		} 
	}

	// Add one yield per read/write
	coop_task_yield();
	return true;
}

bool sdmmc_spi_read(uint8_t *data, uint32_t length) {
	XMC_USIC_CH_RXFIFO_Flush(SDMMC_USIC);
	XMC_USIC_CH_TXFIFO_Flush(SDMMC_USIC);

	if(length < 16) {
		uint16_t mosi_count  = 0;
		uint16_t miso_count  = 0;

		while(mosi_count < length) {
			SDMMC_USIC->IN[0] = 0xFF;
			mosi_count++;
		}

		while(miso_count < length) {
			// TODO: Timeout
			while(!XMC_USIC_CH_RXFIFO_IsEmpty(SDMMC_USIC)) {
				data[miso_count] = SDMMC_USIC->OUTR;
				miso_count++;
			}
		}
	} else {
		bool ret = false;
		while(!ret) {
			// TODO: Timeout
			ret = sdmmc_spi_transceive_async(NULL, data, length);
		}
	}

	// Add one yield per read/write
	coop_task_yield();
	return true;
}

void sdmmc_spi_select(void) {
	XMC_GPIO_SetOutputLow(SDMMC_SELECT_PIN);
	XMC_SPI_CH_EnableSlaveSelect(SDMMC_USIC, SDMMC_SLAVE);
}

void sdmmc_spi_deselect(void) {
	XMC_GPIO_SetOutputHigh(SDMMC_SELECT_PIN);
	XMC_SPI_CH_DisableSlaveSelect(SDMMC_USIC);
}

void sdmmc_spi_deinit(void) {
	XMC_USIC_CH_Disable(SDMMC_USIC);

	// Configure SDMMC pins as input to make sure that there is no interaction
	// for the 1 second timeout until the re-initialization is done
	const XMC_GPIO_CONFIG_t input_pin_config = {
		.mode             = XMC_GPIO_MODE_INPUT_TRISTATE,
		.input_hysteresis = XMC_GPIO_INPUT_HYSTERESIS_STANDARD
	};
	XMC_GPIO_Init(SDMMC_MOSI_PIN, &input_pin_config);
	XMC_GPIO_Init(SDMMC_MISO_PIN, &input_pin_config);
	XMC_GPIO_Init(SDMMC_SELECT_PIN, &input_pin_config);
	XMC_GPIO_Init(SDMMC_SCLK_PIN, &input_pin_config);

	XMC_USIC_CH_TXFIFO_Flush(SDMMC_USIC);
	XMC_USIC_CH_RXFIFO_Flush(SDMMC_USIC);
}

void sdmmc_spi_init(void) {
	// USIC channel configuration
	const XMC_SPI_CH_CONFIG_t channel_config = {
		.baudrate       = SDMMC_SPI_BAUDRATE,
		.bus_mode       = XMC_SPI_CH_BUS_MODE_MASTER,
		.selo_inversion = XMC_SPI_CH_SLAVE_SEL_INV_TO_MSLS,
		.parity_mode    = XMC_USIC_CH_PARITY_MODE_NONE
	};

	// MOSI pin configuration
	const XMC_GPIO_CONFIG_t mosi_pin_config = {
		.mode             = SDMMC_MOSI_PIN_AF,
		.output_level     = XMC_GPIO_OUTPUT_LEVEL_HIGH
	};

	// MISO pin configuration
	const XMC_GPIO_CONFIG_t miso_pin_config = {
		.mode             = XMC_GPIO_MODE_INPUT_TRISTATE,
		.input_hysteresis = XMC_GPIO_INPUT_HYSTERESIS_STANDARD
	};

	// SCLK pin configuration
	const XMC_GPIO_CONFIG_t sclk_pin_config = {
		.mode             = SDMMC_SCLK_PIN_AF,
		.output_level     = XMC_GPIO_OUTPUT_LEVEL_HIGH
	};

	// SELECT pin configuration
	const XMC_GPIO_CONFIG_t select_pin_config = {
		.mode             = SDMMC_SELECT_PIN_AF,
		.output_level     = XMC_GPIO_OUTPUT_LEVEL_HIGH
	};

	// Configure MISO pin
	XMC_GPIO_Init(SDMMC_MISO_PIN, &miso_pin_config);

	// Initialize USIC channel in SPI master mode
	XMC_SPI_CH_Init(SDMMC_USIC, &channel_config);
	SDMMC_USIC->SCTR &= ~USIC_CH_SCTR_PDL_Msk; // Set passive data level to 0
	//SDMMC_USIC->PCR_SSCMode &= ~USIC_CH_PCR_SSCMode_TIWEN_Msk; // Disable time between bytes

	XMC_SPI_CH_SetBitOrderMsbFirst(SDMMC_USIC);

	XMC_SPI_CH_SetWordLength(SDMMC_USIC, (uint8_t)8U);
	XMC_SPI_CH_SetFrameLength(SDMMC_USIC, (uint8_t)64U);

	XMC_SPI_CH_SetTransmitMode(SDMMC_USIC, XMC_SPI_CH_MODE_STANDARD);

	// Configure the clock polarity and clock delay
	XMC_SPI_CH_ConfigureShiftClockOutput(SDMMC_USIC, SDMMC_CLOCK_PASSIVE_LEVEL, SDMMC_CLOCK_OUTPUT);
	XMC_SPI_CH_SetSlaveSelectDelay(SDMMC_USIC, 2);

	// Set input source path
	XMC_SPI_CH_SetInputSource(SDMMC_USIC, SDMMC_MISO_INPUT, SDMMC_MISO_SOURCE);

	// SPI Mode: CPOL=1 and CPHA=1
	((USIC_CH_TypeDef *)SDMMC_USIC)->DX1CR |= USIC_CH_DX1CR_DPOL_Msk;

	// Configure transmit FIFO
	XMC_USIC_CH_TXFIFO_Configure(SDMMC_USIC, SDMMC_TX_FIFO_DATA_POINTER, SDMMC_TX_FIFO_SIZE, 8);

	// Configure receive FIFO
	XMC_USIC_CH_RXFIFO_Configure(SDMMC_USIC, SDMMC_RX_FIFO_DATA_POINTER, SDMMC_RX_FIFO_SIZE, 8);

	// Set service request for tx FIFO transmit interrupt
	XMC_USIC_CH_TXFIFO_SetInterruptNodePointer(SDMMC_USIC, XMC_USIC_CH_TXFIFO_INTERRUPT_NODE_POINTER_STANDARD, SDMMC_SERVICE_REQUEST_TX);  // IRQ SDMMC_IRQ_TX

	// Set service request for rx FIFO receive interrupt
	XMC_USIC_CH_RXFIFO_SetInterruptNodePointer(SDMMC_USIC, XMC_USIC_CH_RXFIFO_INTERRUPT_NODE_POINTER_STANDARD, SDMMC_SERVICE_REQUEST_RX);  // IRQ SDMMC_IRQ_RX
	XMC_USIC_CH_RXFIFO_SetInterruptNodePointer(SDMMC_USIC, XMC_USIC_CH_RXFIFO_INTERRUPT_NODE_POINTER_ALTERNATE, SDMMC_SERVICE_REQUEST_RX); // IRQ SDMMC_IRQ_RX

	//Set priority and enable NVIC node for transmit interrupt
	NVIC_SetPriority((IRQn_Type)SDMMC_IRQ_TX, SDMMC_IRQ_TX_PRIORITY);
	XMC_SCU_SetInterruptControl(SDMMC_IRQ_TX, SDMMC_IRQCTRL_TX);
	NVIC_EnableIRQ((IRQn_Type)SDMMC_IRQ_TX);

	// Set priority and enable NVIC node for receive interrupt
	NVIC_SetPriority((IRQn_Type)SDMMC_IRQ_RX, SDMMC_IRQ_RX_PRIORITY);
	XMC_SCU_SetInterruptControl(SDMMC_IRQ_RX, SDMMC_IRQCTRL_RX);
	NVIC_EnableIRQ((IRQn_Type)SDMMC_IRQ_RX);

	// Start SPI
	XMC_SPI_CH_Start(SDMMC_USIC);

	// Configure SCLK pin
	XMC_GPIO_Init(SDMMC_SCLK_PIN, &sclk_pin_config);

	// Configure slave select pin
	XMC_GPIO_Init(SDMMC_SELECT_PIN, &select_pin_config);

	// Configure MOSI pin
	XMC_GPIO_Init(SDMMC_MOSI_PIN, &mosi_pin_config);

	XMC_USIC_CH_TXFIFO_Flush(SDMMC_USIC);
	XMC_USIC_CH_RXFIFO_Flush(SDMMC_USIC);
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

		if(result != response) {
			coop_task_yield();
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
		if(data != 0xFF) {
			coop_task_yield();
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
	XMC_USIC_CH_SetBaudrate(SDMMC_USIC, SDMMC_BLOCK_SPI_INIT_SPEED, 2); 

	uint8_t buffer[10];

	// allowing 80 clock cycles for initialisation
	sdmmc_spi_deselect();
	sdmmc_spi_read(buffer, 10);
	coop_task_yield();

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

	// Reinitialize SPI to with high speed configured with SDMMC_SPI_BAUDRATE (initialization was done with 300kHz)
	sdmmc_spi_init();

	if(sdmmc.type == 0) {
		logd("Probably no SD card?\n\r");
		sdmmc_spi_deinit();
		return sdmmc_error;
	}

	coop_task_yield();
	sdmmc_error = sdmmc_init_csd();
	if(sdmmc_error != SDMMC_ERROR_OK) {
		sdmmc_spi_deinit();
		return sdmmc_error;
	}

	coop_task_yield();
	sdmmc_error = sdmmc_init_cid();
	if(sdmmc_error != SDMMC_ERROR_OK) {
		sdmmc_spi_deinit();
		return sdmmc_error;
	}
	// TODO: Read SCR here.

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
