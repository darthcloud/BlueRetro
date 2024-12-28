''' Tests for generic HID keyboard. '''


DEVICE_NAME = 'HID Keyboard'
HID_DESC = ('05010906a101850175019508050719e0'
            '29e71500250181029501750881039505'
            '75010508190129059102950175039103'
            '95067508150026ff000507190029ff81'
            '00c0050c0901a1018502150025017501'
            '95160ab1010a23020aae010a8a010940'
            '096f0a210209b609cd09b509e209ea09'
            'e909300a83010a24020a06030a08030a'
            '01030a83010a0a030970810295017502'
            '8103c0')


def test_hid_keyboard_descriptor(blueretro):
    ''' Load a HID descriptor and check if it's parsed right. '''
    rsp = blueretro.send_name(DEVICE_NAME)
    assert rsp['device_name']['device_id'] == 0
    assert rsp['device_name']['device_type'] == 0
    assert rsp['device_name']['device_subtype'] == 0
    assert rsp['device_name']['device_name'] == 'HID Keyboard'
    
    rsp = blueretro.send_hid_desc(HID_DESC)
    assert rsp['hid_reports'][0]["report_id"] == 1
    assert rsp['hid_reports'][0]["report_tag"] == 0
    assert rsp['hid_reports'][0]["usages"][6]["bit_offset"] == 56
    assert rsp['hid_reports'][0]["report_type"] == 0

    assert rsp['hid_reports'][1]["report_id"] == 1
    assert rsp['hid_reports'][1]["report_tag"] == 1

    assert rsp['hid_reports'][2]["report_id"] == 2
    assert rsp['hid_reports'][2]["usages"][0]["usage_page"] == 0x0C
    assert rsp['hid_reports'][2]["usages"][0]["usage"] == 0x1B1
