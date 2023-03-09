''' Tests for the Switch N64 controller. '''
from itertools import islice
from device_data.test_data_generator import btns_generic_test_data
from device_data.test_data_generator import axes_test_data_generator
from bit_helper import swap24
from device_data.sw import sw_n_n64_btns_mask, sw_n_n64_axes
from device_data.br import axis
from device_data.gc import GC, gc_axes


DEVICE_NAME = 'N64 Controller'


def test_sw_n64_controller_default_buttons_mapping(blueretro):
    ''' Press each buttons and check if default mapping is right. '''
    # Connect device
    blueretro.disconnect()
    blueretro.send_system_id(GC)
    blueretro.connect()
    blueretro.send_name(DEVICE_NAME)

    blueretro.get_logs()
    blueretro.expect('# dev: 0 type: 5:15 N64 Controller', timeout=1)

    # Init adapter with a few neutral state report
    for _ in range(2):
        blueretro.send_hid_report(
            'a1300180'
            '000000'
            'c38782'
            '000000'
            '00000000000000000000000000'
            '00000000000000000000000000'
            '0000000000000000000000'
        )

    blueretro.flush_logs()

    # Validate buttons default mapping
    for sw_btns, br_btns in btns_generic_test_data(sw_n_n64_btns_mask):
        blueretro.send_hid_report(
            'a1300180'
            f'{swap24(sw_btns):06x}'
            'c38782'
            '000000'
            '00000000000000000000000000'
            '00000000000000000000000000'
            '0000000000000000000000'
        )

        blueretro.get_logs()

        wireless = blueretro.expect_json('wireless_input')
        br_generic = blueretro.expect_json('generic_input')

        assert wireless['btns'] >> 8 == sw_btns
        assert br_generic['btns'][0] == br_btns

    blueretro.disconnect()


def test_sw_n64_controller_axes_default_scaling(blueretro):
    ''' Set the various axes and check if the scaling is right. '''
    # Connect device
    blueretro.disconnect()
    blueretro.send_system_id(GC)
    blueretro.connect()
    blueretro.send_name(DEVICE_NAME)

    blueretro.get_logs()
    blueretro.expect('# dev: 0 type: 5:15 N64 Controller', timeout=1)

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

    blueretro.flush_logs()

    # Validate axes default scaling
    for axes in axes_test_data_generator(sw_n_n64_axes, gc_axes, 0.0135):
        blueretro.send_hid_report(
            'a1300180'
            '000000'
            f'{swap24(axes[axis.LX]["wireless"] | axes[axis.LY]["wireless"] << 12):06x}'
            '000000'
            '00000000000000000000000000'
            '00000000000000000000000000'
            '0000000000000000000000'
        )

        blueretro.get_logs()

        wireless = blueretro.expect_json('wireless_input')
        br_generic = blueretro.expect_json('generic_input')
        br_mapped = blueretro.expect_json('mapped_input')
        wired = blueretro.expect_json('wired_output')

        assert wireless['axes'][axis.LX] == axes[axis.LX]['wireless']
        assert wireless['axes'][axis.LY] == axes[axis.LY]['wireless']

        assert br_generic['axes'][axis.LX] == axes[axis.LX]['generic']
        assert br_generic['axes'][axis.LY] == axes[axis.LY]['generic']

        assert br_mapped['axes'][axis.LX] == axes[axis.LX]['mapped']
        assert br_mapped['axes'][axis.LY] == axes[axis.LY]['mapped']

        assert wired['axes'][axis.LX] == axes[axis.LX]['wired']
        assert wired['axes'][axis.LY] == axes[axis.LY]['wired']

    blueretro.disconnect()


def test_sw_n64_controller_axes_scaling_with_calib(blueretro):
    ''' Set the various axes and check if the scaling is right. '''
    # Connect device
    blueretro.disconnect()
    blueretro.send_system_id(GC)
    blueretro.connect()
    blueretro.send_name(DEVICE_NAME)

    blueretro.get_logs()
    blueretro.expect('# dev: 0 type: 5:15 N64 Controller', timeout=1)

    # Send calibration data
    blueretro.send_hid_report(
        'a1210280000000c49782000000089010'
        '3d60000012672558c9278545d558ffff'
        'ffffffffffffff000000000000000000'
        '0000'
    )
    blueretro.send_hid_report(
        'a1210280000000c59782000000089010'
        '86600000120f705d5030f33884433884'
        '43333993cdd66c000000000000000000'
        '0000'
    )
    blueretro.send_hid_report(
        'a1210380000000c49782000000089010'
        '9860000012ffffffffffffffffffffff'
        'ffffffffffffff000000000000000000'
        '0000'
    )
    blueretro.send_hid_report(
        'a1210380000000c48782000000089010'
        '1080000016ffffffffffffffffffffff'
        'ffffffffffffffffffffff0000000000'
        '0000'
    )
    blueretro.get_logs()
    calib = blueretro.expect_json("calib_data")
    sw_calib_axes = {axis.LX: {}, axis.LY: {}}
    for ax in islice(axis, 0, 2):
        sw_calib_axes[ax]['neutral'] = calib['neutral'][ax]
        sw_calib_axes[ax]['abs_max'] = min(calib['rel_min'][ax], calib['rel_max'][ax])
        sw_calib_axes[ax]['deadzone'] = calib['deadzone'][ax]

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

    blueretro.flush_logs()

    # Validate axes default scaling
    for axes in axes_test_data_generator(sw_calib_axes, gc_axes, 0.0135):
        blueretro.send_hid_report(
            'a1300180'
            '000000'
            f'{swap24(axes[axis.LX]["wireless"] | axes[axis.LY]["wireless"] << 12):06x}'
            '000000'
            '00000000000000000000000000'
            '00000000000000000000000000'
            '0000000000000000000000'
        )

        blueretro.get_logs()

        wireless = blueretro.expect_json('wireless_input')
        br_generic = blueretro.expect_json('generic_input')
        br_mapped = blueretro.expect_json('mapped_input')
        wired = blueretro.expect_json('wired_output')

        assert wireless['axes'][axis.LX] == axes[axis.LX]['wireless']
        assert wireless['axes'][axis.LY] == axes[axis.LY]['wireless']

        assert br_generic['axes'][axis.LX] == axes[axis.LX]['generic']
        assert br_generic['axes'][axis.LY] == axes[axis.LY]['generic']

        assert br_mapped['axes'][axis.LX] == axes[axis.LX]['mapped']
        assert br_mapped['axes'][axis.LY] == axes[axis.LY]['mapped']

        assert wired['axes'][axis.LX] == axes[axis.LX]['wired']
        assert wired['axes'][axis.LY] == axes[axis.LY]['wired']

    blueretro.disconnect()
