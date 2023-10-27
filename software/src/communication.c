/* warp-energy-manager-bricklet
 * Copyright (C) 2021 Olaf Lüke <olaf@tinkerforge.com>
 *
 * communication.c: TFP protocol message handling
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

#include "communication.h"

#include "bricklib2/utility/communication_callback.h"
#include "bricklib2/protocols/tfp/tfp.h"
#include "bricklib2/logging/logging.h"
#include "bricklib2/utility/util_definitions.h"
#include "bricklib2/warp/meter.h"
#include "bricklib2/warp/rs485.h"

#include "io.h"
#include "led.h"
#include "voltage.h"
#include "eeprom.h"
#include "sd.h"
#include "sdmmc.h"
#include "data_storage.h"

#include "xmc_rtc.h"

// SD lfs format bool is outside of struct to avoid it being overwritten during re-init of SD card
extern bool sd_lfs_format;

static uint8_t get_sd_lfs_status(const uint8_t end, const uint8_t max_length) {
	if(sd.sd_status != SDMMC_ERROR_OK) {
		return WARP_ENERGY_MANAGER_DATA_STATUS_SD_ERROR;
	}

	if(sd.lfs_status != LFS_ERR_OK) {
		return WARP_ENERGY_MANAGER_DATA_STATUS_LFS_ERROR;
	}

	if(end >= max_length) {
		return WARP_ENERGY_MANAGER_DATA_STATUS_QUEUE_FULL;
	}

	return WARP_ENERGY_MANAGER_DATA_STATUS_OK;
}

static uint8_t get_date_status(uint8_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute) {
	// Year: Accept all years

	// Month: 1-12
	if((month < 1) || (month > 12)) {
		return WARP_ENERGY_MANAGER_DATA_STATUS_DATE_OUT_OF_RANGE;
	}

	// Day: 1-31
	if((day < 1) || (day > 31)) {
		return WARP_ENERGY_MANAGER_DATA_STATUS_DATE_OUT_OF_RANGE;
	}

	// Hour: 0-23
	if(hour > 23) {
		return WARP_ENERGY_MANAGER_DATA_STATUS_DATE_OUT_OF_RANGE;
	}

	// Minute: 0-55, 5 minute steps
	if(((minute % 5) != 0) || (minute > 55)) {
		return WARP_ENERGY_MANAGER_DATA_STATUS_DATE_OUT_OF_RANGE;
	}

	return WARP_ENERGY_MANAGER_DATA_STATUS_OK;
}

static uint8_t get_amount_5min_status(uint8_t hour, uint8_t minute, uint16_t amount) {
	if(((((60-minute) / 5) + 12*(24-hour-1)) - amount) < 0) {
		return WARP_ENERGY_MANAGER_DATA_STATUS_DATE_OUT_OF_RANGE;
	}

	return WARP_ENERGY_MANAGER_DATA_STATUS_OK;
}

static uint8_t get_amount_daily_status(uint8_t day, uint16_t amount) {
	if((day-1 + amount) > 31) {
		return WARP_ENERGY_MANAGER_DATA_STATUS_DATE_OUT_OF_RANGE;
	}

	return WARP_ENERGY_MANAGER_DATA_STATUS_OK;
}


BootloaderHandleMessageResponse handle_message(const void *message, void *response) {
	led.connection_lost_time = system_timer_get_ms(); // Reset connection lost time with each message

	switch(tfp_get_fid_from_message(message)) {
		case FID_SET_CONTACTOR: return set_contactor(message);
		case FID_GET_CONTACTOR: return get_contactor(message, response);
		case FID_SET_RGB_VALUE: return set_rgb_value(message);
		case FID_GET_RGB_VALUE: return get_rgb_value(message, response);
		case FID_GET_ENERGY_METER_VALUES: return get_energy_meter_values(message, response);
		case FID_GET_ENERGY_METER_DETAILED_VALUES_LOW_LEVEL: return get_energy_meter_detailed_values_low_level(message, response);
		case FID_GET_ENERGY_METER_STATE: return get_energy_meter_state(message, response);
		case FID_GET_INPUT: return get_input(message, response);
		case FID_SET_OUTPUT: return set_output(message);
		case FID_GET_OUTPUT: return get_output(message, response);
		case FID_GET_INPUT_VOLTAGE: return get_input_voltage(message, response);
		case FID_GET_STATE: return get_state(message, response);
		case FID_GET_UPTIME: return get_uptime(message, response);
		case FID_GET_ALL_DATA_1: return get_all_data_1(message, response);
		case FID_GET_SD_INFORMATION: return get_sd_information(message, response);
		case FID_SET_SD_WALLBOX_DATA_POINT: return set_sd_wallbox_data_point(message, response);
		case FID_GET_SD_WALLBOX_DATA_POINTS: return get_sd_wallbox_data_points(message, response);
		case FID_SET_SD_WALLBOX_DAILY_DATA_POINT: return set_sd_wallbox_daily_data_point(message, response);
		case FID_GET_SD_WALLBOX_DAILY_DATA_POINTS: return get_sd_wallbox_daily_data_points(message, response);
		case FID_SET_SD_ENERGY_MANAGER_DATA_POINT: return set_sd_energy_manager_data_point(message, response);
		case FID_GET_SD_ENERGY_MANAGER_DATA_POINTS: return get_sd_energy_manager_data_points(message, response);
		case FID_SET_SD_ENERGY_MANAGER_DAILY_DATA_POINT: return set_sd_energy_manager_daily_data_point(message, response);
		case FID_GET_SD_ENERGY_MANAGER_DAILY_DATA_POINTS: return get_sd_energy_manager_daily_data_points(message, response);
		case FID_FORMAT_SD: return format_sd(message, response);
		case FID_SET_DATE_TIME: return set_date_time(message);
		case FID_GET_DATE_TIME: return get_date_time(message, response);
		case FID_SET_LED_STATE: return set_led_state(message);
		case FID_GET_LED_STATE: return get_led_state(message, response);
		case FID_GET_DATA_STORAGE: return get_data_storage(message, response);
		case FID_SET_DATA_STORAGE: return set_data_storage(message);
		default: return HANDLE_MESSAGE_RESPONSE_NOT_SUPPORTED;
	}
}


BootloaderHandleMessageResponse set_contactor(const SetContactor *data) {
	io.contactor = data->contactor_value;

	return HANDLE_MESSAGE_RESPONSE_EMPTY;
}

BootloaderHandleMessageResponse get_contactor(const GetContactor *data, GetContactor_Response *response) {
	response->header.length   = sizeof(GetContactor_Response);
	response->contactor_value = io.contactor;

	return HANDLE_MESSAGE_RESPONSE_NEW_MESSAGE;
}

BootloaderHandleMessageResponse set_rgb_value(const SetRGBValue *data) {
	led.r = data->r;
	led.g = data->g;
	led.b = data->b;

	led.use_rgb = true;

	return HANDLE_MESSAGE_RESPONSE_EMPTY;
}

BootloaderHandleMessageResponse get_rgb_value(const GetRGBValue *data, GetRGBValue_Response *response) {
	response->header.length = sizeof(GetRGBValue_Response);
	response->r             = led.r;
	response->g             = led.g;
	response->b             = led.b;

	return HANDLE_MESSAGE_RESPONSE_NEW_MESSAGE;
}

BootloaderHandleMessageResponse set_led_state(const SetLEDState *data) {
	if((led.pattern > WARP_ENERGY_MANAGER_LED_PATTERN_BREATHING) || (led.hue > 359)) {
		return HANDLE_MESSAGE_RESPONSE_INVALID_PARAMETER;
	}

	led.pattern = data->pattern;
	led.hue     = data->hue;

	led.use_rgb = false;

	return HANDLE_MESSAGE_RESPONSE_EMPTY;
}

BootloaderHandleMessageResponse get_led_state(const GetLEDState *data, GetLEDState_Response *response) {
	response->header.length = sizeof(GetLEDState_Response);
	response->hue           = led.hue;
	response->pattern       = led.pattern;

	return HANDLE_MESSAGE_RESPONSE_NEW_MESSAGE;
}

BootloaderHandleMessageResponse get_energy_meter_values(const GetEnergyMeterValues *data, GetEnergyMeterValues_Response *response) {
	response->header.length = sizeof(GetEnergyMeterValues_Response);
	response->power         = sdm_register_fast.power.f;
	response->energy_import = sdm_register_fast.energy_import.f;
	response->energy_export = sdm_register_fast.energy_export.f;
	
	return HANDLE_MESSAGE_RESPONSE_NEW_MESSAGE;
}


BootloaderHandleMessageResponse get_energy_meter_detailed_values_low_level(const GetEnergyMeterDetailedValuesLowLevel *data, GetEnergyMeterDetailedValuesLowLevel_Response *response) {

	static uint32_t packet_payload_index = 0;

	response->header.length = sizeof(GetEnergyMeterDetailedValuesLowLevel_Response);

	const uint8_t packet_length = 60;
	const uint16_t max_end = 85*sizeof(float);
	const uint16_t start = packet_payload_index * packet_length;
	const uint16_t end = MIN(start + packet_length, max_end);
	const uint16_t copy_num = end-start;
	uint8_t *copy_from = (uint8_t*)&meter_register_set;

	response->values_chunk_offset = start/4;
	memcpy(response->values_chunk_data, &copy_from[start], copy_num);

	if(end < max_end) {
		packet_payload_index++;
	} else {
		packet_payload_index = 0;
	}

	return HANDLE_MESSAGE_RESPONSE_NEW_MESSAGE;
}

BootloaderHandleMessageResponse get_energy_meter_state(const GetEnergyMeterState *data, GetEnergyMeterState_Response *response) {
	response->header.length     = sizeof(GetEnergyMeterState_Response);

	if(!meter.each_value_read_once) {
		response->energy_meter_type = WARP_ENERGY_MANAGER_ENERGY_METER_TYPE_NOT_AVAILABLE;
	} else {
		switch(meter.type) {
			case METER_TYPE_UNKNOWN:     response->energy_meter_type = WARP_ENERGY_MANAGER_ENERGY_METER_TYPE_NOT_AVAILABLE; break;
			case METER_TYPE_UNSUPPORTED: response->energy_meter_type = WARP_ENERGY_MANAGER_ENERGY_METER_TYPE_NOT_AVAILABLE; break;
			case METER_TYPE_SDM630:      response->energy_meter_type = WARP_ENERGY_MANAGER_ENERGY_METER_TYPE_SDM630; break;
			case METER_TYPE_SDM72V2:     response->energy_meter_type = WARP_ENERGY_MANAGER_ENERGY_METER_TYPE_SDM72V2; break;
			case METER_TYPE_SDM72CTM:    response->energy_meter_type = WARP_ENERGY_MANAGER_ENERGY_METER_TYPE_SDM72CTM; break;
			case METER_TYPE_SDM630MCTV2: response->energy_meter_type = WARP_ENERGY_MANAGER_ENERGY_METER_TYPE_SDM630MCTV2; break;
			case METER_TYPE_DSZ15DZMOD:  response->energy_meter_type = WARP_ENERGY_MANAGER_ENERGY_METER_TYPE_DSZ15DZMOD; break;
			default:                     response->energy_meter_type = WARP_ENERGY_MANAGER_ENERGY_METER_TYPE_NOT_AVAILABLE; break;
		}
	}

	response->error_count[0]    = rs485.modbus_common_error_counters.timeout;
	response->error_count[1]    = 0; // Global timeout. Currently global timeout triggers watchdog and EVSE will restart, so this will always be 0.
	response->error_count[2]    = rs485.modbus_common_error_counters.illegal_function;
	response->error_count[3]    = rs485.modbus_common_error_counters.illegal_data_address;
	response->error_count[4]    = rs485.modbus_common_error_counters.illegal_data_value;
	response->error_count[5]    = rs485.modbus_common_error_counters.slave_device_failure;

	return HANDLE_MESSAGE_RESPONSE_NEW_MESSAGE;
}

BootloaderHandleMessageResponse get_input(const GetInput *data, GetInput_Response *response) {
	response->header.length = sizeof(GetInput_Response);
	response->input[0]      = (io.input[0] << 0) | (io.input[1] << 1);

	return HANDLE_MESSAGE_RESPONSE_NEW_MESSAGE;
}

BootloaderHandleMessageResponse set_output(const SetOutput *data) {
	io.output = data->output;

	return HANDLE_MESSAGE_RESPONSE_EMPTY;
}

BootloaderHandleMessageResponse get_output(const GetOutput *data, GetOutput_Response *response) {
	response->header.length = sizeof(GetOutput_Response);
	response->output        = io.output;

	return HANDLE_MESSAGE_RESPONSE_NEW_MESSAGE;
}

BootloaderHandleMessageResponse get_input_voltage(const GetInputVoltage *data, GetInputVoltage_Response *response) {
	response->header.length = sizeof(GetInputVoltage_Response);
	response->voltage       = voltage.value;

	return HANDLE_MESSAGE_RESPONSE_NEW_MESSAGE;
}

BootloaderHandleMessageResponse get_state(const GetState *data, GetState_Response *response) {
	response->header.length         = sizeof(GetState_Response);
	response->contactor_check_state = io_get_contactor_check();

	return HANDLE_MESSAGE_RESPONSE_NEW_MESSAGE;
}

BootloaderHandleMessageResponse get_uptime(const GetUptime *data, GetUptime_Response *response) {
	response->header.length = sizeof(GetUptime_Response);
	response->uptime        = system_timer_get_ms();

	return HANDLE_MESSAGE_RESPONSE_NEW_MESSAGE;
}

BootloaderHandleMessageResponse get_all_data_1(const GetAllData1 *data, GetAllData1_Response *response) {
	response->header.length = sizeof(GetAllData1_Response);
	TFPMessageFull parts;

	get_contactor(NULL, (GetContactor_Response*)&parts);
	memcpy(&response->contactor_value, parts.data, sizeof(GetContactor_Response) - sizeof(TFPMessageHeader));

	get_rgb_value(NULL, (GetRGBValue_Response*)&parts);
	memcpy(&response->r, parts.data, sizeof(GetRGBValue_Response) - sizeof(TFPMessageHeader));

	get_energy_meter_values(NULL, (GetEnergyMeterValues_Response*)&parts);
	memcpy(&response->power, parts.data, sizeof(GetEnergyMeterValues_Response) - sizeof(TFPMessageHeader));

	get_energy_meter_state(NULL, (GetEnergyMeterState_Response*)&parts);
	memcpy(&response->energy_meter_type, parts.data, sizeof(GetEnergyMeterState_Response) - sizeof(TFPMessageHeader));

	get_input(NULL, (GetInput_Response*)&parts);
	memcpy(&response->input, parts.data, sizeof(GetInput_Response) - sizeof(TFPMessageHeader));

	get_output(NULL, (GetOutput_Response*)&parts);
	memcpy(&response->output, parts.data, sizeof(GetOutput_Response) - sizeof(TFPMessageHeader));

	get_input_voltage(NULL, (GetInputVoltage_Response*)&parts);
	memcpy(&response->voltage, parts.data, sizeof(GetInputVoltage_Response) - sizeof(TFPMessageHeader));

	get_state(NULL, (GetState_Response*)&parts);
	memcpy(&response->contactor_check_state, parts.data, sizeof(GetState_Response) - sizeof(TFPMessageHeader));

	get_uptime(NULL, (GetUptime_Response*)&parts);
	memcpy(&response->uptime, parts.data, sizeof(GetUptime_Response) - sizeof(TFPMessageHeader));

	return HANDLE_MESSAGE_RESPONSE_NEW_MESSAGE;
}

BootloaderHandleMessageResponse get_sd_information(const GetSDInformation *data, GetSDInformation_Response *response) {
	response->header.length   = sizeof(GetSDInformation_Response);
	response->sd_status       = sd.sd_status;
	response->lfs_status      = sd.lfs_status;
	response->sector_size     = sd.sector_size;
	response->sector_count    = sd.sector_count;
	response->card_type       = sd.card_type;
	response->product_rev     = sd.product_rev;
	response->manufacturer_id = sd.manufacturer_id;
	memcpy(response->product_name, sd.product_name, 5);

	return HANDLE_MESSAGE_RESPONSE_NEW_MESSAGE;
}

BootloaderHandleMessageResponse set_sd_wallbox_data_point(const SetSDWallboxDataPoint *data, SetSDWallboxDataPoint_Response *response) {
	response->header.length = sizeof(SetSDWallboxDataPoint_Response);
	response->status        = get_sd_lfs_status(sd.wallbox_data_point_end, SD_WALLBOX_DATA_POINT_LENGTH);
	if(response->status != WARP_ENERGY_MANAGER_DATA_STATUS_OK) {
		return HANDLE_MESSAGE_RESPONSE_NEW_MESSAGE;
	}
	response->status        = get_date_status(data->year, data->month, data->day, data->hour, data->minute);
	if(response->status != WARP_ENERGY_MANAGER_DATA_STATUS_OK) {
		return HANDLE_MESSAGE_RESPONSE_NEW_MESSAGE;
	}

	memcpy(&sd.wallbox_data_point[sd.wallbox_data_point_end], &data->wallbox_id, sizeof(SetSDWallboxDataPoint) - sizeof(TFPMessageHeader));
	sd.wallbox_data_point_end++;

	return HANDLE_MESSAGE_RESPONSE_NEW_MESSAGE;
}

BootloaderHandleMessageResponse get_sd_wallbox_data_points(const GetSDWallboxDataPoints *data, GetSDWallboxDataPoints_Response *response) {
	response->header.length = sizeof(GetSDWallboxDataPoints_Response);
	response->status        = get_sd_lfs_status(sd.new_sd_wallbox_data_points ? 1: 0, 1);
	if(response->status != WARP_ENERGY_MANAGER_DATA_STATUS_OK) {
		return HANDLE_MESSAGE_RESPONSE_NEW_MESSAGE;
	}
	response->status        = get_date_status(data->year, data->month, data->day, data->hour, data->minute);
	if(response->status != WARP_ENERGY_MANAGER_DATA_STATUS_OK) {
		return HANDLE_MESSAGE_RESPONSE_NEW_MESSAGE;
	}
	response->status        = get_amount_5min_status(data->hour, data->minute, data->amount);
	if(response->status != WARP_ENERGY_MANAGER_DATA_STATUS_OK) {
		return HANDLE_MESSAGE_RESPONSE_NEW_MESSAGE;
	}

	sd.get_sd_wallbox_data_points = *data;
	sd.new_sd_wallbox_data_points = true;

	return HANDLE_MESSAGE_RESPONSE_NEW_MESSAGE;
}

BootloaderHandleMessageResponse set_sd_wallbox_daily_data_point(const SetSDWallboxDailyDataPoint *data, SetSDWallboxDailyDataPoint_Response *response) {
	response->header.length = sizeof(SetSDWallboxDailyDataPoint_Response);
	response->status        = get_sd_lfs_status(sd.wallbox_daily_data_point_end, SD_WALLBOX_DAILY_DATA_POINT_LENGTH);
	if(response->status != WARP_ENERGY_MANAGER_DATA_STATUS_OK) {
		return HANDLE_MESSAGE_RESPONSE_NEW_MESSAGE;
	}
	response->status        = get_date_status(data->year, data->month, data->day, 0, 0);
	if(response->status != WARP_ENERGY_MANAGER_DATA_STATUS_OK) {
		return HANDLE_MESSAGE_RESPONSE_NEW_MESSAGE;
	}

	memcpy(&sd.wallbox_daily_data_point[sd.wallbox_daily_data_point_end], &data->wallbox_id, sizeof(SetSDWallboxDailyDataPoint) - sizeof(TFPMessageHeader));
	sd.wallbox_daily_data_point_end++;

	return HANDLE_MESSAGE_RESPONSE_NEW_MESSAGE;
}

BootloaderHandleMessageResponse get_sd_wallbox_daily_data_points(const GetSDWallboxDailyDataPoints *data, GetSDWallboxDailyDataPoints_Response *response) {
	response->header.length = sizeof(GetSDWallboxDailyDataPoints_Response);
	response->status        = get_sd_lfs_status(sd.new_sd_wallbox_daily_data_points ? 1: 0, 1);
	if(response->status != WARP_ENERGY_MANAGER_DATA_STATUS_OK) {
		return HANDLE_MESSAGE_RESPONSE_NEW_MESSAGE;
	}
	response->status        = get_date_status(data->year, data->month, data->day, 0, 0);
	if(response->status != WARP_ENERGY_MANAGER_DATA_STATUS_OK) {
		return HANDLE_MESSAGE_RESPONSE_NEW_MESSAGE;
	}
	response->status        = get_amount_daily_status(data->day, data->amount);
	if(response->status != WARP_ENERGY_MANAGER_DATA_STATUS_OK) {
		return HANDLE_MESSAGE_RESPONSE_NEW_MESSAGE;
	}

	sd.get_sd_wallbox_daily_data_points = *data;
	sd.new_sd_wallbox_daily_data_points = true;

	return HANDLE_MESSAGE_RESPONSE_NEW_MESSAGE;
}

BootloaderHandleMessageResponse set_sd_energy_manager_data_point(const SetSDEnergyManagerDataPoint *data, SetSDEnergyManagerDataPoint_Response *response) {
	response->header.length = sizeof(SetSDEnergyManagerDataPoint_Response);
	response->status        = get_sd_lfs_status(sd.energy_manager_data_point_end, SD_ENERGY_MANAGER_DATA_POINT_LENGTH);
	if(response->status != WARP_ENERGY_MANAGER_DATA_STATUS_OK) {
		return HANDLE_MESSAGE_RESPONSE_NEW_MESSAGE;
	}
	response->status        = get_date_status(data->year, data->month, data->day, data->hour, data->minute);
	if(response->status != WARP_ENERGY_MANAGER_DATA_STATUS_OK) {
		return HANDLE_MESSAGE_RESPONSE_NEW_MESSAGE;
	}

	memcpy(&sd.energy_manager_data_point[sd.energy_manager_data_point_end], &data->year, sizeof(SetSDEnergyManagerDataPoint) - sizeof(TFPMessageHeader));
	sd.energy_manager_data_point_end++;

	return HANDLE_MESSAGE_RESPONSE_NEW_MESSAGE;
}

BootloaderHandleMessageResponse get_sd_energy_manager_data_points(const GetSDEnergyManagerDataPoints *data, GetSDEnergyManagerDataPoints_Response *response) {
	response->header.length = sizeof(GetSDEnergyManagerDataPoints_Response);
	response->status        = get_sd_lfs_status(sd.new_sd_energy_manager_data_points ? 1: 0, 1);
	if(response->status != WARP_ENERGY_MANAGER_DATA_STATUS_OK) {
		return HANDLE_MESSAGE_RESPONSE_NEW_MESSAGE;
	}
	response->status        = get_date_status(data->year, data->month, data->day, data->hour, data->minute);
	if(response->status != WARP_ENERGY_MANAGER_DATA_STATUS_OK) {
		return HANDLE_MESSAGE_RESPONSE_NEW_MESSAGE;
	}
	response->status        = get_amount_5min_status(data->hour, data->minute, data->amount);
	if(response->status != WARP_ENERGY_MANAGER_DATA_STATUS_OK) {
		return HANDLE_MESSAGE_RESPONSE_NEW_MESSAGE;
	}

	sd.get_sd_energy_manager_data_points = *data;
	sd.new_sd_energy_manager_data_points = true;

	return HANDLE_MESSAGE_RESPONSE_NEW_MESSAGE;
}

BootloaderHandleMessageResponse set_sd_energy_manager_daily_data_point(const SetSDEnergyManagerDailyDataPoint *data, SetSDEnergyManagerDailyDataPoint_Response *response) {
	response->header.length = sizeof(SetSDEnergyManagerDailyDataPoint_Response);
	response->status        = get_sd_lfs_status(sd.energy_manager_daily_data_point_end, SD_ENERGY_MANAGER_DAILY_DATA_POINT_LENGTH);
	if(response->status != WARP_ENERGY_MANAGER_DATA_STATUS_OK) {
		return HANDLE_MESSAGE_RESPONSE_NEW_MESSAGE;
	}
	response->status        = get_date_status(data->year, data->month, data->day, 0, 0);
	if(response->status != WARP_ENERGY_MANAGER_DATA_STATUS_OK) {
		return HANDLE_MESSAGE_RESPONSE_NEW_MESSAGE;
	}

	memcpy(&sd.energy_manager_daily_data_point[sd.energy_manager_daily_data_point_end], &data->year, sizeof(SetSDEnergyManagerDailyDataPoint) - sizeof(TFPMessageHeader));
	sd.energy_manager_daily_data_point_end++;

	return HANDLE_MESSAGE_RESPONSE_NEW_MESSAGE;
}

BootloaderHandleMessageResponse get_sd_energy_manager_daily_data_points(const GetSDEnergyManagerDailyDataPoints *data, GetSDEnergyManagerDailyDataPoints_Response *response) {
	response->header.length = sizeof(GetSDEnergyManagerDailyDataPoints_Response);
	response->status        = get_sd_lfs_status(sd.new_sd_energy_manager_daily_data_points ? 1: 0, 1);
	if(response->status != WARP_ENERGY_MANAGER_DATA_STATUS_OK) {
		return HANDLE_MESSAGE_RESPONSE_NEW_MESSAGE;
	}
	response->status        = get_date_status(data->year, data->month, data->day, 0, 0);
	if(response->status != WARP_ENERGY_MANAGER_DATA_STATUS_OK) {
		return HANDLE_MESSAGE_RESPONSE_NEW_MESSAGE;
	}
	response->status        = get_amount_daily_status(data->day, data->amount);
	if(response->status != WARP_ENERGY_MANAGER_DATA_STATUS_OK) {
		return HANDLE_MESSAGE_RESPONSE_NEW_MESSAGE;
	}

	sd.get_sd_energy_manager_daily_data_points = *data;
	sd.new_sd_energy_manager_daily_data_points = true;

	return HANDLE_MESSAGE_RESPONSE_NEW_MESSAGE;
}

BootloaderHandleMessageResponse format_sd(const FormatSD *data, FormatSD_Response *response) {
	response->header.length = sizeof(FormatSD_Response);
	if(data->password != 0x4223ABCD) {
		response->format_status = WARP_ENERGY_MANAGER_FORMAT_STATUS_PASSWORD_ERROR;
	} else {
		sd_lfs_format = true;
		response->format_status = WARP_ENERGY_MANAGER_FORMAT_STATUS_OK;
	}

	return HANDLE_MESSAGE_RESPONSE_NEW_MESSAGE;
}

BootloaderHandleMessageResponse set_date_time(const SetDateTime *data) {
	XMC_RTC_TIME_t rtc_time = {
		.seconds    = data->seconds,
		.minutes    = data->minutes,
		.hours      = data->hours,
		.days       = data->days,
		.daysofweek = data->days_of_week,
		.month      = data->month,
		.year       = data->year,
	};
	XMC_RTC_SetTime(&rtc_time);

	return HANDLE_MESSAGE_RESPONSE_EMPTY;
}

BootloaderHandleMessageResponse get_date_time(const GetDateTime *data, GetDateTime_Response *response) {
	XMC_RTC_TIME_t rtc_time;
	XMC_RTC_GetTime(&rtc_time);

	response->header.length = sizeof(GetDateTime_Response);
	response->seconds       = rtc_time.seconds;
	response->minutes       = rtc_time.minutes;
	response->hours         = rtc_time.hours;
	response->days          = rtc_time.days;
	response->days_of_week  = rtc_time.daysofweek;
	response->month         = rtc_time.month;
	response->year          = rtc_time.year;

	return HANDLE_MESSAGE_RESPONSE_NEW_MESSAGE;
}

BootloaderHandleMessageResponse get_data_storage(const GetDataStorage *data, GetDataStorage_Response *response) {
	if(data->page >= DATA_STORAGE_PAGES) {
		return HANDLE_MESSAGE_RESPONSE_INVALID_PARAMETER;
	}

	response->header.length = sizeof(GetDataStorage_Response);
	memcpy(response->data, data_storage.storage[data->page], 63);

	return HANDLE_MESSAGE_RESPONSE_NEW_MESSAGE;
}

BootloaderHandleMessageResponse set_data_storage(const SetDataStorage *data) {
	if(data->page >= DATA_STORAGE_PAGES) {
		return HANDLE_MESSAGE_RESPONSE_INVALID_PARAMETER;
	}

	// Copy data into storage and set new change time.
	// Data will be copied from RAM to SD after 10 minutes.
	if(memcmp(data_storage.storage[data->page], data->data, 63) != 0) {
		memcpy(data_storage.storage[data->page], data->data, 63);
		if(data_storage.last_change_time[data->page] == 0) {
			data_storage.last_change_time[data->page] = system_timer_get_ms();
	}
}

return HANDLE_MESSAGE_RESPONSE_EMPTY;
}

bool handle_sd_wallbox_data_points_low_level_callback(void) {
	static bool is_buffered = false;
	static SDWallboxDataPointsLowLevel_Callback cb;

	if(!is_buffered) {
		if(!sd.new_sd_wallbox_data_points_cb) {
			return false;
		}
	
		tfp_make_default_header(&cb.header, bootloader_get_uid(), sizeof(SDWallboxDataPointsLowLevel_Callback), FID_CALLBACK_SD_WALLBOX_DATA_POINTS_LOW_LEVEL);
		cb.data_length = sd.sd_wallbox_data_points_cb_data_length;
		cb.data_chunk_offset = sd.sd_wallbox_data_points_cb_offset;
		memcpy(cb.data_chunk_data, sd.sd_wallbox_data_points_cb_data, 60);

		sd.new_sd_wallbox_data_points_cb = false;
	}

	if(bootloader_spitfp_is_send_possible(&bootloader_status.st)) {
		bootloader_spitfp_send_ack_and_message(&bootloader_status, (uint8_t*)&cb, sizeof(SDWallboxDataPointsLowLevel_Callback));
		is_buffered = false;
		return true;
	} else {
		is_buffered = true;
	}

	return false;
}

bool handle_sd_wallbox_daily_data_points_low_level_callback(void) {
	static bool is_buffered = false;
	static SDWallboxDailyDataPointsLowLevel_Callback cb;

	if(!is_buffered) {
		if(!sd.new_sd_wallbox_daily_data_points_cb) {
			return false;
		}

		tfp_make_default_header(&cb.header, bootloader_get_uid(), sizeof(SDWallboxDailyDataPointsLowLevel_Callback), FID_CALLBACK_SD_WALLBOX_DAILY_DATA_POINTS_LOW_LEVEL);
		cb.data_length = sd.sd_wallbox_daily_data_points_cb_data_length;
		cb.data_chunk_offset = sd.sd_wallbox_daily_data_points_cb_offset;
		memcpy(cb.data_chunk_data, sd.sd_wallbox_daily_data_points_cb_data, 60);

		sd.new_sd_wallbox_daily_data_points_cb = false;
	}

	if(bootloader_spitfp_is_send_possible(&bootloader_status.st)) {
		bootloader_spitfp_send_ack_and_message(&bootloader_status, (uint8_t*)&cb, sizeof(SDWallboxDailyDataPointsLowLevel_Callback));
		is_buffered = false;
		return true;
	} else {
		is_buffered = true;
	}

	return false;
}

bool handle_sd_energy_manager_data_points_low_level_callback(void) {
	static bool is_buffered = false;
	static SDEnergyManagerDataPointsLowLevel_Callback cb;

	if(!is_buffered) {
		if(!sd.new_sd_energy_manager_data_points_cb) {
			return false;
		}
	
		tfp_make_default_header(&cb.header, bootloader_get_uid(), sizeof(SDEnergyManagerDataPointsLowLevel_Callback), FID_CALLBACK_SD_ENERGY_MANAGER_DATA_POINTS_LOW_LEVEL);
		cb.data_length = sd.sd_energy_manager_data_points_cb_data_length;
		cb.data_chunk_offset = sd.sd_energy_manager_data_points_cb_offset;
		memcpy(cb.data_chunk_data, sd.sd_energy_manager_data_points_cb_data, 58);

		sd.new_sd_energy_manager_data_points_cb = false;
	}

	if(bootloader_spitfp_is_send_possible(&bootloader_status.st)) {
		bootloader_spitfp_send_ack_and_message(&bootloader_status, (uint8_t*)&cb, sizeof(SDEnergyManagerDataPointsLowLevel_Callback));
		is_buffered = false;
		return true;
	} else {
		is_buffered = true;
	}

	return false;
}

bool handle_sd_energy_manager_daily_data_points_low_level_callback(void) {
	static bool is_buffered = false;
	static SDEnergyManagerDailyDataPointsLowLevel_Callback cb;

	if(!is_buffered) {
		if(!sd.new_sd_energy_manager_daily_data_points_cb) {
			return false;
		}

		tfp_make_default_header(&cb.header, bootloader_get_uid(), sizeof(SDEnergyManagerDailyDataPointsLowLevel_Callback), FID_CALLBACK_SD_ENERGY_MANAGER_DAILY_DATA_POINTS_LOW_LEVEL);
		cb.data_length = sd.sd_energy_manager_daily_data_points_cb_data_length;
		cb.data_chunk_offset = sd.sd_energy_manager_daily_data_points_cb_offset;
		memcpy(cb.data_chunk_data, sd.sd_energy_manager_daily_data_points_cb_data, 56);

		sd.new_sd_energy_manager_daily_data_points_cb = false;
	}

	if(bootloader_spitfp_is_send_possible(&bootloader_status.st)) {
		bootloader_spitfp_send_ack_and_message(&bootloader_status, (uint8_t*)&cb, sizeof(SDEnergyManagerDailyDataPointsLowLevel_Callback));
		is_buffered = false;
		return true;
	} else {
		is_buffered = true;
	}

	return false;
}

void communication_tick(void) {
	communication_callback_tick();
}

void communication_init(void) {
	communication_callback_init();
}
