''' Tests for the Switch Left Joycon controller. '''
from itertools import islice
from device_data.test_data_generator import btns_generic_test_data
from device_data.test_data_generator import axes_test_data_generator
from bit_helper import swap16, swap24
from device_data.sw import sw_d_jc_btns_mask, sw_n_jc_btns_mask, sw_n_ljc_axes
from device_data.br import axis, hat_to_ld_btns, bt_type, bt_subtype
from device_data.gc import gc_axes


DEVICE_NAME = 'Joy-Con (L)'


def test_sw_left_joycon_controller_default_buttons_mapping_native_report(blueretro):
    ''' Press each buttons and check if default mapping is right. '''
    # Set device name
    rsp = blueretro.send_name(DEVICE_NAME)
    assert rsp['device_name']['device_id'] == 0
    assert rsp['device_name']['device_type'] == bt_type.SW
    assert rsp['device_name']['device_subtype'] == bt_subtype.SW_LEFT_JOYCON
    assert rsp['device_name']['device_name'] == 'Joy-Con (L)'

    # Init adapter with a few neutral state report
    for _ in range(2):
        blueretro.send_hid_report(
            'a1300180'
            '000000'
            'fdf78e'
            '000000'
            '00000000000000000000000000'
            '00000000000000000000000000'
            '0000000000000000000000'
        )

    # Validate buttons default mapping
    for sw_btns, br_btns in btns_generic_test_data(sw_n_jc_btns_mask):
        rsp = blueretro.send_hid_report(
            'a1300180'
            f'{swap24(sw_btns):06x}'
            'fdf78e'
            '000000'
            '00000000000000000000000000'
            '00000000000000000000000000'
            '0000000000000000000000'
        )

        assert rsp['wireless_input']['btns'] >> 8 == sw_btns
        assert rsp['generic_input']['btns'][0] == br_btns


def test_sw_left_joycon_controller_axes_default_scaling_native_report(blueretro):
    ''' Set the various axes and check if the scaling is right. '''
    # Set device name
    rsp = blueretro.send_name(DEVICE_NAME)
    assert rsp['device_name']['device_id'] == 0
    assert rsp['device_name']['device_type'] == bt_type.SW
    assert rsp['device_name']['device_subtype'] == bt_subtype.SW_LEFT_JOYCON
    assert rsp['device_name']['device_name'] == 'Joy-Con (L)'

    # Init adapter with a few neutral state report
    for _ in range(2):
        blueretro.send_hid_report(
            'a1300180'
            '000000'
            '000880'
            '000000'
            '00000000000000000000000000'
            '00000000000000000000000000'
            '0000000000000000000000'
        )

    # Validate axes default scaling
    for axes in axes_test_data_generator(sw_n_ljc_axes, gc_axes, 0.0135):
        rsp = blueretro.send_hid_report(
            'a1300180'
            '000000'
            f'{swap24(axes[axis.LY]["wireless"] | axes[axis.LX]["wireless"] << 12):06x}'
            '000000'
            '00000000000000000000000000'
            '00000000000000000000000000'
            '0000000000000000000000'
        )

        for ax in islice(axis, 0, 2):
            assert rsp['wireless_input']['axes'][ax] == axes[ax]['wireless']
            assert rsp['generic_input']['axes'][ax] == axes[ax]['generic']
            assert rsp['mapped_input']['axes'][ax] == axes[ax]['mapped']
            assert rsp['wired_output']['axes'][ax] == axes[ax]['wired']


