#!/usr/bin/env python3

import struct
import sys
from time import sleep
from collections import namedtuple
from scapy.utils import rdpcap
from scapy.packet import Raw
from scapy.all import raw
import scapy.layers.bluetooth as bt
from injector import BlueRetroInjector


def reassemble_acl_pkt(trace):
    reassembled_trace = []
    new_pkt = None
    new_pkt_len = 0
    for pkt in trace:
        if (pkt.haslayer(bt.HCI_ACL_Hdr) and pkt.PB == 0x02 and
                pkt.haslayer(bt.L2CAP_Hdr) and (len(pkt) - 5) < pkt[bt.L2CAP_Hdr].len):
            new_pkt = pkt
            new_pkt_len = pkt[bt.L2CAP_Hdr].len
            new_pkt.PB = 0x00
        elif pkt.haslayer(bt.HCI_ACL_Hdr) and pkt.PB == 0x01:
            new_pkt[Raw] = Raw(raw(new_pkt[Raw]) + raw(pkt)[5:])
            if (len(new_pkt) - 9) == new_pkt_len:
                reassembled_trace.append(new_pkt)
        else:
            reassembled_trace.append(pkt)
    return reassembled_trace


def reassemble_sdp_data(trace, handle):
    sdp_scid = None
    sdp_data = bytearray(b'')
    for pkt in trace:
        if (pkt.haslayer(bt.HCI_ACL_Hdr) and pkt.handle == handle and
                pkt.haslayer(bt.L2CAP_ConnReq) and pkt.psm == 0x0001):
            sdp_scid = pkt.scid
        if sdp_scid and pkt.haslayer(bt.L2CAP_Hdr) and pkt.cid == sdp_scid:
            end = struct.unpack(">H", raw(pkt[Raw])[5:7])[0] + 7
            sdp_data += raw(pkt[Raw])[7:end]
    return sdp_data


def extract_hid_desc(sdp_data):
    hid_desc = None
    if b'\x09\x02\x06' in sdp_data:
        idx = sdp_data.index(b'\x09\x02\x06') + 3

        if sdp_data[idx] == 0x35:
            idx += 2
        elif sdp_data[idx] == 0x36:
            idx += 3
        else:
            idx += 5

        if sdp_data[idx] == 0x35:
            idx += 4
        elif sdp_data[idx] == 0x36:
            idx += 5
        else:
            idx += 7

        if sdp_data[idx] == 0x25:
            idx += 1
            size = sdp_data[idx]
            idx += 1
        elif sdp_data[idx] == 0x26:
            idx += 1
            size = struct.unpack(">H", sdp_data[idx:idx + 2])[0]
            idx += 2
        else:
            idx += 1
            size = struct.unpack(">I", sdp_data[idx:idx + 4])[0]
            idx += 4

        hid_desc = sdp_data[idx:idx + size]
    return hid_desc


def reassemble_hid_desc(trace, handle, hid_desc_handle):
    hid_desc = bytearray(b'')
    process_rsp = 0
    for pkt in trace:
        if pkt.haslayer(bt.HCI_ACL_Hdr) and pkt.handle == handle:
            if (pkt.haslayer(bt.ATT_Read_Request) and
                    pkt[bt.ATT_Read_Request].gatt_handle == hid_desc_handle):
                process_rsp = 1
                continue
            if pkt.haslayer(bt.ATT_Read_Response) and process_rsp:
                hid_desc += pkt[bt.ATT_Read_Response].value
                process_rsp = 0
                continue
            if (pkt.haslayer(bt.ATT_Read_Blob_Request) and
                    pkt[bt.ATT_Read_Blob_Request].gatt_handle == hid_desc_handle):
                process_rsp = 1
                continue
            if pkt.haslayer(bt.ATT_Read_Blob_Response) and process_rsp:
                hid_desc += pkt[bt.ATT_Read_Blob_Response].value
                process_rsp = 0
                continue
    return hid_desc


