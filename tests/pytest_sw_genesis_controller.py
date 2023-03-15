''' Tests for the Switch MD/Genesis controller. '''
from device_data.test_data_generator import btns_generic_test_data
from bit_helper import swap16, swap24
from device_data.sw import sw_d_gen_btns_mask, sw_n_gen_btns_mask
from device_data.br import hat_to_ld_btns
from device_data.gc import GC


DEVICE_NAME = 'MD/Gen Control Pad'


def test_sw_genesis_controller_default_buttons_mapping_native_report(blueretro):
    ''' Press each buttons and check if default mapping is right. '''
    # Set device name
    blueretro.send_name(DEVICE_NAME)
    blueretro.expect('# dev: 0 type: 5:16 MD/Gen Control Pad')

    # Init adapter with a few neutral state report
    for _ in range(2):
        blueretro.send_hid_report(
            'a1300180'
            '000000'
            '000000'
            '000000'
            '00000000000000000000000000'
            '00000000000000000000000000'
            '0000000000000000000000'
        )

    blueretro.flush_logs()

    # Validate buttons default mapping
    for sw_btns, br_btns in btns_generic_test_data(sw_n_gen_btns_mask):
        blueretro.send_hid_report(
            'a1300180'
            f'{swap24(sw_btns):06x}'
            '000000'
            '000000'
            '00000000000000000000000000'
            '00000000000000000000000000'
            '0000000000000000000000'
        )

        wireless = blueretro.expect_json('wireless_input')
        br_generic = blueretro.expect_json('generic_input')

        assert wireless['btns'] >> 8 == sw_btns
        assert br_generic['btns'][0] == br_btns


def test_sw_genesis_controller_default_buttons_mapping_default_report(blueretro):
    ''' Press each buttons and check if default mapping is right. '''
    # Set device name
    blueretro.send_name(DEVICE_NAME)
    blueretro.expect('# dev: 0 type: 5:16 MD/Gen Control Pad')

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
    for sw_btns, br_btns in btns_generic_test_data(sw_d_gen_btns_mask):
        blueretro.send_hid_report(
            'a13f'
            f'{swap16(sw_btns):04x}'
            '0f'
            '0080008000800080'
        )

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

        wireless = blueretro.expect_json('wireless_input')
        br_generic = blueretro.expect_json('generic_input')

        assert wireless['hat'] == hat_value
        assert br_generic['btns'][0] == br_btns
