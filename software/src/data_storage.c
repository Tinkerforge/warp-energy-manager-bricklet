/* warp-energy-manager-bricklet
 * Copyright (C) 2023 Olaf Lüke <olaf@tinkerforge.com>
 *
 * data_storage.c: Generic persistant data storage (RAM+SD)
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

#include "data_storage.h"
#include "bricklib2/hal/system_timer/system_timer.h"

#include "bricklib2/logging/logging.h"

DataStorage data_storage;

void data_storage_tick(void) {
    for(uint8_t i = 0; i < DATA_STORAGE_PAGES; i++) {
        if(data_storage.last_change_time[i] == 0) {
            continue;
        }

        // 10 minutes after data storage change we write it to SD card
        // This ensures that we write new data with a maximum frequency of 10 minutes
        if(system_timer_is_time_elapsed_ms(data_storage.last_change_time[i], 1000*60*10)) {
            data_storage.last_change_time[i] = 0;
            data_storage.write_to_sd[i]      = true;
        }
    }
}

void data_storage_init(void) {
    memset(&data_storage, 0, sizeof(DataStorage));
}