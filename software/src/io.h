
/* hat-warp-energy-manager-brick
 * Copyright (C) 2021 Olaf Lüke <olaf@tinkerforge.com>
 *
 * io.h: Driver for IO 
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

#ifndef IO_H
#define IO_H

#include <stdint.h>
#include <stdbool.h>

#define IO_CONTACTOR_CHANGE_WAIT_TIME 1500 // Time we don't do contactor check in ms after contactor state change

typedef struct {
    bool contactor;
    bool output;
    bool input[2];

    uint32_t contactor_change_time;
} IO;

extern IO io;

void io_init(void);
void io_tick(void);
bool io_get_contactor_check(void);

#endif