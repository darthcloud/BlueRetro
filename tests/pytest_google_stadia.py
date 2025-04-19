''' Tests for Google Stadia controller. '''
import pytest
from itertools import islice
from device_data.test_data_generator import btns_generic_test_data
from device_data.test_data_generator import axes_test_data_generator
from device_data.br import hat_to_ld_btns
from enum import IntEnum, auto
from bit_helper import bit, swap16
from device_data.br import axis
from device_data.gc import gc_axes


class button(IntEnum):
    ''' Buttons bitfield definition. '''
    CAPTURE = 0
    ASSISTANT = auto()
    LT = auto()
    RT= auto()
    STADIA = auto()
    MENU = auto()
    OPTIONS = auto()
    RJ = auto()
    LJ = auto()
    RB = auto()
    LB = auto()
    Y = auto()
    X = auto()
    B = auto()
    A = auto()

button_mask = [
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    bit(button.ASSISTANT), 0, 0, 0,
    bit(button.X), bit(button.B), bit(button.A), bit(button.Y),
    bit(button.MENU), bit(button.OPTIONS), bit(button.STADIA), bit(button.CAPTURE),
    0, bit(button.LB), 0, bit(button.LJ),
    0, bit(button.RB), 0, bit(button.RJ),
]

axes_meta = {
    axis.LX: {'neutral': 128, 'abs_max': 127, 'abs_min': 127, 'deadzone': 0},
    axis.LY: {'neutral': 128, 'abs_max': 127, 'abs_min': 127, 'polarity': 1, 'deadzone': 0},
    axis.RX: {'neutral': 128, 'abs_max': 127, 'abs_min': 127, 'deadzone': 0},
    axis.RY: {'neutral': 128, 'abs_max': 127, 'abs_min': 127, 'polarity': 1, 'deadzone': 0},
    axis.LM: {'neutral': 0, 'abs_max': 255, 'abs_min': 0, 'deadzone': 0},
    axis.RM: {'neutral': 0, 'abs_max': 255, 'abs_min': 0, 'deadzone': 0},
}

VID = 0x18d1
PID = 0x9400
DEVICE_NAME = 'StadiaTDQQ-58b7'
HID_DESC = bytes([
    0x05, 0x01,        # Usage Page (Generic Desktop)
    0x09, 0x05,        # Usage (Gamepad)
    0xA1, 0x01,        # Collection (Application)
    0x85, 0x03,        #   Report ID (3)
    0x05, 0x01,        #   Usage Page (Generic Desktop)
    0x75, 0x04,        #   Report Size (4)
    0x95, 0x01,        #   Report Count (1)
    0x25, 0x07,        #   Logical Maximum (7)
    0x46, 0x3B, 0x01,  #   Physical Maximum (315)
    0x65, 0x14,        #   Unit (System: English Rotation, Length: Centimeter)
    0x09, 0x39,        #   Usage (Hat Switch)
    0x81, 0x42,        #   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,Null State)
    0x45, 0x00,        #   Physical Maximum (0)
    0x65, 0x00,        #   Unit (None)
    0x75, 0x01,        #   Report Size (1)
    0x95, 0x04,        #   Report Count (4)
    0x81, 0x01,        #   Input (Const,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x05, 0x09,        #   Usage Page (Button)
    0x15, 0x00,        #   Logical Minimum (0)
    0x25, 0x01,        #   Logical Maximum (1)
    0x75, 0x01,        #   Report Size (1)
    0x95, 0x0F,        #   Report Count (15)
    0x09, 0x12,        #   Usage (0x12)
    0x09, 0x11,        #   Usage (0x11)
    0x09, 0x14,        #   Usage (0x14)
    0x09, 0x13,        #   Usage (0x13)
    0x09, 0x0D,        #   Usage (0x0D)
    0x09, 0x0C,        #   Usage (0x0C)
    0x09, 0x0B,        #   Usage (0x0B)
    0x09, 0x0F,        #   Usage (0x0F)
    0x09, 0x0E,        #   Usage (0x0E)
    0x09, 0x08,        #   Usage (0x08)
    0x09, 0x07,        #   Usage (0x07)
    0x09, 0x05,        #   Usage (0x05)
    0x09, 0x04,        #   Usage (0x04)
    0x09, 0x02,        #   Usage (0x02)
    0x09, 0x01,        #   Usage (0x01)
    0x81, 0x02,        #   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x75, 0x01,        #   Report Size (1)
    0x95, 0x01,        #   Report Count (1)
    0x81, 0x01,        #   Input (Const,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x05, 0x01,        #   Usage Page (Generic Desktop)
    0x15, 0x01,        #   Logical Minimum (1)
    0x26, 0xFF, 0x00,  #   Logical Maximum (255)
    0x09, 0x01,        #   Usage (Pointer)
    0xA1, 0x00,        #   Collection (Physical)
    0x09, 0x30,        #     Usage (X)
    0x09, 0x31,        #     Usage (Y)
    0x75, 0x08,        #     Report Size (8)
    0x95, 0x02,        #     Report Count (2)
    0x81, 0x02,        #     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0xC0,              #   End Collection
    0x09, 0x01,        #   Usage (Pointer)
    0xA1, 0x00,        #   Collection (Physical)
    0x09, 0x32,        #     Usage (Z)
    0x09, 0x35,        #     Usage (Rz)
    0x75, 0x08,        #     Report Size (8)
    0x95, 0x02,        #     Report Count (2)
    0x81, 0x02,        #     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0xC0,              #   End Collection
    0x05, 0x02,        #   Usage Page (Simulation Controls)
    0x75, 0x08,        #   Report Size (8)
    0x95, 0x02,        #   Report Count (2)
    0x15, 0x00,        #   Logical Minimum (0)
    0x26, 0xFF, 0x00,  #   Logical Maximum (255)
    0x09, 0xC5,        #   Usage (Brake)
    0x09, 0xC4,        #   Usage (Accelerator)
    0x81, 0x02,        #   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x05, 0x0C,        #   Usage Page (Consumer)
    0x15, 0x00,        #   Logical Minimum (0)
    0x25, 0x01,        #   Logical Maximum (1)
    0x09, 0xE9,        #   Usage (Volume Increment)
    0x09, 0xEA,        #   Usage (Volume Decrement)
    0x75, 0x01,        #   Report Size (1)
    0x95, 0x02,        #   Report Count (2)
    0x81, 0x02,        #   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x09, 0xCD,        #   Usage (Play/Pause)
    0x95, 0x01,        #   Report Count (1)
    0x81, 0x02,        #   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x95, 0x05,        #   Report Count (5)
    0x81, 0x01,        #   Input (Const,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x85, 0x05,        #   Report ID (5)
    0x06, 0x0F, 0x00,  #   Usage Page (Physical Input Device)
    0x09, 0x97,        #   Usage (DC Enable Actuators)
    0x75, 0x10,        #   Report Size (16)
    0x95, 0x02,        #   Report Count (2)
    0x27, 0xFF, 0xFF, 0x00, 0x00,  #   Logical Maximum (65534)
    0x91, 0x02,        #   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0xC0,              # End Collection

    # 182 bytes
])


