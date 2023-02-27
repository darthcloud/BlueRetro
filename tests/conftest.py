import pytest
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


@pytest.fixture
def blueretro(dut, redirect):
    sleep(2) # Wait for QEMU image to boot
    return BlueRetroDut(dut, redirect)
