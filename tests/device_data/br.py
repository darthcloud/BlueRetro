''' Common constants for BlueRetro adapter. '''
from enum import IntEnum, auto
from bit_helper import bit


class bt_conn_type(IntEnum):
    ''' BT connection type. '''
    BT_BR_EDR = 0
    BT_LE = auto()


class bt_type(IntEnum):
    ''' BT device type ID. '''
    NONE = -1
    HID_GENERIC = 0
    PS3 = auto()
    WII = auto()
    XBOX = auto()
    PS = auto()
    SW = auto()


class bt_subtype(IntEnum):
    ''' BT device subtype ID. '''
    SUBTYPE_DEFAULT = 0
    WII_NUNCHUCK = auto()
    WII_CLASSIC = auto()
    WII_CLASSIC_8BIT = auto()
    WII_CLASSIC_PRO = auto()
    WII_CLASSIC_PRO_8BIT = auto()
    WIIU_PRO = auto()
    PS5_DS = auto()
    XBOX_XINPUT = auto()
    XBOX_XS = auto()
    XBOX_ADAPTIVE = auto()
    SW_LEFT_JOYCON = auto()
    SW_RIGHT_JOYCON = auto()
    SW_NES = auto()
    SW_SNES = auto()
    SW_N64 = auto()
    SW_MD_GEN = auto()
    SW_POWERA = auto()
    SW_HYPERKIN_ADMIRAL = auto()
    GBROS = auto()


class system(IntEnum):
    ''' Wired system ID. '''
    WIRED_AUTO = 0
    PARALLEL_1P = auto()
    PARALLEL_2P = auto()
    NES = auto()
    PCE = auto()
    GENESIS = auto()
    SNES = auto()
    CDI = auto()
    CD32 = auto()
    REAL_3DO = auto()
    JAGUAR = auto()
    PSX = auto()
    SATURN = auto()
    PCFX = auto()
    JVS = auto()
    N64 = auto()
    DC = auto()
    PS2 = auto()
    GC = auto()
    WII_EXT = auto()
    VBOY = auto()
    PARALLEL_1P_OD = auto()
    PARALLEL_2P_OD = auto()
    SEA_BOARD = auto()


class report_type(IntEnum):
    ''' Report type ID. '''
    REPORT_NONE = -1
    KB = 0
    MOUSE = auto()
    PAD = auto()
    EXTRA = auto()
    RUMBLE = auto()


class pad(IntEnum):
    ''' Buttons bitfield definition for controllers. '''
    LX_LEFT = 0
    LX_RIGHT = auto()
    LY_DOWN = auto()
    LY_UP = auto()
    RX_LEFT = auto()
    RX_RIGHT = auto()
    RY_DOWN = auto()
    RY_UP = auto()
    LD_LEFT = auto()
    LD_RIGHT = auto()
    LD_DOWN = auto()
    LD_UP = auto()
    RD_LEFT = auto()
    RD_RIGHT = auto()
    RD_DOWN = auto()
    RD_UP = auto()
    RB_LEFT = auto()
    RB_RIGHT = auto()
    RB_DOWN = auto()
    RB_UP = auto()
    MM = auto()
    MS = auto()
    MT = auto()
    MQ = auto()
    LM = auto()
    LS = auto()
    LT = auto()
    LJ = auto()
    RM = auto()
    RS = auto()
    RT = auto()
    RJ = auto()


class axis(IntEnum):
    ''' Axes index definition for controllers. '''
    LX = 0
    LY = auto()
    RX = auto()
    RY = auto()
    LM = auto()
    RM = auto()

class dev_mode(IntEnum):
    ''' Dev mode. '''
    PAD = 0
    PAD_ALT = auto()
    KB = auto()
    MOUSE = auto()


hat_to_ld_btns = [
    bit(pad.LD_UP), bit(pad.LD_UP) | bit(pad.LD_RIGHT),
    bit(pad.LD_RIGHT), bit(pad.LD_DOWN) | bit(pad.LD_RIGHT),
    bit(pad.LD_DOWN), bit(pad.LD_DOWN) | bit(pad.LD_LEFT),
    bit(pad.LD_LEFT), bit(pad.LD_UP) | bit(pad.LD_LEFT),
    0, 0, 0, 0, 0, 0, 0, 0,
]