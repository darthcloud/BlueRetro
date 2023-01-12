#!/usr/bin/env python3

import struct
from serial import Serial
from time import sleep


class BlueRetroInjector:
    def __init__(self, dev="/dev/ttyUSB1", handle=0):
        self.fd = Serial(port=dev, baudrate=921600, timeout=1)
        self.handle = handle

    def __write(self, cmd, handle, data=b''):
        self.fd.write(struct.pack("<BBH", cmd, handle, len(data)) + data)

    def __read(self):
        while self.fd.in_waiting:
            print(self.fd.readline().decode(), end='')

    def get_logs(self):
        self.__read()

    def connect(self):
        self.__write(0x01, self.handle)

    def disconnect(self):
        self.__write(0x02, self.handle)

    def send_name(self, name):
        self.__write(0x03, self.handle, (name + '\0').encode())

    def send_hid_desc(self, hid_desc):
        self.__write(0x04, self.handle, bytes.fromhex(hid_desc))

    def send_hid_report(self, hid_report):
        self.__write(0x05, self.handle, bytes([0x00]) * 9 + bytes.fromhex(hid_report))

    def send_to_bridge(self, hid_report_id, hid_report):
        self.__write(0x06, self.handle, bytes([hid_report_id]) + bytes.fromhex(hid_report))

    def send_global_cfg(self, cfg):
        self.__write(0x07, self.handle, bytes.fromhex(cfg))

    def send_out_cfg(self, index, cfg):
        self.__write(0x08, self.handle, index.to_bytes + bytes.fromhex(cfg))

    def send_in_cfg(self, index, cfg):
        self.__write(0x09, self.handle, index.to_bytes + bytes.fromhex(cfg))


def main():
    name = 'BlueRetro Test'
    hid_desc = '05010906a101850175019508050719e0' \
               '29e71500250181029501750881039505' \
               '75010508190129059102950175039103' \
               '95067508150026ff000507190029ff81' \
               '00c0050c0901a1018502150025017501' \
               '95160ab1010a23020aae010a8a010940' \
               '096f0a210209b609cd09b509e209ea09' \
               'e909300a83010a24020a06030a08030a' \
               '01030a83010a0a030970810295017502' \
               '8103c0'
    hid_report = 'a1010000500000000000'

    bri = BlueRetroInjector()

    bri.connect()

    bri.send_name(name)

    bri.send_hid_desc(hid_desc)

    bri.send_hid_report(hid_report)
    bri.send_hid_report(hid_report)
    bri.send_hid_report(hid_report)
    bri.send_hid_report(hid_report)
    bri.send_hid_report(hid_report)
    bri.send_hid_report(hid_report)
    bri.send_hid_report(hid_report)

    bri.disconnect()

    sleep(1)
    bri.get_logs()


if __name__ == "__main__":
    main()
