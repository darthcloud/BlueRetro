''' Tests for the PS4 DS4 controller. '''
from itertools import islice
from device_data.test_data_generator import btns_generic_test_data
from device_data.test_data_generator import axes_test_data_generator
from bit_helper import swap24
from device_data.ps import ps_btns_mask, ps_axes
from device_data.br import axis, hat_to_ld_btns, bt_type, bt_subtype
from device_data.gc import gc_axes


DEVICE_NAME = 'Wireless Controller'


def test_ps4_ds4_controller_default_buttons_mapping_native_report(blueretro):
    ''' Press each buttons and check if default mapping is right. '''
    # Set device name
    rsp = blueretro.send_name(DEVICE_NAME)
    assert rsp['device_name']['device_id'] == 0
    assert rsp['device_name']['device_type'] == bt_type.PS
    assert rsp['device_name']['device_subtype'] == bt_subtype.SUBTYPE_DEFAULT
    assert rsp['device_name']['device_name'] == 'Wireless Controller'

    # Init adapter with a few neutral state report
    for _ in range(2):
        blueretro.send_hid_report(
            'a111c000'
            '80808080'
            '080000'
            '0000'
            'bdc50415004700e6ff7301c51ba'
            '8ef000000000002000000008000'
            '000080000000008000000080000'
            '000008000000080000000008000'
            '0000800000000000cd2fab27'
        )

    # Validate buttons default mapping
    for btns, br_btns in btns_generic_test_data(ps_btns_mask):
        rsp = blueretro.send_hid_report(
            'a111c000'
            '80808080'
            f'{swap24(btns | 0x8):06x}'
            '0000'
            'bdc50415004700e6ff7301c51ba'
            '8ef000000000002000000008000'
            '000080000000008000000080000'
            '000008000000080000000008000'
            '0000800000000000cd2fab27'
        )

        assert rsp['wireless_input']['btns'] & 0xFFFFF0 == btns
        assert rsp['generic_input']['btns'][0] == br_btns

    # Validate hat default mapping
    for hat_value, br_btns in enumerate(hat_to_ld_btns):
        rsp = blueretro.send_hid_report(
            'a111c000'
            '80808080'
            f'0{hat_value:01x}0000'
            '0000'
            'bdc50415004700e6ff7301c51ba'
            '8ef000000000002000000008000'
            '000080000000008000000080000'
            '000008000000080000000008000'
            '0000800000000000cd2fab27'
        )

        assert rsp['wireless_input']['hat'] == hat_value
        assert rsp['generic_input']['btns'][0] == br_btns


def test_ps4_ds4_controller_axes_default_scaling_native_report(blueretro):
    ''' Set the various axes and check if the scaling is right. '''
    # Set device name
    rsp = blueretro.send_name(DEVICE_NAME)
    assert rsp['device_name']['device_id'] == 0
    assert rsp['device_name']['device_type'] == bt_type.PS
    assert rsp['device_name']['device_subtype'] == bt_subtype.SUBTYPE_DEFAULT
    assert rsp['device_name']['device_name'] == 'Wireless Controller'

    # Init adapter with a few neutral state report
    for _ in range(2):
        blueretro.send_hid_report(
            'a111c000'
            '80808080'
            '080000'
            '0000'
            'bdc50415004700e6ff7301c51ba'
            '8ef000000000002000000008000'
            '000080000000008000000080000'
            '000008000000080000000008000'
            '0000800000000000cd2fab27'
        )

    # Validate axes default scaling
    for axes in axes_test_data_generator(ps_axes, gc_axes, 0.0135):
        rsp = blueretro.send_hid_report(
            'a111c000'
            f'{axes[axis.LX]["wireless"]:02x}{axes[axis.LY]["wireless"]:02x}'
            f'{axes[axis.RX]["wireless"]:02x}{axes[axis.RY]["wireless"]:02x}'
            '080000'
            f'{axes[axis.LM]["wireless"]:02x}{axes[axis.RM]["wireless"]:02x}'
            'bdc50415004700e6ff7301c51ba'
            '8ef000000000002000000008000'
            '000080000000008000000080000'
            '000008000000080000000008000'
            '0000800000000000cd2fab27'
        )

        for ax in islice(axis, 0, 6):
            assert rsp['wireless_input']['axes'][ax] == axes[ax]['wireless']
            assert rsp['generic_input']['axes'][ax] == axes[ax]['generic']
            assert rsp['mapped_input']['axes'][ax] == axes[ax]['mapped']
            assert rsp['wired_output']['axes'][ax] == axes[ax]['wired']


def test_ps4_ds4_controller_default_buttons_mapping_default_report(blueretro):
    ''' Press each buttons and check if default mapping is right. '''
    # Set device name
    rsp = blueretro.send_name(DEVICE_NAME)
    assert rsp['device_name']['device_id'] == 0
    assert rsp['device_name']['device_type'] == bt_type.PS
    assert rsp['device_name']['device_subtype'] == bt_subtype.SUBTYPE_DEFAULT
    assert rsp['device_name']['device_name'] == 'Wireless Controller'

    # Init adapter with a few neutral state report
    for _ in range(2):
        blueretro.send_hid_report(
            'a101'
            '80808080'
            '080000'
            '0000'
        )

    # Validate buttons default mapping
    for btns, br_btns in btns_generic_test_data(ps_btns_mask):
        rsp = blueretro.send_hid_report(
            'a101'
            '80808080'
            f'{swap24(btns | 0x8):06x}'
            '0000'
        )

        assert rsp['wireless_input']['btns'] & 0xFFFFF0 == btns
        assert rsp['generic_input']['btns'][0] == br_btns

    # Validate hat default mapping
    for hat_value, br_btns in enumerate(hat_to_ld_btns):
        rsp = blueretro.send_hid_report(
            'a101'
            '80808080'
            f'0{hat_value:01x}0000'
            '0000'
        )

        assert rsp['wireless_input']['hat'] == hat_value
        assert rsp['generic_input']['btns'][0] == br_btns


def test_ps4_ds4_controller_axes_default_scaling_default_report(blueretro):
    ''' Set the various axes and check if the scaling is right. '''
    # Set device name
    rsp = blueretro.send_name(DEVICE_NAME)
    assert rsp['device_name']['device_id'] == 0
    assert rsp['device_name']['device_type'] == bt_type.PS
    assert rsp['device_name']['device_subtype'] == bt_subtype.SUBTYPE_DEFAULT
    assert rsp['device_name']['device_name'] == 'Wireless Controller'

    # Init adapter with a few neutral state report
    for _ in range(2):
        blueretro.send_hid_report(
            'a101'
            '80808080'
            '080000'
            '0000'
        )

    # Validate axes default scaling
    for axes in axes_test_data_generator(ps_axes, gc_axes, 0.0135):
        rsp = blueretro.send_hid_report(
            'a101'
            f'{axes[axis.LX]["wireless"]:02x}{axes[axis.LY]["wireless"]:02x}'
            f'{axes[axis.RX]["wireless"]:02x}{axes[axis.RY]["wireless"]:02x}'
            '080000'
            f'{axes[axis.LM]["wireless"]:02x}{axes[axis.RM]["wireless"]:02x}'
        )

        for ax in islice(axis, 0, 6):
            assert rsp['wireless_input']['axes'][ax] == axes[ax]['wireless']
            assert rsp['generic_input']['axes'][ax] == axes[ax]['generic']
            assert rsp['mapped_input']['axes'][ax] == axes[ax]['mapped']
            assert rsp['wired_output']['axes'][ax] == axes[ax]['wired']
