''' Tests for the PowerA Switch GC controller. '''
from itertools import islice
from device_data.test_data_generator import btns_generic_test_data
from device_data.test_data_generator import axes_test_data_generator
from bit_helper import swap16, swap24
from device_data.sw import sw_d_pwa_gc_btns_mask, sw_n_pwa_gc_btns_mask, sw_n_axes
from device_data.br import axis, hat_to_ld_btns
from device_data.gc import GC, gc_axes


DEVICE_NAME = 'Lic Pro Controller'


def test_sw_powera_gc_default_buttons_mapping_native_report(blueretro):
    ''' Press each buttons and check if default mapping is right. '''
    # Connect device
    blueretro.disconnect()
    blueretro.send_system_id(GC)
    blueretro.connect()
    blueretro.send_name(DEVICE_NAME)

    blueretro.get_logs()
    blueretro.expect('# dev: 0 type: 5:17 Lic Pro Controller', timeout=1)

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
    for sw_btns, br_btns in btns_generic_test_data(sw_n_pwa_gc_btns_mask):
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


def test_sw_powera_gc_controller_axes_default_scaling_native_report(blueretro):
    ''' Set the various axes and check if the scaling is right. '''
    # Connect device
    blueretro.disconnect()
    blueretro.send_system_id(GC)
    blueretro.connect()
    blueretro.send_name(DEVICE_NAME)

    blueretro.get_logs()
    blueretro.expect('# dev: 0 type: 5:17 Lic Pro Controller', timeout=1)

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


def test_sw_powera_gc_axes_scaling_with_calib_native_report(blueretro):
    ''' Set the various axes and check if the scaling is right. '''
    # Connect device
    blueretro.disconnect()
    blueretro.send_system_id(GC)
    blueretro.connect()
    blueretro.send_name(DEVICE_NAME)

    blueretro.get_logs()
    blueretro.expect('# dev: 0 type: 5:17 Lic Pro Controller', timeout=1)

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
    blueretro.send_hid_report(
        'a121338000000033387d8e0777009010'
        '1080000016ffffffffffffffffffffff'
        'ffffffffffffffffffffff0000000000'
        '0000'
    )
    blueretro.get_logs()
    calib = blueretro.expect_json("calib_data")
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


def test_sw_powera_gc_default_buttons_mapping_default_report(blueretro):
    ''' Press each buttons and check if default mapping is right. '''
    # Connect device
    blueretro.disconnect()
    blueretro.send_system_id(GC)
    blueretro.connect()
    blueretro.send_name(DEVICE_NAME)

    blueretro.get_logs()
    blueretro.expect('# dev: 0 type: 5:17 Lic Pro Controller', timeout=1)

    # Init adapter with a few neutral state report
    for _ in range(2):
        blueretro.send_hid_report(
            'a13f'
            '0000'
            '0f'
            '0080008000800080'
        )

    blueretro.flush_logs()

    # Validate buttons default mapping
    for sw_btns, br_btns in btns_generic_test_data(sw_d_pwa_gc_btns_mask):
        blueretro.send_hid_report(
            'a13f'
            f'{swap16(sw_btns):04x}'
            '0f'
            '0080008000800080'
        )

        blueretro.get_logs()

        wireless = blueretro.expect_json('wireless_input')
        br_generic = blueretro.expect_json('generic_input')

        assert wireless['btns'] == sw_btns
        assert br_generic['btns'][0] == br_btns

    # Validate hat default mapping
    for hat_value, br_btns in enumerate(hat_to_ld_btns):
        blueretro.send_hid_report(
            'a13f'
            '0000'
            f'0{hat_value:01x}'
            '0080008000800080'
        )

        blueretro.get_logs()

        wireless = blueretro.expect_json('wireless_input')
        br_generic = blueretro.expect_json('generic_input')

        assert wireless['hat'] == hat_value
        assert br_generic['btns'][0] == br_btns

    blueretro.disconnect()


