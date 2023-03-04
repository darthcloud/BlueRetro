''' Common constants for BlueRetro adapter. '''
from enum import IntEnum, auto
from collections import namedtuple

AxesData = namedtuple('AxesData', ['wireless', 'generic', 'mapped', 'wired'])

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
