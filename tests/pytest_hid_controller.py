import json
import struct


DEVICE_NAME = 'HID Generic'
HID_DESC = '05010905a1010901a100093009311500' \
           '26ff0066000095027508810205091901' \
           '292015002501660000952075018102c0' \
           'c0'
buttons_wireless_to_generic = {
    0: 0,
    (1 << 0): (1 << 18), (1 << 1): (1 << 17), (1 << 2): (1 << 15),(1 << 3): (1 << 16),
    (1 << 4): (1 << 19), (1 << 5): (1 << 13), (1 << 6): (1 << 25), (1 << 7): (1 << 29),
    (1 << 8): (1 << 24), (1 << 9): (1 << 28), (1 << 10): (1 << 21), (1 << 11): (1 << 20),
    (1 << 12): (1 << 22), (1 << 13): (1 << 27), (1 << 14): (1 << 31), (1 << 15): (1 << 12),
    (1 << 16): (1 << 14), (1 << 17): (1 << 23), (1 << 18): (1 << 26), (1 << 19): (1 << 30),
    0xFFFFFFFF: 0xFFFFF000,
}
buttons_wireless_to_n64 = {
    0: 0,
    (1 << 0): (1 << 7), (1 << 1): (1 << 10), (1 << 2): (1 << 11),(1 << 3): (1 << 6),
    (1 << 4): (1 << 9), (1 << 5): (1 << 8), (1 << 6): (1 << 13), (1 << 7): (1 << 12),
    (1 << 8): (1 << 5), (1 << 9): (1 << 5), (1 << 10): 0, (1 << 11): (1 << 4),
    (1 << 12): 0, (1 << 13): 0, (1 << 14): 0, (1 << 15): 0,
    (1 << 16): 0, (1 << 17): 0, (1 << 18): 0, (1 << 19): 0,
    0xFFFFFFFF: 0x3FF0,
}


def swap32(i):
    return struct.unpack("<I", struct.pack(">I", i))[0]


def test_hid_controller_descriptor(blueretro):
    # Check device is connected
    blueretro.disconnect()
    blueretro.connect()
    blueretro.send_name(DEVICE_NAME)

    blueretro.get_logs()
    blueretro.expect('# dev: 0 type: 0:0 HID Generic', timeout=1)

    # Validate HID descriptor
    blueretro.send_hid_desc(HID_DESC)

    blueretro.get_logs()
    report = json.loads(blueretro.expect('({.*?parsed_hid_report.*?)\n', timeout=1).group(1))

    assert report["report_id"] == 1
    assert report["usages"][2]["bit_offset"] == 16
    assert report["usages"][2]["bit_size"] == 32
    assert report["report_type"] == 2
    assert report["device_type"] == 0
    assert report["device_subtype"] == 0

    blueretro.disconnect()


def test_hid_controller_default_buttons_mapping(blueretro):
    # Connect device
    blueretro.disconnect()
    blueretro.connect()
    blueretro.send_name(DEVICE_NAME)
    blueretro.send_hid_desc(HID_DESC)

    # Init adapter with a few neutral state report
    for _ in range(2):
        blueretro.send_hid_report('a101808000000000')

    blueretro.flush_logs()

    # Validate buttons default mapping
    for hid_btns, br_btns in buttons_wireless_to_generic.items():
        hid_report = 'a1018080' + '{:08x}'.format(swap32(hid_btns))
        blueretro.send_hid_report(hid_report)

        blueretro.get_logs()

        wireless = json.loads(blueretro.expect('({.*?wireless_input.*?)\n', timeout=1).group(1))
        br_generic = json.loads(blueretro.expect('({.*?generic_input.*?)\n', timeout=1).group(1))
        br_mapped = json.loads(blueretro.expect('({.*?mapped_input.*?)\n', timeout=1).group(1))
        wired = json.loads(blueretro.expect('({.*?wired_output.*?)\n', timeout=1).group(1))

        assert wireless['btns'] == hid_btns
        assert br_generic['btns'][0] == br_btns

    blueretro.disconnect()


def test_hid_controller_n64_buttons_mapping(blueretro):
    # Connect device
    blueretro.disconnect()
    blueretro.send_system_id(15)
    blueretro.connect()
    blueretro.send_name(DEVICE_NAME)
    blueretro.send_hid_desc(HID_DESC)

    # Init adapter with a few neutral state report
    for _ in range(2):
        blueretro.send_hid_report('a101808000000000')

    blueretro.flush_logs()

    # Validate buttons default mapping
    for hid_btns, n64_btns in buttons_wireless_to_n64.items():
        hid_report = 'a1018080' + '{:08x}'.format(swap32(hid_btns))
        blueretro.send_hid_report(hid_report)

        blueretro.get_logs()

        wireless = json.loads(blueretro.expect('({.*?wireless_input.*?)\n', timeout=1).group(1))
        br_generic = json.loads(blueretro.expect('({.*?generic_input.*?)\n', timeout=1).group(1))
        br_mapped = json.loads(blueretro.expect('({.*?mapped_input.*?)\n', timeout=1).group(1))
        wired = json.loads(blueretro.expect('({.*?wired_output.*?)\n', timeout=1).group(1))

        assert wireless['btns'] == hid_btns
        assert wired['btns'] == n64_btns

    blueretro.disconnect()
