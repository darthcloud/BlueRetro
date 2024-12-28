''' Tests for generic HID controller. '''
import pytest
from itertools import islice
from device_data.test_data_generator import btns_generic_test_data
from device_data.test_data_generator import axes_test_data_generator
from device_data.test_data_generator import btns_generic_to_wired_test_data
from bit_helper import bit, swap16
from device_data.br import bt_conn_type, system, report_type, axis, dev_mode, hat_to_ld_btns
from device_data.hid import hid_btns_mask
from device_data.gc import gc_axes
from device_data.saturn import saturn, saturn_btns_mask


DEVICE_NAME = 'HID Generic'
HID_DESC = bytes([
    0x05, 0x01,        # Usage Page (Generic Desktop)
    0x09, 0x05,        # Usage (Gamepad)
    0xA1, 0x01,        # Collection (Application)
    0x85, 0x01,        #   Report ID (1)
    0x09, 0x01,        #   Usage (Pointer)
    0xA1, 0x00,        #   Collection (Physical)
    0x09, 0x30,        #     Usage (X)
    0x09, 0x31,        #     Usage (Y)
    0x15, 0x00,        #     Logical Minimum (0)
    0x27, 0xFF, 0x00, 0x00, 0x00,  #     Logical Maximum (254)
    0x95, 0x02,        #     Report Count (2)
    0x75, 0x08,        #     Report Size (8)
    0x81, 0x02,        #     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0xC0,              #   End Collection
    0x09, 0x01,        #   Usage (Pointer)
    0xA1, 0x00,        #   Collection (Physical)
    0x09, 0x32,        #     Usage (Z)
    0x09, 0x35,        #     Usage (Rz)
    0x15, 0x00,        #     Logical Minimum (0)
    0x27, 0x0F, 0x00, 0x00, 0x00,  #     Logical Maximum (14)
    0x95, 0x02,        #     Report Count (2)
    0x75, 0x04,        #     Report Size (4)
    0x81, 0x02,        #     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0xC0,              #   End Collection
    0x05, 0x02,        #   Usage Page (Simulation Controls)
    0x09, 0xC5,        #   Usage (Brake)
    0x15, 0x00,        #   Logical Minimum (0)
    0x26, 0xFF, 0x00,  #   Logical Maximum (255)
    0x95, 0x01,        #   Report Count (1)
    0x75, 0x08,        #   Report Size (8)
    0x81, 0x02,        #   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x05, 0x02,        #   Usage Page (Simulation Controls)
    0x09, 0xC4,        #   Usage (Accelerator)
    0x15, 0x00,        #   Logical Minimum (0)
    0x26, 0xFF, 0x00,  #   Logical Maximum (255)
    0x95, 0x01,        #   Report Count (1)
    0x75, 0x08,        #   Report Size (8)
    0x81, 0x02,        #   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x05, 0x01,        #   Usage Page (Generic Desktop)
    0x09, 0x39,        #   Usage (Hat Switch)
    0x15, 0x01,        #   Logical Minimum (1)
    0x25, 0x08,        #   Logical Maximum (8)
    0x35, 0x00,        #   Physical Minimum (0)
    0x46, 0x3B, 0x01,  #   Physical Maximum (315)
    0x66, 0x14, 0x00,  #   Unit (System: English Rotation, Length: Centimeter)
    0x75, 0x04,        #   Report Size (4)
    0x95, 0x01,        #   Report Count (1)
    0x81, 0x42,        #   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,Null State)
    0x75, 0x04,        #   Report Size (4)
    0x95, 0x01,        #   Report Count (1)
    0x15, 0x00,        #   Logical Minimum (0)
    0x25, 0x00,        #   Logical Maximum (0)
    0x35, 0x00,        #   Physical Minimum (0)
    0x45, 0x00,        #   Physical Maximum (0)
    0x65, 0x00,        #   Unit (None)
    0x81, 0x03,        #   Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x05, 0x09,        #   Usage Page (Button)
    0x19, 0x01,        #   Usage Minimum (0x01)
    0x29, 0x10,        #   Usage Maximum (0x10)
    0x15, 0x00,        #   Logical Minimum (0)
    0x25, 0x01,        #   Logical Maximum (1)
    0x75, 0x01,        #   Report Size (1)
    0x95, 0x10,        #   Report Count (16)
    0x81, 0x02,        #   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0xC0,              # End Collection

    # 137 bytes
])


