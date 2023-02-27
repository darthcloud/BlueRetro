import pytest
import json


def test_injector_sanity(blueretro):
    name = 'BlueRetro Test'
    hid_desc = '05010906a101850175019508050719e0' \
               '29e71500250181029501750881039505' \
               '75010508190129059102950175039103' \
               '95067508150026ff000507190029ff81' \
               '00c0050c0901a1018502150025017501' \
               '95160ab1010a23020aae010a8a010940' \
               '096f0a210209b609cd09b509e209ea09' \
               'e909300a83010a24020a06030a08030a' \
               '01030a83010a0a030970810295017502' \
               '8103c0'

    blueretro.connect()
    blueretro.send_name(name)
    blueretro.send_hid_desc(hid_desc)
    blueretro.disconnect()

    blueretro.get_logs()

    blueretro.expect('# dev: 0 type: 0:0 BlueRetro Test', timeout=1)

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

