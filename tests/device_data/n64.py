from enum import IntEnum, auto
from .br import axis

class n64(IntEnum):
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

n64_axes = {
    axis.LX: {'size_min': -128, 'size_max': 127, 'neutral': 0x00, 'abs_max': 0x54},
    axis.LY: {'size_min': -128, 'size_max': 127, 'neutral': 0x00, 'abs_max': 0x54},
}