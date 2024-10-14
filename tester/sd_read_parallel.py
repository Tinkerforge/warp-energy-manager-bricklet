#!/usr/bin/env python3
# -*- coding: utf-8 -*-

HOST = 'localhost'
#HOST = '192.168.0.88'
PORT = 4223
#EM_UID = '256GKn'
#EM_UID = '2qcheb'
EM_UID = '26dL'
import time
import sys
import traceback

from collections import namedtuple
from tinkerforge.ip_connection import IPConnection
from tinkerforge.bricklet_warp_energy_manager import BrickletWARPEnergyManager

WallboxDataPoint = namedtuple('WallboxDataPoint', ['wallbox_id', 'year', 'month', 'day', 'hour', 'minute', 'flags', 'power'])
WallboxDailyDataPoint = namedtuple('WallboxDailyDataPoint', ['wallbox_id', 'year', 'month', 'day', 'energy'])

EnergyManagerDataPoint = namedtuple('EnergyManagerDataPoint', ['year', 'month', 'day', 'hour', 'minute', 'flags', 'power_grid', 'power_general'])
EnergyManagerDailyDataPoint = namedtuple('EnergyManagerDailyDataPoint', ['year', 'month', 'day', 'energy_grid_in', 'energy_grid_out', 'energy_general_in', 'energy_general_out'])

def cb_wallbox_data_points(data):
    global time_get
    print(time.time() - time_get)
    for i in range(0, len(data), 3):
        flags = data[i]
        power = data[i+1] | (data[i+2] << 8)
        print("cb_wallbox_data_points flags {0}, power {1}".format(flags, power))

def cb_wallbox_daily_data_points(data):
    global time_get
    print(time.time() - time_get)
    for energy in data:
        print("cb_wallbox_daily_data_points energy {0}".format(energy))

def cb_energy_manager_data_points(data):
    global time_get
    print(time.time() - time_get)
    for i in range(0, len(data), 1+7*4):
        flags = data[i]
        power_grid = data[i+1] | (data[i+2] << 8) | (data[i+3] << 16) | (data[i+4] << 24)
        power_general = []
        for j in range(0, 6):
            power_general.append(data[i+5 + j*4] | (data[i+6 + j*4] << 8) | (data[i+7 + j*4] << 16) | (data[i+8 + j*4] << 24))
        print("cb_energy_manager_data_points flags {0}, power_grid {1}, power_general {2}".format(flags, power_grid, power_general))

def cb_energy_manager_daily_data_points(data):
    global time_get
    print(time.time() - time_get)
    for i in range(0, len(data), 1+1+6+6):
        power_grid_in = data[i + 0]
        power_grid_out = data[i + 1]
        power_general_in = [data[i + 2], data[i + 3], data[i + 4], data[i + 5], data[i + 6], data[i + 7]]
        power_general_out = [data[i + 8], data[i + 9], data[i + 10], data[i + 11], data[i + 12], data[i + 13]]
        print("cb_energy_manager_daily_data_points power_grid_in {0}, power_grid_out {1}, power_general_in {2}, power_general_out {3}".format(power_grid_in, power_grid_out, power_general_in, power_general_out))

if __name__ == '__main__':
    ipcon = IPConnection()
    ipcon.connect(HOST, PORT)
    em = BrickletWARPEnergyManager(EM_UID, ipcon)
    em.register_callback(em.CALLBACK_SD_WALLBOX_DATA_POINTS, cb_wallbox_data_points)
    em.register_callback(em.CALLBACK_SD_WALLBOX_DAILY_DATA_POINTS, cb_wallbox_daily_data_points)
    em.register_callback(em.CALLBACK_SD_ENERGY_MANAGER_DATA_POINTS, cb_energy_manager_data_points)
    em.register_callback(em.CALLBACK_SD_ENERGY_MANAGER_DAILY_DATA_POINTS, cb_energy_manager_daily_data_points)

    em.set_response_expected_all(True)
    sd_info = em.get_sd_information()
    print(sd_info)
    count = 0

    def write_stuff():
        em.set_sd_wallbox_data_point(100000+1, 22, 1, 1, 0, 0, 42, 23)
        em.set_sd_wallbox_data_point(100000+1, 22, 1, 1, 0, 5, 1, 2)
        em.set_sd_wallbox_data_point(100000+1, 22, 1, 1, 0, 10, 3, 4)
        em.set_sd_wallbox_data_point(100000+1, 22, 1, 1, 0, 15, 5, 6)
        em.set_sd_wallbox_daily_data_point(100000+1, 22, 1, 1, 123)
        em.set_sd_energy_manager_data_point(22, 1, 1, 0, 0, 127, 32, [23]*6)
        em.set_sd_energy_manager_data_point(22, 1, 1, 0, 0, 127, 1, [4]*6)
        em.set_sd_energy_manager_data_point(22, 1, 1, 0, 0, 127, 2, [5]*6)
        em.set_sd_energy_manager_data_point(22, 1, 1, 0, 0, 127, 3, [6]*6)
        em.set_sd_energy_manager_daily_data_point(22, 1, 1, 42, 32, [4]*6, [5]*6)


    while True:
        time_get = time.time()
        em.get_sd_wallbox_data_points(100000+1, 22, 1, 1, 0, 0, 12*24)
        write_stuff()
        em.get_sd_wallbox_daily_data_points(100000+1, 22, 1, 1, 31)
        write_stuff()
        em.get_sd_energy_manager_data_points(24, 1, 1, 0, 0, 12*24)
        write_stuff()
        em.get_sd_energy_manager_daily_data_points(24, 1, 1, 30)
        write_stuff()

        time.sleep(3)
