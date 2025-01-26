''' Tests for the Xbox One S & Series X|S controllers with BLE firmware. '''
import pytest
from itertools import islice
from device_data.test_data_generator import btns_generic_test_data
from device_data.test_data_generator import axes_test_data_generator
from bit_helper import swap16, swap24
from device_data.xbox import xbox_ble_btns_mask, xbox_axes
from device_data.br import system, dev_mode, bt_conn_type, axis, hat_to_ld_btns
from device_data.gc import gc_axes


DEVICE_NAME = 'Xbox Wireless Controller'
HID_DESC = bytes([
    0x05, 0x01,        # Usage Page (Generic Desktop)
    0x09, 0x05,        # Usage (Gamepad)
    0xA1, 0x01,        # Collection (Application)
    0x85, 0x01,        #   Report ID (1)
    0x09, 0x01,        #   Usage (Pointer)
    0xA1, 0x00,        #   Collection (Physical)
    0x09, 0x30,        #     Usage (X)
    0x09, 0x31,        #     Usage (Y)
    0x15, 0x00,        #     Logical Minimum (0)
    0x27, 0xFF, 0xFF, 0x00, 0x00,  #     Logical Maximum (65534)
    0x95, 0x02,        #     Report Count (2)
    0x75, 0x10,        #     Report Size (16)
    0x81, 0x02,        #     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0xC0,              #   End Collection
    0x09, 0x01,        #   Usage (Pointer)
    0xA1, 0x00,        #   Collection (Physical)
    0x09, 0x32,        #     Usage (Z)
    0x09, 0x35,        #     Usage (Rz)
    0x15, 0x00,        #     Logical Minimum (0)
    0x27, 0xFF, 0xFF, 0x00, 0x00,  #     Logical Maximum (65534)
    0x95, 0x02,        #     Report Count (2)
    0x75, 0x10,        #     Report Size (16)
    0x81, 0x02,        #     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0xC0,              #   End Collection
    0x05, 0x02,        #   Usage Page (Simulation Controls)
    0x09, 0xC5,        #   Usage (Brake)
    0x15, 0x00,        #   Logical Minimum (0)
    0x26, 0xFF, 0x03,  #   Logical Maximum (1023)
    0x95, 0x01,        #   Report Count (1)
    0x75, 0x0A,        #   Report Size (10)
    0x81, 0x02,        #   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x15, 0x00,        #   Logical Minimum (0)
    0x25, 0x00,        #   Logical Maximum (0)
    0x75, 0x06,        #   Report Size (6)
    0x95, 0x01,        #   Report Count (1)
    0x81, 0x03,        #   Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x05, 0x02,        #   Usage Page (Simulation Controls)
    0x09, 0xC4,        #   Usage (Accelerator)
    0x15, 0x00,        #   Logical Minimum (0)
    0x26, 0xFF, 0x03,  #   Logical Maximum (1023)
    0x95, 0x01,        #   Report Count (1)
    0x75, 0x0A,        #   Report Size (10)
    0x81, 0x02,        #   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x15, 0x00,        #   Logical Minimum (0)
    0x25, 0x00,        #   Logical Maximum (0)
    0x75, 0x06,        #   Report Size (6)
    0x95, 0x01,        #   Report Count (1)
    0x81, 0x03,        #   Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x05, 0x01,        #   Usage Page (Generic Desktop)
    0x09, 0x39,        #   Usage (Hat Switch)
    0x15, 0x01,        #   Logical Minimum (1)
    0x25, 0x08,        #   Logical Maximum (8)
    0x35, 0x00,        #   Physical Minimum (0)
    0x46, 0x3B, 0x01,  #   Physical Maximum (315)
    0x66, 0x14, 0x00,  #   Unit (System: English Rotation, Length: Centimeter)
    0x75, 0x04,        #   Report Size (4)
    0x95, 0x01,        #   Report Count (1)
    0x81, 0x42,        #   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,Null State)
    0x75, 0x04,        #   Report Size (4)
    0x95, 0x01,        #   Report Count (1)
    0x15, 0x00,        #   Logical Minimum (0)
    0x25, 0x00,        #   Logical Maximum (0)
    0x35, 0x00,        #   Physical Minimum (0)
    0x45, 0x00,        #   Physical Maximum (0)
    0x65, 0x00,        #   Unit (None)
    0x81, 0x03,        #   Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x05, 0x09,        #   Usage Page (Button)
    0x19, 0x01,        #   Usage Minimum (0x01)
    0x29, 0x0F,        #   Usage Maximum (0x0F)
    0x15, 0x00,        #   Logical Minimum (0)
    0x25, 0x01,        #   Logical Maximum (1)
    0x75, 0x01,        #   Report Size (1)
    0x95, 0x0F,        #   Report Count (15)
    0x81, 0x02,        #   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x15, 0x00,        #   Logical Minimum (0)
    0x25, 0x00,        #   Logical Maximum (0)
    0x75, 0x01,        #   Report Size (1)
    0x95, 0x01,        #   Report Count (1)
    0x81, 0x03,        #   Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x05, 0x0C,        #   Usage Page (Consumer)
    0x0A, 0xB2, 0x00,  #   Usage (Record)
    0x15, 0x00,        #   Logical Minimum (0)
    0x25, 0x01,        #   Logical Maximum (1)
    0x95, 0x01,        #   Report Count (1)
    0x75, 0x01,        #   Report Size (1)
    0x81, 0x02,        #   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x15, 0x00,        #   Logical Minimum (0)
    0x25, 0x00,        #   Logical Maximum (0)
    0x75, 0x07,        #   Report Size (7)
    0x95, 0x01,        #   Report Count (1)
    0x81, 0x03,        #   Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x05, 0x0F,        #   Usage Page (Physical Input Device)
    0x09, 0x21,        #   Usage (Set Effect Report)
    0x85, 0x03,        #   Report ID (3)
    0xA1, 0x02,        #   Collection (Logical)
    0x09, 0x97,        #     Usage (DC Enable Actuators)
    0x15, 0x00,        #     Logical Minimum (0)
    0x25, 0x01,        #     Logical Maximum (1)
    0x75, 0x04,        #     Report Size (4)
    0x95, 0x01,        #     Report Count (1)
    0x91, 0x02,        #     Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x15, 0x00,        #     Logical Minimum (0)
    0x25, 0x00,        #     Logical Maximum (0)
    0x75, 0x04,        #     Report Size (4)
    0x95, 0x01,        #     Report Count (1)
    0x91, 0x03,        #     Output (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x09, 0x70,        #     Usage (Magnitude)
    0x15, 0x00,        #     Logical Minimum (0)
    0x25, 0x64,        #     Logical Maximum (100)
    0x75, 0x08,        #     Report Size (8)
    0x95, 0x04,        #     Report Count (4)
    0x91, 0x02,        #     Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x09, 0x50,        #     Usage (Duration)
    0x66, 0x01, 0x10,  #     Unit (System: SI Linear, Time: Seconds)
    0x55, 0x0E,        #     Unit Exponent (-2)
    0x15, 0x00,        #     Logical Minimum (0)
    0x26, 0xFF, 0x00,  #     Logical Maximum (255)
    0x75, 0x08,        #     Report Size (8)
    0x95, 0x01,        #     Report Count (1)
    0x91, 0x02,        #     Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x09, 0xA7,        #     Usage (Start Delay)
    0x15, 0x00,        #     Logical Minimum (0)
    0x26, 0xFF, 0x00,  #     Logical Maximum (255)
    0x75, 0x08,        #     Report Size (8)
    0x95, 0x01,        #     Report Count (1)
    0x91, 0x02,        #     Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x65, 0x00,        #     Unit (None)
    0x55, 0x00,        #     Unit Exponent (0)
    0x09, 0x7C,        #     Usage (Loop Count)
    0x15, 0x00,        #     Logical Minimum (0)
    0x26, 0xFF, 0x00,  #     Logical Maximum (255)
    0x75, 0x08,        #     Report Size (8)
    0x95, 0x01,        #     Report Count (1)
    0x91, 0x02,        #     Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0xC0,              #   End Collection
    0xC0,              # End Collection

    # 283 bytes
])


