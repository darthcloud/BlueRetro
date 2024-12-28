''' pytest session stuff. '''
import logging
import pytest
from injector import BlueRetroInjector
from device_data.br import system, dev_mode, bt_conn_type

class BlueRetroDut(BlueRetroInjector):
    ''' BlueRetro injector with a few extra for pytest. '''
    pass


@pytest.fixture(scope="session")
def blueretro_dut():
    ''' Fixture that try to return a BlueRetroDut object. '''
    logging.StreamHandler.terminator = ""
    retry = 0
    while True:
        try:
            ret = BlueRetroDut()
            break
        except Exception as e:
            logging.debug(e)
    return ret

@pytest.fixture()
def blueretro(blueretro_dut, request):
    ''' Create and teardown a BT device on BlueRetro DUT. '''
    blueretro_dut.disconnect()

    param = getattr(request, 'param', [system.GC, dev_mode.PAD, bt_conn_type.BT_BR_EDR])
    blueretro_dut.send_out_cfg(0, f"{param[1]:02x}00")
    blueretro_dut.send_system_id(param[0])
    rsp = blueretro_dut.connect(param[2])
    assert rsp['device_conn']['handle'] == 0
    assert rsp['device_conn']['device_id'] == 0
    assert rsp['device_conn']['device_type'] == 0

    yield blueretro_dut

    rsp = blueretro_dut.disconnect()
    assert rsp['device_disconn']['handle'] == 0
    assert rsp['device_disconn']['device_id'] == 0
