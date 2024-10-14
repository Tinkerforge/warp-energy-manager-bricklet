#!/usr/bin/env python3
# -*- coding: utf-8 -*-

HOST = 'wem-22ph'
PORT = 4223
EM_UID = '289qud'
import time

from tinkerforge.ip_connection import IPConnection
from tinkerforge.bricklet_warp_energy_manager import BrickletWARPEnergyManager

def cb_wallbox_daily_data_points(data):
    print(data)

def cb_energy_manager_daily_data_points(data):
    print(data)

if __name__ == '__main__':
    ipcon = IPConnection()
    ipcon.connect(HOST, PORT)
    em = BrickletWARPEnergyManager(EM_UID, ipcon)
    em.register_callback(em.CALLBACK_SD_WALLBOX_DAILY_DATA_POINTS, cb_wallbox_daily_data_points)
    em.register_callback(em.CALLBACK_SD_ENERGY_MANAGER_DAILY_DATA_POINTS, cb_energy_manager_daily_data_points)

    sd_info = em.get_sd_information()
    print(sd_info)

    ret = em.get_sd_energy_manager_daily_data_points(24, 8, 1, 1)
    print(ret)
    time.sleep(1)
