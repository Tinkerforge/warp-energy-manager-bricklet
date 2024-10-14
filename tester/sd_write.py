#!/usr/bin/env python3
# -*- coding: utf-8 -*-

HOST = 'localhost'
PORT = 4223
EM_UID = '256GKn'

from collections import namedtuple
from tinkerforge.ip_connection import IPConnection
from tinkerforge.bricklet_warp_energy_manager import BrickletWARPEnergyManager

WallboxDataPoint = namedtuple('WallboxDataPoint', ['wallbox_id', 'year', 'month', 'day', 'hour', 'minute', 'flags', 'power'])

if __name__ == '__main__':
    ipcon = IPConnection()
    ipcon.connect(HOST, PORT)
    em = BrickletWARPEnergyManager(EM_UID, ipcon)
    sd_info = em.get_sd_information()
    print(sd_info)

    dp = WallboxDataPoint(1, 21, 1, 11, 14, 40, 1, 1000)
    em.set_sd_wallbox_data_point(*dp)
    print('call set_sd_wallbox_data_point: {0}'.format(dp))
