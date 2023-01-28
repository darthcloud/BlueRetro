import pytest
from time import sleep
from injector import BlueRetroInjector


@pytest.mark.parametrize(
    'qemu_extra_args',
    [
        '"-serial tcp::5555,server,nowait"',
    ],
    indirect=True,
)
@pytest.mark.parametrize(
    'qemu_image_path',
    [
        './build/flash_image.bin'
    ],
    indirect=True,
)
def test_injector_sanity(dut, redirect):
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

    sleep(2) # Wait for QEMU image to boot
    bri = BlueRetroInjector()
    bri.connect()
    bri.send_name(name)
    bri.send_hid_desc(hid_desc)
    bri.disconnect()

    sleep(2)
    with redirect():
        bri.get_logs()

    dut.expect('# DBG handle: 0 dev: 0 type: 0', timeout=1)
    dut.expect('# dev: 0 type: 0:0 BlueRetro Test', timeout=1)
    dut.expect('# 1 07E0 0 8 0700 16 8, 0700 24 8, 0700 32 8, 0700 40 8, 0700 48 8, 0700 56 8, rtype: 0 dtype: 0 sub: 0', timeout=1)
    dut.expect('# DBG DISCONN from handle: 0 dev: 0', timeout=1)