def test_hid_controller_b_descriptor(blueretro):
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

    assert rsp['hid_reports'][0]['usages'][0]['usage_page'] == 1
    assert rsp['hid_reports'][0]['usages'][0]['usage'] == 48
    assert rsp['hid_reports'][0]['usages'][0]['bit_offset'] == 0
    assert rsp['hid_reports'][0]['usages'][0]['bit_size'] == 8

    assert rsp['hid_reports'][0]['usages'][1]['usage_page'] == 1
    assert rsp['hid_reports'][0]['usages'][1]['usage'] == 49
    assert rsp['hid_reports'][0]['usages'][1]['bit_offset'] == 8
    assert rsp['hid_reports'][0]['usages'][1]['bit_size'] == 8

    assert rsp['hid_reports'][0]['usages'][2]['usage_page'] == 1
    assert rsp['hid_reports'][0]['usages'][2]['usage'] == 50
    assert rsp['hid_reports'][0]['usages'][2]['bit_offset'] == 16
    assert rsp['hid_reports'][0]['usages'][2]['bit_size'] == 4

    assert rsp['hid_reports'][0]['usages'][3]['usage_page'] == 1
    assert rsp['hid_reports'][0]['usages'][3]['usage'] == 53
    assert rsp['hid_reports'][0]['usages'][3]['bit_offset'] == 20
    assert rsp['hid_reports'][0]['usages'][3]['bit_size'] == 4

    assert rsp['hid_reports'][0]['usages'][4]['usage_page'] == 2
    assert rsp['hid_reports'][0]['usages'][4]['usage'] == 197
    assert rsp['hid_reports'][0]['usages'][4]['bit_offset'] == 24
    assert rsp['hid_reports'][0]['usages'][4]['bit_size'] == 8

    assert rsp['hid_reports'][0]['usages'][5]['usage_page'] == 2
    assert rsp['hid_reports'][0]['usages'][5]['usage'] == 196
    assert rsp['hid_reports'][0]['usages'][5]['bit_offset'] == 32
    assert rsp['hid_reports'][0]['usages'][5]['bit_size'] == 8

    assert rsp['hid_reports'][0]['usages'][6]['usage_page'] == 1
    assert rsp['hid_reports'][0]['usages'][6]['usage'] == 57
    assert rsp['hid_reports'][0]['usages'][6]['bit_offset'] == 40
    assert rsp['hid_reports'][0]['usages'][6]['bit_size'] == 4

    assert rsp['hid_reports'][0]['usages'][7]['usage_page'] == 9
    assert rsp['hid_reports'][0]['usages'][7]['usage'] == 1
    assert rsp['hid_reports'][0]['usages'][7]['bit_offset'] == 48
    assert rsp['hid_reports'][0]['usages'][7]['bit_size'] == 16

    assert rsp['hid_reports'][0]['report_type'] == report_type.PAD


def test_hid_controller_b_default_buttons_mapping(blueretro):
    ''' Press each buttons and check if default mapping is right. '''
    # Set device name
    blueretro.send_name(DEVICE_NAME)
    blueretro.send_hid_desc(HID_DESC)

    # Init adapter with a few neutral state report
    for _ in range(2):
        blueretro.send_hid_report(
            'a101'
            '8080'
            '88'
            '0000'
            '00'
            '0000'
        )

    # Validate buttons default mapping
    for hid_btns, br_btns in btns_generic_test_data(hid_btns_mask, 0xFFFF0000):
        rsp = blueretro.send_hid_report(
            'a101'
            '8080'
            '88'
            '0000'
            '00'
            f'{swap16(hid_btns):04x}'
        )

        assert rsp['wireless_input']['btns'] == hid_btns
        assert rsp['generic_input']['btns'][0] == br_btns

    # Validate hat default mapping
    shifted_hat = hat_to_ld_btns[-1:] + hat_to_ld_btns[:-1]
    for hat_value, br_btns in enumerate(shifted_hat):
        rsp = blueretro.send_hid_report(
            'a101'
            '8080'
            '88'
            '0000'
            f'0{hat_value:01x}'
            '0000'
        )

        assert rsp['wireless_input']['hat'] == hat_value
        assert rsp['generic_input']['btns'][0] == br_btns


