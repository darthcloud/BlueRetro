import pytest
import json
from serial import SerialException
from time import sleep
from injector import BlueRetroInjector

class BlueRetroDut(BlueRetroInjector):
    def __init__(self, dut, redirect, dev="socket://localhost:5555", handle=0):
        super(BlueRetroDut, self).__init__(dev, handle)
        self.expect = dut.expect
        self.redirect = redirect

    def get_logs(self):
        sleep(0.1)
        with self.redirect():
            super(BlueRetroDut, self).get_logs()

    def flush_logs(self):
        sleep(0.1)
        super(BlueRetroDut, self).get_logs()

    def expect_json(self, log_type):
        return json.loads(self.expect('({.*?' + log_type + '.*?)\n', timeout=1).group(1))


@pytest.fixture()
def blueretro(dut, redirect):
    retry = 0
    while True:
        try:
            blueretro = BlueRetroDut(dut, redirect)
            break
        except SerialException:
            sleep(1) # Wait for QEMU
            retry += 1
            if retry == 3:
                raise
    return blueretro