def test_google_stadia_descriptor(blueretro):
    ''' Load a HID descriptor and check if it's parsed right. '''
    # Set device name
    rsp = blueretro.send_name(DEVICE_NAME)
    assert rsp['device_name']['device_id'] == 0
    assert rsp['device_name']['device_type'] == 0
    assert rsp['device_name']['device_subtype'] == 0
    assert rsp['device_name']['device_name'] == DEVICE_NAME

    # Set device VID & PID
    rsp = blueretro.send_vid_pid(VID, PID)
    assert rsp['device_vid_pid']['device_id'] == 0
    assert rsp['device_vid_pid']['vid'] == VID
    assert rsp['device_vid_pid']['pid'] == PID

    # Validate HID descriptor
    rsp = blueretro.send_hid_desc(HID_DESC)
    assert rsp['hid_reports'][0]['report_id'] == 3
    assert rsp['hid_reports'][0]['report_tag'] == 0
    assert rsp['hid_reports'][0]["usages"][0]["usage_page"] == 0x01
    assert rsp['hid_reports'][0]["usages"][0]["usage"] == 0x39
    assert rsp['hid_reports'][0]["usages"][1]["usage_page"] == 0x09
    assert rsp['hid_reports'][0]["usages"][1]["usage"] == 0x12
    assert rsp['hid_reports'][0]["usages"][2]["usage_page"] == 0x09
    assert rsp['hid_reports'][0]["usages"][2]["usage"] == 0x11
    assert rsp['hid_reports'][0]["usages"][3]["usage_page"] == 0x09
    assert rsp['hid_reports'][0]["usages"][3]["usage"] == 0x14
    assert rsp['hid_reports'][0]["usages"][4]["usage_page"] == 0x09
    assert rsp['hid_reports'][0]["usages"][4]["usage"] == 0x13
    assert rsp['hid_reports'][0]["usages"][5]["usage_page"] == 0x09
    assert rsp['hid_reports'][0]["usages"][5]["usage"] == 0x0D
    assert rsp['hid_reports'][0]["usages"][6]["usage_page"] == 0x09
    assert rsp['hid_reports'][0]["usages"][6]["usage"] == 0x0C
    assert rsp['hid_reports'][0]["usages"][7]["usage_page"] == 0x09
    assert rsp['hid_reports'][0]["usages"][7]["usage"] == 0x0B
    assert rsp['hid_reports'][0]["usages"][8]["usage_page"] == 0x09
    assert rsp['hid_reports'][0]["usages"][8]["usage"] == 0x0F
    assert rsp['hid_reports'][0]["usages"][9]["usage_page"] == 0x09
    assert rsp['hid_reports'][0]["usages"][9]["usage"] == 0x0E
    assert rsp['hid_reports'][0]["usages"][10]["usage_page"] == 0x09
    assert rsp['hid_reports'][0]["usages"][10]["usage"] == 0x08
    assert rsp['hid_reports'][0]["usages"][11]["usage_page"] == 0x09
    assert rsp['hid_reports'][0]["usages"][11]["usage"] == 0x07
    assert rsp['hid_reports'][0]["usages"][12]["usage_page"] == 0x09
    assert rsp['hid_reports'][0]["usages"][12]["usage"] == 0x05
    assert rsp['hid_reports'][0]["usages"][13]["usage_page"] == 0x09
    assert rsp['hid_reports'][0]["usages"][13]["usage"] == 0x04
    assert rsp['hid_reports'][0]["usages"][14]["usage_page"] == 0x09
    assert rsp['hid_reports'][0]["usages"][14]["usage"] == 0x02
    assert rsp['hid_reports'][0]["usages"][15]["usage_page"] == 0x09
    assert rsp['hid_reports'][0]["usages"][15]["usage"] == 0x01
    assert rsp['hid_reports'][0]["usages"][16]["usage_page"] == 0x01
    assert rsp['hid_reports'][0]["usages"][16]["usage"] == 0x30
    assert rsp['hid_reports'][0]["usages"][17]["usage_page"] == 0x01
    assert rsp['hid_reports'][0]["usages"][17]["usage"] == 0x31
    assert rsp['hid_reports'][0]["usages"][18]["usage_page"] == 0x01
    assert rsp['hid_reports'][0]["usages"][18]["usage"] == 0x32
    assert rsp['hid_reports'][0]["usages"][19]["usage_page"] == 0x01
    assert rsp['hid_reports'][0]["usages"][19]["usage"] == 0x35
    assert rsp['hid_reports'][0]["usages"][20]["usage_page"] == 0x02
    assert rsp['hid_reports'][0]["usages"][20]["usage"] == 0xC5
    assert rsp['hid_reports'][0]["usages"][21]["usage_page"] == 0x02
    assert rsp['hid_reports'][0]["usages"][21]["usage"] == 0xC4
    assert rsp['hid_reports'][0]['report_type'] == 2

    assert rsp['hid_reports'][1]['report_id'] == 5
    assert rsp['hid_reports'][1]['report_tag'] == 1
    assert rsp['hid_reports'][1]["usages"][0]["usage_page"] == 0x0F
    assert rsp['hid_reports'][1]["usages"][0]["usage"] == 0x97
    assert rsp['hid_reports'][1]["usages"][1]["usage_page"] == 0x0F
    assert rsp['hid_reports'][1]["usages"][1]["usage"] == 0x97

