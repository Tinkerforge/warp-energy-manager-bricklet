#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import time
import sys
import struct
import socket
import threading

"""
struct cm_packet_header {
    uint16_t magic;
    uint16_t length;
    uint16_t seq_num;
    uint8_t version;
    uint8_t padding;
} __attribute__((packed));

struct cm_command_v1 {
    uint16_t allocated_current;
    /* command_flags
    bit 6 - control pilot permanently disconnected
    Other bits must be sent unset and ignored on reception.
    */
    uint8_t command_flags;
    uint8_t padding;
} __attribute__((packed));

struct cm_command_packet {
    cm_packet_header header;
    cm_command_v1 v1;
} __attribute__((packed));

struct cm_state_v1 {
    uint32_t feature_flags; /* unused */
    uint32_t evse_uptime;
    uint32_t charging_time;
    uint16_t allowed_charging_current;
    uint16_t supported_current;

    uint8_t iec61851_state;
    uint8_t charger_state;
    uint8_t error_state;
    /* state_flags
    bit 7 - managed
    bit 6 - control_pilot_permanently_disconnected
    bit 5 - L1_connected
    bit 4 - L2_connected
    bit 3 - L3_connected
    bit 2 - L1_active
    bit 1 - L2_active
    bit 0 - L3_active
    */
    uint8_t state_flags;
    float line_voltages[3];
    float line_currents[3];
    float line_power_factors[3];
    float energy_rel;
    float energy_abs;
} __attribute__((packed));

struct cm_state_packet {
    cm_packet_header header;
    cm_state_v1 v1;
} __attribute__((packed));
"""

class WARPChargerSimulator:
    header_format = "<HHHBx"
    command_format = header_format + "HBx"
    state_format = header_format + "IIIHHBBBBfffffffffff"

    command_len = struct.calcsize(command_format)
    state_len = struct.calcsize(state_format)


    def __init__(self):
        self.listen_addr = "192.168.242.90"
        self.addr = None
        self.loop_recv_run = True
        self.loop_send_run = True

        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.bind((self.listen_addr, 34128))
        self.sock.setblocking(False)

        self.req_seq_num = 0
        self.req_version = 0
        self.req_allocated_current = 0
        self.req_cp_disconnect = False
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
        self.resp_cp_disconnect_state = False

        self.next_seq_num = 0
        self.protocol_version = 1
        self.start = time.time()
        self.charging_time_start = 0

        self.car_connected = True

        self.thread_recv = threading.Thread(target=self.loop_recv)
        self.thread_recv.start()
        self.thread_send = threading.Thread(target=self.loop_send)
        self.thread_send.start()

    def set_ma(self, value):
        self.car_connected = (value != 0)
        print("set_ma():", value, self.car_connected)
        self.update()

    def update(self):
        self.resp_allowed_charging_current = self.req_allocated_current
        if self.resp_allowed_charging_current == 0 and self.car_connected:
            self.resp_iec61851_state = 1
            self.resp_charger_state = 1
        elif self.car_connected:
            self.resp_iec61851_state = 2
            self.resp_charger_state = 3
        else:
            self.resp_iec61851_state = 0
            self.resp_charger_state = 0
        print("update():", self.resp_iec61851_state, self.resp_charger_state)

    def loop_recv(self):
        while self.loop_recv_run:
            time.sleep(0.01)
            try:
                data, self.addr = self.sock.recvfrom(self.command_len)
            except BlockingIOError:
                continue
            if len(data) != self.command_len:
                continue

            magic, length, seq_num, version, allocated_current, command_flags = struct.unpack(self.command_format, data)
            print("recv", magic, length, seq_num, version, allocated_current, command_flags)

            self.req_seq_num = seq_num
            self.req_version = version
            self.req_allocated_current = allocated_current
            self.req_cp_disconnect = (command_flags & 0x40 != 0)
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

            flags = 0
            flags |= 0x80 if self.resp_managed else 0
            flags |= 0x40 if self.resp_cp_disconnect_state else 0

            b = struct.pack(self.state_format,
                            34127,                              # magic
                            self.state_len,                     # length
                            self.next_seq_num,
                            self.protocol_version if not self.resp_wrong_proto_version else 0,
                            0,                                  # features
                            uptime,
                            charging_time,
                            self.resp_allowed_charging_current,
                            self.resp_supported_current,
                            self.resp_iec61851_state,
                            self.resp_charger_state,
                            self.resp_error_state,
                            flags,                              # flags
                            0,  # LV0
                            0,  # LV1
                            0,  # LV2
                            0,  # LC0
                            0,  # LC1
                            0,  # LC2
                            0,  # LPF0
                            0,  # LPF1
                            0,  # LPF2
                            0,  # energy_rel
                            0)  # energy_abs

            if not self.resp_block_seq_num:
                self.next_seq_num += 1
                self.next_seq_num %= 65536

            print("send",   34127,
                            self.state_len,
                            self.next_seq_num,
                            self.protocol_version if not self.resp_wrong_proto_version else 0,
                            0,
                            uptime,
                            charging_time,
                            self.resp_allowed_charging_current,
                            self.resp_supported_current,
                            self.resp_iec61851_state,
                            self.resp_charger_state,
                            self.resp_error_state,
                            0x80 if self.resp_managed else 0,
                            0,  # LV0
                            0,  # LV1
                            0,  # LV2
                            0,  # LC0
                            0,  # LC1
                            0,  # LC2
                            0,  # LPF0
                            0,  # LPF1
                            0,  # LPF2
                            0,  # energy_rel
                            0)  # energy_abs
            self.sock.sendto(b, self.addr)
        
if __name__ == "__main__":
    wc_sim = WARPChargerSimulator()
    input("Press key to exit\n")
    wc_sim.loop_recv_done = False
    wc_sim.loop_send_done = False
