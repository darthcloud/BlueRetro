''' Tests for the Wii Classic controller. '''
from itertools import islice
from device_data.test_data_generator import btns_generic_test_data
from device_data.test_data_generator import axes_test_data_generator
from bit_helper import swap16
from device_data.wii import wii_classic_core_btns_mask, wii_classic_btns_mask
from device_data.wii import wii_classic_axes, wii_classic_8bit_axes
from device_data.br import axis
from device_data.gc import gc_axes


DEVICE_NAME = 'Nintendo RVL-CNT-01'


def test_wii_classic_controller_default_buttons_mapping_8bytes_report(blueretro):
    ''' Press each buttons and check if default mapping is right. '''
    # Set device name
    rsp = blueretro.send_name(DEVICE_NAME)
    assert rsp['device_name']['device_id'] == 0
    assert rsp['device_name']['device_type'] == 2
    assert rsp['device_name']['device_subtype'] == 0
    assert rsp['device_name']['device_name'] == 'Nintendo RVL-CNT-01'

    # Extension init id responce
    rsp = blueretro.send_hid_report(
        'a12100005000fa'
        '0000a4200301'
        '00000000000000000000'
    )

    # Validate device type change
    assert rsp['type_update']['device_id'] == 0
    assert rsp['type_update']['device_type'] == 2
    assert rsp['type_update']['device_subtype'] == 3

    # Init adapter with a few neutral state report
    for _ in range(2):
        blueretro.send_hid_report(
            'a135'
            '0000'
            '86739c'
            '808080801616'
            'ffff'
            '0000000000000000'
        )

    # Validate buttons default mapping
    for btns, br_btns in btns_generic_test_data(wii_classic_core_btns_mask):
        rsp = blueretro.send_hid_report(
            'a135'
            f'{swap16(btns):04x}'
            '86739c'
            '808080801616'
            'ffff'
            '0000000000000000'
        )

        assert rsp['wireless_input']['btns'][0] == btns
        assert rsp['generic_input']['btns'][0] == br_btns

    # Validate buttons default mapping
    for btns, br_btns in btns_generic_test_data(wii_classic_btns_mask):
        btns ^= 0xFFFF
        rsp = blueretro.send_hid_report(
            'a135'
            '0000'
            '86739c'
            '808080801616'
            f'{swap16(btns):04x}'
            '0000000000000000'
        )

        assert rsp['wireless_input']['btns'][1] == btns
        assert rsp['generic_input']['btns'][0] == br_btns


def test_wii_classic_controller_axes_default_scaling_8bytes_report(blueretro):
    ''' Set the various axes and check if the scaling is right. '''
    # Set device name
    rsp = blueretro.send_name(DEVICE_NAME)
    assert rsp['device_name']['device_id'] == 0
    assert rsp['device_name']['device_type'] == 2
    assert rsp['device_name']['device_subtype'] == 0
    assert rsp['device_name']['device_name'] == 'Nintendo RVL-CNT-01'

    # Extension init id responce
    rsp = blueretro.send_hid_report(
        'a12100005000fa'
        '0000a4200301'
        '00000000000000000000'
    )

    # Validate device type change
    assert rsp['type_update']['device_id'] == 0
    assert rsp['type_update']['device_type'] == 2
    assert rsp['type_update']['device_subtype'] == 3

    # Init adapter with a few neutral state report
    for _ in range(2):
        blueretro.send_hid_report(
            'a135'
            '0000'
            '86739c'
            '808080801616'
            'ffff'
            '0000000000000000'
        )

    # Validate axes default scaling
    for axes in axes_test_data_generator(wii_classic_8bit_axes, gc_axes, 0.0135):
        rsp = blueretro.send_hid_report(
            'a135'
            '0000'
            '86739c'
            f'{axes[axis.LX]["wireless"]:02x}{axes[axis.RX]["wireless"]:02x}'
            f'{axes[axis.LY]["wireless"]:02x}{axes[axis.RY]["wireless"]:02x}'
            f'{axes[axis.LM]["wireless"]:02x}{axes[axis.RM]["wireless"]:02x}'
            'ffff'
            '0000000000000000'
        )

        for ax in islice(axis, 0, 6):
            assert rsp['wireless_input']['axes'][ax] == axes[ax]['wireless']
            assert rsp['generic_input']['axes'][ax] == axes[ax]['generic']
            assert rsp['mapped_input']['axes'][ax] == axes[ax]['mapped']
            assert rsp['wired_output']['axes'][ax] == axes[ax]['wired']


