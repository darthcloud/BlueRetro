''' Tests for generic HID controller. '''
import pytest
from device_data.test_data_generator import btns_generic_test_data
from device_data.test_data_generator import btns_generic_to_wired_test_data
from bit_helper import swap32
from device_data.hid import hid_btns_mask
from device_data.n64 import n64_btns_mask
from device_data.br import system, dev_mode, bt_conn_type


DEVICE_NAME = 'HID Generic'
HID_DESC = bytes([
    0x05, 0x01,        # Usage Page (Generic Desktop)
    0x09, 0x05,        # Usage (Gamepad)
    0xA1, 0x01,        # Collection (Application)
    0x09, 0x01,        #   Usage (Pointer)
    0xA1, 0x00,        #   Collection (Physical)
    0x09, 0x30,        #     Usage (X)
    0x09, 0x31,        #     Usage (Y)
    0x15, 0x00,        #     Logical Minimum (0)
    0x26, 0xFF, 0x00,  #     Logical Maximum (255)
    0x66, 0x00, 0x00,  #     Unit (None)
    0x95, 0x02,        #     Report Count (2)
    0x75, 0x08,        #     Report Size (8)
    0x81, 0x02,        #     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x05, 0x09,        #     Usage Page (Button)
    0x19, 0x01,        #     Usage Minimum (0x01)
    0x29, 0x20,        #     Usage Maximum (0x20)
    0x15, 0x00,        #     Logical Minimum (0)
    0x25, 0x01,        #     Logical Maximum (1)
    0x66, 0x00, 0x00,  #     Unit (None)
    0x95, 0x20,        #     Report Count (32)
    0x75, 0x01,        #     Report Size (1)
    0x81, 0x02,        #     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0xC0,              #   End Collection
    0xC0,              # End Collection

    # 49 bytes
])


def test_hid_controller_a_descriptor(blueretro):
    ''' Load a HID descriptor and check if it's parsed right. '''
    # Set device name
    rsp = blueretro.send_name(DEVICE_NAME)
    assert rsp['device_name']['device_id'] == 0
    assert rsp['device_name']['device_type'] == 0
    assert rsp['device_name']['device_subtype'] == 0
    assert rsp['device_name']['device_name'] == 'HID Generic'

    # Validate HID descriptor
    rsp = blueretro.send_hid_desc(HID_DESC)
    assert rsp['hid_reports'][0]['report_id'] == 1
    assert rsp['hid_reports'][0]['usages'][2]["bit_offset"] == 16
    assert rsp['hid_reports'][0]['usages'][2]["bit_size"] == 32
    assert rsp['hid_reports'][0]['report_type'] == 2


def test_hid_controller_a_default_buttons_mapping(blueretro):
    ''' Press each buttons and check if default mapping is right. '''
    # Set device name
    blueretro.send_name(DEVICE_NAME)
    blueretro.send_hid_desc(HID_DESC)

    # Init adapter with a few neutral state report
    for _ in range(2):
        blueretro.send_hid_report('a101808000000000')

    # Validate buttons default mapping
    for hid_btns, br_btns in btns_generic_test_data(hid_btns_mask):
        rsp = blueretro.send_hid_report(
            'a1018080'
            f'{swap32(hid_btns):08x}'
        )

        assert rsp['wireless_input']['btns'] == hid_btns
        assert rsp['generic_input']['btns'][0] == br_btns


@pytest.mark.parametrize('blueretro', [[system.N64, dev_mode.PAD, bt_conn_type.BT_BR_EDR]], indirect=True)
def test_hid_controller_a_n64_buttons_mapping(blueretro):
    ''' Press each buttons and check if N64 mapping is right. '''
    blueretro.send_name(DEVICE_NAME)
    blueretro.send_hid_desc(HID_DESC)

    # Init adapter with a few neutral state report
    for _ in range(2):
        blueretro.send_hid_report('a101808000000000')

    # Validate buttons default mapping
    for hid_btns, n64_btns in btns_generic_to_wired_test_data(hid_btns_mask, n64_btns_mask):
        rsp = blueretro.send_hid_report(
            'a1018080'
            f'{swap32(hid_btns):08x}'
        )

        assert rsp['wireless_input']['btns'] == hid_btns
        assert rsp['wired_output']['btns'] == n64_btns
