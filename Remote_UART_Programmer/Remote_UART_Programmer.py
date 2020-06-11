from intelhex import IntelHex16bit
import argparse

def parseFile(hexFile):
    ih = IntelHex16bit()
    ih.fromfile(hexFile,format='hex')
    pydict = ih.todict()
    print(pydict)

def showVersion():
    print('In the version')
    
def readCode():
    print('In the read')

def writeCode():
    print('In the write')
    parseFile(args.Hex)

parser = argparse.ArgumentParser(prog='Remote_UART_Programmer', description='Commands for Remote_UART_Programmer')     #change it to something appropriate
parser.add_argument('--Dest', metavar='path', type=str, help='the destination path')
parser.add_argument('--Hex', metavar='path', type=str, help='the hex file path')
parser.add_argument('--KM', type=str, help='Key Manager to be selected for flashing of the hex', choices=['KM_West', 'KM_East'], nargs='?')
parser.add_argument('-v', '--version', action='store_true', help='display the versions')
parser.add_argument('-r', '--read', action='store_true', help='read the code back from the PIC memory')
parser.add_argument('-w', '--write', action='store_true', help='write the hex code in the PIC memory')

args = parser.parse_args()

if args.version:
    showVersion()
if args.read:
    readCode()
if args.write:
    writeCode()
