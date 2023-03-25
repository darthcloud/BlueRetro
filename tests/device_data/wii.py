''' Common constants for Wii devices. '''
from enum import IntEnum, auto
from bit_helper import bit
from .br import axis


class wii_core(IntEnum):
    ''' Buttons bitfield definition for default reports. '''
    D_LEFT = 0
    D_RIGHT = auto()
    D_DOWN = auto()
    D_UP = auto()
    PLUS = auto()
    TWO = 8
    ONE = auto()
    B = auto()
    A = auto()
    MINUS = auto()
    HOME = 15


class wii_classic(IntEnum):
    ''' Buttons bitfield definition for Left Joycon default reports. '''
    R = 1
    PLUS = auto()
    HOME = auto()
    MINUS = auto()
    L = auto()
    D_DOWN = auto()
    D_RIGHT = auto()
    D_UP = auto()
    D_LEFT = auto()
    ZR = auto()
    X = auto()
    A = auto()
    Y = auto()
    B = auto()
    ZL = auto()


class wii_nunchuck(IntEnum):
    ''' Buttons bitfield definition for Right Joycon default reports. '''
    Z = 0
    C = auto()


class wiiu(IntEnum):
    ''' Buttons bitfield definition for Genesis default reports. '''
    R = 1
    PLUS = auto()
    HOME = auto()
    MINUS = auto()
    L = auto()
    D_DOWN = auto()
    D_RIGHT = auto()
    D_UP = auto()
    D_LEFT = auto()
    ZR = auto()
    X = auto()
    A = auto()
    Y = auto()
    B = auto()
    ZL = auto()
    RJ = auto()
    LJ = auto()


wii_core_btns_mask = [
    0, 0, 0, 0,
    0, 0, 0, 0,
    bit(wii_core.D_UP), bit(wii_core.D_DOWN), bit(wii_core.D_LEFT), bit(wii_core.D_RIGHT),
    0, 0, 0, 0,
    bit(wii_core.ONE), bit(wii_core.B), bit(wii_core.TWO), bit(wii_core.A),
    bit(wii_core.PLUS), bit(wii_core.MINUS), bit(wii_core.HOME), 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
]


wii_classic_btns_mask = [
    0, 0, 0, 0,
    0, 0, 0, 0,
    bit(wii_classic.D_LEFT), bit(wii_classic.D_RIGHT), bit(wii_classic.D_DOWN), bit(wii_classic.D_UP),
    0, 0, 0, 0,
    bit(wii_classic.Y), bit(wii_classic.A), bit(wii_classic.B), bit(wii_classic.X),
    bit(wii_classic.PLUS), bit(wii_classic.MINUS), bit(wii_classic.HOME), 0,
    0, bit(wii_classic.ZL), bit(wii_classic.L), 0,
    0, bit(wii_classic.ZR), bit(wii_classic.R), 0,
]


wii_classic_pro_btns_mask = [
    0, 0, 0, 0,
    0, 0, 0, 0,
    bit(wii_classic.D_LEFT), bit(wii_classic.D_RIGHT), bit(wii_classic.D_DOWN), bit(wii_classic.D_UP),
    0, 0, 0, 0,
    bit(wii_classic.Y), bit(wii_classic.A), bit(wii_classic.B), bit(wii_classic.X),
    bit(wii_classic.PLUS), bit(wii_classic.MINUS), bit(wii_classic.HOME), 0,
    bit(wii_classic.ZL), bit(wii_classic.L), 0, 0,
    bit(wii_classic.ZR), bit(wii_classic.R), 0, 0,
]


wii_classic_core_btns_mask = [
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    bit(wii_core.D_UP), bit(wii_core.D_DOWN), bit(wii_core.D_LEFT), bit(wii_core.D_RIGHT),
    0, 0, 0, 0,
    0, 0, 0, bit(wii_core.B),
    0, 0, 0, bit(wii_core.ONE),
    0, 0, 0, bit(wii_core.TWO),
]


