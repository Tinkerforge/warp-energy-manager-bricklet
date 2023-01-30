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

#include "io.h"
#include "led.h"
#include "sdm.h"
#include "rs485.h"
#include "voltage.h"
#include "eeprom.h"
#include "sd.h"
#include "sdmmc.h"

BootloaderHandleMessageResponse handle_message(const void *message, void *response) {
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
		case FID_GET_ALL_DATA_1: return get_all_data_1(message, response);
		case FID_GET_SD_INFORMATION: return get_sd_information(message, response);
		case FID_SET_SD_WALLBOX_DATA_POINT: return set_sd_wallbox_data_point(message, response);
		case FID_GET_SD_WALLBOX_DATA_POINTS: return get_sd_wallbox_data_points(message, response);
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

	return HANDLE_MESSAGE_RESPONSE_EMPTY;
}

BootloaderHandleMessageResponse get_rgb_value(const GetRGBValue *data, GetRGBValue_Response *response) {
	response->header.length = sizeof(GetRGBValue_Response);
	response->r             = led.r;
	response->g             = led.g;
	response->b             = led.b;

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
	uint8_t *copy_from = (uint8_t*)&sdm_register;

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
	if(sdm.meter_type == SDM_METER_TYPE_UNKNOWN) {
		response->energy_meter_type = WARP_ENERGY_MANAGER_ENERGY_METER_TYPE_NOT_AVAILABLE;
	} else if(sdm.meter_type == SDM_METER_TYPE_SDM72V2) {
		response->energy_meter_type = WARP_ENERGY_MANAGER_ENERGY_METER_TYPE_SDM72V2;
	} else if(sdm.meter_type == SDM_METER_TYPE_SDM72CTM) {
		response->energy_meter_type = WARP_ENERGY_MANAGER_ENERGY_METER_TYPE_SDM72CTM;
	} else {
		response->energy_meter_type = WARP_ENERGY_MANAGER_ENERGY_METER_TYPE_SDM630;
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
	response->status        = WARP_ENERGY_MANAGER_DATA_STATUS_OK;

	if(sd.sd_status != SDMMC_ERROR_OK) {
		response->status = WARP_ENERGY_MANAGER_DATA_STATUS_SD_ERROR;
		return HANDLE_MESSAGE_RESPONSE_NEW_MESSAGE;
	}

	if(sd.lfs_status != LFS_ERR_OK) {
		response->status = WARP_ENERGY_MANAGER_DATA_STATUS_LFS_ERROR;
		return HANDLE_MESSAGE_RESPONSE_NEW_MESSAGE;
	}

	if(sd.wallbox_data_point_end >= SD_WALLBOX_DATA_POINT_LENGTH) {
		response->status = WARP_ENERGY_MANAGER_DATA_STATUS_QUEUE_FULL;
		return HANDLE_MESSAGE_RESPONSE_NEW_MESSAGE;
	}

	memcpy(&sd.wallbox_data_point[sd.wallbox_data_point_end], &data->wallbox_id, sizeof(SetSDWallboxDataPoint) - sizeof(TFPMessageHeader));
	sd.wallbox_data_point_end++;

	return HANDLE_MESSAGE_RESPONSE_NEW_MESSAGE;
}

BootloaderHandleMessageResponse get_sd_wallbox_data_points(const GetSDWallboxDataPoints *data, GetSDWallboxDataPoints_Response *response) {
	response->header.length = sizeof(GetSDWallboxDataPoints_Response);
	response->status        = WARP_ENERGY_MANAGER_DATA_STATUS_OK;

	if(sd.sd_status != SDMMC_ERROR_OK) {
		response->status = WARP_ENERGY_MANAGER_DATA_STATUS_SD_ERROR;
		return HANDLE_MESSAGE_RESPONSE_NEW_MESSAGE;
	}

	if(sd.lfs_status != LFS_ERR_OK) {
		response->status = WARP_ENERGY_MANAGER_DATA_STATUS_LFS_ERROR;
		return HANDLE_MESSAGE_RESPONSE_NEW_MESSAGE;
	}
	
	if(sd.new_sd_wallbox_data_points) {
		response->status = WARP_ENERGY_MANAGER_DATA_STATUS_QUEUE_FULL;
		return HANDLE_MESSAGE_RESPONSE_NEW_MESSAGE;
	}

	sd.get_sd_wallbox_data_points = *data;
	sd.new_sd_wallbox_data_points = true;

	logd("get_sd_wallbox_data_points start\n\r");

	return HANDLE_MESSAGE_RESPONSE_NEW_MESSAGE;
}

bool handle_sd_wallbox_data_points_low_level_callback(void) {
	static bool is_buffered = false;
	static SDWallboxDataPointsLowLevel_Callback cb;

	if(!sd.new_sd_wallbox_data_points_cb) {
		return false;
	}

	if(!is_buffered) {
		tfp_make_default_header(&cb.header, bootloader_get_uid(), sizeof(SDWallboxDataPointsLowLevel_Callback), FID_CALLBACK_SD_WALLBOX_DATA_POINTS_LOW_LEVEL);
		cb.data_length = sd.sd_wallbox_data_points_cb_data_length;
		cb.data_chunk_offset = sd.sd_wallbox_data_points_cb_offset;
		memcpy(cb.data_chunk_data, sd.sd_wallbox_data_points_cb_data, 60);

		sd.new_sd_wallbox_data_points_cb = false;
		logd("get_sd_wallbox_data_points cb %d\n\r", cb.data_chunk_offset);
	}

	if(bootloader_spitfp_is_send_possible(&bootloader_status.st)) {
		bootloader_spitfp_send_ack_and_message(&bootloader_status, (uint8_t*)&cb, sizeof(SDWallboxDataPointsLowLevel_Callback));
		logd("send! %d\n\r", cb.header.length);
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
