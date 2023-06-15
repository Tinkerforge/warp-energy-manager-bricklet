/* warp-energy-manager-bricklet
 * Copyright (C) 2021 Olaf Lüke <olaf@tinkerforge.com>
 *
 * main.c: Initialization for WARP Energy Manager Bricklet
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

#include <stdio.h>
#include <stdbool.h>

#include "configs/config.h"

#include "bricklib2/warp/rs485.h"
#include "bricklib2/warp/sdm.h"
#include "bricklib2/bootloader/bootloader.h"
#include "bricklib2/hal/system_timer/system_timer.h"
#include "bricklib2/logging/logging.h"
#include "communication.h"

#include "io.h"
#include "led.h"
#include "voltage.h"
#include "eeprom.h"
#include "date_time.h"
#include "sd.h"
#include "data_storage.h"

int main(void) {
	logging_init();
	logd("Start WARP Energy Manager Bricklet\n\r");

	communication_init();
	io_init();
	led_init();
	rs485_init();
	sdm_init();
	voltage_init();
	eeprom_init();
	date_time_init();
	sd_init();
	data_storage_init();

	while(true) {
		bootloader_tick();
		communication_tick();
		io_tick();
		led_tick();
		rs485_tick();
		sdm_tick();
		voltage_tick();
		date_time_tick();
		sd_tick();
		data_storage_tick();
	}
}
