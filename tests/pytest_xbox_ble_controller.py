''' Tests for the Xbox One S & Series X|S controllers with BLE firmware. '''
import pytest
from itertools import islice
from device_data.test_data_generator import btns_generic_test_data
from device_data.test_data_generator import axes_test_data_generator
from bit_helper import swap16, swap24
from device_data.xbox import xbox_ble_btns_mask, xbox_axes
from device_data.br import system, dev_mode, bt_conn_type, axis, hat_to_ld_btns
from device_data.gc import gc_axes


DEVICE_NAME = 'Xbox Wireless Controller'


@pytest.mark.parametrize('blueretro', [[system.GC, dev_mode.PAD, bt_conn_type.BT_LE]], indirect=True)
def test_xbox_ble_controller_default_buttons_mapping(blueretro):
    ''' Press each buttons and check if default mapping is right. '''
    # Set device name
    blueretro.send_name(DEVICE_NAME)
    blueretro.expect('# bt_type_update: dev: 0 type: 3 subtype: 9')

    # Init adapter with a few neutral state report
    for _ in range(2):
        blueretro.send_to_bridge(0x01,
            '00800080'
            '00800080'
            '00000000'
            '00'
            '000000'
        )

    blueretro.flush_logs()

    # Validate buttons default mapping
    for btns, br_btns in btns_generic_test_data(xbox_ble_btns_mask):
        blueretro.send_to_bridge(0x01,
            '00800080'
            '00800080'
            '00000000'
            '00'
            f'{swap24(btns):06x}'
        )

        wireless = blueretro.expect_json('wireless_input')
        br_generic = blueretro.expect_json('generic_input')

        assert wireless['btns'] & 0xFFFFFF == btns
        assert br_generic['btns'][0] == br_btns

    # Validate hat default mapping
    shifted_hat = hat_to_ld_btns[-1:] + hat_to_ld_btns[:-1]
    for hat_value, br_btns in enumerate(shifted_hat):
        blueretro.send_to_bridge(0x01,
            '00800080'
            '00800080'
            '00000000'
            f'0{hat_value:01x}'
            '000000'
        )

        wireless = blueretro.expect_json('wireless_input')
        br_generic = blueretro.expect_json('generic_input')

        assert wireless['hat'] == hat_value
        assert br_generic['btns'][0] == br_btns


@pytest.mark.parametrize('blueretro', [[system.GC, dev_mode.PAD, bt_conn_type.BT_LE]], indirect=True)
def test_xbox_ble_controller_axes_default_scaling(blueretro):
    ''' Set the various axes and check if the scaling is right. '''
    # Set device name
    blueretro.send_name(DEVICE_NAME)
    blueretro.expect('# bt_type_update: dev: 0 type: 3 subtype: 9')

    # Init adapter with a few neutral state report
    for _ in range(2):
        blueretro.send_to_bridge(0x01,
            '00800080'
            '00800080'
            '00000000'
            '00'
            '000000'
        )

    blueretro.flush_logs()

    # Validate axes default scaling
    for axes in axes_test_data_generator(xbox_axes, gc_axes, 0.0135):
        blueretro.send_to_bridge(0x01,
            f'{swap16(axes[axis.LX]["wireless"]):04x}{swap16(axes[axis.LY]["wireless"]):04x}'
            f'{swap16(axes[axis.RX]["wireless"]):04x}{swap16(axes[axis.RY]["wireless"]):04x}'
            f'{swap16(axes[axis.LM]["wireless"]):04x}{swap16(axes[axis.RM]["wireless"]):04x}'
            '00'
            '000000'
        )

        wireless = blueretro.expect_json('wireless_input')
        br_generic = blueretro.expect_json('generic_input')
        br_mapped = blueretro.expect_json('mapped_input')
        wired = blueretro.expect_json('wired_output')

        for ax in islice(axis, 0, 6):
            assert wireless['axes'][ax] == axes[ax]['wireless']
            assert br_generic['axes'][ax] == axes[ax]['generic']
            assert br_mapped['axes'][ax] == axes[ax]['mapped']
            assert wired['axes'][ax] == axes[ax]['wired']