@pytest.mark.parametrize('blueretro', [[system.GC, dev_mode.PAD, bt_conn_type.BT_LE]], indirect=True)
def test_xbox_ble_controller_default_buttons_mapping(blueretro):
    ''' Press each buttons and check if default mapping is right. '''
    # Set device name
    rsp = blueretro.send_name(DEVICE_NAME)
    assert rsp['type_update']['device_id'] == 0
    assert rsp['type_update']['device_type'] == 0
    assert rsp['type_update']['device_subtype'] == 0

    rsp = blueretro.send_hid_desc(HID_DESC)

    # Init adapter with a few neutral state report
    for _ in range(2):
        blueretro.send_to_bridge(0x01,
            '00800080'
            '00800080'
            '00000000'
            '00'
            '000000'
        )

    # Validate buttons default mapping
    for btns, br_btns in btns_generic_test_data(xbox_ble_btns_mask):
        rsp = blueretro.send_to_bridge(0x01,
            '00800080'
            '00800080'
            '00000000'
            '00'
            f'{swap24(btns):06x}'
        )

        assert rsp['wireless_input']['btns'] & 0xFFFFFF == btns
        assert rsp['generic_input']['btns'][0] == br_btns

    # Validate hat default mapping
    shifted_hat = hat_to_ld_btns[-1:] + hat_to_ld_btns[:-1]
    for hat_value, br_btns in enumerate(shifted_hat):
        rsp = blueretro.send_to_bridge(0x01,
            '00800080'
            '00800080'
            '00000000'
            f'0{hat_value:01x}'
            '000000'
        )

        assert rsp['wireless_input']['hat'] == hat_value
        assert rsp['generic_input']['btns'][0] == br_btns


