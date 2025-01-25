#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2025 Jacques Gagnon
# SPDX-License-Identifier: Apache-2.0

from argparse import ArgumentParser
from esptool import main as esptool
from os.path import dirname


def parse_args():
    parser = ArgumentParser()
    parser.add_argument('firmware', help='BlueRetro firmware bin file')
    return parser.parse_args()


def main():
    # Get arguments
    args = parse_args()
    dir = dirname(args.firmware)

    command = [
        '--chip', 'esp32',
        '-b', '460800',
        '--before', 'default_reset',
        '--after', 'hard_reset',
        'write_flash',
        '--flash_mode', 'dio',
        '--flash_size', '4MB',
        '--flash_freq', '40m',
        '0x1000', f'{dir}/bootloader/bootloader.bin',
        '0x8000', f'{dir}/partition_table/partition-table.bin',
        '0xd000', f'{dir}/ota/ota_data_initial.bin',
        '0x10000', args.firmware,
    ]
    esptool(command)


if __name__ == "__main__":
    main()
