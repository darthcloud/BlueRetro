''' Tests for the WiiU Pro controller. '''
from itertools import islice
from device_data.test_data_generator import btns_generic_test_data
from device_data.test_data_generator import axes_test_data_generator
from bit_helper import swap16, swap32
from device_data.wii import wiiu_btns_mask, wiiu_axes
from device_data.br import axis
from device_data.gc import gc_axes


DEVICE_NAME = 'Nintendo RVL-CNT-01-UC'


def test_wiiu_pro_controller_default_buttons_mapping(blueretro):
    ''' Press each buttons and check if default mapping is right. '''
    # Set device name
    blueretro.send_name(DEVICE_NAME)
    blueretro.expect('# dev: 0 type: 2:6 Nintendo RVL-CNT-01-UC')

    # Init adapter with a few neutral state report
    for _ in range(2):
        blueretro.send_hid_report(
            'a1350000808080'
            '00080008'
            '00080008'
            'ffff2f00'
            '00000000'
        )

    blueretro.flush_logs()

    # Validate buttons default mapping
    for btns, br_btns in btns_generic_test_data(wiiu_btns_mask):
        btns ^= 0xFFFFFFFF
        blueretro.send_hid_report(
            'a1350000808080'
            '00080008'
            '00080008'
            f'{swap32(btns):08x}'
            '00000000'
        )

        wireless = blueretro.expect_json('wireless_input')
        br_generic = blueretro.expect_json('generic_input')

        assert wireless['btns'] == btns
        assert br_generic['btns'][0] == br_btns


def test_wiiu_pro_controller_axes_default_scaling(blueretro):
    ''' Set the various axes and check if the scaling is right. '''
    # Set device name
    blueretro.send_name(DEVICE_NAME)
    blueretro.expect('# dev: 0 type: 2:6 Nintendo RVL-CNT-01-UC')

    # Init adapter with a few neutral state report
    for _ in range(2):
        blueretro.send_hid_report(
            'a1350000808080'
            '00080008'
            '00080008'
            'ffff2f00'
            '00000000'
        )

    blueretro.flush_logs()

    # Validate axes default scaling
    for axes in axes_test_data_generator(wiiu_axes, gc_axes, 0.0135):
        blueretro.send_hid_report(
            'a1350000808080'
            f'{swap16(axes[axis.LX]["wireless"]):04x}{swap16(axes[axis.RX]["wireless"]):04x}'
            f'{swap16(axes[axis.LY]["wireless"]):04x}{swap16(axes[axis.RY]["wireless"]):04x}'
            'ffff2f00'
            '00000000'
        )

        wireless = blueretro.expect_json('wireless_input')
        br_generic = blueretro.expect_json('generic_input')
        br_mapped = blueretro.expect_json('mapped_input')
        wired = blueretro.expect_json('wired_output')

        for ax in islice(axis, 0, 4):
            assert wireless['axes'][ax] == axes[ax]['wireless']
            assert br_generic['axes'][ax] == axes[ax]['generic']
            assert br_mapped['axes'][ax] == axes[ax]['mapped']
            assert wired['axes'][ax] == axes[ax]['wired']
