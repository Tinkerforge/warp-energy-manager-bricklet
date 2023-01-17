/* warp-energy-manager-bricklet
 * Copyright (C) 2021 Olaf Lüke <olaf@tinkerforge.com>
 *
 * config_sdmmc.h: Configuration for SD MMC interface
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

#ifndef CONFIG_SDMMC_H
#define CONFIG_SDMMC_H

#include "xmc_gpio.h"
#include "xmc_spi.h"
#include "xmc_usic.h"

#define SDMMC_SPI_BAUDRATE         (20*1000*1000) // 20MHz
#define SDMMC_USIC_CHANNEL         XMC_USIC1_CH0
#define SDMMC_USIC_SPI             XMC_SPI1_CH0

#define SDMMC_SCLK_PORT            XMC_GPIO_PORT4
#define SDMMC_SCLK_PIN             6
#define SDMMC_SCLK_PIN_MODE        (XMC_GPIO_MODE_OUTPUT_PUSH_PULL_ALT7 | P4_6_AF_U1C0_SCLKOUT)

#define SDMMC_SELECT_PORT          XMC_GPIO_PORT4
#define SDMMC_SELECT_PIN           7
#define SDMMC_SELECT_PIN_MODE      XMC_GPIO_MODE_OUTPUT_PUSH_PULL // (XMC_GPIO_MODE_OUTPUT_PUSH_PULL_ALT7 | P4_7_AF_U1C0_SELO0)

#define SDMMC_MOSI_PORT            XMC_GPIO_PORT4
#define SDMMC_MOSI_PIN             5
#define SDMMC_MOSI_PIN_MODE        (XMC_GPIO_MODE_OUTPUT_PUSH_PULL_ALT6 | P4_5_AF_U1C0_DOUT0)

#define SDMMC_MISO_PORT            XMC_GPIO_PORT4
#define SDMMC_MISO_PIN             4
#define SDMMC_MISO_INPUT           XMC_USIC_CH_INPUT_DX0
#define SDMMC_MISO_SOURCE          0b010 // DX0C

#define SDMMC_CLOCK_PASSIVE_LEVEL  XMC_SPI_CH_BRG_SHIFT_CLOCK_PASSIVE_LEVEL_1_DELAY_DISABLED
#define SDMMC_CLOCK_OUTPUT         XMC_SPI_CH_BRG_SHIFT_CLOCK_OUTPUT_SCLK
#define SDMMC_SLAVE                XMC_SPI_CH_SLAVE_SELECT_0

#define SDMMC_RX_FIFO_DATA_POINTER 0
#define SDMMC_TX_FIFO_DATA_POINTER 16
#define SDMMC_RX_FIFO_SIZE         XMC_USIC_CH_FIFO_SIZE_16WORDS
#define SDMMC_TX_FIFO_SIZE         XMC_USIC_CH_FIFO_SIZE_16WORDS

#endif