def test_wii_classic_controller_default_buttons_mapping_6bytes_report(blueretro):
    ''' Press each buttons and check if default mapping is right. '''
    # Set device name
    rsp = blueretro.send_name(DEVICE_NAME)
    assert rsp['device_name']['device_id'] == 0
    assert rsp['device_name']['device_type'] == 2
    assert rsp['device_name']['device_subtype'] == 0
    assert rsp['device_name']['device_name'] == 'Nintendo RVL-CNT-01'

    # Extension init id responce
    rsp = blueretro.send_hid_report(
        'a12100005000fa'
        '0000a4200101'
        '00000000000000000000'
    )

    # Validate device type change
    assert rsp['type_update']['device_id'] == 0
    assert rsp['type_update']['device_type'] == 2
    assert rsp['type_update']['device_subtype'] == 2

    # Init adapter with a few neutral state report
    for _ in range(2):
        blueretro.send_hid_report(
            'a135'
            '0000'
            '86739c'
            'a0201042'
            'ffff'
            '00000000000000000000'
        )

    # Validate buttons default mapping
    for btns, br_btns in btns_generic_test_data(wii_classic_core_btns_mask):
        rsp = blueretro.send_hid_report(
            'a135'
            f'{swap16(btns):04x}'
            '86739c'
            'a0201042'
            'ffff'
            '00000000000000000000'
        )

        assert rsp['wireless_input']['btns'][0] == btns
        assert rsp['generic_input']['btns'][0] == br_btns

    # Validate buttons default mapping
    for btns, br_btns in btns_generic_test_data(wii_classic_btns_mask):
        btns ^= 0xFFFF
        rsp = blueretro.send_hid_report(
            'a135'
            '0000'
            '86739c'
            'a0201042'
            f'{swap16(btns):04x}'
            '00000000000000000000'
        )

        assert rsp['wireless_input']['btns'][1] == btns
        assert rsp['generic_input']['btns'][0] == br_btns


def test_wii_classic_controller_axes_default_scaling_6bytes_report(blueretro):
    ''' Set the various axes and check if the scaling is right. '''
    # Set device name
    rsp = blueretro.send_name(DEVICE_NAME)
    assert rsp['device_name']['device_id'] == 0
    assert rsp['device_name']['device_type'] == 2
    assert rsp['device_name']['device_subtype'] == 0
    assert rsp['device_name']['device_name'] == 'Nintendo RVL-CNT-01'

    # Extension init id responce
    rsp = blueretro.send_hid_report(
        'a12100005000fa'
        '0000a4200101'
        '00000000000000000000'
    )

    # Validate device type change
    assert rsp['type_update']['device_id'] == 0
    assert rsp['type_update']['device_type'] == 2
    assert rsp['type_update']['device_subtype'] == 2

    # Init adapter with a few neutral state report
    for _ in range(2):
        blueretro.send_hid_report(
            'a135'
            '0000'
            '86739c'
            'a0201042'
            'ffff'
            '00000000000000000000'
        )

    # Validate axes default scaling
    # Skip deadzone tests as axes resolution is too low
    axes_gen = axes_test_data_generator(wii_classic_axes, gc_axes, 0.0135)
    for _ in range(5):
        axes = next(axes_gen)

        byte0 = ((axes[axis.RX]["wireless"] << 3) & 0xC0) | axes[axis.LX]["wireless"]
        byte1 = ((axes[axis.RX]["wireless"] << 5) & 0xC0) | axes[axis.LY]["wireless"]
        byte2 = ((axes[axis.RX]["wireless"] << 7) & 0x80) | ((axes[axis.LM]["wireless"] << 2) & 0x60) | axes[axis.RY]["wireless"]
        byte3 = ((axes[axis.LM]["wireless"] << 5) & 0xE0) | axes[axis.RM]["wireless"]

        rsp = blueretro.send_hid_report(
            'a135'
            '0000'
            '86739c'
            f'{byte0:02x}'
            f'{byte1:02x}'
            f'{byte2:02x}'
            f'{byte3:02x}'
            'ffff'
            '00000000000000000000'
        )

        for ax in islice(axis, 0, 6):
            assert rsp['wireless_input']['axes'][ax] == axes[ax]['wireless']
            assert rsp['generic_input']['axes'][ax] == axes[ax]['generic']
            assert rsp['mapped_input']['axes'][ax] == axes[ax]['mapped']
            assert rsp['wired_output']['axes'][ax] == axes[ax]['wired']
