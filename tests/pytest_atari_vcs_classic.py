''' Tests for Atari VCS classic controller. '''
import pytest
from device_data.test_data_generator import btns_generic_test_data
from device_data.br import hat_to_ld_btns
from enum import IntEnum, auto
from bit_helper import bit


class atari_button(IntEnum):
    ''' Buttons bitfield definition. '''
    A = 0
    B = auto()

class atari_consumer(IntEnum):
    ''' Consumer bitfield definition. '''
    SELECT = 0
    START = auto()
    MENU = auto()

atari_button_mask = [
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, bit(atari_button.B), bit(atari_button.A), 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
]

atari_consumer_mask = [
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    bit(atari_consumer.START), bit(atari_consumer.SELECT), bit(atari_consumer.MENU), 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
]

VID = 0x3250
PID = 0x1001
DEVICE_NAME = 'Classic Controller'
HID_DESC = bytes([
    0x05, 0x01,        # Usage Page (Generic Desktop Ctrls)
    0x09, 0x05,        # Usage (Game Pad)
    0xA1, 0x01,        # Collection (Application)
    0x85, 0x01,        #   Report ID (1)
    0xA1, 0x00,        #   Collection (Physical)
    0x05, 0x09,        #     Usage Page (Button)
    0x09, 0x01,        #     Usage (0x01)
    0x15, 0x00,        #     Logical Minimum (0)
    0x25, 0x01,        #     Logical Maximum (1)
    0x95, 0x01,        #     Report Count (1)
    0x75, 0x01,        #     Report Size (1)
    0x81, 0x02,        #     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x09, 0x02,        #     Usage (0x02)
    0x15, 0x00,        #     Logical Minimum (0)
    0x25, 0x01,        #     Logical Maximum (1)
    0x95, 0x01,        #     Report Count (1)
    0x75, 0x01,        #     Report Size (1)
    0x81, 0x02,        #     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x75, 0x01,        #     Report Size (1)
    0x95, 0x01,        #     Report Count (1)
    0x81, 0x03,        #     Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x75, 0x01,        #     Report Size (1)
    0x95, 0x01,        #     Report Count (1)
    0x81, 0x03,        #     Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x75, 0x01,        #     Report Size (1)
    0x95, 0x01,        #     Report Count (1)
    0x81, 0x03,        #     Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x75, 0x01,        #     Report Size (1)
    0x95, 0x01,        #     Report Count (1)
    0x81, 0x03,        #     Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x75, 0x01,        #     Report Size (1)
    0x95, 0x01,        #     Report Count (1)
    0x81, 0x03,        #     Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x75, 0x01,        #     Report Size (1)
    0x95, 0x01,        #     Report Count (1)
    0x81, 0x03,        #     Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x05, 0x0C,        #     Usage Page (Consumer)
    0x15, 0x00,        #     Logical Minimum (0)
    0x25, 0x01,        #     Logical Maximum (1)
    0x75, 0x01,        #     Report Size (1)
    0x95, 0x03,        #     Report Count (3)
    0x0A, 0x24, 0x02,  #     Usage (AC Back)
    0x09, 0x40,        #     Usage (Menu)
    0x0A, 0x23, 0x02,  #     Usage (AC Home)
    0x81, 0x02,        #     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x75, 0x01,        #     Report Size (1)
    0x95, 0x01,        #     Report Count (1)
    0x81, 0x03,        #     Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x05, 0x01,        #     Usage Page (Generic Desktop Ctrls)
    0x09, 0x39,        #     Usage (Hat switch)
    0x15, 0x01,        #     Logical Minimum (1)
    0x25, 0x08,        #     Logical Maximum (8)
    0x75, 0x04,        #     Report Size (4)
    0x95, 0x01,        #     Report Count (1)
    0x81, 0x02,        #     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x05, 0x01,        #     Usage Page (Generic Desktop Ctrls)
    0x09, 0x37,        #     Usage (Dial)
    0x15, 0x00,        #     Logical Minimum (0)
    0x26, 0xFF, 0x03,  #     Logical Maximum (1023)
    0x75, 0x0A,        #     Report Size (10)
    0x95, 0x01,        #     Report Count (1)
    0x81, 0x0A,        #     Input (Data,Var,Abs,Wrap,Linear,Preferred State,No Null Position)
    0x75, 0x01,        #     Report Size (1)
    0x95, 0x01,        #     Report Count (1)
    0x81, 0x03,        #     Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x75, 0x01,        #     Report Size (1)
    0x95, 0x01,        #     Report Count (1)
    0x81, 0x03,        #     Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x75, 0x01,        #     Report Size (1)
    0x95, 0x01,        #     Report Count (1)
    0x81, 0x03,        #     Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x75, 0x01,        #     Report Size (1)
    0x95, 0x01,        #     Report Count (1)
    0x81, 0x03,        #     Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x75, 0x01,        #     Report Size (1)
    0x95, 0x02,        #     Report Count (2)
    0x81, 0x03,        #     Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x09, 0x00,        #     Usage (Undefined)
    0x15, 0x00,        #     Logical Minimum (0)
    0x26, 0xFF, 0x00,  #     Logical Maximum (255)
    0x95, 0x04,        #     Report Count (4)
    0x75, 0x08,        #     Report Size (8)
    0x91, 0x02,        #     Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0xC0,              #   End Collection
    0x85, 0x02,        #   Report ID (2)
    0xA1, 0x00,        #   Collection (Physical)
    0x05, 0x06,        #     Usage Page (Generic Dev Ctrls)
    0x09, 0x20,        #     Usage (Battery Strength)
    0x15, 0x00,        #     Logical Minimum (0)
    0x25, 0x64,        #     Logical Maximum (100)
    0x95, 0x01,        #     Report Count (1)
    0x75, 0x07,        #     Report Size (7)
    0x81, 0x02,        #     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x05, 0x06,        #     Usage Page (Generic Dev Ctrls)
    0x09, 0x00,        #     Usage (Unidentified)
    0x15, 0x00,        #     Logical Minimum (0)
    0x25, 0x01,        #     Logical Maximum (1)
    0x95, 0x01,        #     Report Count (1)
    0x75, 0x01,        #     Report Size (1)
    0x81, 0x02,        #     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x09, 0x00,        #     Usage (Unidentified)
    0x15, 0x00,        #     Logical Minimum (0)
    0x26, 0xFF, 0x00,  #     Logical Maximum (255)
    0x95, 0x1A,        #     Report Count (26)
    0x75, 0x08,        #     Report Size (8)
    0x91, 0x02,        #     Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0xC0,              #   End Collection
    0x85, 0x03,        #   Report ID (3)
    0xA1, 0x00,        #   Collection (Physical)
    0x05, 0x06,        #     Usage Page (Generic Dev Ctrls)
    0x09, 0x22,        #     Usage (Wireless ID)
    0x15, 0x00,        #     Logical Minimum (0)
    0x26, 0xFF, 0x00,  #     Logical Maximum (255)
    0x95, 0x06,        #     Report Count (6)
    0x75, 0x08,        #     Report Size (8)
    0x81, 0x02,        #     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x05, 0x06,        #     Usage Page (Generic Dev Ctrls)
    0x09, 0x00,        #     Usage (Unidentified)
    0x15, 0x00,        #     Logical Minimum (0)
    0x26, 0xFF, 0x00,  #     Logical Maximum (255)
    0x95, 0x02,        #     Report Count (2)
    0x75, 0x08,        #     Report Size (8)
    0x81, 0x02,        #     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x05, 0x00,        #     Usage Page (Undefined)
    0x09, 0x00,        #     Usage (Undefined)
    0x15,              #     Logical Minimum (0)

    # 256 bytes
])


