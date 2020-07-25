###########################################################################

###     Host PC script to update or read data from the microcontrollers 
 
###     Copyright (C) 2020 Priya Pandya <priyapandya274@gmail.com> 

###########################################################################

from intelhex import IntelHex16bit      # read hex file
import argparse                         # create commandline arguments
import serial                           # connect to serial port
import sys                              # sys.exit()
import crc8                             # calculation of CRC8
from curses import ascii                # values of RS and EOT
import textwrap                         # to split string

SequenceNum = {
    'seq_num' : 000
}

RS = hex(ascii.RS)
EOT = hex(ascii.EOT)

def GetCommand(seq_num, command, fields):
    string = ''
    for val in fields:
        string += RS
        string += val
    string += EOT
    
    seq_num += 1
    seq_num = '{:03d}'.format(seq_num)
    SequenceNum['seq_num'] = int(seq_num) 
     
    crc = crc8.crc8()
    crc.update(string.encode())
         
    final_command = command + seq_num + crc.hexdigest() + string
    return final_command

def connect(p):
    try:
        port = serial.Serial(p, timeout=1)
        return port
    except serial.serialutil.SerialException:
        print('SerialException occurred!')
    except Exception as e:
        sys.exit('Error! ' + str(e))

def read_port(port):
    return (port.readline())

def write_port(port, data):
    data_split = textwrap.wrap(data, 8)
    for i in data_split:
        port.write(i.encode())

def BLCommand(port, chip_cmd, mode_cmd):
    write_port(port, chip_cmd)
    ack = read_port(port)
    print(ack)
    '''
    if ack.decode() == 'ACK':
        write_port(port, mode_cmd)
        ack = read_port(port)
        print(ack)
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
    port = connect(args.port)
    chip_fields = [args.chip]
    read_fields = ['read_mode']
    chip_command = GetCommand(SequenceNum['seq_num'], 'S', chip_fields)
    read_command = GetCommand(SequenceNum['seq_num'], 'S', read_fields)
    print(chip_command, read_command)
    BLCommand(port, chip_command, read_command)
    port.close()

def writeCode(args):
    print('In the write')
    port = connect(args.port)
    chip_fields = [args.chip]
    read_fields = ['write_mode']
    chip_command = GetCommand(SequenceNum['seq_num'], 'S', chip_fields)
    write_command = GetCommand(SequenceNum['seq_num'], 'S', read_fields)
    print(chip_command, read_command)
    BLCommand(port, chip_command, write_command)

def call(args):
    if args.version:
        showVersion()
    if args.read:
        readCode(args)
    if args.write:
        writeCode(args)

def main():
    parser = argparse.ArgumentParser(prog='Remote_UART_Programmer', description='Commands for Remote_UART_Programmer')     #change it to something appropriate
    parser.add_argument('--port', metavar='path', type=str, help='the destination path')
    parser.add_argument('--hex', metavar='path', type=str, help='the hex file path')
    parser.add_argument('--chip', type=str, help='Key Manager to be selected for flashing of the hex', choices=['km_west', 'km_east', 'pic32'], nargs='?')
    parser.add_argument('-v', '--version', action='store_true', help='display the versions')
    parser.add_argument('-r', '--read', action='store_true', help='read the code back from the PIC memory')
    parser.add_argument('-w', '--write', action='store_true', help='write the hex code in the PIC memory')

    args = parser.parse_args()
    
    call(args)

if __name__ == '__main__':
    main()
