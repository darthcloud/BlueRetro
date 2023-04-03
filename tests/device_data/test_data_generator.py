''' Generator function that create test data base on source and destination. '''
import numpy as np
from bit_helper import bit
from .br import axis as ax


def btns_generic_test_data(src, mask=0xFFFFFFFF):
    ''' Generate test data for btns. '''
    test_data = {0: 0}
    src_all = 0
    br_all = 0
    for br_btns, src_btns in enumerate(src):
        if (mask & bit(br_btns)) and src_btns:
            if src_btns in test_data:
                test_data[src_btns] |= bit(br_btns)
            else:
                test_data[src_btns] = bit(br_btns)
            src_all |= src_btns
            br_all |= bit(br_btns)
    test_data[src_all] = br_all

    for src_btns, br_btns in test_data.items():
        yield (src_btns, br_btns)


def btns_generic_to_wired_test_data(src, dst, mask=0xFFFFFFFF):
    ''' Generate test data for btns. '''
    test_data = {0: 0}
    src_all = 0
    dst_all = 0
    for br_btns, src_btns in enumerate(src):
        if (mask & bit(br_btns)) and src_btns:
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

    test_data = [{}, {}, {}, {}, {}, {}, {}]
    for axis, _ in src.items():
        if axis in dst:
            if axis in (ax.LY, ax.RX):
                sign = [-1, 1]
            else:
                sign = [1, -1]

            if 'polarity' in src[axis] and src[axis]['polarity']:
                src_sign = [sign[1], sign[0]]
            else:
                src_sign = [sign[0], sign[1]]

            if src_sign[0] > 0:
                src_max = [src[axis]['abs_max'], src[axis]['abs_min']]
            else:
                src_max = [src[axis]['abs_min'], src[axis]['abs_max']]
            if sign[0] > 0:
                dst_max = [dst[axis]['abs_max'], dst[axis]['abs_min']]
            else:
                dst_max = [dst[axis]['abs_min'], dst[axis]['abs_max']]

            one = int(np.single(src_max[0] / dst_max[0]))
            half = int(np.single(one / 2))

            if half == 0:
                half = 1

            src_neutral = src[axis]['neutral']
            dst_neutral = dst[axis]['neutral']
            src_dz = src[axis]['deadzone']

            deadzone = [int(np.single(dz * src_max[0])) + src_dz,
                        int(np.single(dz * src_max[1])) + src_dz]

            if axis in (ax.LM, ax.RM):
                scale = [np.single(dst_max[0] / (src_max[0] - deadzone[0])),
                        np.single(dst_max[0] / (src_max[0] - deadzone[0]))]
            else:
                scale = [np.single(dst_max[0] / (src_max[0] - deadzone[0])),
                        np.single(dst_max[1] / (src_max[1] - deadzone[1]))]

            pull_back = int(np.single(src_max[0] * 0.95))
            pb_dz = int(np.single(dz * pull_back)) + src_dz
            pb_scale = np.single(dst_max[0] / (pull_back - pb_dz))

            id = 0
            # Test neutral value at default max
            test_data[id][axis] = {
                'wireless': src_neutral,
                'generic': 0,
                'mapped': 0,
                'wired': dst_neutral,
            }
            id += 1
            # Test pull-back value
            test_data[id][axis] = {
                'wireless': src_neutral + src_sign[0] * pull_back,
                'generic': src_sign[0] * pull_back,
                'mapped': value(sign[0], pull_back, pb_dz, pb_scale),
                'wired': value(sign[0], pull_back, pb_dz, pb_scale) + dst_neutral,
            }
            id += 1
            # Test maximum value
            test_data[id][axis] = {
                'wireless': src_neutral + src_sign[0] * src_max[0],
                'generic': src_sign[0] * src_max[0],
                'mapped': value(sign[0], src_max[0], deadzone[0], scale[0]),
                'wired': value(sign[0], src_max[0], deadzone[0], scale[0]) + dst_neutral,
            }
            id += 1
            # Test maximum value with reverse polarity
            test_data[id][axis] = {
                'wireless': src_neutral + src_sign[1] * src_max[1],
                'generic': src_sign[1] * src_max[1],
                'mapped': value(sign[1], src_max[1], deadzone[1], scale[1]),
                'wired': value(sign[1], src_max[1], deadzone[1], scale[1]) + dst_neutral,
            }
            id += 1
            # Test neutral value at true max
            test_data[id][axis] = {
                'wireless': src_neutral,
                'generic': 0,
                'mapped': 0,
                'wired': dst_neutral,
            }
            id += 1
            # Test deadzone threshold+
            test_data[id][axis] = {
                'wireless': src_neutral + src_sign[0] * (deadzone[0] + one + half),
                'generic': src_sign[0] * (deadzone[0] + one + half),
                'mapped': sign[0],
                'wired': sign[0] + dst_neutral,
            }
            id += 1
            # Test deadzone threshold-
            test_data[id][axis] = {
                'wireless': src_neutral + src_sign[0] * (deadzone[0] + half),
                'generic': src_sign[0] * (deadzone[0] + half),
                'mapped': 0,
                'wired': dst_neutral,
            }

    for test in test_data:
        yield test
