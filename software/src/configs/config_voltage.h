/* warp-energy-manager-bricklet
 * Copyright (C) 2021 Olaf Lüke <olaf@tinkerforge.com>
 *
 * config_voltage.h: Input voltage driver configurations
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

#ifndef CONFIG_VOLTAGE_H
#define CONFIG_VOLTAGE_H

#include "xmc_gpio.h"
#include "xmc_vadc.h"

#define VOLTAGE_PIN                P2_4
#define VOLTAGE_RESULT_REG         10
#define VOLTAGE_CHANNEL_NUM        6
#define VOLTAGE_CHANNEL_ALIAS      -1
#define VOLTAGE_GROUP_INDEX        1
#define VOLTAGE_GROUP              VADC_G1

#endif