/* warp-energy-manager-bricklet
 * Copyright (C) 2023 Olaf Lüke <olaf@tinkerforge.com>
 *
 * lfs_config.h: Configuration for LFS
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

#ifndef LFS_CONFIG_H
#define LFS_CONFIG_H

// add coop_task include for yield in the mount function
#include "bricklib2/os/coop_task.h"

#define LFS_NO_ASSERT
#define LFS_NO_MALLOC

// LittleFS 2.6.0 changed the on-disk minor version from lfs 2.0 to 2.1.
// The first write updates an existing file system from 2.0 to 2.1.
// LittleFS 2.7.0 added the LFS_MULTIVERSION define and the disk_version config to fix this.
#define LFS_MULTIVERSION

#if 1

#define LFS_NO_DEBUG
#define LFS_NO_WARN
#define LFS_NO_ERROR
#define LFS_TRACE(...)

#else

#include "bricklib2/logging/logging.h"
#define LFS_TRACE(fmt, ...) logd("LFS trace: " fmt "\n\r", __VA_ARGS__)
#define LFS_DEBUG(fmt, ...) logd("LFS debug: " fmt "\n\r", __VA_ARGS__)
#define LFS_WARN(fmt, ...)  logd("LFS warn:  " fmt "\n\r", __VA_ARGS__)
#define LFS_ERROR(fmt, ...) logd("LFS error: " fmt "\n\r", __VA_ARGS__)

#endif

#endif
