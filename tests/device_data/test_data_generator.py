''' Generator function that create test data base on source and destination. '''
import numpy as np
from bit_helper import bit
from .br import axis as ax


def btns_generic_test_data(src):
    ''' Generate test data for btns. '''
    test_data = {0: 0}
    src_all = 0
    br_all = 0
    for br_btns, src_btns in enumerate(src):
        if src_btns:
            if src_btns in test_data:
                test_data[src_btns] |= bit(br_btns)
            else:
                test_data[src_btns] = bit(br_btns)
            src_all |= src_btns
            br_all |= bit(br_btns)
    test_data[src_all] = br_all

    for src_btns, br_btns in test_data.items():
        yield (src_btns, br_btns)


def btns_generic_to_wired_test_data(src, dst):
    ''' Generate test data for btns. '''
    test_data = {0: 0}
    src_all = 0
    dst_all = 0
    for br_btns, src_btns in enumerate(src):
        if src_btns:
            test_data[src_btns] = dst[br_btns]
            src_all |= src_btns
            dst_all |= dst[br_btns]
    test_data[src_all] = dst_all

    for src_btns, dst_btns in test_data.items():
        yield (src_btns, dst_btns)


def axes_test_data_generator(src, dst, dz):
    ''' Generate test data for axes. '''
    def value(sign, src_value, deadzone, scale):
        ''' Compute scaled axes value. '''
        return int(np.single(sign * (src_value - deadzone) * scale))

    test_data = [{}, {}, {}, {}]
    for axis, _ in src.items():
        if axis in dst:
            if axis in (ax.LY, ax.RX):
                sign = -1
            else:
                sign = 1

            if 'polarity' in src[axis] and src[axis]['polarity']:
                src_sign = sign * -1
            else:
                src_sign = sign

            one = int(np.single(src[axis]['abs_max'] / dst[axis]['abs_max']))
            half = int(np.single(one / 2))
            src_neutral = src[axis]['neutral']
            dst_neutral = dst[axis]['neutral']
            src_max = src[axis]['abs_max']
            dst_max = dst[axis]['abs_max']
            src_dz = src[axis]['deadzone']

            deadzone = int(np.single(dz * src_max)) + src_dz
            scale = np.single(dst_max / (src_max - deadzone))

            # Test neutral value
            test_data[0][axis] = {
                'wireless': src_neutral,
                'generic': 0,
                'mapped': 0,
                'wired': dst_neutral,
            }
            # Test maximum value
            test_data[1][axis] = {
                'wireless': src_neutral + src_sign * src_max,
                'generic': src_sign * src_max,
                'mapped': value(sign, src_max, deadzone, scale),
                'wired': value(sign, src_max, deadzone, scale) + dst_neutral,
            }
            # Test deadzone threshold+
            test_data[2][axis] = {
                'wireless': src_neutral + src_sign * (deadzone + one),
                'generic': src_sign * (deadzone + one),
                'mapped': value(sign, (deadzone + one), deadzone, scale),
                'wired': value(sign, (deadzone + one), deadzone, scale) + dst_neutral,
            }
            # Test deadzone threshold-
            test_data[3][axis] = {
                'wireless': src_neutral + src_sign * (deadzone + half),
                'generic': src_sign * (deadzone + half),
                'mapped': 0,
                'wired': dst_neutral,
            }

    for test in test_data:
        yield test
