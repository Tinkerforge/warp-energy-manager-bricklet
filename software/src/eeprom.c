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
#include "bricklib2/warp/meter.h"

void eeprom_load_config(void) {
	uint32_t page[EEPROM_PAGE_SIZE/sizeof(uint32_t)];
	bootloader_read_eeprom_page(EEPROM_CONFIG_PAGE, page);

	// The magic number is not where it is supposed to be.
	// This is either our first startup or something went wrong.
	// We initialize the config data with sane default values.
	if(page[EEPROM_CONFIG_MAGIC_POS] != EEPROM_CONFIG_MAGIC) {
		meter.relative_energy_sum.f       = 0.0f;
		meter.relative_energy_import.f    = 0.0f;
		meter.relative_energy_export.f    = 0.0f;
	} else {
		meter.relative_energy_sum.data    = page[EEPROM_CONFIG_REL_SUM_POS];
		meter.relative_energy_import.data = page[EEPROM_CONFIG_REL_IMPORT_POS];
		meter.relative_energy_export.data = page[EEPROM_CONFIG_REL_EXPORT_POS];
	}

	logd("Load config:\n\r");
	logd(" * rel energy %d %d %d\n\r", sdm.relative_energy_sum.data, sdm.relative_energy_import.data, sdm.relative_energy_export);
}

void eeprom_save_config(void) {
	uint32_t page[EEPROM_PAGE_SIZE/sizeof(uint32_t)];

	page[EEPROM_CONFIG_MAGIC_POS]          = EEPROM_CONFIG_MAGIC;
	if(meter.reset_energy_meter) {
		page[EEPROM_CONFIG_REL_SUM_POS]    = meter_register_set.total_kwh_sum.data;
		page[EEPROM_CONFIG_REL_IMPORT_POS] = meter_register_set.total_import_kwh.data;
		page[EEPROM_CONFIG_REL_EXPORT_POS] = meter_register_set.total_export_kwh.data;
		meter.relative_energy_sum.data     = meter_register_set.total_kwh_sum.data;
		meter.relative_energy_import.data  = meter_register_set.total_import_kwh.data;
		meter.relative_energy_export.data  = meter_register_set.total_export_kwh.data;
	} else {
		page[EEPROM_CONFIG_REL_SUM_POS]    = meter.relative_energy_sum.data;
		page[EEPROM_CONFIG_REL_IMPORT_POS] = meter.relative_energy_import.data;
		page[EEPROM_CONFIG_REL_EXPORT_POS] = meter.relative_energy_export.data;
	}

	meter.reset_energy_meter = false;
	bootloader_write_eeprom_page(EEPROM_CONFIG_PAGE, page);
}

void eeprom_init(void) {
    eeprom_load_config();
}
