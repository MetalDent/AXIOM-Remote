#include "UART.h"

#include <xc.h>
#include <sys/attribs.h>

#include "../Common/Definitions.h"

UART::UART(uint32_t baudRate)
{
    Initialize(baudRate);
}

void UART::Initialize(uint32_t baudRate)
{
    U2MODEbits.ON = 0;

    TRISEbits.TRISE8 = 0; // U2TX out
    TRISEbits.TRISE9 = 1; // U2RX in
    ANSELEbits.ANSE8 = 0; // digital
    ANSELEbits.ANSE9 = 0; // digital

    CFGCONbits.IOLOCK = 0;
    RPE8Rbits.RPE8R = 0b0010; // U2TX
    U2RXRbits.U2RXR = 0b1101; // RPE9
    CFGCONbits.IOLOCK = 1;

    IPC36bits.U2TXIP = 0b000; //! Interrupt priority of 7
    IPC36bits.U2TXIS = 0b00;  //! Interrupt sub-priority of 0
    IPC36bits.U2RXIP = 0b111; //! Interrupt priority of 7
    IPC36bits.U2RXIS = 0b00;  //! Interrupt sub-priority of 0

    IEC4SET = _IEC4_U2RXIE_MASK; //! Rx INT Enable
    IFS4bits.U2TXIF = 0;         //! Clear Tx flag
    IFS4bits.U2RXIF = 0;         //! Clear Rx flag
    
    IEC4bits.U2TXIE = 1;         //! Enable Tx flag
    IPC36bits.U2TXIP = 1;
    IEC4bits.U2RXIE = 1;         //! Enable Rx flag
    IPC36bits.U2RXIP = 1;   
    

    // U2BRG = 24; // 1MBaud @ 50MHz
    // U2BRG = 51; // 115200Baud @ 192MHz
    uint32_t peripheralBusClock = (CPU_FREQ / 2);
    U2BRG = peripheralBusClock / (4 * baudRate) - 1;
    U2STA = 0;

    U2MODEbits.BRGH = 1;
    U2MODEbits.PDSEL = 0b00;
    U2MODEbits.STSEL = 0;
    U2MODEbits.UEN = 0b00;
    U2MODEbits.ON = 1;
    U2STASET = 0x9400; // Enable Transmit and Receive
}

inline void uart2_ch(char ch)
{
    while (U2STAbits.UTXBF)
    {
    }

    U2TXREG = ch;
}

inline void uart2_str0(const uint8_t* str)
{
    while (*str)
        uart2_ch(*str++);
}

void UART::SendText(const uint8_t* message) const
{
    uart2_str0(message);
    uart2_ch('\r');
    uart2_ch('\n');
}

uint8_t cmd[50];
uint8_t size = 0;
uint8_t j = 0;
uint8_t seqNo = 48;
uint8_t chip;
uint8_t data[8];
uartCallbackFunc uartCallback = nullptr;

void UART::SetCallback(uartCallbackFunc callback)
{
    uartCallback = callback;
}

//TODO: remove or modify after initial tests
#define FIFO_SIZE 64
uint8_t pbBuffer[FIFO_SIZE];
uint8_t *pbPut;
void putString(char ch)
{
    *pbPut = ch;
    pbPut++;

    if (pbPut >= (pbBuffer + FIFO_SIZE))
        pbPut = pbBuffer;
}

extern "C" void __ISR(_UART2_RX_VECTOR, IPL7SRS) UART2_ISR(void)
{
    char ch;  
    while (U2STAbits.URXDA)
    {
        ch = U2RXREG;

        // only use the data if there was no error
        putString(ch);
    }    

    IFS4bits.U2RXIF = 0;        // clear rx interrupt flag 
    uartCallback(cmd, size, seqNo, chip);
}

void UART::Process()
{
    while (U2STAbits.URXDA) {	// process buffer
        char ch = U2RXREG;
        uart2_ch(ch);	// echo back   
    }
}
