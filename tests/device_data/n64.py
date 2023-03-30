''' Common constants for N64 devices. '''
from enum import IntEnum, auto
from .br import axis
from bit_helper import bit


N64 = 15


class n64(IntEnum):
    ''' Buttons bitfield definition for controllers. '''
    LD_RIGHT = 0
    LD_LEFT = auto()
    LD_DOWN = auto()
    LD_UP = auto()
    START = auto()
    Z = auto()
    B = auto()
    A = auto()
    C_RIGHT = auto()
    C_LEFT = auto()
    C_DOWN = auto()
    C_UP = auto()
    R = auto()
    L = auto()


n64_btns_mask = [
    0, 0, 0, 0,
    bit(n64.C_LEFT), bit(n64.C_RIGHT), bit(n64.C_DOWN), bit(n64.C_UP),
    bit(n64.LD_LEFT), bit(n64.LD_RIGHT), bit(n64.LD_DOWN), bit(n64.LD_UP),
    0, bit(n64.C_RIGHT), 0, bit(n64.C_UP),
    bit(n64.B), bit(n64.C_DOWN), bit(n64.A), bit(n64.C_LEFT),
    bit(n64.START), 0, 0, 0,
    bit(n64.Z), bit(n64.L), 0, 0,
    bit(n64.Z), bit(n64.R), 0, 0,
]


n64_axes = {
    axis.LX: {'size_min': -128, 'size_max': 127, 'neutral': 0x00, 'abs_max': 0x54, 'abs_min': 0x54},
    axis.LY: {'size_min': -128, 'size_max': 127, 'neutral': 0x00, 'abs_max': 0x54, 'abs_min': 0x54},
}