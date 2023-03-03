''' Tests for the Switch Pro controller. '''
from bit_helper import bit, swap24
from device_data.sw import sw_n, sw_n_axes
from device_data.br import AxesData, pad, axis
from device_data.gc import GC, gc_axes


DEVICE_NAME = 'Pro Controller'
buttons_wireless_to_generic = {
    0: 0,
    bit(sw_n.Y): bit(pad.RB_LEFT), bit(sw_n.X): bit(pad.RB_UP),
    bit(sw_n.B): bit(pad.RB_DOWN),bit(sw_n.A): bit(pad.RB_RIGHT),
    bit(sw_n.R_SR): bit(pad.RT), bit(sw_n.R_SL): bit(pad.LT),
    bit(sw_n.R): bit(pad.RS), bit(sw_n.ZR): bit(pad.RM),
    bit(sw_n.MINUS): bit(pad.MS), bit(sw_n.PLUS): bit(pad.MM),
    bit(sw_n.RJ): bit(pad.RJ), bit(sw_n.LJ): bit(pad.LJ),
    bit(sw_n.HOME): bit(pad.MT), bit(sw_n.CAPTURE): bit(pad.MQ),
    bit(sw_n.DOWN): bit(pad.LD_DOWN), bit(sw_n.UP): bit(pad.LD_UP),
    bit(sw_n.RIGHT): bit(pad.LD_RIGHT), bit(sw_n.LEFT): bit(pad.LD_LEFT),
    bit(sw_n.L_SR): bit(pad.RT), bit(sw_n.L_SL): bit(pad.LT),
    bit(sw_n.L): bit(pad.LS), bit(sw_n.ZL): bit(pad.LM),
    0xFFFFFF: 0xFFFF0F00,
}
axes_wireless_to_generic = [
    {
        axis.LX: AxesData(sw_n_axes[axis.LX]['neutral'], 0, 0, gc_axes[axis.LX]['neutral']),
        axis.LY: AxesData(sw_n_axes[axis.LY]['neutral'], 0, 0, gc_axes[axis.LY]['neutral']),
        axis.RX: AxesData(sw_n_axes[axis.RX]['neutral'], 0, 0, gc_axes[axis.RX]['neutral']),
        axis.RY: AxesData(sw_n_axes[axis.RY]['neutral'], 0, 0, gc_axes[axis.RY]['neutral']),
    },
    {
        axis.LX: AxesData(
            sw_n_axes[axis.LX]['neutral'] + sw_n_axes[axis.LX]['abs_max'],
            sw_n_axes[axis.LX]['abs_max'],
            (gc_axes[axis.LX]['abs_max'] / sw_n_axes[axis.LX]['abs_max'])
                * sw_n_axes[axis.LX]['abs_max'],
            ((gc_axes[axis.LX]['abs_max'] / sw_n_axes[axis.LX]['abs_max'])
                * sw_n_axes[axis.LX]['abs_max']) + gc_axes[axis.LX]['neutral']),
        axis.LY: AxesData(
            sw_n_axes[axis.LY]['neutral'] - sw_n_axes[axis.LY]['abs_max'],
            -sw_n_axes[axis.LY]['abs_max'],
            -(gc_axes[axis.LY]['abs_max'] / sw_n_axes[axis.LY]['abs_max'])
                * sw_n_axes[axis.LY]['abs_max'],
            (-(gc_axes[axis.LY]['abs_max'] / sw_n_axes[axis.LY]['abs_max'])
                * sw_n_axes[axis.LY]['abs_max']) + gc_axes[axis.LY]['neutral']),
        axis.RX: AxesData(
            sw_n_axes[axis.RX]['neutral'] - sw_n_axes[axis.RX]['abs_max'],
            -sw_n_axes[axis.RX]['abs_max'],
            -(gc_axes[axis.RX]['abs_max'] / sw_n_axes[axis.RX]['abs_max'])
                * sw_n_axes[axis.RX]['abs_max'],
            (-(gc_axes[axis.RX]['abs_max'] / sw_n_axes[axis.RX]['abs_max'])
                * sw_n_axes[axis.RX]['abs_max']) + gc_axes[axis.RX]['neutral']),
        axis.RY: AxesData(
            sw_n_axes[axis.RY]['neutral'] + sw_n_axes[axis.RY]['abs_max'],
            sw_n_axes[axis.RY]['abs_max'],
            (gc_axes[axis.RY]['abs_max'] / sw_n_axes[axis.RY]['abs_max'])
                * sw_n_axes[axis.RY]['abs_max'],
            ((gc_axes[axis.RY]['abs_max'] / sw_n_axes[axis.RY]['abs_max'])
                * sw_n_axes[axis.RY]['abs_max']) + gc_axes[axis.RY]['neutral']),
    },
]


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
    for sw_btns, br_btns in buttons_wireless_to_generic.items():
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
        _br_mapped = blueretro.expect_json('mapped_input')
        _wired = blueretro.expect_json('wired_output')

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
    for axes in axes_wireless_to_generic:
        blueretro.send_hid_report(
            'a1300180'
            '000000'
            f'{swap24(axes[axis.LX].wireless | axes[axis.LY].wireless << 12):06x}'
            f'{swap24(axes[axis.RX].wireless | axes[axis.RY].wireless << 12):06x}'
            '00000000000000000000000000'
            '00000000000000000000000000'
            '0000000000000000000000'
        )

        blueretro.get_logs()

        wireless = blueretro.expect_json('wireless_input')
        br_generic = blueretro.expect_json('generic_input')
        br_mapped = blueretro.expect_json('mapped_input')
        wired = blueretro.expect_json('wired_output')

        assert wireless['axes'][axis.LX] == axes[axis.LX].wireless
        assert wireless['axes'][axis.LY] == axes[axis.LY].wireless
        assert wireless['axes'][axis.RX] == axes[axis.RX].wireless
        assert wireless['axes'][axis.RY] == axes[axis.RY].wireless

        assert br_generic['axes'][axis.LX] == axes[axis.LX].generic
        assert br_generic['axes'][axis.LY] == axes[axis.LY].generic
        assert br_generic['axes'][axis.RX] == axes[axis.RX].generic
        assert br_generic['axes'][axis.RY] == axes[axis.RY].generic

        assert br_mapped['axes'][axis.LX] == axes[axis.LX].mapped
        assert br_mapped['axes'][axis.LY] == axes[axis.LY].mapped
        assert br_mapped['axes'][axis.RX] == axes[axis.RX].mapped
        assert br_mapped['axes'][axis.RY] == axes[axis.RY].mapped

        assert wired['axes'][axis.LX] == axes[axis.LX].wired
        assert wired['axes'][axis.LY] == axes[axis.LY].wired
        assert wired['axes'][axis.RX] == axes[axis.RX].wired
        assert wired['axes'][axis.RY] == axes[axis.RY].wired

    blueretro.disconnect()
