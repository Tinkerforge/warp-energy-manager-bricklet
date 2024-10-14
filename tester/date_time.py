#!/usr/bin/env python3
# -*- coding: utf-8 -*-

HOST = 'localhost'
PORT = 4223
EM_UID = '256GKn'
#EM_UID = '26dL'

import time

from tinkerforge.ip_connection import IPConnection
from tinkerforge.bricklet_warp_energy_manager import BrickletWARPEnergyManager

if __name__ == '__main__':
    ipcon = IPConnection()
    ipcon.connect(HOST, PORT)
    em = BrickletWARPEnergyManager(EM_UID, ipcon)

    em.set_date_time(0, 26, 16, 19, 1, 1, 2023)
    print("before", em.get_date_time())
    time.sleep(300)
    print("after", em.get_date_time())
