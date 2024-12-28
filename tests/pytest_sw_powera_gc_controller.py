''' Tests for the PowerA Switch GC controller. '''
from itertools import islice
from device_data.test_data_generator import btns_generic_test_data
from device_data.test_data_generator import axes_test_data_generator
from bit_helper import swap16, swap24
from device_data.sw import sw_d_pwa_gc_btns_mask, sw_n_pwa_gc_btns_mask, sw_n_axes
from device_data.br import axis, hat_to_ld_btns
from device_data.gc import gc_axes


DEVICE_NAME = 'Lic Pro Controller'


def test_sw_powera_gc_default_buttons_mapping_native_report(blueretro):
    ''' Press each buttons and check if default mapping is right. '''
    # Set device name
    rsp = blueretro.send_name(DEVICE_NAME)
    assert rsp['device_name']['device_id'] == 0
    assert rsp['device_name']['device_type'] == 5
    assert rsp['device_name']['device_subtype'] == 17
    assert rsp['device_name']['device_name'] == 'Lic Pro Controller'

    # Init adapter with a few neutral state report
    for _ in range(2):
        blueretro.send_hid_report(
            'a1300180'
            '000000'
            'f3d780'
            'e9977b'
            '00000000000000000000000000'
            '00000000000000000000000000'
            '0000000000000000000000'
        )

    # Validate buttons default mapping
    for sw_btns, br_btns in btns_generic_test_data(sw_n_pwa_gc_btns_mask):
        rsp = blueretro.send_hid_report(
            'a1300180'
            f'{swap24(sw_btns):06x}'
            'f3d780'
            'e9977b'
            '00000000000000000000000000'
            '00000000000000000000000000'
            '0000000000000000000000'
        )

        assert rsp['wireless_input']['btns'] >> 8 == sw_btns
        assert rsp['generic_input']['btns'][0] == br_btns


def test_sw_powera_gc_controller_axes_default_scaling_native_report(blueretro):
    ''' Set the various axes and check if the scaling is right. '''
    # Set device name
    rsp = blueretro.send_name(DEVICE_NAME)
    assert rsp['device_name']['device_id'] == 0
    assert rsp['device_name']['device_type'] == 5
    assert rsp['device_name']['device_subtype'] == 17
    assert rsp['device_name']['device_name'] == 'Lic Pro Controller'

    # Init adapter with a few neutral state report
    for _ in range(2):
        blueretro.send_hid_report(
            'a1300180'
            '000000'
            '000880'
            '000880'
            '00000000000000000000000000'
            '00000000000000000000000000'
            '0000000000000000000000'
        )

    # Validate axes default scaling
    for axes in axes_test_data_generator(sw_n_axes, gc_axes, 0.0135):
        rsp = blueretro.send_hid_report(
            'a1300180'
            '000000'
            f'{swap24(axes[axis.LX]["wireless"] | axes[axis.LY]["wireless"] << 12):06x}'
            f'{swap24(axes[axis.RX]["wireless"] | axes[axis.RY]["wireless"] << 12):06x}'
            '00000000000000000000000000'
            '00000000000000000000000000'
            '0000000000000000000000'
        )

        for ax in islice(axis, 0, 4):
            assert rsp['wireless_input']['axes'][ax] == axes[ax]['wireless']
            assert rsp['generic_input']['axes'][ax] == axes[ax]['generic']
            assert rsp['mapped_input']['axes'][ax] == axes[ax]['mapped']
            assert rsp['wired_output']['axes'][ax] == axes[ax]['wired']


