''' Tests for the RF Switch Brawler64 controller. '''
from pytest import approx
from device_data.test_data_generator import btns_generic_test_data
from device_data.test_data_generator import axes_test_data_generator
from bit_helper import swap16
from device_data.sw import sw_rf_brawler64_btns_mask, sw_rf_brawler64_axes
from device_data.br import axis
from device_data.gc import GC, gc_axes


DEVICE_NAME = 'N64 Controller'


def test_sw_rf_brawler64_controller_default_buttons_mapping(blueretro):
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
            'a13f'
            '0000'
            '0f'
            '0080008000800080'
        )

    blueretro.flush_logs()

    # Validate buttons default mapping
    for sw_btns, br_btns in btns_generic_test_data(sw_rf_brawler64_btns_mask):
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

    blueretro.disconnect()


def test_sw_rf_brawler64_controller_axes_default_scaling(blueretro):
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
            'a13f'
            '0000'
            '0f'
            '0080008000800080'
        )

    blueretro.flush_logs()

    # Validate axes default scaling
    for axes in axes_test_data_generator(sw_rf_brawler64_axes, gc_axes, 0.0135):
        blueretro.send_hid_report(
            'a13f'
            '0000'
            '0f'
            f'{swap16(axes[axis.LX]["wireless"]):04x}{swap16(axes[axis.LY]["wireless"]):04x}'
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
            assert br_generic['axes'][axis.LX] == approx(axes[axis.LX]['generic'], 1)
            assert br_mapped['axes'][axis.LX] == approx(axes[axis.LX]['mapped'], 1)
            assert wired['axes'][axis.LX] == approx(axes[axis.LX]['wired'], 1)
        else:
            assert wireless['axes'][axis.LX] == axes[axis.LX]['wireless']
            assert br_generic['axes'][axis.LX] == axes[axis.LX]['generic']
            assert br_mapped['axes'][axis.LX] == axes[axis.LX]['mapped']
            assert wired['axes'][axis.LX] == axes[axis.LX]['wired']

        if axes[axis.LY]['wireless'] >= (2 ** 16):
            # When type size is max out, positive value max is one unit lower
            assert wireless['axes'][axis.LY] == approx(axes[axis.LY]['wireless'], 1)
            assert br_generic['axes'][axis.LY] == approx(axes[axis.LY]['generic'], 1)
            assert br_mapped['axes'][axis.LY] == approx(axes[axis.LY]['mapped'], 1)
            assert wired['axes'][axis.LY] == approx(axes[axis.LY]['wired'], 1)
        else:
            assert wireless['axes'][axis.LY] == axes[axis.LY]['wireless']
            assert br_generic['axes'][axis.LY] == axes[axis.LY]['generic']
            assert br_mapped['axes'][axis.LY] == axes[axis.LY]['mapped']
            assert wired['axes'][axis.LY] == axes[axis.LY]['wired']

    blueretro.disconnect()