def test_google_stadia_default_buttons_mapping(blueretro):
    ''' Press each buttons and check if default mapping is right. '''
     # Set device name
    blueretro.send_name(DEVICE_NAME)
    blueretro.send_hid_desc(HID_DESC)

    # Init adapter with a few neutral state report
    for _ in range(2):
        blueretro.send_to_bridge(0x03,
            '08'
            '0000'
            '80808080'
            '0000'
            '00'
        )

    # Validate buttons default mapping
    for hid_btns, br_btns in btns_generic_test_data(button_mask):
        rsp = blueretro.send_to_bridge(0x03,
            '08'
            f'{swap16(hid_btns):04x}'
            '80808080'
            '0000'
            '00'
        )

        assert rsp['wireless_input']['btns'] == hid_btns
        assert rsp['generic_input']['btns'][0] == br_btns

    # Validate hat default mapping
    for hat_value, br_btns in enumerate(hat_to_ld_btns):
        rsp = blueretro.send_to_bridge(0x03,
            f'0{hat_value:01x}'
            '0000'
            '80808080'
            '0000'
            '00'
        )

        assert rsp['wireless_input']['hat'] == hat_value
        assert rsp['generic_input']['btns'][0] == br_btns

def test_google_stadia_axes_default_scaling(blueretro):
    ''' Set the various axes and check if the scaling is right. '''
     # Set device name
    blueretro.send_name(DEVICE_NAME)
    blueretro.send_hid_desc(HID_DESC)

    # Init adapter with a few neutral state report
    for _ in range(2):
        blueretro.send_to_bridge(0x03,
            '08'
            '0000'
            '80808080'
            '0000'
            '00'
        )

    # Validate axes default scaling
    for axes in axes_test_data_generator(axes_meta, gc_axes, 0.0135):
        rsp = blueretro.send_to_bridge(0x03,
            '08'
            '0000'
            f'{axes[axis.LX]["wireless"]:02x}{axes[axis.LY]["wireless"]:02x}'
            f'{axes[axis.RX]["wireless"]:02x}{axes[axis.RY]["wireless"]:02x}'
            f'{axes[axis.LM]["wireless"]:02x}{axes[axis.RM]["wireless"]:02x}'
            '00'
        )

        for ax in islice(axis, 0, 6):
            assert rsp['wireless_input']['axes'][ax] == axes[ax]['wireless']
            assert rsp['generic_input']['axes'][ax] == axes[ax]['generic']
            assert rsp['mapped_input']['axes'][ax] == axes[ax]['mapped']
            assert rsp['wired_output']['axes'][ax] == axes[ax]['wired']