def test_sw_left_joycon_controller_axes_scaling_with_calib_native_report(blueretro):
    ''' Set the various axes and check if the scaling is right. '''
    # Set device name
    rsp = blueretro.send_name(DEVICE_NAME)
    assert rsp['device_name']['device_id'] == 0
    assert rsp['device_name']['device_type'] == bt_type.SW
    assert rsp['device_name']['device_subtype'] == bt_subtype.SW_LEFT_JOYCON
    assert rsp['device_name']['device_name'] == 'Joy-Con (L)'

    # Send calibration data
    blueretro.send_hid_report(
        'a121264e000000d1278f000000b09010'
        '3d60000012dca44486d785675446ffff'
        'ffffffffffffff000000000000000000'
        '0000'
    )
    blueretro.send_hid_report(
        'a1212c4e000000d2378f000000b09010'
        '8660000012190049bc40e1daa22ddaa2'
        '2da66aaa900449000000000000000000'
        '0000'
    )
    blueretro.send_hid_report(
        'a121324e000000d1378f000000b09010'
        '9860000012190049bc40e1daa22ddaa2'
        '2da66aaa900449000000000000000000'
        '0000'
    )
    rsp = blueretro.send_hid_report(
        'a121384e000000d2278f000000b09010'
        '1080000016b2a1b12444dc978b4f2445'
        'ffffffffffffffffffffff0000000000'
        '0000'
    )
    calib = rsp["calib_data"]
    sw_calib_axes = {axis.LX: {'polarity': 1}, axis.LY: {}}
    for ax in islice(axis, 0, 2):
        sw_calib_axes[ax ^ 1]['neutral'] = calib['neutral'][ax]
        sw_calib_axes[ax ^ 1]['abs_max'] = calib['rel_max'][ax]
        sw_calib_axes[ax ^ 1]['abs_min'] = calib['rel_min'][ax]
        sw_calib_axes[ax ^ 1]['deadzone'] = calib['deadzone'][ax]

    # Init adapter with a few neutral state report
    for _ in range(2):
        blueretro.send_hid_report(
            'a1300180'
            '000000'
            '000880'
            '000000'
            '00000000000000000000000000'
            '00000000000000000000000000'
            '0000000000000000000000'
        )

    # Validate axes default scaling
    for axes in axes_test_data_generator(sw_calib_axes, gc_axes, 0.0135):
        rsp = blueretro.send_hid_report(
            'a1300180'
            '000000'
            f'{swap24(axes[axis.LY]["wireless"] | axes[axis.LX]["wireless"] << 12):06x}'
            '000000'
            '00000000000000000000000000'
            '00000000000000000000000000'
            '0000000000000000000000'
        )

        for ax in islice(axis, 0, 2):
            assert rsp['wireless_input']['axes'][ax] == axes[ax]['wireless']
            assert rsp['generic_input']['axes'][ax] == axes[ax]['generic']
            assert rsp['mapped_input']['axes'][ax] == axes[ax]['mapped']
            assert rsp['wired_output']['axes'][ax] == axes[ax]['wired']


def test_sw_left_joycon_controller_default_buttons_mapping_default_report(blueretro):
    ''' Press each buttons and check if default mapping is right. '''
    # Set device name
    rsp = blueretro.send_name(DEVICE_NAME)
    assert rsp['device_name']['device_id'] == 0
    assert rsp['device_name']['device_type'] == bt_type.SW
    assert rsp['device_name']['device_subtype'] == bt_subtype.SW_LEFT_JOYCON
    assert rsp['device_name']['device_name'] == 'Joy-Con (L)'

    # Init adapter with a few neutral state report
    for _ in range(2):
        blueretro.send_hid_report(
            'a13f'
            '0000'
            '0f'
            '0080008000800080'
        )

    # Validate buttons default mapping
    for sw_btns, br_btns in btns_generic_test_data(sw_d_jc_btns_mask):
        rsp = blueretro.send_hid_report(
            'a13f'
            f'{swap16(sw_btns):04x}'
            '0f'
            '0080008000800080'
        )

        assert rsp['wireless_input']['btns'] == sw_btns
        assert rsp['generic_input']['btns'][0] == br_btns

    # Validate hat default mapping
    mapped_result = [
        [0, 100], [100, 100], [100, 0], [100, -100],
        [0, -100], [-100, -100], [-100, 0], [-100, 100],
        [0, 0], [0, 0], [0, 0], [0, 0],
        [0, 0], [0, 0], [0, 0], [0, 0],
    ]
    for hat_value, br_btns in enumerate(hat_to_ld_btns):
        rsp = blueretro.send_hid_report(
            'a13f'
            '0000'
            f'0{hat_value:01x}'
            '0080008000800080'
        )

        assert rsp['wireless_input']['hat'] == hat_value
        assert rsp['generic_input']['btns'][0] == br_btns >> 8
        assert rsp['mapped_input']['axes'][axis.LX] == mapped_result[hat_value][axis.LX]
        assert rsp['mapped_input']['axes'][axis.LY] == mapped_result[hat_value][axis.LY]
