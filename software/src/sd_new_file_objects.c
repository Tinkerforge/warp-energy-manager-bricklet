/* warp-energy-manager-bricklet
 * Copyright (C) 2023 Olaf Lüke <olaf@tinkerforge.com>
 *
 * sd_new_file_objects.c: New file objects
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

#include "sd_new_file_objects.h"

const Wallbox5MinDataFile sd_new_file_wb_5min = {
    .metadata = {
        .magic   = SD_METADATA_MAGIC,
        .version = SD_METADATA_VERSION,
        .type    = SD_METADATA_TYPE_WB_5MIN,
    },
    .data = {
        [0 ... SD_5MIN_PER_DAY-1] = {
            .flags = SD_5MIN_FLAG_NO_DATA,
        },
    },
};

const Wallbox1DayDataFile sd_new_file_wb_1day = {
    .metadata = {
        .magic   = SD_METADATA_MAGIC,
        .version = SD_METADATA_VERSION,
        .type    = SD_METADATA_TYPE_WB_1DAY,
    },
    .data = {
        [0 ... SD_1DAY_PER_MONTH-1] = {
            .energy = UINT32_MAX,
        },
    },
};

const EnergyManager5MinDataFile sd_new_file_em_5min = {
    .metadata = {
        .magic   = SD_METADATA_MAGIC,
        .version = SD_METADATA_VERSION,
        .type    = SD_METADATA_TYPE_EM_5MIN,
    },
    .data = {
        [0 ... SD_5MIN_PER_DAY-1] = {
            .flags = SD_5MIN_FLAG_NO_DATA,
        },
    },
};

const EnergyManager1DayDataFile sd_new_file_em_1day = {
    .metadata = {
        .magic   = SD_METADATA_MAGIC,
        .version = SD_METADATA_VERSION,
        .type    = SD_METADATA_TYPE_EM_1DAY,
    },
    .data = {
        [0 ... SD_1DAY_PER_MONTH-1] = {
            .energy_grid_in_ = UINT32_MAX,
            .energy_grid_out = UINT32_MAX,
            .energy_general_in = {UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX},
            .energy_general_out = {UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX},
        },
    },
};