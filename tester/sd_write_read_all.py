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
WallboxDailyDataPoint = namedtuple('WallboxDailyDataPoint', ['wallbox_id', 'year', 'month', 'day', 'energy'])

EnergyManagerDataPoint = namedtuple('EnergyManagerDataPoint', ['year', 'month', 'day', 'hour', 'minute', 'flags', 'power_grid', 'power_general'])
EnergyManagerDailyDataPoint = namedtuple('EnergyManagerDailyDataPoint', ['year', 'month', 'day', 'energy_grid_in', 'energy_grid_out', 'energy_general_in', 'energy_general_out'])

def cb_wallbox_data_points(data):
    for i in range(0, len(data), 3):
        flags = data[i]
        power = data[i+1] | (data[i+2] << 8)
        print("flags {0}, power {1}".format(flags, power))

def cb_wallbox_daily_data_points(data):
    for energy in data:
        print("energy {0}".format(energy))

def cb_energy_manager_data_points(data):
    for i in range(0, len(data), 1+7*4):
        flags = data[i]
        power_grid = data[i+1] | (data[i+2] << 8) | (data[i+3] << 16) | (data[i+4] << 24)
        power_general = []
        for j in range(0, 6):
            power_general.append(data[i+5 + j*4] | (data[i+6 + j*4] << 8) | (data[i+7 + j*4] << 16) | (data[i+8 + j*4] << 24))
        print("flags {0}, power_grid {1}, power_general {2}".format(flags, power_grid, power_general))

def cb_energy_manager_daily_data_points(data):
    for i in range(0, len(data), 1+1+6+6):
        power_grid_in = data[i + 0]
        power_grid_out = data[i + 1]
        power_general_in = [data[i + 2], data[i + 3], data[i + 4], data[i + 5], data[i + 6], data[i + 7]]
        power_general_out = [data[i + 8], data[i + 9], data[i + 10], data[i + 11], data[i + 12], data[i + 13]]
        print("power_grid_in {0}, power_grid_out {1}, power_general_in {2}, power_general_out {3}".format(power_grid_in, power_grid_out, power_general_in, power_general_out))

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

    print("Write 1 day 5min wb data")
    for year in range(22, 23):
        for month in range(1, 2):
            for day in range(1, 2):
                for hour in range(0, 24):
                    for minute in range(0, 12):
                        for wb in range(1, 3):
                            dpw = WallboxDataPoint(wb, year, month, day, hour, minute*5, count % 128, count % 0xFFFF)
                                
                            count += 1
                            while True:
                                try:
                                    t = time.time()
                                    status = em.set_sd_wallbox_data_point(*dpw)
                                except:
                                    traceback.print_exc()
                                    time.sleep(1)
                                    break
                                if status == em.DATA_STATUS_QUEUE_FULL:
                                    print('full')
                                    time.sleep(0.05)
                                    continue
                                elif status == em.DATA_STATUS_OK:
                                    print('call set_sd_wallbox_data_point ({0:02f}): {1}'.format((time.time()-t)*1000, dpw))
                                    break
                                else:
                                    print('call set_sd_wallbox_data_point error {0}'.format(status))
                                    sys.exit(0)

    print("Read 1 day 5min wb data")
    for year in range(22, 23):
        for month in range(1, 2):
            for day in range(1, 2):
                for wb in range(1, 3):
                    ret = 1
                    while ret != 0:
                        ret = em.get_sd_wallbox_data_points(wb, year, month, day, 0, 0, 12*24)
                        if ret != 0:
                            print("ret {0}".format(ret))
                        time.sleep(1)
    

    time.sleep(10)
    print("Write 1 month daily wb data")
    for year in range(22, 23):
        for month in range(1, 2):
            for day in range(1, 31):
                for wb in range(1, 3):
                    wddp = WallboxDailyDataPoint(wb, year, month, day, count)
                                
                    count += 1
                    while True:
                        try:
                            t = time.time()
                            status = em.set_sd_wallbox_daily_data_point(*wddp)
                        except:
                            traceback.print_exc()
                            time.sleep(1)
                            break
                        if status == em.DATA_STATUS_QUEUE_FULL:
                            print('full')
                            time.sleep(0.05)
                            continue
                        elif status == em.DATA_STATUS_OK:
                            print('call set_sd_wallbox_daily_data_point ({0:02f}): {1}'.format((time.time()-t)*1000, wddp))
                            break
                        else:
                            print('call set_sd_wallbox_daily_data_point error {0}'.format(status))
                            sys.exit(0)

    print("Read 1 month daily wb data")
    for year in range(22, 23):
        for month in range(1, 2):
            for day in range(1, 2):
                for wb in range(1, 3):
                    ret = 1
                    while ret != 0:
                        ret = em.get_sd_wallbox_daily_data_points(wb, year, month, day, 30)
                        if ret != 0:
                            print("ret {0}".format(ret))
                        time.sleep(1)


    time.sleep(10)
    print("Write 1 day 5min em data")
    for year in range(22, 23):
        for month in range(1, 2):
            for day in range(1, 2):
                for hour in range(0, 24):
                    for minute in range(0, 12):
                        emdp = EnergyManagerDataPoint(year, month, day, hour, minute*5, count % 128, count % 0xFFFF, [count % 0xFFFF]*6)
                            
                        count += 1
                        while True:
                            try:
                                t = time.time()
                                status = em.set_sd_energy_manager_data_point(*emdp)
                            except:
                                traceback.print_exc()
                                time.sleep(1)
                                break
                            if status == em.DATA_STATUS_QUEUE_FULL:
                                print('full')
                                time.sleep(0.05)
                                continue
                            elif status == em.DATA_STATUS_OK:
                                print('call set_sd_energy_manager_data_point ({0:02f}): {1}'.format((time.time()-t)*1000, emdp))
                                break
                            else:
                                print('call set_sd_energy_manager_data_point error {0}'.format(status))
                                sys.exit(0)

    print("Read 1 day 5min em data")
    for year in range(22, 23):
        for month in range(1, 2):
            for day in range(1, 2):
                ret = 1
                while ret != 0:
                    ret = em.get_sd_energy_manager_data_points(year, month, day, 0, 0, 12*24)
                    if ret != 0:
                        print("ret {0}".format(ret))
                    time.sleep(1)


    time.sleep(10)
    print("Write 1 month daily em data")
    for year in range(22, 23):
        for month in range(1, 2):
            for day in range(1, 31):
                emddp = EnergyManagerDailyDataPoint(year, month, day, count % 0xFFFF, count % 0xFFFF, [count % 0xFFFF]*6, [count % 0xFFFF]*6)
                            
                count += 1
                while True:
                    try:
                        t = time.time()
                        status = em.set_sd_energy_manager_daily_data_point(*emddp)
                    except:
                        traceback.print_exc()
                        time.sleep(1)
                        break
                    if status == em.DATA_STATUS_QUEUE_FULL:
                        print('full')
                        time.sleep(0.05)
                        continue
                    elif status == em.DATA_STATUS_OK:
                        print('call set_sd_wallbox_daily_data_point ({0:02f}): {1}'.format((time.time()-t)*1000, emddp))
                        break
                    else:
                        print('call set_sd_wallbox_daily_data_point error {0}'.format(status))
                        sys.exit(0)

    print("Read 1 month daily em data")
    for year in range(22, 23):
        for month in range(1, 2):
            for day in range(1, 2):
                ret = 1
                while ret != 0:
                    ret = em.get_sd_energy_manager_daily_data_points(year, month, day, 30)
                    if ret != 0:
                        print("ret {0}".format(ret))
                    time.sleep(1)
    time.sleep(10)
