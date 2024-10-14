#!/usr/bin/env python3
# -*- coding: utf-8 -*-

HOST = 'localhost'
PORT = 4223
EM_UID = '256GKn'

from collections import namedtuple
from tinkerforge.ip_connection import IPConnection
from tinkerforge.bricklet_warp_energy_manager import BrickletWARPEnergyManager

import time

if __name__ == '__main__':
    ipcon = IPConnection()
    ipcon.connect(HOST, PORT)
    em = BrickletWARPEnergyManager(EM_UID, ipcon)

    sd_info = em.get_sd_information()
    print(sd_info)
    
    em.set_data_storage(0, list(range(0, 63)))
    em.set_data_storage(1, list(range(64, 127)))

    time.sleep(1)
    print(em.get_data_storage(0))
    print(em.get_data_storage(1))