def test_sw_powera_gc_axes_scaling_with_calib_native_report(blueretro):
    ''' Set the various axes and check if the scaling is right. '''
    # Set device name
    rsp = blueretro.send_name(DEVICE_NAME)
    assert rsp['device_name']['device_id'] == 0
    assert rsp['device_name']['device_type'] == 5
    assert rsp['device_name']['device_subtype'] == 17
    assert rsp['device_name']['device_name'] == 'Lic Pro Controller'

    # Send calibration data
    blueretro.send_hid_report(
        'a121218000000032487d8e0777009010'
        '3d60000012ffffffffffffffffffffff'
        'ffffffffffffff000000000000000000'
        '0000'
    )
    blueretro.send_hid_report(
        'a121278000000033387d8e1777009010'
        '8660000012ffffffffffffffffffffff'
        'ffffffffffffff000000000000000000'
        '0000'
    )
    blueretro.send_hid_report(
        'a1212d8000000033387d8f0777009010'
        '9860000012ffffffffffffffffffffff'
        'ffffffffffffff000000000000000000'
        '0000'
    )
    rsp = blueretro.send_hid_report(
        'a121338000000033387d8e0777009010'
        '1080000016ffffffffffffffffffffff'
        'ffffffffffffffffffffff0000000000'
        '0000'
    )
    calib = rsp['calib_data']
    for ax in islice(axis, 0, 4):
        assert calib['neutral'][ax] == 0
        assert calib['rel_min'][ax] == 0
        assert calib['rel_max'][ax] == 0
        assert calib['deadzone'][ax] == 0xFFF

    # Init adapter with a few neutral state report
    for _ in range(2):
        blueretro.send_hid_report(
            'a1300180'
            '000000'
            '000880'
            '000880'
            '00000000000000000000000000'
            '00000000000000000000000000'
            '0000000000000000000000'
        )

    # Validate axes default scaling
    for axes in axes_test_data_generator(sw_n_axes, gc_axes, 0.0135):
        rsp = blueretro.send_hid_report(
            'a1300180'
            '000000'
            f'{swap24(axes[axis.LX]["wireless"] | axes[axis.LY]["wireless"] << 12):06x}'
            f'{swap24(axes[axis.RX]["wireless"] | axes[axis.RY]["wireless"] << 12):06x}'
            '00000000000000000000000000'
            '00000000000000000000000000'
            '0000000000000000000000'
        )

        for ax in islice(axis, 0, 4):
            assert rsp['wireless_input']['axes'][ax] == axes[ax]['wireless']
            assert rsp['generic_input']['axes'][ax] == axes[ax]['generic']
            assert rsp['mapped_input']['axes'][ax] == axes[ax]['mapped']
            assert rsp['wired_output']['axes'][ax] == axes[ax]['wired']


def test_sw_powera_gc_default_buttons_mapping_default_report(blueretro):
    ''' Press each buttons and check if default mapping is right. '''
    # Set device name
    rsp = blueretro.send_name(DEVICE_NAME)
    assert rsp['device_name']['device_id'] == 0
    assert rsp['device_name']['device_type'] == 5
    assert rsp['device_name']['device_subtype'] == 17
    assert rsp['device_name']['device_name'] == 'Lic Pro Controller'

    # Init adapter with a few neutral state report
    for _ in range(2):
        blueretro.send_hid_report(
            'a13f'
            '0000'
            '0f'
            '0080008000800080'
        )

    # Validate buttons default mapping
    for sw_btns, br_btns in btns_generic_test_data(sw_d_pwa_gc_btns_mask):
        rsp = blueretro.send_hid_report(
            'a13f'
            f'{swap16(sw_btns):04x}'
            '0f'
            '0080008000800080'
        )

        assert rsp['wireless_input']['btns'] == sw_btns
        assert rsp['generic_input']['btns'][0] == br_btns

    # Validate hat default mapping
    for hat_value, br_btns in enumerate(hat_to_ld_btns):
        rsp = blueretro.send_hid_report(
            'a13f'
            '0000'
            f'0{hat_value:01x}'
            '0080008000800080'
        )

        assert rsp['wireless_input']['hat'] == hat_value
        assert rsp['generic_input']['btns'][0] == br_btns