def test_hid_controller_b_axes_default_scaling(blueretro):
    ''' Set the various axes and check if the scaling is right. '''
    # Set device name
    blueretro.send_name(DEVICE_NAME)
    blueretro.send_hid_desc(HID_DESC)

    # Init adapter with a few neutral state report
    for _ in range(2):
        blueretro.send_hid_report(
            'a101'
            '8080'
            '88'
            '0000'
            '00'
            '0000'
        )

    hid_axes = {
        axis.LX: {'neutral': 0x80, 'abs_max': 0x7F, 'abs_min': 0x80, 'deadzone': 0},
        axis.LY: {'neutral': 0x80, 'abs_max': 0x7F, 'abs_min': 0x80, 'polarity': 1, 'deadzone': 0},
        axis.RX: {'neutral': 0x8, 'abs_max': 0x7, 'abs_min': 0x8, 'deadzone': 0},
        axis.RY: {'neutral': 0x8, 'abs_max': 0x7, 'abs_min': 0x8, 'polarity': 1, 'deadzone': 0},
        axis.LM: {'neutral': 0x00, 'abs_max': 0xFF, 'abs_min': 0x00, 'deadzone': 0},
        axis.RM: {'neutral': 0x00, 'abs_max': 0xFF, 'abs_min': 0x00, 'deadzone': 0},
    }

    # Validate axes default scaling
    # Skip deadzone tests as axes resolution is too low
    axes_gen = axes_test_data_generator(hid_axes, gc_axes, 0.0135)
    for _ in range(5):
        axes = next(axes_gen)
        rsp = blueretro.send_hid_report(
            'a101'
            f'{axes[axis.LX]["wireless"]:02x}{axes[axis.LY]["wireless"]:02x}'
            f'{axes[axis.RY]["wireless"]:01x}{axes[axis.RX]["wireless"]:01x}'
            f'{axes[axis.LM]["wireless"]:02x}{axes[axis.RM]["wireless"]:02x}'
            '00'
            '0000'
        )

        for ax in islice(axis, 0, 6):
            assert rsp['wireless_input']['axes'][ax] == axes[ax]['wireless']
            assert rsp['generic_input']['axes'][ax] == axes[ax]['generic']
            assert rsp['mapped_input']['axes'][ax] == pytest.approx(axes[ax]['mapped'], 0.1)
            assert rsp['wired_output']['axes'][ax] == pytest.approx(axes[ax]['wired'], 0.1)


@pytest.mark.parametrize('blueretro', [[system.SATURN, dev_mode.PAD_ALT, bt_conn_type.BT_BR_EDR]], indirect=True)
def test_hid_controller_b_saturn_buttons_mapping(blueretro):
    ''' Press each buttons and check if Saturn mapping is right. '''
    blueretro.send_name(DEVICE_NAME)
    blueretro.send_hid_desc(HID_DESC)

    # Init adapter with a few neutral state report
    for _ in range(2):
        blueretro.send_hid_report(
            'a101'
            '8080'
            '88'
            '0000'
            '00'
            '0000'
        )

    # Validate buttons default mapping
    for hid_btns, wired_btns in btns_generic_to_wired_test_data(hid_btns_mask, saturn_btns_mask, 0x221F0000):
        wired_btns ^= 0xFFFF
        rsp = blueretro.send_hid_report(
            'a101'
            '8080'
            '88'
            '0000'
            '00'
            f'{swap16(hid_btns):04x}'
        )

        assert rsp['wireless_input']['btns'] == hid_btns
        assert rsp['wired_output']['btns'] == wired_btns


@pytest.mark.parametrize('blueretro', [[system.SATURN, dev_mode.PAD_ALT, bt_conn_type.BT_BR_EDR]], indirect=True)
def test_hid_controller_b_saturn_triggers_mapping(blueretro):
    ''' Press each buttons and check if Saturn triggers mapping is right. '''
    blueretro.send_name(DEVICE_NAME)
    blueretro.send_hid_desc(HID_DESC)

    # Init adapter with a few neutral state report
    for _ in range(3):
        blueretro.send_hid_report(
            'a101'
            '8080'
            '88'
            '0000'
            '00'
            '0000'
        )

    # Force pull-back removal on triggers
    blueretro.send_hid_report(
        'a101'
        '8080'
        '88'
        'FFFF'
        '00'
        '0000'
    )

    # Test trigger ON threshold for digital bits
    rsp = blueretro.send_hid_report(
        'a101'
        '8080'
        '88'
        '9090'
        '00'
        '0000'
    )
    assert rsp['wired_output']['btns'] ^ 0xFFFF == bit(saturn.L) | bit(saturn.R)

    # Test trigger below ON threshold but over OFF threshold for digital bits
    rsp = blueretro.send_hid_report(
        'a101'
        '8080'
        '88'
        '6060'
        '00'
        '0000'
    )
    assert rsp['wired_output']['btns'] ^ 0xFFFF == bit(saturn.L) | bit(saturn.R)

    # Test trigger below OFF threshold for digital bits
    rsp = blueretro.send_hid_report(
        'a101'
        '8080'
        '88'
        '4040'
        '00'
        '0000'
    )
    assert rsp['wired_output']['btns'] ^ 0xFFFF == 0
