/* sd-card-bricklet
 * Copyright (C) 2022 Olaf Lüke <olaf@tinkerforge.com>
 *
 * diskio.c: diskio port for SD Card Bricklet
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

// Based on auto-generated code from Infineon DAVE IDE.

#include "../ffconf.h"
#include "../diskio.h"
#include "../ff.h"

#include "xmc_common.h"
#include "xmc_rtc.h"
#include "SDMMC_BLOCK/sdmmc_block.h"

#define FATFS_VOLUMES 1

DRESULT FATFS_errorcodes[5] = {
  RES_OK ,
  RES_ERROR,
  RES_WRPRT,
  RES_NOTRDY,
  RES_PARERR
};

DSTATUS FATFS_statuscodes[4] = {
  (DSTATUS)0,
  (DSTATUS)STA_NOINIT,
  (DSTATUS)STA_NODISK,
  (DSTATUS)STA_PROTECT
};

/*
 * The function performs the disk initialization.
 */
DSTATUS disk_initialize(BYTE drive) /* Physical drive number (0..) */
{
  DSTATUS diskstatus = (DSTATUS)0;
  uint32_t status;

  /* If drive number is greater than the maximum drives allowed  */
  if (drive >= FATFS_VOLUMES)
  {
    diskstatus = (DSTATUS)((uint8_t)STA_NODISK | (uint8_t)STA_NOINIT);
  }
  else
  {
    /* Call the Initialize function. */
    status = SDMMC_BLOCK_Initialize(&SDMMC_BLOCK_0);
    /* Fatfs to Device Abstraction Layer Error Code Mapping */
    diskstatus = FATFS_statuscodes[status];
  }
  return (diskstatus);
}

/*
 * The function gets the disk status information.
 */
DSTATUS disk_status(BYTE drive)		/* Physical drive number (0..) */
{
  DSTATUS diskstatus;
  uint32_t status;
  /* If drive number is greater than the maximum drives allowed  */
  if (drive >= (uint8_t)FATFS_VOLUMES)
  {
    diskstatus = (DSTATUS)((uint8_t)STA_NODISK | (uint8_t)STA_NOINIT);
  }
  else
  {
    /* Call the Initialize function.*/
    status = SDMMC_BLOCK_GetStatus(&SDMMC_BLOCK_0);
    /* Fatfs to Block Layer Error Code Mapping */
    diskstatus = FATFS_statuscodes[status];
  }
  return (diskstatus);
}

/*
 * The function reads the blocks of data from the disk.
 */
DRESULT disk_read(
  BYTE drive,		  /* Physical drive number (0..) */
  BYTE *buffer,	      /* Data buffer to store read data */
  DWORD sectornumber, /* Sector address (LBA) */
  UINT sectorcount    /* Number of sectors to read */
)
{
  DRESULT diskresult;
  uint32_t result;
  /* If drive number is greater than the maximum drives allowed  */
  if (drive >= (uint8_t)FATFS_VOLUMES )
  {
    diskresult = RES_PARERR;
  }
  /* If sector count is less than 1. Minimum 1 sector is needed*/
  else if (sectorcount < (uint8_t)1)
  {
    diskresult = RES_PARERR;
  }
  /*Call the ReadBlkPtr function.*/
  else
  {
    result = (uint32_t)SDMMC_BLOCK_ReadBlock(&SDMMC_BLOCK_0, (uint8_t *)buffer, (uint32_t)sectornumber, sectorcount);

    /* FatFs to Device Abstraction Layer Error Code Mapping */
    diskresult = FATFS_errorcodes[result];
  }
  return (diskresult);
}

/*
 * The function writes the blocks of data on the disk.
 */
#if _USE_WRITE
DRESULT disk_write(
  BYTE drive,			/* Physical drive number (0..) */
  const BYTE *buffer,	/* Data to be written */
  DWORD sectornumber,	/* Sector address (LBA) */
  UINT sectorcount	    /* Number of sectors to write */
)
{
  DRESULT diskresult;
  uint32_t result;
  /* If drive number is greater than the maximum drives allowed  */
  if (drive >= (uint8_t)FATFS_VOLUMES)
  {
    diskresult = RES_PARERR;
  }
  /* If sector count is less than 1. Minimum 1 sector is needed*/
  else if (sectorcount < (uint8_t)1)
  {
    diskresult = RES_PARERR;
  }
  /*Call the WriteBlkPtr function.*/
  else
  {
    result = (uint32_t)SDMMC_BLOCK_WriteBlock(&SDMMC_BLOCK_0,(uint8_t *)buffer, (uint32_t)sectornumber, sectorcount);
    /* FatFs to Device Abstraction Layer Error Code Mapping */
    diskresult = FATFS_errorcodes[result];
  }
  return (diskresult);
}
#endif

/*
 * The function performs the various IOCTL operation.
 */
#if _USE_IOCTL
DRESULT disk_ioctl(
  BYTE drive,		/* Physical drive number (0..) */
  BYTE command,	    /* Control code */
  void *buffer      /* Buffer to send/receive control data */
)
{
  DRESULT diskresult;
  uint32_t result;
  if (drive >= (uint8_t)FATFS_VOLUMES)
  {
    diskresult = RES_PARERR;
  }
  /*Call the Ioctl function.*/
  else
  {
    result = SDMMC_BLOCK_Ioctl(&SDMMC_BLOCK_0,command, buffer);
    /* FatFs to Block Layer Error Code Mapping */
    diskresult = FATFS_errorcodes[result];
  }
  return (diskresult);
}
#endif

/**
 * This is a real time clock service to be called from FatFs module.
 */
DWORD get_fattime()
{
#if ((FATFS_NORTC == 0U) && (FATFS_READONLY == 0U))
  XMC_RTC_TIME_t time = {{0UL}};

  XMC_RTC_GetTime(&time);

  time.days += 1;
  time.month += 1;

  /* Pack date and time into a DWORD variable */
  return (((DWORD)(time.year - 1980UL) << 25UL) | ((DWORD)time.month << 21UL) | ((DWORD)time.days << 16UL)
          | ((DWORD)time.hours << 11UL)
          | ((DWORD)time.minutes << 5UL)
          | ((DWORD)time.seconds >> 1UL));
#endif

#if (FATFS_NORTC == 1U)
  struct tm stdtime;
  DWORD current_time;

  current_time = (((stdtime.tm_year - 1980UL) << 25UL)  | (stdtime.tm_mon << 21UL) | (stdtime.tm_mday << 16UL) |
                   (stdtime.tm_hour << 11UL) | (stdtime.tm_min << 5UL) | (stdtime.tm_sec >> 1UL));

  return (current_time);
#endif
}


