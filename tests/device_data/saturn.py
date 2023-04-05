''' Common constants for Saturn devices. '''
from enum import IntEnum, auto
from .br import axis
from bit_helper import bit


class saturn(IntEnum):
    ''' Buttons bitfield definition for controllers. '''
    B = 0
    C = auto()
    A = auto()
    START = auto()
    LD_UP = auto()
    LD_DOWN = auto()
    LD_LEFT = auto()
    LD_RIGHT = auto()
    L = 11
    Z = auto()
    Y = auto()
    X = auto()
    R = auto()


saturn_btns_mask = [
    0, 0, 0, 0,
    0, 0, 0, 0,
    bit(saturn.LD_LEFT), bit(saturn.LD_RIGHT), bit(saturn.LD_DOWN), bit(saturn.LD_UP),
    0, 0, 0, 0,
    bit(saturn.A), bit(saturn.C), bit(saturn.B), bit(saturn.Y),
    bit(saturn.START), 0, 0, 0,
    bit(saturn.L), bit(saturn.X), 0, 0,
    bit(saturn.R), bit(saturn.Z), 0, 0,
]


saturn_axes = {
    axis.LX: {'size_min': -128, 'size_max': 127, 'neutral': 0x80, 'abs_max': 0x7F, 'abs_min': 0x80},
    axis.LY: {'size_min': -128, 'size_max': 127, 'neutral': 0x80, 'abs_max': 0x7F, 'abs_min': 0x80},
    axis.LM: {'size_min': 0, 'size_max': 255, 'neutral': 0x00, 'abs_max': 0xFF, 'abs_min': 0x00},
    axis.RM: {'size_min': 0, 'size_max': 255, 'neutral': 0x00, 'abs_max': 0xFF, 'abs_min': 0x00},
}
