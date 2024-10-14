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

WallboxDailyDataPoint = namedtuple('WallboxDailyDataPoint', ['wallbox_id', 'year', 'month', 'day', 'energy'])

def cb_wallbox_daily_data_points(data):
    print(data)

if __name__ == '__main__':
    ipcon = IPConnection()
    ipcon.connect(HOST, PORT)
    em = BrickletWARPEnergyManager(EM_UID, ipcon)
    em.register_callback(em.CALLBACK_SD_WALLBOX_DAILY_DATA_POINTS, cb_wallbox_daily_data_points)

    em.set_response_expected_all(True)
    sd_info = em.get_sd_information()
    print(sd_info)

#    em.get_sd_wallbox_data_points(1, 22, 0, 0, 0, 0, 20)

    """
    count = 0
    for month in range(1, 13):
        for day in range(1, 31):
            for wb in range(1, 8):
                wddp = WallboxDailyDataPoint(wb, 22, month, day, count)
                            
                count += 1
                t = time.time()
                try:
                    status = em.set_sd_wallbox_daily_data_point(*wddp)
                except:
                    traceback.print_exc()
                    time.sleep(1)
                    continue
                if status == em.DATA_STATUS_QUEUE_FULL:
                    print('full')
                    time.sleep(0.1)
                elif status == em.DATA_STATUS_OK:
                    print('call set_sd_wallbox_daily_data_point ({0:02f}): {1}'.format((time.time()-t)*1000, wddp))
                    time.sleep(0.1)
                else:
                    print('call set_sd_wallbox_daily_data_point error {0}'.format(status))
                    sys.exit(0)
    """


    """
    while True:
        for month in range(1, 13):
            for wb in range(1, 8):
                print('get', wb, 22, month, 0)
                ret = 1
                while ret != 0:
                    ret = em.get_sd_wallbox_daily_data_points(wb, 22, month, 0, 31)
                    if ret != 0:
                        print("ret {0}".format(ret))
                    time.sleep(1)
    """
    
#    """
    count = 0
    month = 1
    for day in range(1, 31):
        wb = 1
        wddp = WallboxDailyDataPoint(wb, 22, month, day, count)
                    
        count += 1
        t = time.time()
        try:
            status = em.set_sd_wallbox_daily_data_point(*wddp)
        except:
            traceback.print_exc()
            time.sleep(1)
            continue
        if status == em.DATA_STATUS_QUEUE_FULL:
            print('full')
            time.sleep(1)
        elif status == em.DATA_STATUS_OK:
            print('call set_sd_wallbox_daily_data_point ({0:02f}): {1}'.format((time.time()-t)*1000, wddp))
            time.sleep(0.2)
        else:
            print('call set_sd_wallbox_daily_data_point error {0}'.format(status))
            sys.exit(0)
#    """

#    """
    ret = 1
    while ret != 0:
        ret = em.get_sd_wallbox_daily_data_points(1, 22, 1, 1, 30)
        print(ret)
        if ret != 0:
            print("ret {0}".format(ret))
        time.sleep(1)
#    """

    while True:
        time.sleep(1)
