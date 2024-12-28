''' Tests for the Wii controller. '''
from device_data.test_data_generator import btns_generic_test_data
from bit_helper import swap16
from device_data.wii import wii_core_btns_mask


DEVICE_NAME = 'Nintendo RVL-CNT-01'


def test_wii_controller_default_buttons_mapping(blueretro):
    ''' Press each buttons and check if default mapping is right. '''
    # Set device name
    rsp = blueretro.send_name(DEVICE_NAME)
    assert rsp['device_name']['device_id'] == 0
    assert rsp['device_name']['device_type'] == 2
    assert rsp['device_name']['device_subtype'] == 0
    assert rsp['device_name']['device_name'] == 'Nintendo RVL-CNT-01'

    # Init adapter with a few neutral state report
    for _ in range(2):
        blueretro.send_hid_report(
            'a135'
            '0000'
            '6e85a1'
            'ffffffffffffffffffffffffff'
            'ffffff'
        )

    # Validate buttons default mapping
    for btns, br_btns in btns_generic_test_data(wii_core_btns_mask):
        rsp = blueretro.send_hid_report(
            'a135'
            f'{swap16(btns):04x}'
            '6e85a1'
            'ffffffffffffffffffffffffff'
            'ffffff'
        )

        assert rsp['wireless_input']['btns'] == btns
        assert rsp['generic_input']['btns'][0] == br_btns
