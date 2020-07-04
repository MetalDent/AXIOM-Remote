# Some examples to run the script:

## To read data from a PIC:

`python3 Remote_UART_Programmer.py --read --chip km_west --port /dev/ttyACM0`

The above command reads data from the Key Manager west and the Bootloader (PIC32) is connected to the host PC on the port /dev/ttyACM0

## To write a HEX file data on a PIC:

`python3 Remote_UART_Programmer.py --write --chip km_west --port /dev/ttyACM0 --hex /path/to/hex`

The above command will write the HEX file to the Key Manager west
