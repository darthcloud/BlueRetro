#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2024 Jacques Gagnon
# SPDX-License-Identifier: Apache-2.0
import struct
import subprocess
import sys
import time
from argparse import ArgumentParser
from enum import IntEnum
from glob import glob
from serial import Serial
from socket import socket, AF_INET, SOCK_STREAM
from uuid import uuid4


socat = None
btmon = None
sock = None


class H5Type(IntEnum):
    ACK_PKT = 0
    CMD_PKT = 1
    ACL_PKT = 2
    SCO_PKT = 3
    EVT_PKT = 4
    ISO_PKT = 5
    VENDOR_PKT = 14
    LC_PKT = 15


class BtsnoopOpcode(IntEnum):
    NEW_INDEX = 0
    DEL_INDEX = 1
    COMMAND_PKT = 2
    EVENT_PKT = 3
    ACL_TX_PKT = 4
    ACL_RX_PKT = 5
    SCO_TX_PKT = 6
    SCO_RX_PKT = 7
    OPEN_INDEX = 8
    CLOSE_INDEX = 9
    INDEX_INFO = 10
    VENDOR_DIAG = 11
    SYSTEM_NOTE = 12
    USER_LOGGING = 13
    CTRL_OPEN = 14
    CTRL_CLOSE = 15
    CTRL_COMMAND = 16
    CTRL_EVENT = 17
    ISO_TX_PKT = 18
    ISO_RX_PKT = 19


def btsnoop_opcode(tx, type):
    opcode = None

    if type == H5Type.CMD_PKT:
        opcode = BtsnoopOpcode.COMMAND_PKT
    elif type == H5Type.ACL_PKT:
        if tx:
            opcode = BtsnoopOpcode.ACL_TX_PKT
        else:
            opcode = BtsnoopOpcode.ACL_RX_PKT
    elif type == H5Type.SCO_PKT:
        if tx:
            opcode = BtsnoopOpcode.SCO_TX_PKT
        else:
            opcode = BtsnoopOpcode.SCO_RX_PKT
    elif type == H5Type.EVT_PKT:
        opcode = BtsnoopOpcode.EVENT_PKT
    elif type == H5Type.ISO_PKT:
        if tx:
            opcode = BtsnoopOpcode.ISO_TX_PKT
        else:
            opcode = BtsnoopOpcode.ISO_RX_PKT
    return opcode


def parse_args():
    parser = ArgumentParser()
    parser.add_argument('--tty0', help='TTY connected to H5 HCI TX|RX')
    parser.add_argument('--tty1', help='TTY connected to H5 HCI TX|RX')
    parser.add_argument('-b', '--baud', type=int, default=1500000, help='TTY baudrate')
    parser.add_argument('-d', '--data', type=int, default=8, help='TTY data bytesize')
    parser.add_argument('-p', '--parity', default='E', help='TTY parity: N, E, O, M, S')
    parser.add_argument('-s', '--stop', type=float, default=1, help='TTY stopbits')
    parser.add_argument('-w', '--write', help='Save trace in btsnoop format')
    return parser.parse_args()


def except_hook(type, value, tb):
    global socat, btmon, sock

    print('')

    if isinstance(sock, socket):
        sock.close()
        print('Socket closed')
    if isinstance(btmon, subprocess.Popen):
        btmon.kill()
        print('btmon closed')
    if isinstance(socat, subprocess.Popen):
        socat.kill()
        print('socat closed')
   
    if type is KeyboardInterrupt:
        print('Capture ended sucessfully')
        sys.exit(0)
    else:
        import traceback
        error = ''.join(traceback.format_exception(type, value, tb))
        print(error)


def main():
    global socat, btmon, sock

    # Get arguments
    args = parse_args()

    # Use first two USB tty by default
    if args.tty0 is None and args.tty1 is None:
        args.tty0, args.tty1, *_ = glob('/dev/ttyUSB*')

    # Deinit hook on failure or keyboard exit
    sys.excepthook = except_hook

    # Create virtual tty
    socat_tty = f'/tmp/socat-{uuid4()}'
    socat_cmd = [
        'socat', '-dd',
        f'PTY,raw,echo=0,link={socat_tty},nonblock,group-late=dialout,mode=660,b921600',
        'TCP-LISTEN:0,reuseaddr,fork'
    ]
    socat = subprocess.Popen(socat_cmd, stdout = subprocess.DEVNULL, stderr = subprocess.PIPE)
    socat.stderr.readline()
    socat_port = int(socat.stderr.readline().decode().split(':')[-1])

    # Launch Bluez's btmon on our virtual tty
    btmon_cmd = ['btmon', '--tty', socat_tty, '--tty-speed', '921600']
    if args.write is not None:
        btmon_cmd.extend(['-w', args.write])
    btmon = subprocess.Popen(btmon_cmd)

    # Connect to virtual tty socket
    sock = socket(AF_INET, SOCK_STREAM)
    sock.connect(('localhost', socat_port))

    # Setup ttys we want to sniff H5 from
    ttys = []
    if args.tty0 is not None:
        ttys.append(Serial(port=args.tty0, baudrate=args.baud, bytesize=args.data, parity=args.parity, stopbits=args.stop))
    if args.tty1 is not None:
        ttys.append(Serial(port=args.tty1, baudrate=args.baud, bytesize=args.data, parity=args.parity, stopbits=args.stop))

    # Test with btsnoop note
    time.sleep(1)
    note = b'Ready to capture...'
    note_hdr = struct.pack('<HHBB', 4 + len(note), BtsnoopOpcode.SYSTEM_NOTE, 0, 0)
    sock.send(note_hdr + note)

    # ttys read & socket send loop
    while True:
        for tty in ttys:
            if tty.in_waiting:
                # Get slip frame from tty
                slip = tty.read_until(expected=b'\xC0')

                # Best-effort timestamp, no way to know precisely when this got into the HW buffer 
                ts = time.perf_counter_ns() // 100000

                # Decode slip frame
                pkt = slip[:-1].replace(b'\xDB\xDC', b'\xC0').replace(b'\xDB\xDD', b'\xDB')
                if len(pkt) < 4:
                    continue
                
                # Validate H5 header
                chksum = sum(pkt[0:4]) % 256
                if chksum != 0xFF:
                    continue

                # Extract payload len & type
                len_type = struct.unpack("<BHB", pkt[0:4])[1]
                payload_len = len_type >> 4
                type = len_type & 0xF

                # Flag the TX tty
                # This assume we get some cmd pkt before any ACL one
                if type == 1:
                    tty.dir = 'TX'

                # Get BTSNOOP opcode base on type & direction
                opcode = btsnoop_opcode(hasattr(tty, 'dir'), type)
                if opcode is None:
                    continue

                # Craft btmon tty header & send to socat socket
                btmon_hdr = struct.pack("<HHBBBI", payload_len + 4 + 5, opcode, 0, 5, 8, ts)
                sock.send(btmon_hdr + pkt[4:4 + payload_len])


if __name__ == "__main__":
    main()
