''' Tests for generic HID keyboard. '''
import json
from device_data.gc import GC


DEVICE_NAME = 'HID Keyboard'
HID_DESC = '05010906a101850175019508050719e0' \
           '29e71500250181029501750881039505' \
           '75010508190129059102950175039103' \
           '95067508150026ff000507190029ff81' \
           '00c0050c0901a1018502150025017501' \
           '95160ab1010a23020aae010a8a010940' \
           '096f0a210209b609cd09b509e209ea09' \
           'e909300a83010a24020a06030a08030a' \
           '01030a83010a0a030970810295017502' \
           '8103c0'


def test_hid_keyboard_descriptor(blueretro):
    blueretro.disconnect()
    blueretro.send_system_id(GC)
    blueretro.connect()
    blueretro.send_name(DEVICE_NAME)
    blueretro.send_hid_desc(HID_DESC)
    blueretro.disconnect()

    blueretro.get_logs()

    blueretro.expect('# dev: 0 type: 0:0 HID Keyboard', timeout=1)

    report = json.loads(blueretro.expect('({.*?parsed_hid_report.*?)\n', timeout=1).group(1))
    assert report["report_id"] == 1
    assert report["usages"][7]["bit_offset"] == 64
    assert report["report_type"] == 0
    assert report["device_type"] == 0
    assert report["device_subtype"] == 0

    report = json.loads(blueretro.expect('({.*?parsed_hid_report.*?)\n', timeout=1).group(1))
    assert report["report_id"] == 2
    assert report["usages"][0]["usage_page"] == 0x0C
    assert report["usages"][0]["usage"] == 0x1B1
    assert report["report_type"] == 3
    assert report["device_type"] == -1
    assert report["device_subtype"] == 0
