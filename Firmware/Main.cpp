// Configuration
#include "../Bootloader/Configuration/PIC32Config.h"

#include <xc.h>
#include <sys/attribs.h>
#include <stdio.h>
#include <string.h>

// Periphery
#include "../Bootloader/Periphery/UART/UART.h"
#include "../Bootloader/Periphery/USB/USBCDCDevice.h"
#include "../Bootloader/Periphery/ICSP/ICSProgrammer.h"

#include "CRC8.cpp"

//USBCDCDevice cdcDevice;
UART uart(115200);
//ICSProgrammer icsp;
CRC8 getCRC;
char outputBuffer[128];
uint8_t remoteSequenceNo = (uint8_t)'\x30';

/*
void GetKMData()
{
    icsp.EnterLVP();
    icsp.SendCommand(ICSPCommand::LoadConfiguration, 1);
    uint16_t address = 0x8000;

    for (int i = 0; i < 30; i++)
    {
        uint16_t value = icsp.Receive();
        sprintf(outputBuffer, "Address: 0x%.8X, value/dec: %u, value/hex: 0x%X\r\n", address + i, value, value);
        cdcDevice.Send((uint8_t*)outputBuffer, strlen(outputBuffer));
        icsp.SendCommand(ICSPCommand::IncrementAddress);
    }
    
    icsp.SendCommand(ICSPCommand::ResetAddress);
    uint16_t count = 0;
    while (count <= 0x3FFF)
    {
        uint16_t value = icsp.Receive();
        sprintf(outputBuffer, "0x%.8X: %d 0x%X\r\n", count, value, value);
        cdcDevice.Send((uint8_t*)outputBuffer, strlen(outputBuffer));
        icsp.SendCommand(ICSPCommand::IncrementAddress);

        count++;
    }
    
    icsp.ExitLVP();
}

void WriteData()
{
    cdcDevice.Send((uint8_t*)"In write", 8);
}

void SelectChip(uint8_t chip, uint8_t mode)
{
    if (chip == 101)
    {
        icsp.SelectKM(KeyManager::East);
            
        if (mode == 114)
            GetKMData();
        else if (mode == 119)
            WriteData();
    }
        
    else if (chip == 119)
    {
        icsp.SelectKM(KeyManager::West);
        
        if (mode == 114)
            GetKMData();
        else if (mode == 119)
            WriteData();
    }    
}

void GetInfo(volatile uint8_t* command, uint8_t i, uint8_t seqNo, uint8_t* fields, uint8_t size, uint8_t chip)
{
    uint8_t operation;
    uint8_t sequenceNumber;
    uint8_t crc;
    uint8_t* ack;
    uint8_t remoteCRC;
    uint8_t* remoteOperation = (uint8_t*)"\x52";
    
    operation = command[0];
    sequenceNumber = command[1];
    crc = command[2];
    
    uint8_t check_crc = getCRC.GetCRC(fields, size - i);
    
    if (((int)sequenceNumber == seqNo) && (crc == check_crc))
    {
        ack = (uint8_t*)"\x1e\x41\x43\x4b\x04";
        remoteCRC = getCRC.GetCRC(ack, 5);
        remoteSequenceNo++;
        
        sprintf(outputBuffer, "%s%d%d%s\r\n", remoteOperation, remoteSequenceNo, remoteCRC, ack);
        cdcDevice.Send((uint8_t*)outputBuffer, strlen(outputBuffer));
            
        if (sequenceNumber == 50)
        {
            SelectChip(chip, command[4]);
        }
    }
}

void ProcessCommand(volatile uint8_t* command, uint8_t size, uint8_t seqNo, uint8_t chip)
{
    if (seqNo < 51)
    {
        uint8_t i = 0;
        uint8_t* fields;
        
        while (command[i] != '\x1e')
        {
            i++;
        }

        uint8_t* RS = (uint8_t*)&command[i];
        fields = (uint8_t*)strstr((char*)command, (const char*)RS);
        
        GetInfo(command, i, seqNo, fields, size, chip);
    }
    
    else
    {
        cdcDevice.Send((uint8_t*)command, size);
    }
}
*/
__attribute__(( weak )) void UARTCallback(volatile uint8_t* command, uint8_t size, uint8_t seqNo, uint8_t chip)
{
    //ProcessCommand(command, size, seqNo, chip);
    
    //uart.SendText((const uint8_t*)command);
    uart.SendText((uint8_t*)"Test");
}

int main()
{
    uart.Initialize(115200);
    uart.SetCallback(&UARTCallback);

    while (1)
    {
        uart.Process();
    }

    return 0;
}