def test_atari_vcs_classic_descriptor(blueretro):
    ''' Load a HID descriptor and check if it's parsed right. '''
    # Set device name
    rsp = blueretro.send_name(DEVICE_NAME)
    assert rsp['device_name']['device_id'] == 0
    assert rsp['device_name']['device_type'] == 0
    assert rsp['device_name']['device_subtype'] == 0
    assert rsp['device_name']['device_name'] == 'Classic Controller'

    # Validate HID descriptor
    rsp = blueretro.send_hid_desc(HID_DESC)
    assert rsp['hid_reports'][0]['report_id'] == 1
    assert rsp['hid_reports'][0]['report_tag'] == 0
    assert rsp['hid_reports'][0]["usages"][0]["usage_page"] == 0x09
    assert rsp['hid_reports'][0]["usages"][0]["usage"] == 0x01
    assert rsp['hid_reports'][0]["usages"][1]["usage_page"] == 0x09
    assert rsp['hid_reports'][0]["usages"][1]["usage"] == 0x02
    assert rsp['hid_reports'][0]["usages"][2]["usage_page"] == 0x0C
    assert rsp['hid_reports'][0]["usages"][2]["usage"] == 0x224
    assert rsp['hid_reports'][0]["usages"][3]["usage_page"] == 0x0C
    assert rsp['hid_reports'][0]["usages"][3]["usage"] == 0x40
    assert rsp['hid_reports'][0]["usages"][4]["usage_page"] == 0x0C
    assert rsp['hid_reports'][0]["usages"][4]["usage"] == 0x223
    assert rsp['hid_reports'][0]["usages"][5]["usage_page"] == 0x01
    assert rsp['hid_reports'][0]["usages"][5]["usage"] == 0x39
    assert rsp['hid_reports'][0]["usages"][6]["usage_page"] == 0x01
    assert rsp['hid_reports'][0]["usages"][6]["usage"] == 0x37
    assert rsp['hid_reports'][0]['report_type'] == 2

    assert rsp['hid_reports'][1]['report_id'] == 1
    assert rsp['hid_reports'][1]['report_tag'] == 1
    assert rsp['hid_reports'][1]["usages"][0]["usage_page"] == 0x01
    assert rsp['hid_reports'][1]["usages"][0]["usage"] == 0x00
    assert rsp['hid_reports'][1]["usages"][1]["usage_page"] == 0x01
    assert rsp['hid_reports'][1]["usages"][1]["usage"] == 0x00
    assert rsp['hid_reports'][1]["usages"][2]["usage_page"] == 0x01
    assert rsp['hid_reports'][1]["usages"][2]["usage"] == 0x00
    assert rsp['hid_reports'][1]["usages"][3]["usage_page"] == 0x01
    assert rsp['hid_reports'][1]["usages"][3]["usage"] == 0x00
    assert 'report_type' not in rsp['hid_reports'][1]

