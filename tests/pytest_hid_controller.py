''' Tests for generic HID controller. '''
from device_data.test_data_generator import btns_generic_test_data
from device_data.test_data_generator import btns_generic_to_wired_test_data
from bit_helper import bit, swap32
from device_data.hid import hid, hid_btns_mask
from device_data.n64 import N64, n64, n64_btns_mask
from device_data.br import pad
from device_data.gc import GC


DEVICE_NAME = 'HID Generic'
HID_DESC = ('05010905a1010901a100093009311500'
            '26ff0066000095027508810205091901'
            '292015002501660000952075018102c0'
            'c0')


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
    for hid_btns, br_btns in btns_generic_test_data(hid_btns_mask):
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
    for hid_btns, n64_btns in btns_generic_to_wired_test_data(hid_btns_mask, n64_btns_mask):
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
