#!/usr/bin/env python

import serial
import sys

neutralBaudRate = 9600
baudRate = int(sys.argv[1]) if len(sys.argv) > 1 else 14400
portName = sys.argv[2] if len(sys.argv) > 2 else "/dev/ttyACM0"
ser = serial.Serial(portName, baudRate)
ser.close()

ser = serial.Serial(portName, neutralBaudRate)
ser.close()


