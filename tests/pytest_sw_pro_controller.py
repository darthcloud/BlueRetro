''' Tests for the Switch Pro controller. '''
from itertools import islice
from device_data.test_data_generator import btns_generic_test_data
from device_data.test_data_generator import axes_test_data_generator
from bit_helper import swap24
from device_data.sw import sw_n_btns_mask, sw_n_axes
from device_data.br import axis
from device_data.gc import GC, gc_axes


DEVICE_NAME = 'Pro Controller'


def test_sw_pro_controller_default_buttons_mapping(blueretro):
    ''' Press each buttons and check if default mapping is right. '''
    # Connect device
    blueretro.disconnect()
    blueretro.send_system_id(GC)
    blueretro.connect()
    blueretro.send_name(DEVICE_NAME)

    blueretro.get_logs()
    blueretro.expect('# dev: 0 type: 5:0 Pro Controller', timeout=1)

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

    blueretro.flush_logs()

    # Validate buttons default mapping
    for sw_btns, br_btns in btns_generic_test_data(sw_n_btns_mask):
        blueretro.send_hid_report(
            'a1300180'
            f'{swap24(sw_btns):06x}'
            'f3d780'
            'e9977b'
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


def test_sw_pro_controller_axes_default_scaling(blueretro):
    ''' Set the various axes and check if the scaling is right. '''
    # Connect device
    blueretro.disconnect()
    blueretro.send_system_id(GC)
    blueretro.connect()
    blueretro.send_name(DEVICE_NAME)

    blueretro.get_logs()
    blueretro.expect('# dev: 0 type: 5:0 Pro Controller', timeout=1)

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

    blueretro.flush_logs()

    # Validate axes default scaling
    for axes in axes_test_data_generator(sw_n_axes, gc_axes, 0.0135):
        blueretro.send_hid_report(
            'a1300180'
            '000000'
            f'{swap24(axes[axis.LX]["wireless"] | axes[axis.LY]["wireless"] << 12):06x}'
            f'{swap24(axes[axis.RX]["wireless"] | axes[axis.RY]["wireless"] << 12):06x}'
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
        assert wireless['axes'][axis.RX] == axes[axis.RX]['wireless']
        assert wireless['axes'][axis.RY] == axes[axis.RY]['wireless']

        assert br_generic['axes'][axis.LX] == axes[axis.LX]['generic']
        assert br_generic['axes'][axis.LY] == axes[axis.LY]['generic']
        assert br_generic['axes'][axis.RX] == axes[axis.RX]['generic']
        assert br_generic['axes'][axis.RY] == axes[axis.RY]['generic']

        assert br_mapped['axes'][axis.LX] == axes[axis.LX]['mapped']
        assert br_mapped['axes'][axis.LY] == axes[axis.LY]['mapped']
        assert br_mapped['axes'][axis.RX] == axes[axis.RX]['mapped']
        assert br_mapped['axes'][axis.RY] == axes[axis.RY]['mapped']

        assert wired['axes'][axis.LX] == axes[axis.LX]['wired']
        assert wired['axes'][axis.LY] == axes[axis.LY]['wired']
        assert wired['axes'][axis.RX] == axes[axis.RX]['wired']
        assert wired['axes'][axis.RY] == axes[axis.RY]['wired']

    blueretro.disconnect()


def test_sw_pro_controller_axes_scaling_with_calib(blueretro):
    ''' Set the various axes and check if the scaling is right. '''
    # Connect device
    blueretro.disconnect()
    blueretro.send_system_id(GC)
    blueretro.connect()
    blueretro.send_name(DEVICE_NAME)

    blueretro.get_logs()
    blueretro.expect('# dev: 0 type: 5:0 Pro Controller', timeout=1)

    # Send calibration data
    blueretro.send_hid_report(
        'a121234000000027687fcd277a009010'
        '3d60000012f56564e1477ff8d564b167'
        '79f0755fbc7562000000000000000000'
        '0000'
    )
    blueretro.send_hid_report(
        'a121294000000028687fcd277a009010'
        '86600000120f3061ae90d9d414544115'
        '54c7799c333663000000000000000000'
        '0000'
    )
    blueretro.send_hid_report(
        'a1212f4000000028687fcd377a009010'
        '98600000120f3061ae90d9d414544115'
        '54c7799c333663000000000000000000'
        '0000'
    )
    blueretro.send_hid_report(
        'a121354000000027687fcd277a009010'
        '1080000016b2a10e6664e4c77dfc8563'
        'b2a1b1d778f185610226640000000000'
        '0000'
    )
    blueretro.get_logs()
    calib = blueretro.expect_json("calib_data")
    sw_calib_axes = {axis.LX: {}, axis.LY: {}, axis.RX: {}, axis.RY: {}}
    for ax in islice(axis, 0, 4):
        sw_calib_axes[ax]['neutral'] = calib['neutral'][ax]
        sw_calib_axes[ax]['abs_max'] = min(calib['rel_min'][ax], calib['rel_max'][ax])
        sw_calib_axes[ax]['deadzone'] = calib['deadzone'][ax]

    print(sw_calib_axes)
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

    blueretro.flush_logs()

    # Validate axes default scaling
    for axes in axes_test_data_generator(sw_calib_axes, gc_axes, 0.0135):
        blueretro.send_hid_report(
            'a1300180'
            '000000'
            f'{swap24(axes[axis.LX]["wireless"] | axes[axis.LY]["wireless"] << 12):06x}'
            f'{swap24(axes[axis.RX]["wireless"] | axes[axis.RY]["wireless"] << 12):06x}'
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
        assert wireless['axes'][axis.RX] == axes[axis.RX]['wireless']
        assert wireless['axes'][axis.RY] == axes[axis.RY]['wireless']

        assert br_generic['axes'][axis.LX] == axes[axis.LX]['generic']
        assert br_generic['axes'][axis.LY] == axes[axis.LY]['generic']
        assert br_generic['axes'][axis.RX] == axes[axis.RX]['generic']
        assert br_generic['axes'][axis.RY] == axes[axis.RY]['generic']

        assert br_mapped['axes'][axis.LX] == axes[axis.LX]['mapped']
        assert br_mapped['axes'][axis.LY] == axes[axis.LY]['mapped']
        assert br_mapped['axes'][axis.RX] == axes[axis.RX]['mapped']
        assert br_mapped['axes'][axis.RY] == axes[axis.RY]['mapped']

        assert wired['axes'][axis.LX] == axes[axis.LX]['wired']
        assert wired['axes'][axis.LY] == axes[axis.LY]['wired']
        assert wired['axes'][axis.RX] == axes[axis.RX]['wired']
        assert wired['axes'][axis.RY] == axes[axis.RY]['wired']

    blueretro.disconnect()
