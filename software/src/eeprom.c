/* hat-warp-energy-manager-brick
 * Copyright (C) 2021 Olaf Lüke <olaf@tinkerforge.com>
 *
 * eeprom.c: Save and load configuration
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

#include "eeprom.h"

#include "bricklib2/logging/logging.h"
#include "bricklib2/bootloader/bootloader.h"
#include "sdm.h"

void eeprom_load_config(void) {
	uint32_t page[EEPROM_PAGE_SIZE/sizeof(uint32_t)];
	bootloader_read_eeprom_page(EEPROM_CONFIG_PAGE, page);

	// The magic number is not where it is supposed to be.
	// This is either our first startup or something went wrong.
	// We initialize the config data with sane default values.
	if(page[EEPROM_CONFIG_MAGIC_POS] != EEPROM_CONFIG_MAGIC) {
		sdm.relative_energy.f      = 0.0f;
	} else {
		sdm.relative_energy.data   = page[EEPROM_CONFIG_REL_ENERGY_POS];
	}

	logd("Load config:\n\r");
	logd(" * rel energy %d\n\r", sdm.relative_energy.data);
}

void eeprom_save_config(void) {
	uint32_t page[EEPROM_PAGE_SIZE/sizeof(uint32_t)];

	page[EEPROM_CONFIG_MAGIC_POS]          = EEPROM_CONFIG_MAGIC;
	if(sdm.reset_energy_meter) {
		page[EEPROM_CONFIG_REL_ENERGY_POS] = sdm_register_fast.absolute_energy.data;
		sdm.relative_energy.data           = sdm_register_fast.absolute_energy.data;
	} else {
		page[EEPROM_CONFIG_REL_ENERGY_POS] = sdm.relative_energy.data;
	}

	sdm.reset_energy_meter = false;
	bootloader_write_eeprom_page(EEPROM_CONFIG_PAGE, page);
}

void eeprom_init(void) {
    eeprom_load_config();
}