def test_atari_vcs_classic_descriptor_patch(blueretro):
    ''' Given VID & PID validate HID descriptor patch is apply. '''
    # Set device name
    rsp = blueretro.send_name(DEVICE_NAME)
    assert rsp['device_name']['device_id'] == 0
    assert rsp['device_name']['device_type'] == 0
    assert rsp['device_name']['device_subtype'] == 0
    assert rsp['device_name']['device_name'] == 'Classic Controller'

    # Set device VID & PID
    rsp = blueretro.send_vid_pid(VID, PID)
    assert rsp['device_vid_pid']['device_id'] == 0
    assert rsp['device_vid_pid']['vid'] == 0x3250
    assert rsp['device_vid_pid']['pid'] == 0x1001

    # Validate HID descriptor
    rsp = blueretro.send_hid_desc(HID_DESC)
    assert rsp['hid_reports'][0]['report_id'] == 1
    assert rsp['hid_reports'][0]['report_tag'] == 0
    assert rsp['hid_reports'][0]["usages"][0]["usage_page"] == 0x09
    assert rsp['hid_reports'][0]["usages"][0]["usage"] == 0x01
    assert rsp['hid_reports'][0]["usages"][1]["usage_page"] == 0x09
    assert rsp['hid_reports'][0]["usages"][1]["usage"] == 0x02
    assert rsp['hid_reports'][0]["usages"][2]["usage_page"] == 0x0C
    assert rsp['hid_reports'][0]["usages"][2]["usage"] == 0x224
    assert rsp['hid_reports'][0]["usages"][3]["usage_page"] == 0x0C
    assert rsp['hid_reports'][0]["usages"][3]["usage"] == 0x40
    assert rsp['hid_reports'][0]["usages"][4]["usage_page"] == 0x0C
    assert rsp['hid_reports'][0]["usages"][4]["usage"] == 0x223
    assert rsp['hid_reports'][0]["usages"][5]["usage_page"] == 0x01
    assert rsp['hid_reports'][0]["usages"][5]["usage"] == 0x39
    assert rsp['hid_reports'][0]["usages"][6]["usage_page"] == 0x01
    assert rsp['hid_reports'][0]["usages"][6]["usage"] == 0x37
    assert rsp['hid_reports'][0]['report_type'] == 2

    assert rsp['hid_reports'][1]['report_id'] == 1
    assert rsp['hid_reports'][1]['report_tag'] == 1
    assert rsp['hid_reports'][1]["usages"][0]["usage_page"] == 0x0F
    assert rsp['hid_reports'][1]["usages"][0]["usage"] == 0x70
    assert rsp['hid_reports'][1]["usages"][1]["usage_page"] == 0x0F
    assert rsp['hid_reports'][1]["usages"][1]["usage"] == 0x50
    assert rsp['hid_reports'][1]["usages"][2]["usage_page"] == 0x0F
    assert rsp['hid_reports'][1]["usages"][2]["usage"] == 0xA7
    assert rsp['hid_reports'][1]["usages"][3]["usage_page"] == 0x0F
    assert rsp['hid_reports'][1]["usages"][3]["usage"] == 0x7C
    assert rsp['hid_reports'][1]['report_type'] == 3

