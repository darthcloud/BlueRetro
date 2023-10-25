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
    blueretro.send_name(DEVICE_NAME)
    blueretro.send_hid_desc(HID_DESC)

    blueretro.expect('# dev: 0 type: 0:0 HID Keyboard')

    report = blueretro.expect_json('parsed_hid_report')
    assert report["report_id"] == 1
    assert report["report_tag"] == 0
    assert report["usages"][6]["bit_offset"] == 56
    assert report["report_type"] == 0

    report = blueretro.expect_json('parsed_hid_report')
    assert report["report_id"] == 1
    assert report["report_tag"] == 1

    report = blueretro.expect_json('parsed_hid_report')
    assert report["report_id"] == 2
    assert report["usages"][0]["usage_page"] == 0x0C
    assert report["usages"][0]["usage"] == 0x1B1
    assert report["report_type"] == 3