def find_device(trace):
    mode = None
    bd_addr = None
    handle = None
    for pkt in trace:
        if pkt.haslayer(bt.HCI_Event_Hdr) and pkt.haslayer(Raw) and pkt.code == 0x03:
            handle = struct.unpack("<H", raw(pkt[Raw])[1:3])[0] & 0xFFF
            bd_addr = raw(pkt[Raw])[3:9]
            mode = "BR/EDR"
            break
        if (pkt.haslayer(bt.HCI_Event_LE_Meta) and pkt.haslayer(Raw) and
                (pkt[bt.HCI_Event_LE_Meta].event == 0x01 or pkt[bt.HCI_Event_LE_Meta].event == 0x0A)):
            handle = struct.unpack("<H", raw(pkt[Raw])[1:3])[0]
            bd_addr = raw(pkt[Raw])[5:11]
            mode = "LE"
            break
    return mode, bd_addr, handle


def find_device_name_bredr(trace, bd_addr):
    name = None
    for pkt in trace:
        if (pkt.haslayer(bt.HCI_Event_Hdr) and pkt.code == 0x2F and raw(pkt[Raw])[1:7] == bd_addr and
                raw(pkt[Raw])[15] and raw(pkt[Raw])[16] == 0x09):
            name = raw(pkt[Raw])[17:17 + raw(pkt[Raw])[15]].decode()
            break
        if pkt.haslayer(bt.HCI_Event_Hdr) and pkt.code == 0x07 and raw(pkt[Raw])[1:7] == bd_addr:
            name = raw(pkt[Raw])[7:238].decode()
            break
    return name


def find_device_name_le(trace, bd_addr):
    name = None
    for pkt in trace:
        if (pkt.haslayer(bt.HCI_Event_LE_Meta) and pkt[bt.HCI_Event_LE_Meta].event == 0x0D and
                raw(pkt[Raw])[4:10] == bd_addr and raw(pkt[Raw])[26] == 0x08):
            name = raw(pkt[Raw])[27:27 + raw(pkt[Raw])[25]].decode()[:19]
            break
    return name


def find_hid_desc_att_handle(trace, handle):
    hid_desc_handle = None
    for pkt in trace:
        if (pkt.haslayer(bt.HCI_ACL_Hdr) and pkt.handle == handle and
                pkt.haslayer(bt.ATT_Read_By_Type_Response) and
                pkt[bt.ATT_Read_By_Type_Response].len == 7):
            for attribute in pkt[bt.ATT_Read_By_Type_Response].handles:
                if attribute.value[3:5] == b'\x4b\x2a':
                    hid_desc_handle = struct.unpack("<H", attribute.value[1:3])[0]
                    break
    return hid_desc_handle


def find_hid_report_att_handle(trace, handle):
    Report = namedtuple('Report', ['start_hdl', 'end_hdl', 'handle', 'id', 'type'])
    reports = []
    for pkt in trace:
        if (pkt.haslayer(bt.HCI_ACL_Hdr) and pkt.handle == handle and
                pkt.haslayer(bt.ATT_Read_By_Type_Response) and
                pkt[bt.ATT_Read_By_Type_Response].len == 7):
            for attribute in pkt[bt.ATT_Read_By_Type_Response].handles:
                if attribute.value[3:5] == b'\x4d\x2a':
                    reports.append([attribute.handle, 0xFFFF, 0, 0, 0])

    for idx, report in enumerate(reports):
        if idx > 0:
            reports[idx - 1][1] = report[0]

    for pkt in trace:
        if (pkt.haslayer(bt.HCI_ACL_Hdr) and pkt.handle == handle and
                pkt.haslayer(bt.ATT_Find_Information_Response) and
                pkt[bt.ATT_Find_Information_Response].format == 0x01):
            for attribute in pkt[bt.ATT_Find_Information_Response].handles:
                if attribute.value == 0x2908:
                    for report in reports:
                        if report[0] < attribute.handle < report[1]:
                            report[2] = attribute.handle

    process_rsp = 0
    report_id = 0
    for pkt in trace:
        if pkt.haslayer(bt.HCI_ACL_Hdr) and pkt.handle == handle:
            if pkt.haslayer(bt.ATT_Read_Request):
                for idx, report in enumerate(reports):
                    if pkt[bt.ATT_Read_Request].gatt_handle == report[2]:
                        process_rsp = 1
                        report_id = idx
                        break
                continue
            if pkt.haslayer(bt.ATT_Read_Response) and process_rsp:
                reports[report_id][3] = pkt[bt.ATT_Read_Response].value[0]
                reports[report_id][4] = pkt[bt.ATT_Read_Response].value[1]
                process_rsp = 0
                continue
    ret = []
    for report in reports:
        ret.append(Report(*report))
    return ret


