''' Common constants for PlayStation devices. '''
from enum import IntEnum, auto
from bit_helper import bit
from .br import axis


class ps(IntEnum):
    ''' Buttons bitfield definition for reports. '''
    S = 4
    X = auto()
    C = auto()
    T = auto()
    L1 = auto()
    R1 = auto()
    L2 = auto()
    R2 = auto()
    SHARE = auto()
    OPTIONS = auto()
    L3 = auto()
    R3 = auto()
    PS = auto()
    TP = auto()
    MUTE = auto()


ps_btns_mask = [
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    bit(ps.S), bit(ps.C), bit(ps.X), bit(ps.T),
    bit(ps.OPTIONS), bit(ps.SHARE), bit(ps.PS), bit(ps.TP),
    0, bit(ps.L1), 0, bit(ps.L3),
    0, bit(ps.R1), 0, bit(ps.R3),
]


ps_axes = {
    axis.LX: {'neutral': 0x80, 'abs_max': 0x7F, 'abs_min': 0x80, 'deadzone': 0},
    axis.LY: {'neutral': 0x80, 'abs_max': 0x7F, 'abs_min': 0x80, 'polarity': 1, 'deadzone': 0},
    axis.RX: {'neutral': 0x80, 'abs_max': 0x7F, 'abs_min': 0x80, 'deadzone': 0},
    axis.RY: {'neutral': 0x80, 'abs_max': 0x7F, 'abs_min': 0x80, 'polarity': 1, 'deadzone': 0},
    axis.LM: {'neutral': 0x00, 'abs_max': 0xFF, 'abs_min': 0x00, 'deadzone': 0},
    axis.RM: {'neutral': 0x00, 'abs_max': 0xFF, 'abs_min': 0x00, 'deadzone': 0},
}
