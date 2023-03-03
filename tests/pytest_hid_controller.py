import json
from bit_helper import bit, swap32
from device_data.hid import hid
from device_data.br import pad
from device_data.n64 import n64


DEVICE_NAME = 'HID Generic'
HID_DESC = '05010905a1010901a100093009311500' \
           '26ff0066000095027508810205091901' \
           '292015002501660000952075018102c0' \
           'c0'
buttons_wireless_to_generic = {
    0: 0,
    bit(hid.A): bit(pad.RB_DOWN), bit(hid.B): bit(pad.RB_RIGHT),
    bit(hid.C): bit(pad.RD_UP), bit(hid.X): bit(pad.RB_LEFT),
    bit(hid.Y): bit(pad.RB_UP), bit(hid.Z): bit(pad.RD_RIGHT),
    bit(hid.LB): bit(pad.LS), bit(hid.RB): bit(pad.RS),
    bit(hid.L): bit(pad.LM), bit(hid.R): bit(pad.RM),
    bit(hid.SELECT): bit(pad.MS), bit(hid.START): bit(pad.MM),
    bit(hid.MENU): bit(pad.MT), bit(hid.LJ): bit(pad.LJ),
    bit(hid.RJ): bit(pad.RJ), bit(15): bit(pad.RD_LEFT),
    bit(16): bit(pad.RD_DOWN), bit(17): bit(pad.MQ),
    bit(18): bit(pad.LT), bit(19): bit(pad.RT),
    0xFFFFFFFF: 0xFFFFF000,
}
buttons_wireless_to_n64 = {
    0: 0,
    bit(hid.A): bit(n64.A), bit(hid.B): bit(n64.C_DOWN),
    bit(hid.C): bit(n64.C_UP), bit(hid.X): bit(n64.B),
    bit(hid.Y): bit(n64.C_LEFT), bit(hid.Z): bit(n64.C_RIGHT),
    bit(hid.LB): bit(n64.L), bit(hid.RB): bit(n64.R),
    bit(hid.L): bit(n64.Z), bit(hid.R): bit(n64.Z),
    bit(hid.SELECT): 0, bit(hid.START): bit(n64.START),
    bit(hid.MENU): 0, bit(hid.LJ): 0,
    bit(hid.RJ): 0, bit(15): 0,
    bit(16): 0, bit(17): 0,
    bit(18): 0, bit(19): 0,
    0xFFFFFFFF: 0x3FF0,
}


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
