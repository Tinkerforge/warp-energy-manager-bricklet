/* warp-energy-manager-bricklet
 * Copyright (C) 2023 Olaf Lüke <olaf@tinkerforge.com>
 *
 * data_storage.h: Generic persistant data storage (RAM+SD)
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

#ifndef DATA_STORAGE_H
#define DATA_STORAGE_H

#include <stdint.h>
#include <stdbool.h>

#define DATA_STORAGE_PAGES 3
#define DATA_STORAGE_SIZE  64

typedef struct {
    uint8_t storage[DATA_STORAGE_PAGES][DATA_STORAGE_SIZE];

    uint32_t last_change_time[DATA_STORAGE_PAGES];
    bool write_to_sd[DATA_STORAGE_PAGES];
    bool read_from_sd[DATA_STORAGE_PAGES];
} DataStorage;

extern DataStorage data_storage;

void data_storage_tick(void);
void data_storage_init(void);

#endif
