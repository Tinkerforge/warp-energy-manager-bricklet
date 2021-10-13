/* warp-energy-manager-bricklet
 * Copyright (C) 2021 Olaf Lüke <olaf@tinkerforge.com>
 *
 * voltage.h: Driver input voltage measurement
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

#ifndef VOLTAGE_H
#define VOLTAGE_H

#include <stdint.h>

typedef struct {
	uint16_t value;

	uint64_t value_sum;
	uint32_t value_sum_count;

	uint32_t last_time;
} Voltage;

extern Voltage voltage;

void voltage_init(void);
void voltage_tick(void);

#endif