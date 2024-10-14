#!/usr/bin/env python3
# -*- coding: utf-8 -*-

HOST = 'localhost'
PORT = 4223
#EM_UID = '26dL'
EM_UID = '256GKn'

from collections import namedtuple
from tinkerforge.ip_connection import IPConnection
from tinkerforge.bricklet_warp_energy_manager import BrickletWARPEnergyManager
import random

WallboxDataPoint = namedtuple('WallboxDataPoint', ['wallbox_id', 'year', 'month', 'day', 'hour', 'minute', 'flags', 'power'])
WallboxDailyDataPoint = namedtuple('WallboxDailyDataPoint', ['wallbox_id', 'year', 'month', 'day', 'energy'])
EnergyManagerDataPoint = namedtuple('EnergyManagerDataPoint', ['year', 'month', 'day', 'hour', 'minute', 'flags', 'power_grid', 'power_general'])
EnergyManagerDailyDataPoint = namedtuple('EnergyManagerDailyDataPoint', ['year', 'month', 'day', 'energy_grid_in', 'energy_grid_out', 'energy_general_in', 'energy_general_out'])

import time

if __name__ == '__main__':
    ipcon = IPConnection()
    ipcon.connect(HOST, PORT)
    em = BrickletWARPEnergyManager(EM_UID, ipcon)
    print(em.get_all_data_1())


    while True:
        wdp1 = WallboxDataPoint(random.randint(100, 10000), 23, 3, 9, 11, 30, 0, 0)
        wdp1 = WallboxDataPoint(10001, 23, 3, 9, 11, 30, 0, 0)
        emdp1 = EnergyManagerDataPoint(23, 3, 9, 11, 30, 0, 0, [1]*6)
        status1 = em.set_sd_wallbox_data_point(*wdp1)
        status2 = em.set_sd_energy_manager_data_point(*emdp1)
        print(status1)
        print(status2)
        print(em.get_uptime())
        for i in range(40):
            print(em.get_all_data_1())
            time.sleep(0.01)