def test_sw_powera_gc_controller_axes_default_scaling_default_report(blueretro):
    ''' Set the various axes and check if the scaling is right. '''
    # Connect device
    blueretro.disconnect()
    blueretro.send_system_id(GC)
    blueretro.connect()
    blueretro.send_name(DEVICE_NAME)

    blueretro.get_logs()
    blueretro.expect('# dev: 0 type: 5:17 Lic Pro Controller', timeout=1)

    # Init adapter with a few neutral state report
    for _ in range(2):
        blueretro.send_hid_report(
            'a13f'
            '0000'
            '0f'
            '0080008000800080'
        )

    blueretro.flush_logs()

    # Validate axes default scaling
    for axes in axes_test_data_generator(sw_n_axes, gc_axes, 0.0135):
        lx_shift = axes[axis.LX]['wireless'] << 4
        ly_inverted = ((axes[axis.LY]["wireless"] << 4) ^ 0xFFFF) + 1
        rx_shift = axes[axis.RX]['wireless'] << 4
        ry_inverted = ((axes[axis.RY]["wireless"] << 4) ^ 0xFFFF) + 1
        blueretro.send_hid_report(
            'a13f'
            '0000'
            '0f'
            f'{swap16(lx_shift):04x}{swap16(ly_inverted):04x}'
            f'{swap16(rx_shift):04x}{swap16(ry_inverted):04x}'
        )

        blueretro.get_logs()

        wireless = blueretro.expect_json('wireless_input')
        br_generic = blueretro.expect_json('generic_input')
        br_mapped = blueretro.expect_json('mapped_input')
        wired = blueretro.expect_json('wired_output')

        assert wireless['axes'][axis.LX] == lx_shift
        assert wireless['axes'][axis.LY] == ly_inverted
        assert wireless['axes'][axis.RX] == rx_shift
        assert wireless['axes'][axis.RY] == ry_inverted

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


def test_sw_powera_gc_axes_scaling_with_calib_default_report(blueretro):
    ''' Set the various axes and check if the scaling is right. '''
    # Connect device
    blueretro.disconnect()
    blueretro.send_system_id(GC)
    blueretro.connect()
    blueretro.send_name(DEVICE_NAME)

    blueretro.get_logs()
    blueretro.expect('# dev: 0 type: 5:17 Lic Pro Controller', timeout=1)

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
    blueretro.send_hid_report(
        'a121338000000033387d8e0777009010'
        '1080000016ffffffffffffffffffffff'
        'ffffffffffffffffffffff0000000000'
        '0000'
    )
    blueretro.get_logs()
    calib = blueretro.expect_json("calib_data")
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

    blueretro.flush_logs()

    # Validate axes default scaling
    for axes in axes_test_data_generator(sw_n_axes, gc_axes, 0.0135):
        lx_shift = axes[axis.LX]['wireless'] << 4
        ly_inverted = ((axes[axis.LY]["wireless"] << 4) ^ 0xFFFF) + 1
        rx_shift = axes[axis.RX]['wireless'] << 4
        ry_inverted = ((axes[axis.RY]["wireless"] << 4) ^ 0xFFFF) + 1
        blueretro.send_hid_report(
            'a13f'
            '0000'
            '0f'
            f'{swap16(lx_shift):04x}{swap16(ly_inverted):04x}'
            f'{swap16(rx_shift):04x}{swap16(ry_inverted):04x}'
        )

        blueretro.get_logs()

        wireless = blueretro.expect_json('wireless_input')
        br_generic = blueretro.expect_json('generic_input')
        br_mapped = blueretro.expect_json('mapped_input')
        wired = blueretro.expect_json('wired_output')

        assert wireless['axes'][axis.LX] == lx_shift
        assert wireless['axes'][axis.LY] == ly_inverted
        assert wireless['axes'][axis.RX] == rx_shift
        assert wireless['axes'][axis.RY] == ry_inverted

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
