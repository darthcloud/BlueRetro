''' Common constants for HID devices. '''
from enum import IntEnum, auto

class hid(IntEnum):
    ''' Buttons bitfield definition for a typical HID controllers. '''
    A = 0
    B = auto()
    C = auto()
    X = auto()
    Y = auto()
    Z = auto()
    LB = auto()
    RB = auto()
    L = auto()
    R = auto()
    SELECT = auto()
    START = auto()
    MENU = auto()
    LJ = auto()
    RJ = auto()
