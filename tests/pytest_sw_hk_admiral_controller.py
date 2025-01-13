''' Tests for the HK Switch Admiral controller. '''
from itertools import islice
from device_data.test_data_generator import btns_generic_test_data
from device_data.test_data_generator import axes_test_data_generator
from bit_helper import swap16
from device_data.sw import sw_hk_admiral_btns_mask, sw_hk_admiral_axes
from device_data.br import axis, hat_to_ld_btns
from device_data.gc import gc_axes


DEVICE_NAME = 'Hyperkin Pad'


def test_sw_hk_admiral_controller_default_buttons_mapping(blueretro):
    ''' Press each buttons and check if default mapping is right. '''
    # Set device name
    rsp = blueretro.send_name(DEVICE_NAME)
    assert rsp['device_name']['device_id'] == 0
    assert rsp['device_name']['device_type'] == 5
    assert rsp['device_name']['device_subtype'] == 17
    assert rsp['device_name']['device_name'] == 'Hyperkin Pad'

    # Init adapter with a few neutral state report
    for _ in range(2):
        blueretro.send_hid_report(
            'a13f'
            '0000'
            '00'
            '0080008000800080'
        )

    # Validate buttons default mapping
    for sw_btns, br_btns in btns_generic_test_data(sw_hk_admiral_btns_mask):
        rsp = blueretro.send_hid_report(
            'a13f'
            f'{swap16(sw_btns):04x}'
            '00'
            '0080008000800080'
        )

        assert rsp['wireless_input']['btns'] == sw_btns
        assert rsp['generic_input']['btns'][0] == br_btns

    # Validate hat default mapping
    shifted_hat = hat_to_ld_btns[-1:] + hat_to_ld_btns[:-1]
    for hat_value, br_btns in enumerate(shifted_hat):
        rsp = blueretro.send_hid_report(
            'a13f'
            '0000'
            f'0{hat_value:01x}'
            '0080008000800080'
        )

        assert rsp['wireless_input']['hat'] == hat_value
        assert rsp['generic_input']['btns'][0] == br_btns


def test_sw_hk_admiral_controller_axes_default_scaling(blueretro):
    ''' Set the various axes and check if the scaling is right. '''
    # Set device name
    rsp = blueretro.send_name(DEVICE_NAME)
    assert rsp['device_name']['device_id'] == 0
    assert rsp['device_name']['device_type'] == 5
    assert rsp['device_name']['device_subtype'] == 17
    assert rsp['device_name']['device_name'] == 'Hyperkin Pad'

    # Init adapter with a few neutral state report
    for _ in range(2):
        blueretro.send_hid_report(
            'a13f'
            '0000'
            '0f'
            '0080008000800080'
        )

    # Validate axes default scaling
    for axes in axes_test_data_generator(sw_hk_admiral_axes, gc_axes, 0.0135):
        ly = axes[axis.LY]["wireless"]
        if ly == 2 ** 16:
            ly -= 1

        ly_inverted = (ly ^ 0xFFFF) + 1

        rsp = blueretro.send_hid_report(
            'a13f'
            '0000'
            '0f'
            f'{swap16(axes[axis.LX]["wireless"]):04x}{swap16(ly_inverted):04x}'
            '00800080'
        )

        wireless_value = (axes[axis.LX]['wireless'], ly_inverted)

        for ax in islice(axis, 0, 2):
            assert rsp['wireless_input']['axes'][ax] == wireless_value[ax]
            assert rsp['generic_input']['axes'][ax] == int(axes[ax]['generic'] / 16)
            assert rsp['mapped_input']['axes'][ax] == axes[ax]['mapped']
            assert rsp['wired_output']['axes'][ax] == axes[ax]['wired']
