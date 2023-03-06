''' pytest session stuff. '''
import json
from time import sleep
import pytest
from serial import SerialException
from injector import BlueRetroInjector

class BlueRetroDut(BlueRetroInjector):
    ''' BlueRetro injector with a few extra for pytest. '''
    def __init__(self, dut, redirect, dev="socket://localhost:5555", handle=0):
        ''' Add pytest-embedded fixtures for convenience. '''
        super().__init__(dev, handle)
        self.expect = dut.expect
        self.redirect = redirect

    def get_logs(self):
        ''' Fetch the logs and redirect them to pytest-embedded. '''
        sleep(0.1)
        with self.redirect():
            super().get_logs()

    def flush_logs(self):
        ''' Fetch the logs and discard them. '''
        sleep(0.1)
        super().get_logs()

    def expect_json(self, log_type):
        ''' Expect a log formatted in json and parse it. '''
        return json.loads(self.expect('({.{13}' + log_type + '.*?)\n', timeout=1).group(1))


@pytest.fixture()
def blueretro(dut, redirect):
    ''' Fixture that try to return a BlueRetroDut object. '''
    retry = 0
    while True:
        try:
            ret = BlueRetroDut(dut, redirect)
            break
        except SerialException:
            sleep(1) # Wait for QEMU
            retry += 1
            if retry == 3:
                raise
    return ret
