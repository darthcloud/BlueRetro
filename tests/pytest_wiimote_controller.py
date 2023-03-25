''' Tests for the Wii controller. '''
from device_data.test_data_generator import btns_generic_test_data
from bit_helper import swap16
from device_data.wii import wii_core_btns_mask


DEVICE_NAME = 'Nintendo RVL-CNT-01'


def test_wii_controller_default_buttons_mapping(blueretro):
    ''' Press each buttons and check if default mapping is right. '''
    # Set device name
    blueretro.send_name(DEVICE_NAME)
    blueretro.expect('# dev: 0 type: 2:0 Nintendo RVL-CNT-01')

    # Init adapter with a few neutral state report
    for _ in range(2):
        blueretro.send_hid_report(
            'a135'
            '0000'
            '6e85a1'
            'ffffffffffffffffffffffffff'
            'ffffff'
        )

    blueretro.flush_logs()

    # Validate buttons default mapping
    for btns, br_btns in btns_generic_test_data(wii_core_btns_mask):
        blueretro.send_hid_report(
            'a135'
            f'{swap16(btns):04x}'
            '6e85a1'
            'ffffffffffffffffffffffffff'
            'ffffff'
        )

        wireless = blueretro.expect_json('wireless_input')
        br_generic = blueretro.expect_json('generic_input')

        assert wireless['btns'] == btns
        assert br_generic['btns'][0] == br_btns
