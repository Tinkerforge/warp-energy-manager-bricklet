/* hat-warp-energy-manager-brick
 * Copyright (C) 2021 Olaf Lüke <olaf@tinkerforge.com>
 *
 * io.c: Driver for IO
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

#include "io.h"

#include "configs/config_io.h"

#include "xmc_gpio.h"

#include "bricklib2/hal/system_timer/system_timer.h"
#include "bricklib2/logging/logging.h"

IO io;

void io_init(void) {
	memset(&io, 0, sizeof(IO));
	const XMC_GPIO_CONFIG_t io_config_output = {
		.mode             = XMC_GPIO_MODE_OUTPUT_PUSH_PULL,
		.output_level     = XMC_GPIO_OUTPUT_LEVEL_HIGH,
	};

	const XMC_GPIO_CONFIG_t io_config_input = {
		.mode             = XMC_GPIO_MODE_INPUT_TRISTATE,
		.input_hysteresis = XMC_GPIO_INPUT_HYSTERESIS_LARGE,
	};

	XMC_GPIO_Init(IO_CONTACTOR_PIN, &io_config_output);
	XMC_GPIO_Init(IO_OUTPUT_PIN, &io_config_output);

	XMC_GPIO_Init(IO_INPUT0_PIN, &io_config_input);
	XMC_GPIO_Init(IO_INPUT1_PIN, &io_config_input);
}

void io_tick(void) {
	if(io.contactor) { // Active low
		XMC_GPIO_SetOutputLow(IO_CONTACTOR_PIN);
	} else {
		XMC_GPIO_SetOutputHigh(IO_CONTACTOR_PIN);
	}

	if(io.output) { // Active low
		XMC_GPIO_SetOutputLow(IO_OUTPUT_PIN);
	} else {
		XMC_GPIO_SetOutputHigh(IO_OUTPUT_PIN);
	}

	io.input[0] = XMC_GPIO_GetInput(IO_INPUT0_PIN);
	io.input[1] = XMC_GPIO_GetInput(IO_INPUT1_PIN);
}