def test_sw_powera_gc_controller_axes_default_scaling_default_report(blueretro):
    ''' Set the various axes and check if the scaling is right. '''
    # Set device name
    rsp = blueretro.send_name(DEVICE_NAME)
    assert rsp['device_name']['device_id'] == 0
    assert rsp['device_name']['device_type'] == 5
    assert rsp['device_name']['device_subtype'] == 17
    assert rsp['device_name']['device_name'] == 'Lic Pro Controller'

    # Init adapter with a few neutral state report
    for _ in range(2):
        blueretro.send_hid_report(
            'a13f'
            '0000'
            '0f'
            '0080008000800080'
        )

    # Validate axes default scaling
    for axes in axes_test_data_generator(sw_n_axes, gc_axes, 0.0135):
        lx_shift = axes[axis.LX]['wireless'] << 4
        ly_inverted = ((axes[axis.LY]["wireless"] << 4) ^ 0xFFFF) + 1
        rx_shift = axes[axis.RX]['wireless'] << 4
        ry_inverted = ((axes[axis.RY]["wireless"] << 4) ^ 0xFFFF) + 1
        rsp = blueretro.send_hid_report(
            'a13f'
            '0000'
            '0f'
            f'{swap16(lx_shift):04x}{swap16(ly_inverted):04x}'
            f'{swap16(rx_shift):04x}{swap16(ry_inverted):04x}'
        )

        wireless_value = (lx_shift, ly_inverted, rx_shift, ry_inverted)

        for ax in islice(axis, 0, 4):
            assert rsp['wireless_input']['axes'][ax] == wireless_value[ax]
            assert rsp['generic_input']['axes'][ax] == axes[ax]['generic']
            assert rsp['mapped_input']['axes'][ax] == axes[ax]['mapped']
            assert rsp['wired_output']['axes'][ax] == axes[ax]['wired']


def test_sw_powera_gc_axes_scaling_with_calib_default_report(blueretro):
    ''' Set the various axes and check if the scaling is right. '''
    # Set device name
    rsp = blueretro.send_name(DEVICE_NAME)
    assert rsp['device_name']['device_id'] == 0
    assert rsp['device_name']['device_type'] == 5
    assert rsp['device_name']['device_subtype'] == 17
    assert rsp['device_name']['device_name'] == 'Lic Pro Controller'

    # Send calibration data
    blueretro.send_hid_report(
        'a121218000000032487d8e0777009010'
        '3d60000012ffffffffffffffffffffff'
        'ffffffffffffff000000000000000000'
        '0000'
    )
    blueretro.send_hid_report(
        'a121278000000033387d8e1777009010'
        '8660000012ffffffffffffffffffffff'
        'ffffffffffffff000000000000000000'
        '0000'
    )
    blueretro.send_hid_report(
        'a1212d8000000033387d8f0777009010'
        '9860000012ffffffffffffffffffffff'
        'ffffffffffffff000000000000000000'
        '0000'
    )
    rsp = blueretro.send_hid_report(
        'a121338000000033387d8e0777009010'
        '1080000016ffffffffffffffffffffff'
        'ffffffffffffffffffffff0000000000'
        '0000'
    )
    calib = rsp['calib_data']
    for ax in islice(axis, 0, 4):
        assert calib['neutral'][ax] == 0
        assert calib['rel_min'][ax] == 0
        assert calib['rel_max'][ax] == 0
        assert calib['deadzone'][ax] == 0xFFF

    # Init adapter with a few neutral state report
    for _ in range(2):
        blueretro.send_hid_report(
            'a13f'
            '0000'
            '0f'
            '0080008000800080'
        )

    # Validate axes default scaling
    for axes in axes_test_data_generator(sw_n_axes, gc_axes, 0.0135):
        lx_shift = axes[axis.LX]['wireless'] << 4
        ly_inverted = ((axes[axis.LY]["wireless"] << 4) ^ 0xFFFF) + 1
        rx_shift = axes[axis.RX]['wireless'] << 4
        ry_inverted = ((axes[axis.RY]["wireless"] << 4) ^ 0xFFFF) + 1
        rsp = blueretro.send_hid_report(
            'a13f'
            '0000'
            '0f'
            f'{swap16(lx_shift):04x}{swap16(ly_inverted):04x}'
            f'{swap16(rx_shift):04x}{swap16(ry_inverted):04x}'
        )

        wireless_value = (lx_shift, ly_inverted, rx_shift, ry_inverted)

        for ax in islice(axis, 0, 4):
            assert rsp['wireless_input']['axes'][ax] == wireless_value[ax]
            assert rsp['generic_input']['axes'][ax] == axes[ax]['generic']
            assert rsp['mapped_input']['axes'][ax] == axes[ax]['mapped']
            assert rsp['wired_output']['axes'][ax] == axes[ax]['wired']
