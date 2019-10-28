import datetime
import serial
import struct
import sys

port = "/dev/ttyACM0"

if len(sys.argv) == 2:
    port = sys.argv[1]

sercon = serial.Serial(port=port, baudrate=9600)
date = datetime.datetime.now()

line = struct.pack(
    "B B B B B ", date.month, date.day, date.hour, date.minute, date.second
)
print(len(line))
print(line);
sercon.write(line)
while True:
    try:
        v = sercon.read(1).decode("ASCII")
        sys.stdout.write(v)
    except:
        break
