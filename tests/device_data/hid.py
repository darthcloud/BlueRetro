''' Common constants for HID devices. '''
from enum import IntEnum, auto
from bit_helper import bit


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
    B16 = auto()
    B17 = auto()
    B18 = auto()
    B19 = auto()
    B20 = auto()


hid_btns_mask = [
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    bit(hid.B16), bit(hid.Z), bit(hid.B17), bit(hid.C),
    bit(hid.X), bit(hid.B), bit(hid.A), bit(hid.Y),
    bit(hid.START), bit(hid.SELECT), bit(hid.MENU), bit(hid.B18),
    bit(hid.L), bit(hid.LB), bit(hid.B19), bit(hid.LJ),
    bit(hid.R), bit(hid.RB), bit(hid.B20), bit(hid.RJ),
]