wii_nunchuck_btns_mask = [
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    bit(wii_nunchuck.Z), bit(wii_nunchuck.C), 0, 0,
    0, 0, 0, 0,
]


wii_nunchuck_core_btns_mask = [
    0, 0, 0, 0,
    0, 0, 0, 0,
    bit(wii_core.D_LEFT), bit(wii_core.D_RIGHT), bit(wii_core.D_DOWN), bit(wii_core.D_UP),
    0, 0, 0, 0,
    bit(wii_core.ONE), bit(wii_core.TWO), bit(wii_core.A), 0,
    bit(wii_core.PLUS), bit(wii_core.MINUS), bit(wii_core.HOME), 0,
    0, 0, 0, 0,
    bit(wii_core.B), 0, 0, 0,
]


wiiu_btns_mask = [
    0, 0, 0, 0,
    0, 0, 0, 0,
    bit(wiiu.D_LEFT), bit(wiiu.D_RIGHT), bit(wiiu.D_DOWN), bit(wiiu.D_UP),
    0, 0, 0, 0,
    bit(wiiu.Y), bit(wiiu.A), bit(wiiu.B), bit(wiiu.X),
    bit(wiiu.PLUS), bit(wiiu.MINUS), bit(wiiu.HOME), 0,
    bit(wiiu.ZL), bit(wiiu.L), 0, bit(wiiu.LJ),
    bit(wiiu.ZR), bit(wiiu.R), 0, bit(wiiu.RJ),
]


wii_classic_axes = {
    axis.LX: {'neutral': 0x20, 'abs_max': 0x1B, 'abs_min': 0x1B, 'deadzone': 0},
    axis.LY: {'neutral': 0x20, 'abs_max': 0x1B, 'abs_min': 0x1B, 'deadzone': 0},
    axis.RX: {'neutral': 0x10, 'abs_max': 0x0D, 'abs_min': 0x0D, 'deadzone': 0},
    axis.RY: {'neutral': 0x10, 'abs_max': 0x0D, 'abs_min': 0x0D, 'deadzone': 0},
    axis.LM: {'neutral': 0x02, 'abs_max': 0x1D, 'abs_min': 0x00, 'deadzone': 0},
    axis.RM: {'neutral': 0x02, 'abs_max': 0x1D, 'abs_min': 0x00, 'deadzone': 0},
}


wii_classic_8bit_axes = {
    axis.LX: {'neutral': 0x80, 'abs_max': 0x66, 'abs_min': 0x66, 'deadzone': 0},
    axis.LY: {'neutral': 0x80, 'abs_max': 0x66, 'abs_min': 0x66, 'deadzone': 0},
    axis.RX: {'neutral': 0x80, 'abs_max': 0x66, 'abs_min': 0x66, 'deadzone': 0},
    axis.RY: {'neutral': 0x80, 'abs_max': 0x66, 'abs_min': 0x66, 'deadzone': 0},
    axis.LM: {'neutral': 0x16, 'abs_max': 0xDA, 'abs_min': 0x00, 'deadzone': 0},
    axis.RM: {'neutral': 0x16, 'abs_max': 0xDA, 'abs_min': 0x00, 'deadzone': 0},
}


wii_nunchuck_axes = {
    axis.LX: {'neutral': 0x80, 'abs_max': 0x63, 'abs_min': 0x63, 'deadzone': 0},
    axis.LY: {'neutral': 0x80, 'abs_max': 0x63, 'abs_min': 0x63, 'deadzone': 0},
}


wiiu_axes = {
    axis.LX: {'neutral': 0x800, 'abs_max': 0x44C, 'abs_min': 0x44C, 'deadzone': 0},
    axis.LY: {'neutral': 0x800, 'abs_max': 0x44C, 'abs_min': 0x44C, 'deadzone': 0},
    axis.RX: {'neutral': 0x800, 'abs_max': 0x44C, 'abs_min': 0x44C, 'deadzone': 0},
    axis.RY: {'neutral': 0x800, 'abs_max': 0x44C, 'abs_min': 0x44C, 'deadzone': 0},
}
