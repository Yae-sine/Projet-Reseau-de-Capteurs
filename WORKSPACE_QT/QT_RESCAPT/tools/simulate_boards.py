#!/usr/bin/env python3
"""Simulate the 3 STM32 boards on a (virtual) CAN bus to test the IHM.
   sudo modprobe vcan && sudo ip link add dev vcan0 type vcan && sudo ip link set up vcan0
   python3 simulate_boards.py vcan0        then:  ./ihm_rescapt vcan0
Only the Python standard library is needed (socket.AF_CAN, Linux)."""
import math, socket, struct, sys, time

iface = sys.argv[1] if len(sys.argv) > 1 else "vcan0"
s = socket.socket(socket.AF_CAN, socket.SOCK_RAW, socket.CAN_RAW)
s.bind((iface,))
s.setblocking(False)

def send(cid, data):
    s.send(struct.pack("=IB3x8s", cid, len(data), data.ljust(8, b"\0")))

motor_mode, manual_v, thr, vl_mode = 0, 0, 10.0, 0
t0 = time.time(); last = {}
def every(name, period):
    now = time.time()
    if now - last.get(name, 0) >= period:
        last[name] = now; return True
    return False

while True:
    try:                                   # commands from the IHM
        while True:
            fr = s.recv(16)
            cid, ln, data = struct.unpack("=IB3x8s", fr)
            if cid == 0x12:
                motor_mode, manual_v, t = struct.unpack(">BhH", data[:5]); thr = t / 100
            elif cid == 0x24:
                vl_mode = data[0]
            elif cid == 0x34:
                print("IMU reset requested")
    except BlockingIOError:
        pass
    t = time.time() - t0
    if every("wind", 0.5):
        wind = 12 + 10 * math.sin(t / 5)
        v = manual_v if motor_mode == 1 else (200 if motor_mode == 2 and wind >= thr else 0)
        send(0x11, struct.pack(">HHBBh", int(wind * 100), int(wind / 2.4 * 0.5), motor_mode, v != 0, v))
    if every("vl", 0.2):
        val = int(100 + 80 * math.sin(t)) if vl_mode == 0 else int(400 + 300 * math.sin(t / 3))
        send(0x21, struct.pack(">BI", vl_mode, val))
    if every("env", 0.5):
        send(0x22, struct.pack(">Ih", int((1013 + 3 * math.sin(t / 20)) * 100), int((22 + math.sin(t / 10)) * 100)))
        send(0x23, struct.pack(">Hh", int((45 + 5 * math.sin(t / 15)) * 100), int((22.5 + math.sin(t / 10)) * 100)))
    if every("quat", 0.02):
        a = 0.6 * math.sin(t); ax = (0, math.sin(t / 2), math.cos(t / 2))
        q = (math.cos(a / 2), *(c * math.sin(a / 2) for c in ax))
        send(0x31, struct.pack(">hhhh", *(int(c * 10000) for c in q)))
    if every("raw", 0.1):
        send(0x32, struct.pack(">hhh", 10, -20, 1000))
        send(0x33, struct.pack(">hhh", 5, -3, 12))
    time.sleep(0.005)
