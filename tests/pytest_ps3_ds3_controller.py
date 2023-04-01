''' Tests for the PS3 DS3 controller. '''
from itertools import islice
from device_data.test_data_generator import btns_generic_test_data
from device_data.test_data_generator import axes_test_data_generator
from bit_helper import swap32
from device_data.ps3 import ps3_btns_mask, ps3_axes
from device_data.br import axis
from device_data.gc import gc_axes


DEVICE_NAME = 'PLAYSTATION(R)3'


def test_ps3_ds3_controller_default_buttons_mapping(blueretro):
    ''' Press each buttons and check if default mapping is right. '''
    # Set device name
    blueretro.send_name(DEVICE_NAME)
    blueretro.expect('# dev: 0 type: 1:0 PLAYSTATION\\(R\\)3')

    # Init adapter with a few neutral state report
    for _ in range(2):
        blueretro.send_hid_report(
            'a101'
            '00000000'
            '00'
            '80808080'
            '0000000000000000'
            '0000'
            '000000000000000000030516ffd'
            '4000033ad77004001ef027401d8'
            '01fe'
        )

    blueretro.flush_logs()

    # Validate buttons default mapping
    for btns, br_btns in btns_generic_test_data(ps3_btns_mask):
        blueretro.send_hid_report(
            'a101'
            f'{swap32(btns):08x}'
            '00'
            '80808080'
            '0000000000000000'
            '0000'
            '000000000000000000030516ffd'
            '4000033ad77004001ef027401d8'
            '01fe'
        )

        wireless = blueretro.expect_json('wireless_input')
        br_generic = blueretro.expect_json('generic_input')

        assert wireless['btns'] == btns
        assert br_generic['btns'][0] == br_btns


def test_ps3_ds3_controller_axes_default_scaling(blueretro):
    ''' Set the various axes and check if the scaling is right. '''
    # Set device name
    blueretro.send_name(DEVICE_NAME)
    blueretro.expect('# dev: 0 type: 1:0 PLAYSTATION\\(R\\)3')

    # Init adapter with a few neutral state report
    for _ in range(2):
        blueretro.send_hid_report(
            'a101'
            '00000000'
            '00'
            '80808080'
            '0000000000000000'
            '0000'
            '000000000000000000030516ffd'
            '4000033ad77004001ef027401d8'
            '01fe'
        )

    blueretro.flush_logs()

    # Validate axes default scaling
    for axes in axes_test_data_generator(ps3_axes, gc_axes, 0.0135):
        blueretro.send_hid_report(
            'a101'
            '00000000'
            '00'
            f'{axes[axis.LX]["wireless"]:02x}{axes[axis.LY]["wireless"]:02x}'
            f'{axes[axis.RX]["wireless"]:02x}{axes[axis.RY]["wireless"]:02x}'
            '0000000000000000'
            f'{axes[axis.LM]["wireless"]:02x}{axes[axis.RM]["wireless"]:02x}'
            '000000000000000000030516ffd'
            '4000033ad77004001ef027401d8'
            '01fe'
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