def hid_report_generator_bredr(trace, handle):
    hid_intr_scid = None
    prev_pkt = None
    for pkt in trace:
        if (pkt.haslayer(bt.HCI_ACL_Hdr) and pkt.handle == handle and
                pkt.haslayer(bt.L2CAP_ConnReq) and pkt.psm == 0x0013):
            hid_intr_scid = pkt.scid
        if (hid_intr_scid and pkt.haslayer(bt.L2CAP_Hdr) and pkt.cid == hid_intr_scid and
                raw(pkt[Raw])[0] == 0xA1):
            if prev_pkt != raw(pkt[Raw]):
                yield raw(pkt[Raw])
                prev_pkt = raw(pkt[Raw])


def hid_report_generator_le(trace, handle, meta):
    prev_pkt = None
    for pkt in trace:
        if (pkt.haslayer(bt.HCI_ACL_Hdr) and pkt.handle == handle and
                pkt.haslayer(bt.ATT_Handle_Value_Notification)):
            for report in meta:
                if report.start_hdl < pkt.gatt_handle < report.end_hdl:
                    if prev_pkt != pkt.value:
                        yield report.id, pkt.value
                        prev_pkt = pkt.value
                    break


def main():
    trace = rdpcap(sys.argv[1])

    # Open serial port
    bri = BlueRetroInjector()

    # Create a device on BR
    bri.connect()
    bri.get_logs()

    # Reassemble any ACL fragmented frame first
    rtrace = reassemble_acl_pkt(trace)

    # Find BD_ADDR & ACL handle
    mode, bd_addr, handle = find_device(rtrace)

    if mode == "BR/EDR":
        # Find device name
        name = find_device_name_bredr(rtrace, bd_addr)

        # Send name to BR
        if name:
            bri.send_name(name)
            bri.get_logs()

        # Reassemble SDP data
        sdp_data = reassemble_sdp_data(rtrace, handle)

        # Find HID descriptor
        hid_desc = extract_hid_desc(sdp_data)

        # Send HID descriptor to BR
        if hid_desc:
            bri.send_hid_desc(hid_desc.hex())
            bri.get_logs()

        # Send HID report to BR
        hid_reports = hid_report_generator_bredr(rtrace, handle)
        for report in hid_reports:
            bri.send_hid_report(report.hex())
            sleep(0.1)
            bri.get_logs()

    elif mode == "LE":
        # Find device name
        name = find_device_name_le(trace, bd_addr)

        # Send name to BR
        if name:
            bri.send_name(name)
            bri.get_logs()

        # Find HID descriptor handle
        hid_desc_handle = find_hid_desc_att_handle(rtrace, handle)

        # Find reports handles, id & type
        reports_meta = find_hid_report_att_handle(rtrace, handle)

        # Reassemble HID descriptor
        hid_desc = reassemble_hid_desc(rtrace, handle, hid_desc_handle)

        # Send HID descriptor to BR
        if hid_desc:
            bri.send_hid_desc(hid_desc.hex())
            bri.get_logs()

        # Send HID report to BR
        hid_reports = hid_report_generator_le(rtrace, handle, reports_meta)
        for report_id, report in hid_reports:
            bri.send_to_bridge(report_id, report.hex())
            sleep(0.1)
            bri.get_logs()

    bri.disconnect()

    sleep(1)
    bri.get_logs()


if __name__ == "__main__":
    main()
