/* warp-energy-manager-bricklet
 * Copyright (C) 2023 Olaf Lüke <olaf@tinkerforge.com>
 *
 * sd_new_file_objects.h: New file objects
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

#ifndef SD_NEW_FILE_OBJECTS_H

#include "sd.h"

extern const Wallbox5MinDataFile sd_new_file_wb_5min;
extern const Wallbox1DayDataFile sd_new_file_wb_1day;
extern const EnergyManager5MinDataFile sd_new_file_em_5min;
extern const EnergyManager1DayDataFile sd_new_file_em_1day;

#endif