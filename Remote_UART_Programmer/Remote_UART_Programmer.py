###########################################################################
###     Host PC script to update or read data from the microcontrollers 
 
###     Copyright (C) 2020 Priya Pandya <priyapandya274@gmail.com> 
###########################################################################

from intelhex import IntelHex16bit
import argparse
import serial
import sys
import enum

p = '/dev/ttyACM0'

class Chip(enum.Enum):
    KM_Wast = 'S123E9 chip 1'
    KM_East = 'S123E9 chip 2'
    PIC32 = 'S123E9 chip 3'

class BootloaderCommands(enum.Enum):
    read_mode = 'G123DD read_data'
    write_mode = 'S124EA write_data'

def connect():
    try:
        port = serial.Serial(p, timeout=1)
        return port
    except serial.serialutil.SerialException:
        print('SerialException ocurred!')
    except Exception as e:
        sys.exit('Error! ' + str(e))

def read_port(port):
    print(port.readlines())

def write_port(port, data):
    port.write(data)

def BLCommand(port, chip, mode):
    write_port(port, chip)
    ack = read_port(port)
    if ack == 'ACK':
        write_port(port, mode)
        ack = read_port(port)
        if ack == 'ACK':
            print('')   #send data
        else:
           sys.exit('Error!')
    else:
        sys.exit('Error!') 

def parseFile(hexFile):
    ih = IntelHex16bit()
    ih.fromfile(hexFile,format='hex')
    return ih
    
def showVersion():
    print('In the version')
    
def readCode():
    print('In the read')
    port = connect()
    select_chip = args.chip
    BLCommand(port, Chip.select_chip.value, BootloaderCommands.read_mode.value)

def writeCode():
    print('In the write')
    data = parseFile(args.Hex)
    port = connect()
    select_chip = args.chip
    BLCommand(port, Chip.select_chip.value, BootloaderCommands.write_mode.value)

def call(args):
    if args.version:
        showVersion()
    if args.read:
        readCode()
    if args.write:
        writeCode()

def main():
    parser = argparse.ArgumentParser(prog='Remote_UART_Programmer', description='Commands for Remote_UART_Programmer')     #change it to something appropriate
    parser.add_argument('--dest', metavar='path', type=str, help='the destination path')
    parser.add_argument('--hex', metavar='path', type=str, help='the hex file path')
    parser.add_argument('--chip', type=str, help='Key Manager to be selected for flashing of the hex', choices=['KM_West', 'KM_East', 'PIC32'], nargs='?')
    parser.add_argument('-v', '--version', action='store_true', help='display the versions')
    parser.add_argument('-r', '--read', action='store_true', help='read the code back from the PIC memory')
    parser.add_argument('-w', '--write', action='store_true', help='write the hex code in the PIC memory')

    args = parser.parse_args()
    
    call(args)

if __name__ == '__main__':
    main()
