###########################################################################

###     Host PC script to update or read data from the microcontrollers 
 
###     Copyright (C) 2020 Priya Pandya <priyapandya274@gmail.com> 

###########################################################################

from intelhex import IntelHex16bit
import argparse
import serial
import sys
import crc8
from binascii import hexlify

SequenceNum = {
    'seq_num' : 000
}

def GetCommand(seq_num, command, fields):
    RS = '1e'
    EOT = '04'
    string = ''
    string_hex = ''
    for val in fields:
        string += RS
        string_hex += RS
        string += val
        string_hex += hexlify(val.encode()).decode()
    string += RS + EOT
    string_hex += RS + EOT
    
    seq_num += 1
    seq_num = '{:03d}'.format(seq_num)
    SequenceNum['seq_num'] = int(seq_num) 
     
    crc = crc8.crc8()
    crc.update(string_hex.encode())
     
    final_command = command + seq_num + crc.hexdigest() + string
    return final_command

def connect(p):
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
    port.write(data.encode())

def BLCommand(port, chip_cmd, mode_cmd):
    write_port(port, chip_cmd)
    '''
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
    '''

def parseFile(hexFile):
    ih = IntelHex16bit()
    ih.fromfile(hexFile,format='hex')
    return ih
    
def showVersion():
    print('In the version')
    
def readCode(args):
    print('In the read')
    port = connect(args.dest)
    chip_fields = [args.chip]
    read_fields = ['read_mode']
    chip_command = GetCommand(SequenceNum['seq_num'], 'S', chip_fields)
    read_command = GetCommand(SequenceNum['seq_num'], 'S', read_fields)
    print(chip_command, read_command)
    BLCommand(port, chip_command, read_command)

def writeCode(args):
    print('In the write')
    data = parseFile(args.hex)
    port = connect(args.dest)
    #BLCommand(port, Chip[args.chip], BootloaderCommands['write_mode'])

def call(args):
    if args.version:
        showVersion()
    if args.read:
        readCode(args)
    if args.write:
        writeCode(args)

def main():
    parser = argparse.ArgumentParser(prog='Remote_UART_Programmer', description='Commands for Remote_UART_Programmer')     #change it to something appropriate
    parser.add_argument('--dest', metavar='path', type=str, help='the destination path')
    parser.add_argument('--hex', metavar='path', type=str, help='the hex file path')
    parser.add_argument('--chip', type=str, help='Key Manager to be selected for flashing of the hex', choices=['km_west', 'km_east', 'pic32'], nargs='?')
    parser.add_argument('-v', '--version', action='store_true', help='display the versions')
    parser.add_argument('-r', '--read', action='store_true', help='read the code back from the PIC memory')
    parser.add_argument('-w', '--write', action='store_true', help='write the hex code in the PIC memory')

    args = parser.parse_args()
    
    call(args)

if __name__ == '__main__':
    main()
