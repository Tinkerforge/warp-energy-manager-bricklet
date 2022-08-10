#!/usr/bin/env python3
# -*- coding: utf-8 -*-


import time
import sys
import struct
import socket
import threading


"""
struct packet_header {
    uint8_t seq_num;
    uint8_t version;
    uint16_t padding;
} __attribute__ ((packed));

struct request_packet {
    packet_header header;

    uint16_t allocated_current;
} __attribute__ ((packed));

struct response_packet {
    packet_header header;

    uint8_t iec61851_state;
    uint8_t vehicle_state;
    uint8_t error_state;
    uint8_t charge_release;
    uint32_t uptime;
    uint32_t charging_time;
    uint16_t allowed_charging_current;
    uint16_t supported_current;
    bool managed;
} __attribute__ ((packed));
"""



class WARPChargerSimulator:
    header_format = "<BBH"
    request_format = header_format + "H"
    response_format = header_format + "BBBIIHH?"

    request_len = struct.calcsize(request_format)
    response_len = struct.calcsize(response_format)


    def __init__(self):
        self.listen_addr = "192.168.178.27"
        self.addr = None
        self.loop_recv_run = True
        self.loop_send_run = True

        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.bind((self.listen_addr, 34128))
        self.sock.setblocking(False)

        self.req_seq_num = 0
        self.req_version = 0
        self.req_allocated_current = 0
        self.resp_seq_num = 0
        self.resp_block_seq_num = False
        self.resp_version = 0
        self.resp_wrong_proto_version = 0
        self.resp_iec61851_state = 0
        self.resp_charger_state = 0
        self.resp_error_state = 0
        self.resp_uptime = 0
        self.resp_block_uptime = False
        self.resp_charging_time = 0
        self.resp_allowed_charging_current = 0
        self.resp_supported_current = 32000
        self.resp_managed = True

        self.next_seq_num = 0
        self.protocol_version = 3
        self.start = time.time()
        self.charging_time_start = 0

        self.car_connected = True


        self.thread_recv = threading.Thread(target=self.loop_recv)
        self.thread_recv.start()
        self.thread_send = threading.Thread(target=self.loop_send)
        self.thread_send.start()

    def set_ma(self, value):
        self.car_connected = (value != 0)
        print(value, self.car_connected)
        self.update()

    def update(self):
        self.resp_allowed_charging_current = self.req_allocated_current
        if self.resp_allowed_charging_current == 0 and self.car_connected:
            self.resp_iec61851_state = 1
            self.resp_charger_state = 2
        elif self.car_connected:
            self.resp_iec61851_state = 2
            self.resp_charger_state = 3
        else:
            self.resp_iec61851_state = 0
            self.resp_charger_state = 0

    def loop_recv(self):
        while self.loop_recv_run:
            time.sleep(0.01)
            try:
                data, self.addr = self.sock.recvfrom(self.request_len)
            except BlockingIOError:
                continue
            if len(data) != self.request_len:
                continue

            seq_num, version, _, allocated_current = struct.unpack(self.request_format, data)
            print("recv", seq_num, version, _, allocated_current)

            self.req_seq_num = seq_num
            self.req_version = version
            self.req_allocated_current = allocated_current
            if allocated_current == 0 and self.resp_iec61851_state == 2:
                self.charging_time_start = 0
                self.resp_iec61851_state = 1
                self.resp_charger_state = 1
            
            self.update()

    def loop_send(self):
        while self.loop_send_run:
            time.sleep(1)
            if self.addr is None:
                continue

            self.resp_seq_num = self.next_seq_num
            self.resp_version = self.protocol_version

            if self.resp_block_uptime:
                uptime = self.resp_uptime
            else:
                uptime = int((time.time() - self.start) * 1000)
            self.resp_uptime = uptime

            if self.charging_time_start == 0 and self.resp_iec61851_state == 2:
                self.charging_time_start = time.time()

            if self.charging_time_start != 0 and self.resp_iec61851_state == 0:
                self.charging_time_start = 0

            charging_time = 0 if self.charging_time_start == 0 else int((time.time() - self.charging_time_start) * 1000)
            self.resp_charging_time = charging_time

            b = struct.pack(self.response_format,
                            self.next_seq_num,
                            self.protocol_version if not self.resp_wrong_proto_version else 234,
                            0,
                            self.resp_iec61851_state,
                            self.resp_charger_state,
                            self.resp_error_state,
                            uptime,
                            charging_time,
                            self.resp_allowed_charging_current,
                            self.resp_supported_current,
                            self.resp_managed)

            if not self.resp_block_seq_num:
                self.next_seq_num += 1
                self.next_seq_num %= 256

            print("send",   self.next_seq_num,
                            self.protocol_version if not self.resp_wrong_proto_version else 234,
                            0,
                            self.resp_iec61851_state,
                            self.resp_charger_state,
                            self.resp_error_state,
                            uptime,
                            charging_time,
                            self.resp_allowed_charging_current,
                            self.resp_supported_current,
                            self.resp_managed)
            self.sock.sendto(b, self.addr)
        
if __name__ == "__main__":
    wc_sim = WARPChargerSimulator()
    input("Press key to exit\n")
    wc_sim.loop_recv_done = False
    wc_sim.loop_send_done = False
