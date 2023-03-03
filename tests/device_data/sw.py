from enum import IntEnum, auto
from .br import axis

class sw_n(IntEnum):
    Y = 0
    X = auto()
    B = auto()
    A = auto()
    R_SR = auto()
    R_SL = auto()
    R = auto()
    ZR = auto()
    MINUS = auto()
    PLUS = auto()
    RJ = auto()
    LJ = auto()
    HOME = auto()
    CAPTURE = auto()
    DOWN = 16
    UP = auto()
    RIGHT = auto()
    LEFT = auto()
    L_SR = auto()
    L_SL = auto()
    L = auto()
    ZL = auto()

sw_n_axes = {
    axis.LX: {'neutral': 0x800, 'abs_max': 0x578, 'deadzone': 0xAE},
    axis.LY: {'neutral': 0x800, 'abs_max': 0x578, 'deadzone': 0xAE},
    axis.RX: {'neutral': 0x800, 'abs_max': 0x578, 'deadzone': 0xAE},
    axis.RY: {'neutral': 0x800, 'abs_max': 0x578, 'deadzone': 0xAE},
}
