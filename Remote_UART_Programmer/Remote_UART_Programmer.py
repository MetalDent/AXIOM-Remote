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
    'sequenceNumber' : 0
}

RS = chr(ascii.RS)
EOT = chr(ascii.EOT)

def GetCommand(sequenceNumber, command, fields):
    string = ''
    for val in fields:
        string += RS
        string += val
    string += EOT
    
    sequenceNumber += 1
    SequenceNum['sequenceNumber'] = sequenceNumber 
     
    crc = crc8.crc8()
    crc.update(string.encode())
    c = int(crc.hexdigest(), base=16)
         
    final_command = "{}{}{}{}".format(command, str(sequenceNumber), chr(c), string)
    print(final_command)
    
    return final_command

def connect(p):
    try:
        port = serial.Serial(p, timeout=1)
        return port
    except serial.serialutil.SerialException:
        print('SerialException occurred!')
    except Exception as e:
        sys.exit('Error! ' + str(e))
        
def toString(s):
    string = ''
    return (string.join(s))

def read_port(port):
    return (port.readlines())

def write_port(port, data):    
    data_split = []
    for i in range(0, len(data), 8):
        data_split.append(data[0+i:8+i])
    
    if type(data) == str:
        for i in data_split:
            byte_data = bytearray(i.encode())
            if len(byte_data) == 9:
                byte_data = byte_data[:2] + byte_data[3:]            
            port.write(byte_data)
    
    else:
        for i in data_split:
            stringData = toString(i)
            print(stringData, len(stringData))
            byte_data = bytearray(stringData.encode())
            #print(byte_data, len(byte_data))
            #port.write(byte_data)
        
def BLCommand(port, chip_cmd, mode_cmd, data = []):
    write_port(port, chip_cmd)
    ack = port.readline()
    print(ack)
    if b'ACK' in ack:
        print('ACK')
        write_port(port, mode_cmd)
        if 'read' in mode_cmd:
            data = read_port(port)
            for d in data:
                print(d)
        elif 'write' in mode_cmd:
            ack = port.readline()
            print(ack)
            if b'ACK' in ack:
                print('ACK')
            else:
                sys.exit('Error!')    
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
    
def readCode(args):
    print('In the read')
    port = connect(args.port)
    chip_fields = [args.chip]
    read_fields = ['read']
    chip_command = GetCommand(SequenceNum['sequenceNumber'], 'S', chip_fields)
    read_command = GetCommand(SequenceNum['sequenceNumber'], 'S', read_fields)
    BLCommand(port, chip_command, read_command)
    port.close()

def writeCode(args):
    print('In the write')
    port = connect(args.port)
    chip_fields = [args.chip]
    read_fields = ['write']
    chip_command = GetCommand(SequenceNum['sequenceNumber'], 'S', chip_fields)
    write_command = GetCommand(SequenceNum['sequenceNumber'], 'S', read_fields)
    data = parseFile(args.hex)
    dataList = []
    for i in range(len(data)):
        dataList.append(chr(data[i]))
    #BLCommand(port, chip_command, write_command, dataList)

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
