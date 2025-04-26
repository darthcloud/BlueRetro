#!/usr/bin/env python3

import json
import struct
from websocket import WebSocket
from time import sleep


class BlueRetroInjector:
    def __init__(self, url="ws://localhost:8001/ws", handle=0):
        self.handle = handle
        self.ws = WebSocket()
        self.ws.connect(url)

    def __del__(self):
        self.ws.close()

    def __write(self, cmd, handle, data=b''):
        self.ws.send_binary(struct.pack("<BBH", cmd, handle, len(data)) + data)

    def __read(self):
        return json.loads(self.ws.recv_data()[1].decode())

    def get_logs(self):
        pass

    def connect(self, bt_conn_type=0):
        self.__write(0x01, self.handle, bt_conn_type.to_bytes(1, 'big'))
        return self.__read()

    def disconnect(self):
        self.__write(0x02, self.handle)
        return self.__read()

    def send_name(self, name):
        self.__write(0x03, self.handle, (name + '\0').encode())
        return self.__read()

    def send_hid_desc(self, hid_desc):
        self.__write(0x04, self.handle, hid_desc)
        return self.__read()

    def send_hid_report(self, hid_report):
        self.__write(0x05, self.handle, bytes([0x00]) * 9 + bytes.fromhex(hid_report))
        return self.__read()

    def send_to_bridge(self, hid_report_id, hid_report):
        self.__write(0x06, self.handle, bytes([hid_report_id]) + bytes.fromhex(hid_report))
        return self.__read()

    def send_global_cfg(self, cfg):
        self.__write(0x07, self.handle, bytes.fromhex(cfg))
        return self.__read()

    def send_out_cfg(self, index, cfg):
        self.__write(0x08, self.handle, index.to_bytes(1, 'big') + bytes.fromhex(cfg))
        return self.__read()

    def send_in_cfg(self, index, cfg):
        self.__write(0x09, self.handle, index.to_bytes(1, 'big') + bytes.fromhex(cfg))
        return self.__read()

    def send_system_id(self, system_id):
        self.__write(0x0A, self.handle, system_id.to_bytes(1, 'big'))
        return self.__read()

    def send_vid_pid(self, vid, pid):
        self.__write(0x0B, self.handle, struct.pack("<HH", vid, pid))
        return self.__read()

    def send_cov_dump(self):
        self.__write(0x0C, self.handle)
        return self.__read()


def main():
    name = 'BlueRetro Test'
    hid_desc = bytes.fromhex(
        '05010906a101850175019508050719e0'
        '29e71500250181029501750881039505'
        '75010508190129059102950175039103'
        '95067508150026ff000507190029ff81'
        '00c0050c0901a1018502150025017501'
        '95160ab1010a23020aae010a8a010940'
        '096f0a210209b609cd09b509e209ea09'
        'e909300a83010a24020a06030a08030a'
        '01030a83010a0a030970810295017502'
        '8103c0'
    )
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


if __name__ == "__main__":
    main()
