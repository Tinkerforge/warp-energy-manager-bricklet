/* warp-energy-manager-bricklet
 * Copyright (C) 2022 Olaf Lüke <olaf@tinkerforge.com>
 *
 * date_time.c: Real-Time Clock and date+time handling
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

#include "date_time.h"
#include "bricklib2/logging/logging.h"

#include "xmc_rtc.h"

DateTime date_time;

void date_time_init(void) {
	const XMC_RTC_CONFIG_t rtc_config = {
		{
			.seconds = 0U,
			.minutes = 0U,
			.hours = 0U,
			.days = 0U,
			.daysofweek = XMC_RTC_WEEKDAY_THURSDAY,
			.month = XMC_RTC_MONTH_JANUARY,
			.year = 1970U
		}, {
			.seconds = 0U,
			.minutes = 1U,
			.hours = 0U,
			.days = 0U,
			.month = XMC_RTC_MONTH_JANUARY,
			.year = 1970U
		},
		.prescaler = 0x7FFFU
	};

	XMC_RTC_STATUS_t rtc_status = XMC_RTC_Init(&rtc_config);
	if(rtc_status == XMC_RTC_STATUS_OK) {
		XMC_RTC_Start();
	} else {
		loge("XMC_RTC_Init() failed, status = %d\r\n", rtc_status);
	}
}

void date_time_tick(void) {

}