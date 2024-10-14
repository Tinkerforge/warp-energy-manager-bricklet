#!/usr/bin/env python3
# -*- coding: utf-8 -*-

HOST = 'localhost'
PORT = 4223
#EM_UID = '26dL'
EM_UID = '256GKn'

from collections import namedtuple
from tinkerforge.ip_connection import IPConnection
from tinkerforge.bricklet_warp_energy_manager import BrickletWARPEnergyManager

import time

if __name__ == '__main__':
    ipcon = IPConnection()
    ipcon.connect(HOST, PORT)
    em = BrickletWARPEnergyManager(EM_UID, ipcon)
#    while True:
#        for i in range(360):
#            em.set_led_state(0, i)
#            time.sleep(0.01)

    em.set_led_state(em.LED_PATTERN_BREATHING, 240)