@pytest.mark.parametrize('blueretro', [[system.GC, dev_mode.PAD, bt_conn_type.BT_LE]], indirect=True)
def test_xbox_ble_controller_axes_default_scaling(blueretro):
    ''' Set the various axes and check if the scaling is right. '''
    # Set device name
    rsp = blueretro.send_name(DEVICE_NAME)
    assert rsp['type_update']['device_id'] == 0
    assert rsp['type_update']['device_type'] == 0
    assert rsp['type_update']['device_subtype'] == 0

    rsp = blueretro.send_hid_desc(HID_DESC)

    # Init adapter with a few neutral state report
    for _ in range(2):
        blueretro.send_to_bridge(0x01,
            '00800080'
            '00800080'
            '00000000'
            '00'
            '000000'
        )

    # Validate axes default scaling
    for axes in axes_test_data_generator(xbox_axes, gc_axes, 0.0135):
        rsp = blueretro.send_to_bridge(0x01,
            f'{swap16(axes[axis.LX]["wireless"]):04x}{swap16(axes[axis.LY]["wireless"]):04x}'
            f'{swap16(axes[axis.RX]["wireless"]):04x}{swap16(axes[axis.RY]["wireless"]):04x}'
            f'{swap16(axes[axis.LM]["wireless"]):04x}{swap16(axes[axis.RM]["wireless"]):04x}'
            '00'
            '000000'
        )

        for ax in islice(axis, 0, 6):
            assert rsp['wireless_input']['axes'][ax] == axes[ax]['wireless']
            assert rsp['generic_input']['axes'][ax] == axes[ax]['generic']
            assert rsp['mapped_input']['axes'][ax] == axes[ax]['mapped']
            assert rsp['wired_output']['axes'][ax] == axes[ax]['wired']
