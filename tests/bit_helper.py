import struct

def bit(i):
    return 1 << i

def swap32(i):
    return struct.unpack("<I", struct.pack(">I", i))[0]
