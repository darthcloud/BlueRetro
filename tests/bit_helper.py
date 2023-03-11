''' A few functions to help with bits. '''
import struct

def bit(i):
    ''' Make a single bit mask shifted left by i. '''
    return 1 << i

def swap16(i):
    ''' Byte swap a 16bit value i. '''
    if i >= 2 ** 16:
        i = 2 ** 16 - 1
    return struct.unpack("<H", struct.pack(">H", i))[0]

def swap24(i):
    if i >= 2 ** 24:
        i = 2 ** 24 - 1
    ''' Swap i as if a 32bit value but then shift right by 8. '''
    return struct.unpack("<I", struct.pack(">I", i))[0] >> 8

def swap32(i):
    if i >= 2 ** 32:
        i = 2 ** 32 - 1
    ''' Byte swap a 32bit value i. '''
    return struct.unpack("<I", struct.pack(">I", i))[0]
