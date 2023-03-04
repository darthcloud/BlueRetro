''' Common constants for GameCube devices. '''
from enum import IntEnum, auto
from .br import axis

GC = 18

class gc(IntEnum):
    ''' Buttons bitfield definition for controllers. '''
    A = 0
    B = auto()
    X = auto()
    Y = auto()
    START = auto()
    LD_LEFT = 8
    LD_RIGHT = auto()
    LD_DOWN = auto()
    LD_UP = auto()
    Z = auto()
    R = auto()
    L = auto()

gc_axes = {
    axis.LX: {'size_min': -128, 'size_max': 127, 'neutral': 0x80, 'abs_max': 0x64},
    axis.LY: {'size_min': -128, 'size_max': 127, 'neutral': 0x80, 'abs_max': 0x64},
    axis.RX: {'size_min': -128, 'size_max': 127, 'neutral': 0x80, 'abs_max': 0x5C},
    axis.RY: {'size_min': -128, 'size_max': 127, 'neutral': 0x80, 'abs_max': 0x5C},
    axis.LM: {'size_min': 0, 'size_max': 255, 'neutral': 0x20, 'abs_max': 0xD0},
    axis.RM: {'size_min': 0, 'size_max': 255, 'neutral': 0x20, 'abs_max': 0xD0},
}
