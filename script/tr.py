#!/usr/bin/env python3
"""
tr.py is used to translate and print sensor readings from the raw
byte-pack sent by the sensor node.
"""

import binascii
import base64
import datetime
import struct
import sys


CCS811_STATUS =["OK", "invalid ID", "I2C error", "internal error", "generic error"]

class Reading:
    __slots__ = [
        "when",
        "hw",
        "uptime",
        "temperature",
        "calibration_temperature",
        "humidity",
        "pressure",
        "co2",
        "tvoc",
        "voltage",
        "ccs811_status",
        "calibrated",
        "fix",
        "sats"
    ]

    def __init__(self, data):
        (year, month, day, hour, minute, second) = data[:6]
        self.when = datetime.datetime(year, month, day, hour, minute, second)
        self.hw = data[6]
        self.uptime = data[7]
        self.temperature = data[8]
        self.calibration_temperature = data[9]
        self.humidity = data[10]
        self.pressure = data[11] / 1000.0
        self.co2 = data[12]
        self.tvoc = data[13]
        self.voltage = data[14] / 100.0
        self.ccs811_status = data[15]
        if data[16] == 1:
            self.calibrated = True
        else:
            self.calibrated = False
        if data[17] == 1:
            self.fix = True
        self.sats = data[18]

    def hardware(self):
        hw = []
        if (self.hw & 1) != 0:
            hw.append("BME280")
        if (self.hw & 2) != 0:
            hw.append("CCS811")
        if self.hw & 4:
            hw.append("PCF8523 RTC")
        if self.hw & 8:
            hw.append("SD")
        if self.hw & 16:
            hw.append("GPS")

        return hw

    def ccs811_status_str(self):
        if self.ccs811_status >= len(CCS811_STATUS):
            return 'unknown'
        return CCS811_STATUS[self.ccs811_status]

    def __str__(self):
        return f"""Timestamp: {self.when}
\tHardware: {self.hardware()} ({self.hw})
\tUptime: {self.uptime}s
\tTemperature: {self.temperature:0.1f}°C
\t\tCalibrated? {self.calibrated}
\t\tCalibration temperature: {self.calibration_temperature:0.1f}°C
\tHumidity: {self.humidity:0.1f}%
\tPressure: {self.pressure:0.3f} kPa
\tCO2: {self.co2} ppm
\tTVOC: {self.tvoc} ppb
\tVoltage: {self.voltage}V
\tCCS811 status: {self.ccs811_status_str()}
\tSats: {self.sats} (fix={self.fix})
"""


def unpack(data):
    try:
        data = binascii.unhexlify(data.replace(" ", ""))
    except binascii.Error:
        data = base64.decodebytes(data.encode('utf-8'))
    return struct.unpack("<HBBBBBBIffffiiBBBBB", data)


def translate(data):
    print(Reading(unpack(data)))


def main():
    if len(sys.argv) > 1:
        for arg in sys.argv[1:]:
            print(translate(arg))


if __name__ == "__main__":
    main()
