''' Tests for generic HID controller. '''
import pytest
from device_data.test_data_generator import btns_generic_test_data
from device_data.test_data_generator import btns_generic_to_wired_test_data
from bit_helper import swap32
from device_data.hid import hid_btns_mask
from device_data.n64 import n64_btns_mask
from device_data.br import system, dev_mode, bt_conn_type


DEVICE_NAME = 'HID Generic'
HID_DESC = ('05010905a1010901a100093009311500'
            '26ff0066000095027508810205091901'
            '292015002501660000952075018102c0'
            'c0')


def test_hid_controller_a_descriptor(blueretro):
    ''' Load a HID descriptor and check if it's parsed right. '''
    # Set device name
    blueretro.send_name(DEVICE_NAME)
    blueretro.expect('# dev: 0 type: 0:0 HID Generic')

    # Validate HID descriptor
    blueretro.send_hid_desc(HID_DESC)
    report = blueretro.expect_json('parsed_hid_report')

    assert report["report_id"] == 1
    assert report["usages"][2]["bit_offset"] == 16
    assert report["usages"][2]["bit_size"] == 32
    assert report["report_type"] == 2


def test_hid_controller_a_default_buttons_mapping(blueretro):
    ''' Press each buttons and check if default mapping is right. '''
    # Set device name
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

        wireless = blueretro.expect_json('wireless_input')
        br_generic = blueretro.expect_json('generic_input')

        assert wireless['btns'] == hid_btns
        assert br_generic['btns'][0] == br_btns


@pytest.mark.parametrize('blueretro', [[system.N64, dev_mode.PAD, bt_conn_type.BT_BR_EDR]], indirect=True)
def test_hid_controller_a_n64_buttons_mapping(blueretro):
    ''' Press each buttons and check if N64 mapping is right. '''
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

        wireless = blueretro.expect_json('wireless_input')
        wired = blueretro.expect_json('wired_output')

        assert wireless['btns'] == hid_btns
        assert wired['btns'] == n64_btns
