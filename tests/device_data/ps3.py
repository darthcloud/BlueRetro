''' Common constants for PlayStation3 devices. '''
from enum import IntEnum, auto
from bit_helper import bit
from .br import axis


class ps3(IntEnum):
    ''' Buttons bitfield definition for reports. '''
    SELECT = 8
    L3 = auto()
    R3 = auto()
    START = auto()
    D_UP = auto()
    D_RIGHT = auto()
    D_DOWN = auto()
    D_LEFT = auto()
    L2 = auto()
    R2 = auto()
    L1 = auto()
    R1 = auto()
    T = auto()
    C = auto()
    X = auto()
    S = auto()
    PS = auto()


ps3_btns_mask = [
    0, 0, 0, 0,
    0, 0, 0, 0,
    bit(ps3.D_LEFT), bit(ps3.D_RIGHT), bit(ps3.D_DOWN), bit(ps3.D_UP),
    0, 0, 0, 0,
    bit(ps3.S), bit(ps3.C), bit(ps3.X), bit(ps3.T),
    bit(ps3.START), bit(ps3.SELECT), bit(ps3.PS), 0,
    0, bit(ps3.L1), 0, bit(ps3.L3),
    0, bit(ps3.R1), 0, bit(ps3.R3),
]


ps3_axes = {
    axis.LX: {'neutral': 0x80, 'abs_max': 0x7F, 'abs_min': 0x80, 'deadzone': 0},
    axis.LY: {'neutral': 0x80, 'abs_max': 0x7F, 'abs_min': 0x80, 'polarity': 1, 'deadzone': 0},
    axis.RX: {'neutral': 0x80, 'abs_max': 0x7F, 'abs_min': 0x80, 'deadzone': 0},
    axis.RY: {'neutral': 0x80, 'abs_max': 0x7F, 'abs_min': 0x80, 'polarity': 1, 'deadzone': 0},
    axis.LM: {'neutral': 0x00, 'abs_max': 0xFF, 'abs_min': 0x00, 'deadzone': 0},
    axis.RM: {'neutral': 0x00, 'abs_max': 0xFF, 'abs_min': 0x00, 'deadzone': 0},
}
