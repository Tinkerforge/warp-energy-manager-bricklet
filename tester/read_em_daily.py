#!/usr/bin/env python3
# -*- coding: utf-8 -*-

HOST = 'wem-22ph'
PORT = 4223
EM_UID = '289qud'
import time

from tinkerforge.ip_connection import IPConnection
from tinkerforge.bricklet_warp_energy_manager import BrickletWARPEnergyManager

def cb_energy_manager_data_points(data):
    print(data)

if __name__ == '__main__':
    ipcon = IPConnection()
    ipcon.connect(HOST, PORT)
    em = BrickletWARPEnergyManager(EM_UID, ipcon)
    em.register_callback(em.CALLBACK_SD_ENERGY_MANAGER_DATA_POINTS, cb_energy_manager_data_points)

    sd_info = em.get_sd_information()
    print(sd_info)

    ret = em.get_sd_energy_manager_data_points(24, 10, 14, 0, 0, 288)
    print(ret)
    time.sleep(1)
