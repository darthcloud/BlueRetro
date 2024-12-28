''' Tests for the PS5 DS controller. '''
from itertools import islice
from device_data.test_data_generator import btns_generic_test_data
from device_data.test_data_generator import axes_test_data_generator
from bit_helper import swap24
from device_data.ps import ps_btns_mask, ps_axes
from device_data.br import axis, hat_to_ld_btns
from device_data.gc import gc_axes


DEVICE_NAME = 'Wireless Controller'


def test_ps5_ds_controller_default_buttons_mapping_native_report(blueretro):
    ''' Press each buttons and check if default mapping is right. '''
    # Set device name
    rsp = blueretro.send_name(DEVICE_NAME)
    assert rsp['device_name']['device_id'] == 0
    assert rsp['device_name']['device_type'] == 4
    assert rsp['device_name']['device_subtype'] == 0
    assert rsp['device_name']['device_name'] == 'Wireless Controller'

    # Init adapter with a few neutral state report
    for _ in range(2):
        rsp |= blueretro.send_hid_report(
            'a13141'
            '80808080'
            '0000'
            '01'
            '080000'
            '0033d7728806000200f9fffffe1'
            'a20b405eeee5104018000000080'
            '000000000909000000000062005'
            '204040000345467f7b6bcc63d00'
            '000000000000000088975b7f'
        )

    # Validate device type change
    assert rsp['type_update']['device_id'] == 0
    assert rsp['type_update']['device_type'] == 4
    assert rsp['type_update']['device_subtype'] == 7

    # Validate buttons default mapping
    for btns, br_btns in btns_generic_test_data(ps_btns_mask):
        rsp = blueretro.send_hid_report(
            'a13141'
            '80808080'
            '0000'
            '01'
            f'{swap24(btns | 0x8):06x}'
            '0033d7728806000200f9fffffe1'
            'a20b405eeee5104018000000080'
            '000000000909000000000062005'
            '204040000345467f7b6bcc63d00'
            '000000000000000088975b7f'
        )

        assert rsp['wireless_input']['btns'] & 0xFFFFF0 == btns
        assert rsp['generic_input']['btns'][0] == br_btns

    # Validate hat default mapping
    for hat_value, br_btns in enumerate(hat_to_ld_btns):
        rsp = blueretro.send_hid_report(
            'a13141'
            '80808080'
            '0000'
            '01'
            f'0{hat_value:01x}0000'
            '0033d7728806000200f9fffffe1'
            'a20b405eeee5104018000000080'
            '000000000909000000000062005'
            '204040000345467f7b6bcc63d00'
            '000000000000000088975b7f'
        )

        assert rsp['wireless_input']['hat'] == hat_value
        assert rsp['generic_input']['btns'][0] == br_btns


def test_ps5_ds_controller_axes_default_scaling_native_report(blueretro):
    ''' Set the various axes and check if the scaling is right. '''
    # Set device name
    rsp = blueretro.send_name(DEVICE_NAME)
    assert rsp['device_name']['device_id'] == 0
    assert rsp['device_name']['device_type'] == 4
    assert rsp['device_name']['device_subtype'] == 0
    assert rsp['device_name']['device_name'] == 'Wireless Controller'

    # Init adapter with a few neutral state report
    for _ in range(2):
        rsp |= blueretro.send_hid_report(
            'a13141'
            '80808080'
            '0000'
            '01'
            '080000'
            '0033d7728806000200f9fffffe1'
            'a20b405eeee5104018000000080'
            '000000000909000000000062005'
            '204040000345467f7b6bcc63d00'
            '000000000000000088975b7f'
        )

    # Validate device type change
    assert rsp['type_update']['device_id'] == 0
    assert rsp['type_update']['device_type'] == 4
    assert rsp['type_update']['device_subtype'] == 7

    # Validate axes default scaling
    for axes in axes_test_data_generator(ps_axes, gc_axes, 0.0135):
        rsp = blueretro.send_hid_report(
            'a13141'
            f'{axes[axis.LX]["wireless"]:02x}{axes[axis.LY]["wireless"]:02x}'
            f'{axes[axis.RX]["wireless"]:02x}{axes[axis.RY]["wireless"]:02x}'
            f'{axes[axis.LM]["wireless"]:02x}{axes[axis.RM]["wireless"]:02x}'
            '01'
            '080000'
            '0033d7728806000200f9fffffe1'
            'a20b405eeee5104018000000080'
            '000000000909000000000062005'
            '204040000345467f7b6bcc63d00'
            '000000000000000088975b7f'
        )

        for ax in islice(axis, 0, 6):
            assert rsp['wireless_input']['axes'][ax] == axes[ax]['wireless']
            assert rsp['generic_input']['axes'][ax] == axes[ax]['generic']
            assert rsp['mapped_input']['axes'][ax] == axes[ax]['mapped']
            assert rsp['wired_output']['axes'][ax] == axes[ax]['wired']


