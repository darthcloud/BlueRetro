''' Tests for generic HID controller. '''
from bit_helper import bit, swap32
from device_data.hid import hid
from device_data.br import pad
from device_data.n64 import N64, n64
from device_data.gc import GC


DEVICE_NAME = 'HID Generic'
HID_DESC = ('05010905a1010901a100093009311500'
            '26ff0066000095027508810205091901'
            '292015002501660000952075018102c0'
            'c0')
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
    ''' Load a HID descriptor and check if it's parsed right. '''
    # Check device is connected
    blueretro.disconnect()
    blueretro.send_system_id(GC)
    blueretro.connect()
    blueretro.send_name(DEVICE_NAME)

    blueretro.get_logs()
    blueretro.expect('# dev: 0 type: 0:0 HID Generic', timeout=1)

    # Validate HID descriptor
    blueretro.send_hid_desc(HID_DESC)

    blueretro.get_logs()
    report = blueretro.expect_json('parsed_hid_report')

    assert report["report_id"] == 1
    assert report["usages"][2]["bit_offset"] == 16
    assert report["usages"][2]["bit_size"] == 32
    assert report["report_type"] == 2
    assert report["device_type"] == 0
    assert report["device_subtype"] == 0

    blueretro.disconnect()


def test_hid_controller_default_buttons_mapping(blueretro):
    ''' Press each buttons and check if default mapping is right. '''
    # Connect device
    blueretro.disconnect()
    blueretro.send_system_id(GC)
    blueretro.connect()
    blueretro.send_name(DEVICE_NAME)
    blueretro.send_hid_desc(HID_DESC)

    # Init adapter with a few neutral state report
    for _ in range(2):
        blueretro.send_hid_report('a101808000000000')

    blueretro.flush_logs()

    # Validate buttons default mapping
    for hid_btns, br_btns in buttons_wireless_to_generic.items():
        blueretro.send_hid_report(
            'a1018080'
            f'{swap32(hid_btns):08x}'
        )

        blueretro.get_logs()

        wireless = blueretro.expect_json('wireless_input')
        br_generic = blueretro.expect_json('generic_input')
        _br_mapped = blueretro.expect_json('mapped_input')
        _wired = blueretro.expect_json('wired_output')

        assert wireless['btns'] == hid_btns
        assert br_generic['btns'][0] == br_btns

    blueretro.disconnect()


def test_hid_controller_n64_buttons_mapping(blueretro):
    ''' Press each buttons and check if N64 mapping is right. '''
    # Connect device
    blueretro.disconnect()
    blueretro.send_system_id(N64)
    blueretro.connect()
    blueretro.send_name(DEVICE_NAME)
    blueretro.send_hid_desc(HID_DESC)

    # Init adapter with a few neutral state report
    for _ in range(2):
        blueretro.send_hid_report('a101808000000000')

    blueretro.flush_logs()

    # Validate buttons default mapping
    for hid_btns, n64_btns in buttons_wireless_to_n64.items():
        blueretro.send_hid_report(
            'a1018080'
            f'{swap32(hid_btns):08x}'
        )

        blueretro.get_logs()

        wireless = blueretro.expect_json('wireless_input')
        _br_generic = blueretro.expect_json('generic_input')
        _br_mapped = blueretro.expect_json('mapped_input')
        wired = blueretro.expect_json('wired_output')

        assert wireless['btns'] == hid_btns
        assert wired['btns'] == n64_btns

    blueretro.disconnect()
