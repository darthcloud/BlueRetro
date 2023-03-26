''' Tests for the Wii Nunchuck controller. '''
from itertools import islice
from device_data.test_data_generator import btns_generic_test_data
from device_data.test_data_generator import axes_test_data_generator
from bit_helper import swap16
from device_data.wii import wii_nunchuck_core_btns_mask, wii_nunchuck_btns_mask
from device_data.wii import wii_nunchuck_axes
from device_data.br import axis
from device_data.gc import gc_axes


DEVICE_NAME = 'Nintendo RVL-CNT-01'


def test_wii_nunchuck_controller_default_buttons_mapping(blueretro):
    ''' Press each buttons and check if default mapping is right. '''
    # Set device name
    blueretro.send_name(DEVICE_NAME)
    blueretro.expect('# dev: 0 type: 2:0 Nintendo RVL-CNT-01')

    # Extension init id responce
    blueretro.send_hid_report(
        'a12100005000fa'
        '0000a4200000'
        '00000000000000000000'
    )

    # Validate device type change
    blueretro.expect('# bt_type_update: dev: 0 type: 2 subtype: 1')

    # Init adapter with a few neutral state report
    for _ in range(2):
        blueretro.send_hid_report(
            'a135'
            '0040'
            '827299'
            '8080'
            '716ba8'
            '73'
            '00000000000000000000'
        )
    blueretro.flush_logs()

    # Validate buttons default mapping
    for btns, br_btns in btns_generic_test_data(wii_nunchuck_core_btns_mask):
        blueretro.send_hid_report(
            'a135'
            f'{swap16(btns):04x}'
            '827299'
            '8080'
            '716ba8'
            '73'
            '00000000000000000000'
        )

        wireless = blueretro.expect_json('wireless_input')
        br_generic = blueretro.expect_json('generic_input')

        assert wireless['btns'][0] == btns
        assert br_generic['btns'][0] == br_btns

    # Validate buttons default mapping
    for btns, br_btns in btns_generic_test_data(wii_nunchuck_btns_mask):
        btns ^= 0xFF
        blueretro.send_hid_report(
            'a135'
            '0040'
            '827299'
            '8080'
            '716ba8'
            f'{btns:02x}'
            '00000000000000000000'
        )

        wireless = blueretro.expect_json('wireless_input')
        br_generic = blueretro.expect_json('generic_input')

        assert wireless['btns'][1] == btns
        assert br_generic['btns'][0] == br_btns


def test_wii_nunchuck_controller_axes_default_scaling(blueretro):
    ''' Set the various axes and check if the scaling is right. '''
    # Set device name
    blueretro.send_name(DEVICE_NAME)
    blueretro.expect('# dev: 0 type: 2:0 Nintendo RVL-CNT-01')

    # Extension init id responce
    blueretro.send_hid_report(
        'a12100005000fa'
        '0000a4200000'
        '00000000000000000000'
    )

    # Validate device type change
    blueretro.expect('# bt_type_update: dev: 0 type: 2 subtype: 1')

    # Init adapter with a few neutral state report
    for _ in range(2):
        blueretro.send_hid_report(
            'a135'
            '0040'
            '827299'
            '8080'
            '716ba8'
            '73'
            '00000000000000000000'
        )
    blueretro.flush_logs()

    # Validate axes default scaling
    # Skip deadzone tests as axes resolution is too low
    axes_gen = axes_test_data_generator(wii_nunchuck_axes, gc_axes, 0.0135)
    for _ in range(5):
        axes = next(axes_gen)
        blueretro.send_hid_report(
            'a135'
            '0040'
            '827299'
            f'{axes[axis.LX]["wireless"]:02x}{axes[axis.LY]["wireless"]:02x}'
            '716ba8'
            '73'
            '00000000000000000000'
        )

        wireless = blueretro.expect_json('wireless_input')
        br_generic = blueretro.expect_json('generic_input')
        br_mapped = blueretro.expect_json('mapped_input')
        wired = blueretro.expect_json('wired_output')

        for ax in islice(axis, 0, 2):
            assert wireless['axes'][ax] == axes[ax]['wireless']
            assert br_generic['axes'][ax] == axes[ax]['generic']
            assert br_mapped['axes'][ax] == axes[ax]['mapped']
            assert wired['axes'][ax] == axes[ax]['wired']
