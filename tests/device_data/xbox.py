''' Common constants for Xbox devices. '''
from enum import IntEnum, auto
from bit_helper import bit
from .br import axis


class xbox_ble(IntEnum):
    ''' Buttons bitfield definition for reports. '''
    A = 0
    B = auto()
    X = 3
    Y = auto()
    LB = 6
    RB = auto()
    VIEW = 10
    MENU = auto()
    XBOX = auto()
    LJ = auto()
    RJ = auto()
    SHARE = 16


xbox_ble_btns_mask = [
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    bit(xbox_ble.X), bit(xbox_ble.B), bit(xbox_ble.A), bit(xbox_ble.Y),
    bit(xbox_ble.MENU), bit(xbox_ble.VIEW), bit(xbox_ble.XBOX), bit(xbox_ble.SHARE),
    0, bit(xbox_ble.LB), 0, bit(xbox_ble.LJ),
    0, bit(xbox_ble.RB), 0, bit(xbox_ble.RJ),
]


xbox_axes = {
    axis.LX: {'neutral': 0x8000, 'abs_max': 0x7FFF, 'abs_min': 0x8000, 'deadzone': 0},
    axis.LY: {'neutral': 0x8000, 'abs_max': 0x7FFF, 'abs_min': 0x8000, 'polarity': 1, 'deadzone': 0},
    axis.RX: {'neutral': 0x8000, 'abs_max': 0x7FFF, 'abs_min': 0x8000, 'deadzone': 0},
    axis.RY: {'neutral': 0x8000, 'abs_max': 0x7FFF, 'abs_min': 0x8000, 'polarity': 1, 'deadzone': 0},
    axis.LM: {'neutral': 0x000, 'abs_max': 0x3FF, 'abs_min': 0x000, 'deadzone': 0},
    axis.RM: {'neutral': 0x000, 'abs_max': 0x3FF, 'abs_min': 0x000, 'deadzone': 0},
}
