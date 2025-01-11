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


socat = None
btmon = None
sock = None


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



def parse_args():
    parser = ArgumentParser()
    parser.add_argument('--tty', help='TTY connected to BTSNOOP monitor')
    parser.add_argument('-b', '--baud', type=int, default=921600, help='TTY baudrate')
    parser.add_argument('-d', '--data', type=int, default=8, help='TTY data bytesize')
    parser.add_argument('-p', '--parity', default='N', help='TTY parity: N, E, O, M, S')
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
    if args.tty is None:
        args.tty, *_ = glob('/dev/ttyUSB*')

    # Deinit hook on failure or keyboard exit
    sys.excepthook = except_hook

    # Create virtual tty
    socat_cmd = [
        'socat',
        'PTY,raw,echo=0,link=/tmp/socat,nonblock,group-late=dialout,mode=660,b921600',
        'TCP-LISTEN:8008,reuseaddr,fork'
    ]
    socat = subprocess.Popen(socat_cmd, stdout = subprocess.DEVNULL, stderr = subprocess.DEVNULL)
    time.sleep(1)

    # Launch Bluez's btmon on our virtual tty
    btmon_cmd = ['btmon', '--tty', '/tmp/socat', '--tty-speed', '921600']
    if args.write is not None:
        btmon_cmd.extend(['-w', args.write])
    btmon = subprocess.Popen(btmon_cmd)

    # Connect to virtual tty socket
    sock = socket(AF_INET, SOCK_STREAM)
    sock.connect(('localhost', 8008))

    # Setup ttys we want to sniff BTSNOOP from
    tty = Serial(port=args.tty, baudrate=args.baud, bytesize=args.data, parity=args.parity, stopbits=args.stop)

    # ttys read & socket send loop
    pkt = b''
    while True:
        pkt = pkt[1:]
        pkt += tty.read(11 - len(pkt))

        if len(pkt) != 11:
            continue

        pkt_len, opcode, flags, ext_len, ext_type, _ = struct.unpack('<HHBBBI', pkt)

        if opcode not in BtsnoopOpcode:
            continue

        if flags != 0:
            continue

        if ext_len != 5:
            continue

        if ext_type != 8:
            continue

        pkt += tty.read(pkt_len - 9)

        sock.send(pkt)
        print(pkt.hex())
        pkt = b''


if __name__ == "__main__":
    main()
