#!/usr/bin/env python3
# -*- coding: utf-8 -*-

HOST = 'localhost'
PORT = 4223
EM_UID = '256GKn'
#EM_UID = '2qcheb'
import time
import sys
import traceback

from collections import namedtuple
from tinkerforge.ip_connection import IPConnection
from tinkerforge.bricklet_warp_energy_manager import BrickletWARPEnergyManager

WallboxDataPoint = namedtuple('WallboxDataPoint', ['wallbox_id', 'year', 'month', 'day', 'hour', 'minute', 'flags', 'power'])

def cb_wallbox_data_points(data):
    flags = []
    power = []
    for i in range(0, len(data), 3):
        flags.append(data[i])
        power.append(data[i+1] | (data[i+2] << 8))
    print(flags)
    print(power)


if __name__ == '__main__':
    ipcon = IPConnection()
    ipcon.connect(HOST, PORT)
    em = BrickletWARPEnergyManager(EM_UID, ipcon)
    em.register_callback(em.CALLBACK_SD_WALLBOX_DATA_POINTS, cb_wallbox_data_points)

    em.set_response_expected_all(True)
    sd_info = em.get_sd_information()
    print(sd_info)

#    em.get_sd_wallbox_data_points(1, 22, 0, 0, 0, 0, 20)

#    """
    count = 0
    for month in range(1, 13):
        for day in range(1, 31):
            for hour in range(0, 24):
                for minute in range(0, 12):
                    for wb in range(1, 8):
                        dpw = WallboxDataPoint(wb, 22, month, day, hour, minute*5, count % 128, count % 0xFFFF)
                            
                        count += 1
                        t = time.time()
                        try:
                            status = em.set_sd_wallbox_data_point(*dpw)
                        except:
                            traceback.print_exc()
                            time.sleep(1)
                            continue
                        if status == em.DATA_STATUS_QUEUE_FULL:
                            print('full')
                            time.sleep(0.1)
                        elif status == em.DATA_STATUS_OK:
                            print('call set_sd_wallbox_data_point ({0:02f}): {1}'.format((time.time()-t)*1000, dpw))
                        else:
                            print('call set_sd_wallbox_data_point error {0}'.format(status))
                            sys.exit(0)
#    """


    """
    for month in range(1, 13):
        for day in range(1, 31):
            for hour in range(0, 24):
                for minute in range(0, 12):
                    t = time.time()
                    dpr = em.get_sd_wallbox_data_point(1, 22, month, day, hour, minute*5)
                    if dpr.status == em.DATA_STATUS_QUEUE_FULL:
                        print('full')
                        time.sleep(0.1)
                    elif dpr.status == em.DATA_STATUS_OK:
                        print('call get_sd_wallbox_data_point ({0}: {1}'.format(time.time()-t, dpr))
                    else:
                        print('call get_sd_wallbox_data_point error {0}'.format(status))
                        sys.exit(0)
    """

    
    while True:
        for month in range(1, 13):
            for day in range(1, 31):
                print('get', 1, 22, month, day, 0, 0)
                ret = 1
                while ret != 0:
                    ret = em.get_sd_wallbox_data_points(1, 22, month, day, 0, 0, 12*24)
                    if ret != 0:
                        print("ret {0}".format(ret))
                    time.sleep(1)
    

    while True:
        time.sleep(1)
