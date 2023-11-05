''' pytest session stuff. '''
import json
import re
import logging
from time import sleep
import pytest
from serial import SerialException
from injector import BlueRetroInjector
from device_data.br import system, dev_mode, bt_conn_type

class BlueRetroDut(BlueRetroInjector):
    ''' BlueRetro injector with a few extra for pytest. '''
    def expect(self, pattern):
        ''' Search for pattern in serial buffer. '''
        retry = 100
        while retry:
            while self.fd.in_waiting:
                line = self.fd.readline().decode()
                logging.debug(line)
                match = re.search(pattern, line)
                if match:
                    return match
                if 'CORE DUMP START' in line:
                    retry += 1000
            retry -= 1
            sleep(0.01)
        assert None == pattern

    def flush_logs(self):
        ''' Fetch the logs and discard them. '''
        sleep(0.1)
        while self.fd.in_waiting:
            line = self.fd.readline().decode()
            logging.debug(line)

    def expect_json(self, log_type):
        ''' Expect a log formatted in json and parse it. '''
        return json.loads(self.expect('({.{13}' + log_type + '.*?)\n').group(1))


@pytest.fixture(scope="session")
def blueretro_dut():
    ''' Fixture that try to return a BlueRetroDut object. '''
    logging.StreamHandler.terminator = ""
    retry = 0
    while True:
        try:
            ret = BlueRetroDut()
            break
        except SerialException:
            sleep(1) # Wait for QEMU
            retry += 1
            if retry == 10:
                raise
    return ret

@pytest.fixture()
def blueretro(blueretro_dut, request):
    ''' Create and teardown a BT device on BlueRetro DUT. '''
    blueretro_dut.disconnect()

    param = getattr(request, 'param', [system.GC, dev_mode.PAD, bt_conn_type.BT_BR_EDR])
    blueretro_dut.send_out_cfg(0, f"{param[1]:02x}00")
    blueretro_dut.send_system_id(param[0])
    blueretro_dut.connect(param[2])
    blueretro_dut.expect('# DBG handle: 0 dev: 0 type: 0')

    yield blueretro_dut

    blueretro_dut.disconnect()
    blueretro_dut.expect('# DBG DISCONN from handle: 0 dev: 0')
