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
HID_DESC = (
    '05010905a10185010901a10009300931'
    '150027ff000000950275088102c00901'
    'a100093209351500270f000000950275'
    '048102c0050209c5150026ff00950175'
    '088102050209c4150026ff0095017508'
    '810205010939150125083500463b0166'
    '14007504950181427504950115002500'
    '35004500650081030509190129101500'
    '2501750195108102c0'
)


def test_hid_controller_b_descriptor(blueretro):
    ''' Load a HID descriptor and check if it's parsed right. '''
    # Set device name
    blueretro.send_name(DEVICE_NAME)
    blueretro.expect('# dev: 0 type: 0:0 HID Generic')

    # Validate HID descriptor
    blueretro.send_hid_desc(HID_DESC)
    report = blueretro.expect_json('parsed_hid_report')

    assert report["report_id"] == 1

    assert report["usages"][0]["usage_page"] == 1
    assert report["usages"][0]["usage"] == 48
    assert report["usages"][0]["bit_offset"] == 0
    assert report["usages"][0]["bit_size"] == 8

    assert report["usages"][1]["usage_page"] == 1
    assert report["usages"][1]["usage"] == 49
    assert report["usages"][1]["bit_offset"] == 8
    assert report["usages"][1]["bit_size"] == 8

    assert report["usages"][2]["usage_page"] == 1
    assert report["usages"][2]["usage"] == 50
    assert report["usages"][2]["bit_offset"] == 16
    assert report["usages"][2]["bit_size"] == 4

    assert report["usages"][3]["usage_page"] == 1
    assert report["usages"][3]["usage"] == 53
    assert report["usages"][3]["bit_offset"] == 20
    assert report["usages"][3]["bit_size"] == 4

    assert report["usages"][4]["usage_page"] == 2
    assert report["usages"][4]["usage"] == 197
    assert report["usages"][4]["bit_offset"] == 24
    assert report["usages"][4]["bit_size"] == 8

    assert report["usages"][5]["usage_page"] == 2
    assert report["usages"][5]["usage"] == 196
    assert report["usages"][5]["bit_offset"] == 32
    assert report["usages"][5]["bit_size"] == 8

    assert report["usages"][6]["usage_page"] == 1
    assert report["usages"][6]["usage"] == 57
    assert report["usages"][6]["bit_offset"] == 40
    assert report["usages"][6]["bit_size"] == 4

    assert report["usages"][7]["usage_page"] == 9
    assert report["usages"][7]["usage"] == 1
    assert report["usages"][7]["bit_offset"] == 48
    assert report["usages"][7]["bit_size"] == 16

    assert report["report_type"] == report_type.PAD


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

    blueretro.flush_logs()

    # Validate buttons default mapping
    for hid_btns, br_btns in btns_generic_test_data(hid_btns_mask, 0xAA7FA000):
        blueretro.send_hid_report(
            'a101'
            '8080'
            '88'
            '0000'
            '00'
            f'{swap16(hid_btns):04x}'
        )

        wireless = blueretro.expect_json('wireless_input')
        br_generic = blueretro.expect_json('generic_input')

        assert wireless['btns'] == hid_btns
        assert br_generic['btns'][0] == br_btns

    # Validate hat default mapping
    shifted_hat = hat_to_ld_btns[-1:] + hat_to_ld_btns[:-1]
    for hat_value, br_btns in enumerate(shifted_hat):
        blueretro.send_hid_report(
            'a101'
            '8080'
            '88'
            '0000'
            f'0{hat_value:01x}'
            '0000'
        )

        wireless = blueretro.expect_json('wireless_input')
        br_generic = blueretro.expect_json('generic_input')

        assert wireless['hat'] == hat_value
        assert br_generic['btns'][0] == br_btns


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

    blueretro.flush_logs()

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
        blueretro.send_hid_report(
            'a101'
            f'{axes[axis.LX]["wireless"]:02x}{axes[axis.LY]["wireless"]:02x}'
            f'{axes[axis.RY]["wireless"]:01x}{axes[axis.RX]["wireless"]:01x}'
            f'{axes[axis.LM]["wireless"]:02x}{axes[axis.RM]["wireless"]:02x}'
            '00'
            '0000'
        )

        wireless = blueretro.expect_json('wireless_input')
        br_generic = blueretro.expect_json('generic_input')
        br_mapped = blueretro.expect_json('mapped_input')
        wired = blueretro.expect_json('wired_output')

        for ax in islice(axis, 0, 6):
            assert wireless['axes'][ax] == axes[ax]['wireless']
            assert br_generic['axes'][ax] == axes[ax]['generic']
            assert br_mapped['axes'][ax] == pytest.approx(axes[ax]['mapped'], 0.1)
            assert wired['axes'][ax] == pytest.approx(axes[ax]['wired'], 0.1)


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

    blueretro.flush_logs()

    # Validate buttons default mapping
    for hid_btns, wired_btns in btns_generic_to_wired_test_data(hid_btns_mask, saturn_btns_mask, 0xAA7FA000):
        wired_btns ^= 0xFFFF
        blueretro.send_hid_report(
            'a101'
            '8080'
            '88'
            '0000'
            '00'
            f'{swap16(hid_btns):04x}'
        )

        wireless = blueretro.expect_json('wireless_input')
        wired = blueretro.expect_json('wired_output')

        assert wireless['btns'] == hid_btns
        assert wired['btns'] == wired_btns


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

    blueretro.expect_json('wired_output')

    # Test trigger ON threshold for digital bits
    blueretro.send_hid_report(
        'a101'
        '8080'
        '88'
        '9090'
        '00'
        '0000'
    )
    wired = blueretro.expect_json('wired_output')
    assert wired['btns'] ^ 0xFFFF == bit(saturn.L) | bit(saturn.R)

    # Test trigger below ON threshold but over OFF threshold for digital bits
    blueretro.send_hid_report(
        'a101'
        '8080'
        '88'
        '6060'
        '00'
        '0000'
    )
    wired = blueretro.expect_json('wired_output')
    assert wired['btns'] ^ 0xFFFF == bit(saturn.L) | bit(saturn.R)

    # Test trigger below OFF threshold for digital bits
    blueretro.send_hid_report(
        'a101'
        '8080'
        '88'
        '4040'
        '00'
        '0000'
    )
    wired = blueretro.expect_json('wired_output')
    assert wired['btns'] ^ 0xFFFF == 0
