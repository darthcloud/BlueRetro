''' Tests for the HK Switch Admiral controller. '''
from pytest import approx
from device_data.test_data_generator import btns_generic_test_data
from device_data.test_data_generator import axes_test_data_generator
from bit_helper import swap16
from device_data.sw import sw_hk_admiral_btns_mask, sw_hk_admiral_axes
from device_data.br import axis, hat_to_ld_btns
from device_data.gc import GC, gc_axes


DEVICE_NAME = 'Hyperkin Pad'


def test_sw_hk_admiral_controller_default_buttons_mapping(blueretro):
    ''' Press each buttons and check if default mapping is right. '''
    # Connect device
    blueretro.disconnect()
    blueretro.send_system_id(GC)
    blueretro.connect()
    blueretro.send_name(DEVICE_NAME)

    blueretro.get_logs()
    blueretro.expect('# dev: 0 type: 5:18 Hyperkin Pad', timeout=1)

    # Init adapter with a few neutral state report
    for _ in range(2):
        blueretro.send_hid_report(
            'a13f'
            '0000'
            '00'
            '0080008000800080'
        )

    blueretro.flush_logs()

    # Validate buttons default mapping
    for sw_btns, br_btns in btns_generic_test_data(sw_hk_admiral_btns_mask):
        blueretro.send_hid_report(
            'a13f'
            f'{swap16(sw_btns):04x}'
            '00'
            '0080008000800080'
        )

        blueretro.get_logs()

        wireless = blueretro.expect_json('wireless_input')
        br_generic = blueretro.expect_json('generic_input')

        assert wireless['btns'] == sw_btns
        assert br_generic['btns'][0] == br_btns

    # Validate hat default mapping
    shifted_hat = hat_to_ld_btns[-1:] + hat_to_ld_btns[:-1]
    for hat_value, br_btns in enumerate(shifted_hat):
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


def test_sw_hk_admiral_controller_axes_default_scaling(blueretro):
    ''' Set the various axes and check if the scaling is right. '''
    # Connect device
    blueretro.disconnect()
    blueretro.send_system_id(GC)
    blueretro.connect()
    blueretro.send_name(DEVICE_NAME)

    blueretro.get_logs()
    blueretro.expect('# dev: 0 type: 5:18 Hyperkin Pad', timeout=1)

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
    for axes in axes_test_data_generator(sw_hk_admiral_axes, gc_axes, 0.0135):
        ly_inverted = (axes[axis.LY]["wireless"] ^ 0xFFFF) + 1
        blueretro.send_hid_report(
            'a13f'
            '0000'
            '0f'
            f'{swap16(axes[axis.LX]["wireless"]):04x}{swap16(ly_inverted):04x}'
            '00800080'
        )

        blueretro.get_logs()

        wireless = blueretro.expect_json('wireless_input')
        br_generic = blueretro.expect_json('generic_input')
        br_mapped = blueretro.expect_json('mapped_input')
        wired = blueretro.expect_json('wired_output')

        if axes[axis.LX]['wireless'] >= (2 ** 16):
            # When type size is max out, positive value max is one unit lower
            assert wireless['axes'][axis.LX] == approx(axes[axis.LX]['wireless'], 1)
            assert br_generic['axes'][axis.LX] == approx(int(axes[axis.LX]['generic'] / 16), 1)
            assert br_mapped['axes'][axis.LX] == approx(axes[axis.LX]['mapped'], 1)
            assert wired['axes'][axis.LX] == approx(axes[axis.LX]['wired'], 1)
        else:
            assert wireless['axes'][axis.LX] == axes[axis.LX]['wireless']
            assert br_generic['axes'][axis.LX] == int(axes[axis.LX]['generic'] / 16)
            assert br_mapped['axes'][axis.LX] == axes[axis.LX]['mapped']
            assert wired['axes'][axis.LX] == axes[axis.LX]['wired']

        if ly_inverted >= (2 ** 16):
            # When type size is max out, positive value max is one unit lower
            assert wireless['axes'][axis.LY] == approx(ly_inverted, 1)
            assert br_generic['axes'][axis.LY] == approx(int(axes[axis.LY]['generic'] / 16), 1)
            assert br_mapped['axes'][axis.LY] == approx(axes[axis.LY]['mapped'], 1)
            assert wired['axes'][axis.LY] == approx(axes[axis.LY]['wired'], 1)
        else:
            assert wireless['axes'][axis.LY] == ly_inverted
            assert br_generic['axes'][axis.LY] == int(axes[axis.LY]['generic'] / 16)
            assert br_mapped['axes'][axis.LY] == axes[axis.LY]['mapped']
            assert wired['axes'][axis.LY] == axes[axis.LY]['wired']

    blueretro.disconnect()
