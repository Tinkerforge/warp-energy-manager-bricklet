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

#include "relay.h"
#include "led.h"
#include "sdm630.h"
#include "rs485.h"

BootloaderHandleMessageResponse handle_message(const void *message, void *response) {
	switch(tfp_get_fid_from_message(message)) {
		case FID_SET_RELAY: return set_relay(message);
		case FID_GET_RELAY: return get_relay(message, response);
		case FID_SET_RGB_VALUE: return set_rgb_value(message);
		case FID_GET_RGB_VALUE: return get_rgb_value(message, response);
		case FID_GET_ENERGY_METER_VALUES: return get_energy_meter_values(message, response);
		case FID_GET_ENERGY_METER_DETAILED_VALUES_LOW_LEVEL: return get_energy_meter_detailed_values_low_level(message, response);
		case FID_GET_ENERGY_METER_STATE: return get_energy_meter_state(message, response);
		case FID_RESET_ENERGY_METER: return reset_energy_meter(message);
		case FID_GET_INPUT: return get_input(message, response);
		case FID_SET_OUTPUT: return set_output(message);
		case FID_GET_OUTPUT: return get_output(message, response);
		case FID_SET_INPUT_CONFIGURATION: return set_input_configuration(message);
		case FID_GET_INPUT_CONFIGURATION: return get_input_configuration(message, response);
		case FID_GET_INPUT_VOLTAGE: return get_input_voltage(message, response);
		case FID_GET_STATE: return get_state(message, response);
		default: return HANDLE_MESSAGE_RESPONSE_NOT_SUPPORTED;
	}
}


BootloaderHandleMessageResponse set_relay(const SetRelay *data) {
	relay.value = data->value;

	return HANDLE_MESSAGE_RESPONSE_EMPTY;
}

BootloaderHandleMessageResponse get_relay(const GetRelay *data, GetRelay_Response *response) {
	response->header.length = sizeof(GetRelay_Response);
	response->value         = relay.value;

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
	response->header.length    = sizeof(GetEnergyMeterValues_Response);
	response->power            = sdm630_register_fast.power.f;
	response->energy_absolute  = sdm630_register_fast.absolute_energy.f;
	response->energy_relative  = sdm630_register_fast.absolute_energy.f - sdm630.relative_energy.f;
	response->phases_active[0] = ((sdm630_register_fast.current_per_phase[0].f > 0.01f) << 0) |
	                             ((sdm630_register_fast.current_per_phase[1].f > 0.01f) << 1) |
	                             ((sdm630_register_fast.current_per_phase[2].f > 0.01f) << 2);
	response->phases_connected[0] = (sdm630.phases_connected[0] << 0) |
	                                (sdm630.phases_connected[1] << 1) |
	                                (sdm630.phases_connected[2] << 2);

	return HANDLE_MESSAGE_RESPONSE_NEW_MESSAGE;
}


BootloaderHandleMessageResponse get_energy_meter_detailed_values_low_level(const GetEnergyMeterDetailedValuesLowLevel *data, GetEnergyMeterDetailedValuesLowLevel_Response *response) {
	static uint32_t packet_payload_index = 0;

	response->header.length = sizeof(GetEnergyMeterDetailedValuesLowLevel_Response);

	const uint8_t packet_length = 60;
	const uint16_t max_end = 84*sizeof(float);
	const uint16_t start = packet_payload_index * packet_length;
	const uint16_t end = MIN(start + packet_length, max_end);
	const uint16_t copy_num = end-start;
	uint8_t *copy_from = (uint8_t*)&sdm630_register;

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
	response->header.length  = sizeof(GetEnergyMeterState_Response);
	response->available      = sdm630.available;
	response->error_count[0] = rs485.modbus_common_error_counters.timeout;
	response->error_count[1] = 0; // Global timeout. Currently global timeout triggers watchdog and EVSE will restart, so this will always be 0.
	response->error_count[2] = rs485.modbus_common_error_counters.illegal_function;
	response->error_count[3] = rs485.modbus_common_error_counters.illegal_data_address;
	response->error_count[4] = rs485.modbus_common_error_counters.illegal_data_value;
	response->error_count[5] = rs485.modbus_common_error_counters.slave_device_failure;

	return HANDLE_MESSAGE_RESPONSE_NEW_MESSAGE;
}

BootloaderHandleMessageResponse reset_energy_meter(const ResetEnergyMeter *data) {
	sdm630.reset_energy_meter = true;
//	evse_save_config(); // TODO

	return HANDLE_MESSAGE_RESPONSE_EMPTY;
}

BootloaderHandleMessageResponse get_input(const GetInput *data, GetInput_Response *response) {
	response->header.length = sizeof(GetInput_Response);

	return HANDLE_MESSAGE_RESPONSE_NEW_MESSAGE;
}

BootloaderHandleMessageResponse set_output(const SetOutput *data) {

	return HANDLE_MESSAGE_RESPONSE_EMPTY;
}

BootloaderHandleMessageResponse get_output(const GetOutput *data, GetOutput_Response *response) {
	response->header.length = sizeof(GetOutput_Response);

	return HANDLE_MESSAGE_RESPONSE_NEW_MESSAGE;
}

BootloaderHandleMessageResponse set_input_configuration(const SetInputConfiguration *data) {

	return HANDLE_MESSAGE_RESPONSE_EMPTY;
}

BootloaderHandleMessageResponse get_input_configuration(const GetInputConfiguration *data, GetInputConfiguration_Response *response) {
	response->header.length = sizeof(GetInputConfiguration_Response);

	return HANDLE_MESSAGE_RESPONSE_NEW_MESSAGE;
}

BootloaderHandleMessageResponse get_input_voltage(const GetInputVoltage *data, GetInputVoltage_Response *response) {
	response->header.length = sizeof(GetInputVoltage_Response);

	return HANDLE_MESSAGE_RESPONSE_NEW_MESSAGE;
}

BootloaderHandleMessageResponse get_state(const GetState *data, GetState_Response *response) {
	response->header.length = sizeof(GetState_Response);

	return HANDLE_MESSAGE_RESPONSE_NEW_MESSAGE;
}

void communication_tick(void) {
//	communication_callback_tick();
}

void communication_init(void) {
//	communication_callback_init();
}