def test_ps5_ds_controller_default_buttons_mapping_default_report(blueretro):
    ''' Press each buttons and check if default mapping is right. '''
    # Set device name
    rsp = blueretro.send_name(DEVICE_NAME)
    assert rsp['device_name']['device_id'] == 0
    assert rsp['device_name']['device_type'] == 4
    assert rsp['device_name']['device_subtype'] == 0
    assert rsp['device_name']['device_name'] == 'Wireless Controller'

    # Init adapter in PS5 mode
    for _ in range(2):
        rsp |= blueretro.send_hid_report(
            'a13141'
            '80808080'
            '0000'
            '01'
            '080000'
            '0033d7728806000200f9fffffe1'
            'a20b405eeee5104018000000080'
            '000000000909000000000062005'
            '204040000345467f7b6bcc63d00'
            '000000000000000088975b7f'
        )

    # Validate device type change
    assert rsp['type_update']['device_id'] == 0
    assert rsp['type_update']['device_type'] == 4
    assert rsp['type_update']['device_subtype'] == 7

    # Init adapter back to default report mode
    for _ in range(2):
        rsp |= blueretro.send_hid_report(
            'a101'
            '80808080'
            '080000'
            '0000'
        )

    # Validate device type change
    assert rsp['type_update']['device_id'] == 0
    assert rsp['type_update']['device_type'] == 4
    assert rsp['type_update']['device_subtype'] == 0

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


def test_ps5_ds_controller_axes_default_scaling_default_report(blueretro):
    ''' Set the various axes and check if the scaling is right. '''
    # Set device name
    rsp = blueretro.send_name(DEVICE_NAME)
    assert rsp['device_name']['device_id'] == 0
    assert rsp['device_name']['device_type'] == 4
    assert rsp['device_name']['device_subtype'] == 0
    assert rsp['device_name']['device_name'] == 'Wireless Controller'

    # Init adapter in PS5 mode
    for _ in range(2):
        rsp |= blueretro.send_hid_report(
            'a13141'
            '80808080'
            '0000'
            '01'
            '080000'
            '0033d7728806000200f9fffffe1'
            'a20b405eeee5104018000000080'
            '000000000909000000000062005'
            '204040000345467f7b6bcc63d00'
            '000000000000000088975b7f'
        )

    # Validate device type change
    assert rsp['type_update']['device_id'] == 0
    assert rsp['type_update']['device_type'] == 4
    assert rsp['type_update']['device_subtype'] == 7

    # Init adapter back to default report mode
    for _ in range(2):
        rsp |= blueretro.send_hid_report(
            'a101'
            '80808080'
            '080000'
            '0000'
        )

    # Validate device type change
    assert rsp['type_update']['device_id'] == 0
    assert rsp['type_update']['device_type'] == 4
    assert rsp['type_update']['device_subtype'] == 0

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
