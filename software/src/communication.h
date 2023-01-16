/* warp-energy-manager-bricklet
 * Copyright (C) 2023 Olaf Lüke <olaf@tinkerforge.com>
 *
 * communication.h: TFP protocol message handling
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

#ifndef COMMUNICATION_H
#define COMMUNICATION_H

#include <stdint.h>
#include <stdbool.h>

#include "bricklib2/protocols/tfp/tfp.h"
#include "bricklib2/bootloader/bootloader.h"

// Default functions
BootloaderHandleMessageResponse handle_message(const void *data, void *response);
void communication_tick(void);
void communication_init(void);

// Constants

#define WARP_ENERGY_MANAGER_ENERGY_METER_TYPE_NOT_AVAILABLE 0
#define WARP_ENERGY_MANAGER_ENERGY_METER_TYPE_SDM72 1
#define WARP_ENERGY_MANAGER_ENERGY_METER_TYPE_SDM630 2
#define WARP_ENERGY_MANAGER_ENERGY_METER_TYPE_SDM72V2 3
#define WARP_ENERGY_MANAGER_ENERGY_METER_TYPE_SDM72CTM 4

#define WARP_ENERGY_MANAGER_BOOTLOADER_MODE_BOOTLOADER 0
#define WARP_ENERGY_MANAGER_BOOTLOADER_MODE_FIRMWARE 1
#define WARP_ENERGY_MANAGER_BOOTLOADER_MODE_BOOTLOADER_WAIT_FOR_REBOOT 2
#define WARP_ENERGY_MANAGER_BOOTLOADER_MODE_FIRMWARE_WAIT_FOR_REBOOT 3
#define WARP_ENERGY_MANAGER_BOOTLOADER_MODE_FIRMWARE_WAIT_FOR_ERASE_AND_REBOOT 4

#define WARP_ENERGY_MANAGER_BOOTLOADER_STATUS_OK 0
#define WARP_ENERGY_MANAGER_BOOTLOADER_STATUS_INVALID_MODE 1
#define WARP_ENERGY_MANAGER_BOOTLOADER_STATUS_NO_CHANGE 2
#define WARP_ENERGY_MANAGER_BOOTLOADER_STATUS_ENTRY_FUNCTION_NOT_PRESENT 3
#define WARP_ENERGY_MANAGER_BOOTLOADER_STATUS_DEVICE_IDENTIFIER_INCORRECT 4
#define WARP_ENERGY_MANAGER_BOOTLOADER_STATUS_CRC_MISMATCH 5

#define WARP_ENERGY_MANAGER_STATUS_LED_CONFIG_OFF 0
#define WARP_ENERGY_MANAGER_STATUS_LED_CONFIG_ON 1
#define WARP_ENERGY_MANAGER_STATUS_LED_CONFIG_SHOW_HEARTBEAT 2
#define WARP_ENERGY_MANAGER_STATUS_LED_CONFIG_SHOW_STATUS 3

// Function and callback IDs and structs
#define FID_SET_CONTACTOR 1
#define FID_GET_CONTACTOR 2
#define FID_SET_RGB_VALUE 3
#define FID_GET_RGB_VALUE 4
#define FID_GET_ENERGY_METER_VALUES 5
#define FID_GET_ENERGY_METER_DETAILED_VALUES_LOW_LEVEL 6
#define FID_GET_ENERGY_METER_STATE 7
#define FID_GET_INPUT 8
#define FID_SET_OUTPUT 9
#define FID_GET_OUTPUT 10
#define FID_GET_INPUT_VOLTAGE 11
#define FID_GET_STATE 12
#define FID_GET_ALL_DATA_1 13
#define FID_GET_SD_INFORMATION 14
#define FID_SET_SD_WALLBOX_DATA_POINT 15
#define FID_GET_SD_WALLBOX_DATA_POINT 16


typedef struct {
	TFPMessageHeader header;
	bool contactor_value;
} __attribute__((__packed__)) SetContactor;

typedef struct {
	TFPMessageHeader header;
} __attribute__((__packed__)) GetContactor;

typedef struct {
	TFPMessageHeader header;
	bool contactor_value;
} __attribute__((__packed__)) GetContactor_Response;

typedef struct {
	TFPMessageHeader header;
	uint8_t r;
	uint8_t g;
	uint8_t b;
} __attribute__((__packed__)) SetRGBValue;

typedef struct {
	TFPMessageHeader header;
} __attribute__((__packed__)) GetRGBValue;

typedef struct {
	TFPMessageHeader header;
	uint8_t r;
	uint8_t g;
	uint8_t b;
} __attribute__((__packed__)) GetRGBValue_Response;

typedef struct {
	TFPMessageHeader header;
} __attribute__((__packed__)) GetEnergyMeterValues;

typedef struct {
	TFPMessageHeader header;
	float power;
	float energy_import;
	float energy_export;
} __attribute__((__packed__)) GetEnergyMeterValues_Response;

typedef struct {
	TFPMessageHeader header;
} __attribute__((__packed__)) GetEnergyMeterDetailedValuesLowLevel;

typedef struct {
	TFPMessageHeader header;
	uint16_t values_chunk_offset;
	float values_chunk_data[15];
} __attribute__((__packed__)) GetEnergyMeterDetailedValuesLowLevel_Response;

typedef struct {
	TFPMessageHeader header;
} __attribute__((__packed__)) GetEnergyMeterState;

typedef struct {
	TFPMessageHeader header;
	uint8_t energy_meter_type;
	uint32_t error_count[6];
} __attribute__((__packed__)) GetEnergyMeterState_Response;

typedef struct {
	TFPMessageHeader header;
} __attribute__((__packed__)) GetInput;

typedef struct {
	TFPMessageHeader header;
	uint8_t input[1];
} __attribute__((__packed__)) GetInput_Response;

typedef struct {
	TFPMessageHeader header;
	bool output;
} __attribute__((__packed__)) SetOutput;

typedef struct {
	TFPMessageHeader header;
} __attribute__((__packed__)) GetOutput;

typedef struct {
	TFPMessageHeader header;
	bool output;
} __attribute__((__packed__)) GetOutput_Response;

typedef struct {
	TFPMessageHeader header;
} __attribute__((__packed__)) GetInputVoltage;

typedef struct {
	TFPMessageHeader header;
	uint16_t voltage;
} __attribute__((__packed__)) GetInputVoltage_Response;

typedef struct {
	TFPMessageHeader header;
} __attribute__((__packed__)) GetState;

typedef struct {
	TFPMessageHeader header;
	uint8_t contactor_check_state;
} __attribute__((__packed__)) GetState_Response;

typedef struct {
	TFPMessageHeader header;
} __attribute__((__packed__)) GetAllData1;

typedef struct {
	TFPMessageHeader header;
	bool contactor_value;
	uint8_t r;
	uint8_t g;
	uint8_t b;
	float power;
	float energy_import;
	float energy_export;
	uint8_t energy_meter_type;
	uint32_t error_count[6];
	uint8_t input[1];
	bool output;
	uint16_t voltage;
	uint8_t contactor_check_state;
} __attribute__((__packed__)) GetAllData1_Response;

typedef struct {
	TFPMessageHeader header;
} __attribute__((__packed__)) GetSDInformation;

typedef struct {
	TFPMessageHeader header;
	uint32_t sd_status;
	uint32_t lfs_status;
	uint16_t sector_size;
	uint32_t sector_count;
	uint32_t card_type;
	uint8_t product_rev;
	char product_name[5];
	uint8_t manufacturer_id;
} __attribute__((__packed__)) GetSDInformation_Response;

typedef struct {
	TFPMessageHeader header;
	uint8_t wallbox_id;
	uint8_t year;
	uint8_t month;
	uint8_t day;
	uint8_t hour;
	uint8_t minute;
	uint32_t charge_tracker_id_start;
	uint32_t charge_tracker_id_end;
	uint16_t flags_start;
	uint16_t flags_end;
	uint16_t line_voltages[3];
	uint16_t line_currents[3];
	uint8_t line_power_factors[3];
	uint16_t max_current;
	float energy_abs;
} __attribute__((__packed__)) SetSDWallboxDataPoint;

typedef struct {
	TFPMessageHeader header;
	uint8_t wallbox_id;
	uint8_t year;
	uint8_t month;
	uint8_t day;
	uint8_t hour;
	uint8_t minute;
} __attribute__((__packed__)) GetSDWallboxDataPoint;

typedef struct {
	TFPMessageHeader header;
	uint32_t charge_tracker_id_start;
	uint32_t charge_tracker_id_end;
	uint16_t flags_start;
	uint16_t flags_end;
	uint16_t line_voltages[3];
	uint16_t line_currents[3];
	uint8_t line_power_factors[3];
	uint16_t max_current;
	float energy_abs;
} __attribute__((__packed__)) GetSDWallboxDataPoint_Response;


// Function prototypes
BootloaderHandleMessageResponse set_contactor(const SetContactor *data);
BootloaderHandleMessageResponse get_contactor(const GetContactor *data, GetContactor_Response *response);
BootloaderHandleMessageResponse set_rgb_value(const SetRGBValue *data);
BootloaderHandleMessageResponse get_rgb_value(const GetRGBValue *data, GetRGBValue_Response *response);
BootloaderHandleMessageResponse get_energy_meter_values(const GetEnergyMeterValues *data, GetEnergyMeterValues_Response *response);
BootloaderHandleMessageResponse get_energy_meter_detailed_values_low_level(const GetEnergyMeterDetailedValuesLowLevel *data, GetEnergyMeterDetailedValuesLowLevel_Response *response);
BootloaderHandleMessageResponse get_energy_meter_state(const GetEnergyMeterState *data, GetEnergyMeterState_Response *response);
BootloaderHandleMessageResponse get_input(const GetInput *data, GetInput_Response *response);
BootloaderHandleMessageResponse set_output(const SetOutput *data);
BootloaderHandleMessageResponse get_output(const GetOutput *data, GetOutput_Response *response);
BootloaderHandleMessageResponse get_input_voltage(const GetInputVoltage *data, GetInputVoltage_Response *response);
BootloaderHandleMessageResponse get_state(const GetState *data, GetState_Response *response);
BootloaderHandleMessageResponse get_all_data_1(const GetAllData1 *data, GetAllData1_Response *response);
BootloaderHandleMessageResponse get_sd_information(const GetSDInformation *data, GetSDInformation_Response *response);
BootloaderHandleMessageResponse set_sd_wallbox_data_point(const SetSDWallboxDataPoint *data);
BootloaderHandleMessageResponse get_sd_wallbox_data_point(const GetSDWallboxDataPoint *data, GetSDWallboxDataPoint_Response *response);

// Callbacks


#define COMMUNICATION_CALLBACK_TICK_WAIT_MS 1
#define COMMUNICATION_CALLBACK_HANDLER_NUM 0
#define COMMUNICATION_CALLBACK_LIST_INIT \


#endif
