#!/usr/bin/env python3

# Copyright (c) 2021 Intel Corporation
# SPDX-License-Identifier: Apache-2.0

import os
import re

ZEPHYR_BASE = os.getenv("ZEPHYR_BASE")

regex_flags = re.MULTILINE | re.VERBOSE
dt_drv_compat_regex = re.compile(r'\#define\s+DT_DRV_COMPAT\s+(\w+)', regex_flags)
sensor_chan_regex = re.compile(r'SENSOR_CHAN_(\w+)', regex_flags)

def parse_files(base):
    '''
    Parse sensor driver source files to determine which sensor channels each
    driver supports. Returns a dictionary of sensor channel sets indexed by
    devicetree compatible string.

    Some driver implementations span multiple source files, therefore we merge
    channels into one set if they use the same devietree compatible.
    '''
    sensors = {}
    for root, dirs, files in os.walk(base, topdown=True):
        for filename in files:
            path = os.path.join(root, filename)

            if not path.endswith(".c"):
                continue

            with open(path, "r") as f:
                contents = f.read()

            dt_drv_compat_match = dt_drv_compat_regex.search(contents)
            sensor_chan_match = sensor_chan_regex.findall(contents)

            if not dt_drv_compat_match or not sensor_chan_match:
                continue

            dt_drv_compat = dt_drv_compat_match[1]
            sensor_chan = set(sensor_chan_match)

            if dt_drv_compat in sensors:
                sensors[dt_drv_compat] |= sensor_chan
            else:
                sensors[dt_drv_compat] = sensor_chan

    return sensors

def get_sensor_types(channels):
    '''
    Determine the type of sensor based on groups of channels supported. For
    example, a sensor that supports any accelerometer channel (X, Y, or Z) is
    an "Accelerometer". A sensor that supports any accelerometer channel and any
    magnetometer channel is an "Accelerometer/Magnetometer".
    '''
    accel = {'ACCEL_' + x for x in 'XYZ'}
    gyro = {'GYRO_' + x for x in 'XYZ'}
    mag = {'MAGN_' + x for x in 'XYZ'}
    gas = {'CO2', 'VOC', 'GAS_RES'}
    light = {'BLUE', 'GREEN', 'RED', 'LIGHT'}
    gauge = {'GAUGE_' + x for x in ['AVG_CURRENT', 'CYCLE_COUNT']}

    types = set()

    if (channels & accel):
        types.add('Accelerometer')
    if (channels & gyro):
        types.add('Gyroscope')
    if (channels & mag):
        types.add('Magnetometer')
    if (channels & gas):
        types.add('Gas')
    if (channels & light):
        types.add('Light')
    if (channels & gauge):
        types.add('Fuel Gauge')

    if ('AMBIENT_TEMP' in channels):
        types.add('Temperature')
    if ('HUMIDITY' in channels):
        types.add('Humidity')
    if ('PRESS' in channels):
        types.add('Pressure')
    if ('PROX' in channels):
        types.add('Proximity')
    if ('RPM' in channels):
        types.add('Tachometer')

    return types

def main():
    folder = os.path.join(ZEPHYR_BASE, 'drivers', 'sensor')

    sensors = parse_files(folder)

    print('Vendor, Part Number, Type')

    for dt_drv_compat in sorted(sensors.keys()):
        if "_" in dt_drv_compat:
            vendor, part_number = dt_drv_compat.split('_', 1)
        else:
            vendor = ""
            part_number = dt_drv_compat

        channels = sensors[dt_drv_compat]

        types = get_sensor_types(channels)

        print(f'{vendor}, {part_number},', '/'.join(sorted(types)))

if __name__ == "__main__":
    main()
