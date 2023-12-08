/* hat-warp-energy-manager-brick
 * Copyright (C) 2021 Olaf Lüke <olaf@tinkerforge.com>
 *
 * eeprom.h: Save and load configuration
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

#ifndef EEPROM_H
#define EEPROM_H

#define EEPROM_CONFIG_PAGE              1
#define EEPROM_CONFIG_MAGIC_POS         0
#define EEPROM_CONFIG_REL_SUM_POS       1
#define EEPROM_CONFIG_REL_IMPORT_POS    2
#define EEPROM_CONFIG_REL_EXPORT_POS    3
#define EEPROM_CONFIG_MAGIC             0x34567891

void eeprom_save_config(void);
void eeprom_init(void);

#endif