def test_atari_vcs_classic_default_buttons_mapping(blueretro):
    ''' Press each buttons and check if default mapping is right. '''
    # Set device name
    blueretro.send_name(DEVICE_NAME)
    blueretro.send_hid_desc(HID_DESC)

    # Init adapter with a few neutral state report
    for _ in range(2):
        blueretro.send_hid_report('a10100000000')

    # Validate buttons default mapping
    for hid_btns, br_btns in btns_generic_test_data(atari_button_mask):
        rsp = blueretro.send_hid_report(
            'a101'
            f'{hid_btns:02x}'
            '0'
            '0'
            '0000'
        )

        assert rsp['wireless_input']['btns'] == hid_btns
        assert rsp['generic_input']['btns'][0] == br_btns

    # Validate consumer buttons default mapping
    for hid_btns, br_btns in btns_generic_test_data(atari_consumer_mask):
        rsp = blueretro.send_hid_report(
            'a101'
            '00'
            '0'
            f'{hid_btns:01x}'
            '0000'
        )

        # Consumer is shift by a byte since BR merge all buttons in single usage
        assert rsp['wireless_input']['btns'] == hid_btns << 8
        assert rsp['generic_input']['btns'][0] == br_btns

    # Validate hat default mapping
    shifted_hat = hat_to_ld_btns[-1:] + hat_to_ld_btns[:-1]
    for hat_value, br_btns in enumerate(shifted_hat):
        rsp = blueretro.send_hid_report(
            'a101'
            '00'
            f'{hat_value:01x}'
            '0'
            '0000'
        )

        assert rsp['wireless_input']['hat'] == hat_value
        assert rsp['generic_input']['btns'][0] == br_btns
