#include "USBCDCDevice.h"

#include <xc.h>
#include <sys/attribs.h>

#include "USBDefinitions.h"

inline void uart2_ch(char ch)
{
    while (U2STAbits.UTXBF)
    {
    }

    U2TXREG = ch;
}

inline void uart2_str0(const char* str)
{
    while (*str)
        uart2_ch(*str++);
}

static inline void uart2_hex(uint8_t hex)
{
    hex &= 0xF;
    if (hex > 9)
        uart2_ch(hex + 'A' - 10);
    else
        uart2_ch(hex + '0');
}

static inline void uart2_byte(uint8_t val)
{
    uart2_hex(val >> 4);
    uart2_hex(val);
}

static inline void uart2_word(uint16_t val)
{
    uart2_byte(val >> 8);
    uart2_byte(val);
}

static inline void uart2_long(uint32_t val)
{
    uart2_word(val >> 16);
    uart2_word(val);
}

inline void uart2_str0_nl(const char* str)
{
    while (*str)
    {
        uart2_ch(*str++);
    }

    uart2_ch('\r');
    uart2_ch('\n');
}

static uint8_t usbAddress = 0;

void USBCDCDevice::Initialize()
{
    USBCSR0bits.SOFTCONN = 0; // Disable USB for setup

    usbAddress = 0;       // We haven't been given an address yet
    USBCSR0bits.FUNC = 0; // Initially set the USB address to 0 until later when the host assigns us an address

    USBCSR2bits.RESETIE = 1; // Enable the reset interrupt
    IEC4bits.USBIE = 1;      // Enable the USB interrupt
    IFS4bits.USBIF = 0;      // Clear the USB interrupt flag.
    IPC33bits.USBIP = 7;     // USB Interrupt Priority 7
    IPC33bits.USBIS = 1;     // USB Interrupt Sub-Priority 1

    USBCSR0bits.HSEN = 1; // Enable High Speed (480Mbps) USB mode

    USBCSR0bits.SOFTCONN = 1; // Enable USB
}

void USBCDCDevice::Process()
{
}
void USBCDCDevice::Send(uint8_t* data, uint16_t length)
{
}

inline void SetupEndpoints()
{
    USBE1CSR0bits.MODE = 0; // EP1 is OUT (OUT from host = in to device = RX)
    USBE2CSR0bits.MODE = 0; // EP2 is OUT (for now, will be IN at times)

    // Clear all interrupt flags
    USBCSR0bits.EP0IF = 0;
    USBCSR0bits.EP1TXIF = 0;
    USBCSR0bits.EP2TXIF = 0;
    USBCSR1bits.EP1RXIF = 0;
    USBCSR1bits.EP2RXIF = 0;

    // Set the maximum transfer length for each endpoint
    // Configure endpoint 0 first.
    USBE0CSR0bits.TXMAXP = 16; // Set endpoint 0 buffer size to 16 bytes (can be a maximum of 64 for EP0)

    // And next my custom endpoints
    USBE1CSR0bits.TXMAXP =
        16; // Endpoint 1 - Maximum TX payload / transaction (can be a maximum of 64 for CDC "Interrupt IN endpoint"
    USBE2CSR0bits.TXMAXP = 512; // Endpoint 2 - Maximum TX payload / transaction (512 is the maximum, set to 512)
    USBE2CSR1bits.RXMAXP = 512; // Endpoint 2 - Maximum RX payload / transaction (512 is the maximum, set to 512)

    // Specify which kinds of endpoint we will be using
    USBE1CSR2bits.PROTOCOL = 3; // Endpoint 1 - Interrupt mode
    USBE2CSR2bits.PROTOCOL = 2; // Endpoint 2 TX - Bulk mode
    USBE2CSR3bits.PROTOCOL = 2; // Endpoint 2 RX - Bulk mode

    // Enable DISNYET
    USBE1CSR1bits.PIDERR = 0; // Clear DISNYET to enable NYET replies
    USBE2CSR1bits.PIDERR = 0; // Clear DISNYET to enable NYET replies

    // Set up buffer location for endpoint 1
    USBCSR3bits.ENDPOINT = 1;
    USBFIFOA = 0x08;
    USBIENCSR0bits.CLRDT = 1;

    // Set up buffer locations for endpoint 2
    USBCSR3bits.ENDPOINT = 2;
    USBFIFOA = 0x000A004A;
    USBIENCSR0bits.CLRDT = 1;
    USBE2CSR3bits.TXFIFOSZ = 0x9; // Transmit FIFO Size bits - 512 bytes
    USBE2CSR3bits.RXFIFOSZ = 0x9; // Receive FIFO Size bits - 512 bytes

    // Set maximum size for each packet before splitting occurs
    USBOTGbits.RXFIFOSZ = 0b0110; // 0b0110 = 512 bytes
    USBOTGbits.TXFIFOSZ = 0b0110; // 0b0110 = 512 bytes

    // Set receive endpoint 2 to High Speed
    USBE2CSR3bits.SPEED = 1;

    // Disable Isochronous mode for endpoints 1 and 2
    USBE1CSR0bits.ISO = 0; // Isochronous TX Endpoint Disable bit (Device mode).
    USBE2CSR0bits.ISO = 0; // Isochronous TX Endpoint Disable bit (Device mode).

    // Set up endpoint interrupts
    // Initially clear all the interrupt enables (IE)
    USBCSR1 = 0;
    USBCSR2 = 0;

    USBCSR1bits.EP0IE = 1;   // Endpoint 0 interrupt enable
    USBCSR1bits.EP1TXIE = 1; // Endpoint 1 TX interrupt enable
    USBCSR2bits.EP1RXIE = 1; // Endpoint 1 RX interrupt enable
    USBCSR1bits.EP2TXIE = 1; // Endpoint 2 TX interrupt enable
    USBCSR2bits.EP2RXIE = 1; // Endpoint 2 RX interrupt enable
    USBCSR2bits.RESETIE = 1;
    USBCSR2bits.RESUMEIE = 1;
    USBCSR2bits.SOFIE = 1;

    // Set current endpoint to EP2
    USBCSR3bits.ENDPOINT = 2;
}

uint16_t receivedBytesEP0 = 0;
uint16_t sendBytesEP0 = 0;
uint8_t* fifoBuffer = nullptr;
uint8_t bufferRX[64];
uint8_t bufferTX[64];

uint8_t addressUSB = 0;
bool addressSet = false;

struct USBPacket
{
    uint8_t requestType;
    uint8_t request;
    uint16_t value;
    uint16_t index;
    uint16_t length;
} packet;

inline void ProcessEP0_RX(uint16_t length)
{
    uart2_str0("Length: ");
    uart2_byte(length);
    uart2_str0(" | ");

    fifoBuffer = (uint8_t*)&USBFIFO0;

    for (uint16_t cnt = 0; cnt < length; cnt++)
    {
        bufferRX[cnt] = *(fifoBuffer + (cnt & 3)); // Store the received bytes in EP[0].rx_buffer[]
    }

    USBE0CSR0bits.RXRDYC = 1;
}

#define USB_EP0_WAIT_TIMEOUT 1000

int USB_EP0_Wait_TXRDY()
{
    int timeout;

    timeout = 0;

    while (USBE0CSR0bits.TXRDY)
    {
        timeout++;
        if (timeout > USB_EP0_WAIT_TIMEOUT)
            return 1;
    };

    return 0;
}

void USB_send_EP0()
{
    int cnt;

    fifoBuffer = (uint8_t*)&USBFIFO0;

    if (USB_EP0_Wait_TXRDY())
        return;

    cnt = 0;

    while (cnt < sendBytesEP0)
    {
        *fifoBuffer = bufferTX[cnt]; // Send the bytes
        cnt++;
        // Have we sent 64 bytes?
        if ((cnt > 0) && (cnt % 64 == 0))
        {
            // Set TXRDY and wait for it to be cleared before sending any more bytes
            USBE0CSR0bits.TXRDY = 1;
            if (USB_EP0_Wait_TXRDY())
                return;
        }
    }

    if (cnt % 64 != 0)
    {
        USBE0CSR0bits.TXRDY = 1;
    }
}

void USB_queue_EP0(unsigned char* buffer, int size, int max_size)
{
    int cnt;

    if (max_size < size)
        size = max_size;

    sendBytesEP0 = size;

    for (cnt = 0; cnt < size; cnt++)
        bufferTX[cnt] = buffer[cnt];

    USB_send_EP0();
}

extern "C" void __ISR(_USB_VECTOR, IPL7SRS) USB_handler()
{
    if (USBCSR2bits.RESETIF)
    {
        SetupEndpoints();
        // So when this is set, the USB bus has been reset and we actually need to set up all the USB endpoints now

        // USB_init_endpoints();

        uart2_str0_nl("<RESET>");

        USBCSR2bits.RESETIF = 0;
    }

    // Endpoint 0 Interrupt Handler
    if (USBCSR0bits.EP0IF == 1)
    {
        if (addressSet) // Do we need the set the USB address?
        {
            uart2_str0_nl("<SET_ADDR>");
            USBCSR0bits.FUNC = addressUSB & 0x7F;
            addressSet = false;
        }

        if (USBE0CSR0bits.RXRDY)
        {
            uart2_str0("<RXRDY> | ");
            uint8_t dataLength = USBE0CSR2bits.RXCNT;
            ProcessEP0_RX(dataLength);
            // USB_receive_EP0();

            packet.requestType = bufferRX[0];
            uart2_ch('<');
            uart2_byte(packet.requestType);
            uart2_ch('>');

            packet.request = bufferRX[1];
            uart2_ch('<');
            uart2_byte(packet.request);
            uart2_ch('>');

            packet.value = (uint16_t)(bufferRX[3] << 8) | bufferRX[2];
            uart2_ch('<');
            uart2_word(packet.value);
            uart2_ch('>');

            packet.index = (uint16_t)(bufferRX[5] << 8) | bufferRX[4];
            uart2_ch('<');
            uart2_word(packet.index);
            uart2_ch('>');

            packet.length = (uint16_t)(bufferRX[7] << 8) | bufferRX[6];
            uart2_ch('<');
            uart2_word(packet.length);
            uart2_ch('>');

            uart2_str0_nl("");

            switch (packet.request)
            {
            case 0x5:
                addressUSB = packet.value;
                addressSet = true;
                USBE0CSR0bits.DATAEND = 1;
                USBE0CSR0bits.RXRDYC = 1;
                break;
            case 0x6:
                switch (packet.value >> 8)
                {
                case 0x1: // Get device descriptor
                    uart2_str0_nl("<GET_DESC>");
                    USB_queue_EP0((unsigned char*)&dev_desc, sizeof(dev_desc), packet.length);
                    break;
                }
                // USB_transaction.bmRequestType = EP[0].rx_buffer[0];
                // USB_transaction.bmRequest = EP[0].rx_buffer[1];
                // USB_transaction.wValue = (int)(EP[0].rx_buffer[3] << 8) | EP[0].rx_buffer[2];
                // USB_transaction.wIndex = (int)(EP[0].rx_buffer[5] << 8) | EP[0].rx_buffer[4];
                // USB_transaction.wLength = (int)(EP[0].rx_buffer[7] << 8) | EP[0].rx_buffer[6];

                // USB_EP0_handle_transaction();

                break;
            }

            if (packet.length == 0)
            {
                USBE0CSR0bits.DATAEND = 1; // End of Data Control bit (Device mode)
            }
        }

        if (USBE0CSR0bits.SETEND)
        {
            USBE0CSR0bits.SETENDC = 1;
        }

        uart2_str0_nl("");

        USBCSR0bits.EP0IF = 0; // Clear the USB EndPoint 0 Interrupt Flag.
    }
}

// #include <xc.h>
// #include <sys/attribs.h>

// #include "USBDefinitions.h"

// #define LOOP_SIZE 256
// #define LOOP_MASK (LOOP_SIZE - 1)

// volatile static char loop[LOOP_SIZE];

// volatile static unsigned loop_wi = 0;
// volatile static unsigned loop_ri = 0;

// volatile static int ep0rbc; // EP0 Received Bytes Count
// volatile static int ep2rbc; // EP2 Received Bytes Count

// volatile static int ep0tbc; // EP0 Transmit Byte Count
// volatile static int ep1tbc; // EP1 Transmit Byte Count
// volatile static int ep3tbc; // EP3 Transmit Byte Count

// #define EP3TXBUFFSIZE 8                             // Also in multiples of 8
// volatile static uint8_t ep3data[EP3TXBUFFSIZE * 8]; // USB end point 3 data - USB FIFO size

// #define EP2RXBUFFSIZE 8                             // Also in multiples of 8
// volatile static uint8_t ep2data[EP2RXBUFFSIZE * 8]; // USB end point 2 data - USB FIFO size

// volatile static uint8_t *ep3ptr; // EP3 Transmit Data Pointer

// #define max(a, b) (((a) > (b)) ? (a) : (b))
// #define min(a, b) (((a) < (b)) ? (a) : (b))

// #define MAXPKT 64

// #define EP0TXBUFFSIZE 8 // Multiples of 8, e.g.- 8 (x8) equals a size of 64 bytes.
// #define EP1RXBUFFSIZE 8 // Also in multiples of 8
// //#define EP2RXBUFFSIZE 8 	// Also in multiples of 8

// //#define EP0TXBUFFSIZE 8 	// Multiples of 8, e.g.- 8 (x8) equals a size of 64 bytes.
// //#define EP1RXBUFFSIZE 8 	// Also in multiples of 8
// //#define EP3TXBUFFSIZE 8 	// Also in multiples of 8

// #define uart2_SIZE 4096
// #define uart2_MASK (uart2_SIZE - 1)

// volatile uint8_t *ep0ptr; // EP0 Transmit Data Pointer
// volatile uint8_t *ep1ptr; // EP1 Transmit Data Pointer

// volatile uint8_t ep0data[EP0TXBUFFSIZE * 8]; // USB end point 0 data - USB FIFO size
// volatile uint8_t ep1data[EP1RXBUFFSIZE * 8]; // USB end point 1 data - USB FIFO size

// //volatile char buf[uart2_SIZE];
// //volatile unsigned uart2_wi = 0;
// //volatile unsigned uart2_ri = 0;

// inline void uart2_ch(char ch)
// {
// }

// inline void uart2_str0(const char* str)
// {
// }

// inline void uart2_hex(uint8_t hex)
// {
// }

// inline void uart2_byte(uint8_t val)
// {
// }

// inline void uart2_word(uint16_t val)
// {
// }

// inline void uart2_long(uint32_t val)
// {
// }

// /*void uart2_hex(uint8_t hex)
// {
//     hex &= 0xF;
//     if (hex > 9)
//         uart2_ch(hex + 'A' - 10);
//     else
//         uart2_ch(hex + '0');
// }

// void uart2_byte(uint8_t val)
// {
//     uart2_hex(val >> 4);
//     uart2_hex(val);
// }

// void uart2_word(uint16_t val)
// {
//     uart2_byte(val >> 8);
//     uart2_byte(val);
// }

// void uart2_long(uint32_t val)
// {
//     uart2_word(val >> 16);
//     uart2_word(val);
// }*/

// /*void uart2_str0(const char *str)
// {
//     while (*str)
//         uart2_ch(*str++);
// }*/

// //static inline
// //void uart2_ch(char ch)
// //{
// //    while(U2STAbits.UTXBF);
// //    U2TXREG = ch;
// //}

// /********************************************************************
//  ** String Descriptors						   **
//  *********************************************************************/
// #define STR_DESC(l) (((((l) + 1) << 1) & 0xFF) | 0x0300)

// const unsigned short string0[] = {
//     STR_DESC(1), // bLength + bDescriptorType
//     0x0409       // wLANGID: English (US)
// };

// const unsigned short string1[] = {
//     STR_DESC(11), // bLength + bDescriptorType
//     'a', 'p', 'e', 'r', 't', 'u', 's', '.', 'o', 'r', 'g'};

// const unsigned short string2[] = {
//     STR_DESC(12), // bLength + bDescriptorType
//     'A', 'X', 'I', 'O', 'M', ' ',
//     'R', 'e', 'm', 'o', 't', 'e'};

// const unsigned short string3[] = {
//     STR_DESC(8), // bLength + bDescriptorType
//     'A', 'R', '0', '0', '0', '0', '0', '1'};

// const unsigned short string4[] = {
//     STR_DESC(7), // bLength + bDescriptorType
//     'D', 'e', 'f', 'a', 'u', 'l', 't'};

// const unsigned short string5[] = {
//     STR_DESC(3), // bLength + bDescriptorType
//     'C', 'D', 'C'};

// void loop_ch(char ch)
// {
//     loop[loop_wi++ & LOOP_MASK] = ch;
// }

// enum
// {
//     S_RESET,
//     S_IDLE,
//     S_TXPKT,
//     S_STATUS,
//     S_ZERO,
//     S_RXPKT
// };

// const char *sstr[] = {
//     [S_RESET] = "RE",
//     [S_IDLE] = "ID",
//     [S_TXPKT] = "TX",
//     [S_STATUS] = "ST",
//     [S_ZERO] = "ZE",
//     [S_RXPKT] = "RX"};

// volatile int ep0s;

// volatile uint8_t *ptr;

// volatile uint16_t bmbRequest; // bmRequestType, bRequest

// volatile uint16_t wValue;
// volatile uint16_t wIndex;
// volatile uint16_t wLength;

// volatile uint8_t dev_addr;
// volatile uint8_t usb_conf;

// void usb_ep1_tx(void)
// {
//     volatile uint8_t *ep1fifo;
//     ep1fifo = (uint8_t *)&USBFIFO1; // for now 8 bits at a time.

//     uart2_str0("TX1\n");
//     uart2_ch('[');
//     uart2_word(ep1tbc);
//     uart2_ch(']');

//     if (!usb_conf)
//         return;

//     if (!ep1tbc)
//         return;

//     if (USBE1CSR0bits.TXPKTRDY)
//         return;

//     uart2_ch('|');
//     for (int i = 0; i < MAXPKT; i++)
//     {
//         uart2_byte(*ep1ptr);
//         *ep1fifo = *ep1ptr++; // Load data into FIFO
//         if (!--ep1tbc)
//             break;
//     }
//     uart2_ch('|');

//     USBE1CSR0bits.TXPKTRDY = 1;
// }

// void usb_ep2_rx()
// {
//     ep2rbc = USBE2CSR2bits.RXCNT; // Endpoint 2 - Received Bytes Count

//     uart2_ch('|');
//     ptr = (uint8_t *)&USBFIFO2;
//     for (int i = 0; i < ep2rbc; i++)
//     {
//         ep2data[i] = ptr[i & 3];
//         uart2_byte(ep2data[i]);
//     }
//     uart2_ch('|');

//     uart2_str0("RX2\n");
//     USBE2CSR1bits.RXPKTRDY = 0; // Data has been unloaded
// }

// void usb_ep3_tx(void)
// {
//     volatile uint8_t *ep3fifo;
//     ep3fifo = (uint8_t *)&USBFIFO3; // for now 8 bits at a time.

//     uart2_str0("TX3\n");
//     uart2_ch('[');
//     uart2_word(ep3tbc);
//     uart2_ch(']');

//     if (!usb_conf)
//         return;

//     if (!ep3tbc)
//         return;

//     if (USBE3CSR0bits.TXPKTRDY)
//         return;

//     uart2_ch('|');
//     for (int i = 0; i < MAXPKT; i++)
//     {
//         uart2_byte(*ep3ptr);
//         *ep3fifo = *ep3ptr++; // Load data into FIFO
//         if (!--ep3tbc)
//             break;
//     }
//     uart2_ch('|');

//     USBE3CSR0bits.TXPKTRDY = 1;
// }

// #define USB_EP0_TX(s)            \
//     do                           \
//     {                            \
//         ep0ptr = (uint8_t *)(s); \
//         ep0tbc = *ep0ptr;        \
//     } while (0)

// #define USB_EP0_TXL(s, l)        \
//     do                           \
//     {                            \
//         ep0ptr = (uint8_t *)(s); \
//         ep0tbc = (l);            \
//     } while (0)

// #define USB_EP3_TXL(s, l)        \
//     do                           \
//     {                            \
//         ep3ptr = (uint8_t *)(s); \
//         ep3tbc = (l);            \
//     } while (0)

// int usb_ep0_ctrl_read(void)
// {
//     int ret = 0;

//     uart2_str0("\nCR:");
//     uart2_word(bmbRequest);
//     uart2_ch(',');
//     uart2_word(wValue);
//     uart2_ch(',');
//     uart2_word(wIndex);
//     uart2_ch(',');
//     uart2_word(wLength);

//     switch (bmbRequest)
//     {
//     case 0x8006: // Get Descriptor(s)
//         uart2_str0("[GET_DESC]");

//         switch (ep0data[3])
//         {
//         case 0x00: // Undefined
//             break;

//         case 0x01: // Device Descriptor
//             uart2_str0("[DEVICE]");

//             USB_EP0_TXL(dev_desc, min(wLength, sizeof(dev_desc)));
//             ret = 1;
//             break;

//         case 0x02: // Configuration Descriptor
//             uart2_str0("[CONFIG]");

//             USB_EP0_TXL(conf_desc, min(wLength, TOTAL_LEN));
//             ret = 1;
//             break;

//         case 0x03: // String Descriptors
//             uart2_str0("[STRING]");

//             switch (ep0data[2])
//             {
//             case 0x00: // Language ID
//                 USB_EP0_TX(string0);
//                 ret = 1;
//                 break;

//             case 0x01: // Manufacturer
//                 USB_EP0_TX(string1);
//                 ret = 1;
//                 break;

//             case 0x02: // Product
//                 USB_EP0_TX(string2);
//                 ret = 1;
//                 break;

//             case 0x03: // Serial
//                 USB_EP0_TX(string3);
//                 ret = 1;
//                 break;

//             case 0x04: // Configuration
//                 USB_EP0_TX(string4);
//                 ret = 1;
//                 break;

//             case 0x05: // Interface
//                 USB_EP0_TX(string5);
//                 ret = 1;
//                 break;

//             default:
//                 break;
//             } // End of switch ep0data[2]
//             break;

//         default:
//             break;

//         } // End of switch ep0data[3].
//         break;

//     default:
//         uart2_str0("[UNKNOWN]");
//         break;
//     } // End of switch bmbRequest.

//     uart2_ch(';');
//     uart2_word(ep0tbc);
//     uart2_ch('#');
//     uart2_hex(ret);
//     uart2_ch('\r');
//     uart2_ch('\n');

//     return ret;
// }

// int usb_ep0_ctrl_write(void)
// {
//     int ret = 0;

//     uart2_str0("\nCW:");
//     uart2_word(bmbRequest);
//     uart2_ch(',');
//     uart2_word(wValue);
//     uart2_ch(',');
//     uart2_word(wIndex);
//     uart2_ch(',');
//     uart2_word(wLength);
//     uart2_ch(' ');

//     switch (bmbRequest)
//     {
//     case 0x0008: // Get Configuration
//         uart2_str0("[GET_CONFIGURATION]");
//         ret = 1;
//         break;

//     case 0x2120: // CDC Set Line Coding
//         uart2_str0("[CDC_SET_LINE_CODING]");
//         ret = 1;
//         break;

//     default:
//         uart2_str0("[UNKNOWN]");
//         break;
//     }

//     uart2_ch('#');
//     uart2_hex(ret);
//     uart2_ch('\r');
//     uart2_ch('\n');

//     return ret;
// }

// int usb_ep0_zero(void)
// {
//     int ret = 0;

//     uart2_str0("\nCZ:");
//     uart2_word(bmbRequest);
//     uart2_ch(',');
//     uart2_word(wValue);
//     uart2_ch(',');
//     uart2_word(wIndex);
//     uart2_ch(' ');

//     switch (bmbRequest)
//     {
//     case 0x0001: // Clear Feature
//         uart2_str0("[CLEAR_FEATURE]");
//         ret = 1;
//         break;

//     case 0x0003: // Set Feature
//         uart2_str0("[SET_FEATURE]");
//         ret = 1;
//         break;

//     case 0x0005: // Save Address
//         uart2_str0("[SAVE_ADDRESS]");
//         dev_addr = wValue & 0xFF;
//         ret = 1;
//         break;

//     case 0x0009: // Set Configuration
//         uart2_str0("[SET_CONFIGURATION]");
//         usb_conf = wValue & 0xFF;
//         ret = 1;
//         break;

//     case 0x2122: // CDC Set Control Line State
//         uart2_str0("[CDC_SET_CONTROL_LINE_STATE]");
//         ret = 1;
//         break;

//     default:
//         uart2_str0("[UNKNOWN]");
//         break;
//     }

//     uart2_ch('#');
//     uart2_hex(ret);
//     uart2_ch('\r');
//     uart2_ch('\n');

//     return ret;
// }

// #define EP0_STATE(s)          \
//     do                        \
//     {                         \
//         int next = (s);       \
//         uart2_ch('{');          \
//         uart2_str0(sstr[ep0s]); \
//         uart2_str0("->");       \
//         uart2_str0(sstr[next]); \
//         uart2_ch('}');          \
//         ep0s = next;          \
//     } while (0)

// int usb_ep0_tx(void)
// {
//     volatile uint8_t *ep0fifo;
//     ep0fifo = (uint8_t *)&USBFIFO0; // for now 8 bits at a time.

//     uart2_ch('[');
//     uart2_word(ep0tbc);
//     uart2_ch(']');

//     // uart2_ch('|');
//     for (int i = 0; i < MAXPKT; i++)
//     {
//         // uart2_byte(*ep0ptr);
//         *ep0fifo = *ep0ptr++; // Load data into FIFO
//         if (!--ep0tbc)
//             break;
//     }
//     // uart2_ch('|');

//     if (ep0tbc)
//     { // more to come
//         uart2_ch('+');
//         USBE0CSR0bits.TXRDY = 1;
//         return S_TXPKT;
//     }
//     else
//     { // last packet
//         uart2_ch('/');
//         USBE0CSR0 |= _USBE0CSR0_TXRDY_MASK | _USBE0CSR0_DATAEND_MASK;

//         // USBE0CSR0bits.TXRDY = 1;
//         // USBE0CSR0bits.DATAEND = 1;
//         return S_STATUS;
//     }
// }

// void usb_ep0_addr(void)
// {
//     if (dev_addr && !USBCSR0bits.FUNC)
//     {
//         uart2_str0("ADDR=");
//         uart2_word(dev_addr);
//         uart2_ch('\r');
//         uart2_ch('\n');

//         USBCSR0bits.FUNC = dev_addr & 0x7F;
//     }
// }

// void usb_ep0_stall(void)
// {
//     uart2_str0("STALL");
//     USBE0CSR0bits.STALL = 1;
// }

// void usb_ep0_flush(void)
// {
//     uart2_str0("FLUSH");
//     USBE0CSR0bits.FLUSH = 1;
// }

// void usb_ep0(void)
// {
//     uart2_ch('(');
//     uart2_word(USBE0CSR0 >> 16);
//     uart2_ch(')');

//     if (ep0s == S_RESET)
//         EP0_STATE(S_IDLE);

//     if (USBE0CSR0bits.SETEND)
//     {
//         USBE0CSR0bits.SETENDC = 1;
//         uart2_str0("SETEND-");
//         if (ep0s != S_ZERO)
//             EP0_STATE(S_IDLE);
//     }

//     if (USBE0CSR0bits.STALLED)
//     {
//         uart2_str0("STALLED");
//         USBE0CSR0bits.STALLED = 0;
//         return;
//     }

//     switch (ep0s)
//     {
//     case S_TXPKT: // Transmit Packet
//         EP0_STATE(usb_ep0_tx());
//         return;

//     case S_ZERO:   // Zero Status
//     case S_STATUS: // Status
//         usb_ep0_addr();
//         EP0_STATE(S_IDLE);
//         return;
//     }

//     if (!USBE0CSR0bits.RXRDY)
//     {
//         uart2_ch('!');
//         return;
//     }

//     ep0rbc = USBE0CSR2bits.RXCNT; // Received Bytes Count

//     if (ep0rbc > MAXPKT)
//     {
//         usb_ep0_stall();
//         EP0_STATE(S_IDLE);
//         return;
//     }

//     uart2_ch('|');
//     ptr = (uint8_t *)&USBFIFO0;
//     for (int i = 0; i < ep0rbc; i++)
//     {
//         ep0data[i] = ptr[i & 3];
//         uart2_byte(ep0data[i]);
//     }
//     uart2_ch('|');

//     switch (ep0s)
//     {
//     case S_IDLE: // Idle State
//         if (ep0rbc != 8)
//         {
//             usb_ep0_flush();
//             return;
//         }

//         bmbRequest = (ep0data[0] << 8) | ep0data[1];
//         wValue = (ep0data[3] << 8) | ep0data[2];
//         wIndex = (ep0data[5] << 8) | ep0data[4];
//         wLength = (ep0data[7] << 8) | ep0data[6];

//         if (wLength == 0)
//         {
//             if (usb_ep0_zero())
//             {
//                 USBE0CSR0 |= _USBE0CSR0_RXRDYC_MASK | _USBE0CSR0_DATAEND_MASK;

//                 // USBE0CSR0bits.RXRDYC = 1;
//                 // USBE0CSR0bits.DATAEND = 1;
//                 EP0_STATE(S_ZERO);
//                 return;
//             }
//             else
//             {
//                 usb_ep0_stall();
//                 return;
//             }
//         }

//         if (bmbRequest & 0x8000)
//         { // control read
//             if (usb_ep0_ctrl_read())
//             {
//                 USBE0CSR0bits.RXRDYC = 1;
//                 EP0_STATE(usb_ep0_tx());
//             }
//             else
//             {
//                 usb_ep0_stall();
//             }
//         }
//         else
//         { // control write
//             USBE0CSR0bits.RXRDYC = 1;
//             EP0_STATE(S_RXPKT);
//         }
//         break;

//     case S_RXPKT: // Receive Packet

//         /* TODO: handle multiple packets */

//         usb_ep0_ctrl_write();
//         USBE0CSR0 |= _USBE0CSR0_RXRDYC_MASK | _USBE0CSR0_DATAEND_MASK;

//         // USBE0CSR0bits.RXRDYC = 1;
//         // USBE0CSR0bits.DATAEND = 1;
//         EP0_STATE(S_IDLE);
//         break;
//     }
// }

// void usb_reset_ep0(void)
// {
//     USBCSR3bits.ENDPOINT = 0;
//     USBE0CSR0 = 0;
//     USBFIFOAbits.RXFIFOAD = 0x00;
//     USBFIFOAbits.TXFIFOAD = 0x08;
//     USBOTGbits.TXFIFOSZ = 0x9;
//     USBOTGbits.RXFIFOSZ = 0x9;
//     USBCSR1bits.EP0IE = 1;
// }

// void usb_reset_ep1(void)
// {
//     USBE3CSR0bits.MODE = 1;
//     USBE3CSR0bits.TXMAXP = EP3TXBUFFSIZE;
//     USBE3CSR2bits.PROTOCOL = 3;

//     USBCSR3bits.ENDPOINT = 1;
//     USBFIFOAbits.TXFIFOAD = 0x010;
//     USBCSR3bits.ENDPOINT = 0;
//     USBE3CSR3bits.TXFIFOSZ = 0x6;

//     USBE3CSR2bits.SPEED = 1;
//     USBE3CSR0bits.ISO = 0;
//     USBCSR1bits.EP3TXIE = 1;
// }

// void usb_reset_ep2(void)
// {
//     USBE2CSR0bits.MODE = 0;
//     USBE2CSR1bits.RXMAXP = EP2RXBUFFSIZE;
//     USBE2CSR3bits.PROTOCOL = 2;

//     USBCSR3bits.ENDPOINT = 2;
//     USBFIFOAbits.RXFIFOAD = 0x020;
//     USBCSR3bits.ENDPOINT = 0;
//     USBE2CSR3bits.RXFIFOSZ = 0x9;

//     USBE2CSR3bits.SPEED = 1;
//     USBE2CSR1bits.ISO = 0;
//     USBCSR2bits.EP2RXIE = 1;
// }

// void usb_reset_ep3(void)
// {
//     USBE3CSR0bits.MODE = 1;
//     USBE3CSR0bits.TXMAXP = EP3TXBUFFSIZE;
//     USBE3CSR2bits.PROTOCOL = 2;

//     USBCSR3bits.ENDPOINT = 3;
//     USBFIFOAbits.TXFIFOAD = 0x030;
//     USBCSR3bits.ENDPOINT = 0;
//     USBE3CSR3bits.TXFIFOSZ = 0x9;

//     USBE3CSR2bits.SPEED = 1;
//     USBE3CSR0bits.ISO = 0;
//     USBCSR1bits.EP3TXIE = 1;
// }

// void usb_reset(void)
// {
//     uart2_str0("RESET");

//     dev_addr = 0;
//     usb_conf = 0;

//     USBCSR0bits.FUNC = 0;
//     usb_reset_ep0();
//     usb_reset_ep1();
//     usb_reset_ep2();
//     usb_reset_ep3();

//     EP0_STATE(S_RESET);
// }

// extern "C" void __ISR(_USB_VECTOR, IPL7SRS) USB_ISR(void)
// {
//     uint32_t csr0 = USBCSR0;
//     uint32_t csr1 = USBCSR1;
//     uint32_t csr2 = USBCSR2;
//     // IFS4CLR = _IFS4_USBIF_MASK;		// Reset the USB Interrupt flag

//     uart2_ch('<');
//     uart2_word(csr0 >> 8);
//     uart2_ch(':');
//     uart2_byte(csr1);
//     uart2_ch(':');
//     uart2_byte(csr2 >> 16);
//     uart2_ch('.');

//     /* Endpoint 0 Interrupt Handler */
//     if (csr0 & _USBCSR0_EP0IF_MASK)
//     {
//         usb_ep0();
//     }

//     /* Endpoint 1 TX Interrupt Handler */
//     if (csr0 & _USBCSR0_EP1TXIF_MASK)
//     { // Endpoint 1 Transmit.
//         usb_ep1_tx();
//     }

//     /* Endpoint 2 RX Interrupt Handler */
//     if (csr1 & _USBCSR1_EP2RXIF_MASK)
//     { // Endpoint 2 Receive
//         usb_ep2_rx();
//         uart2_ch('*');
//         uart2_word(ep2rbc);
//         uart2_ch('*');
//         /* for (int i=0; i<ep2rbc; i++)
//             loop_ch(ep2data[i]); */
//         loop_ch(ep2data[0]);
//     }

//     /* Endpoint 3 TX Interrupt Handler */
//     if (csr0 & _USBCSR0_EP3TXIF_MASK)
//     { // Endpoint 3 Transmit
//         usb_ep3_tx();
//     }

//     if (csr2 & _USBCSR2_RESETIF_MASK)
//     { // Bus Reset
//         usb_reset();
//     }

//     uart2_ch('>');
//     uart2_ch('\r');
//     uart2_ch('\n');

//     IFS4CLR = _IFS4_USBIF_MASK; // Reset the USB Interrupt flag
// }

// void USBCDCDevice::Initialize()
// {
//     USBCSR0bits.SOFTCONN = 0; // USB D+/D- lines are disabled and tristated

//     USBCSR2bits.RESETIE = 1;  // ?? When enabled IN packets/Set Address aren't received (unknown reason).
//     USBCSR2bits.RESUMEIE = 0; // ?? When enabled IN packets/Set Address aren't received (unknown reason).

//     USBCSR0bits.EP0IF = 0;   // clear interrupt
//     USBCSR1bits.EP1RXIF = 0; // clear interrupt
//     USBCSR0bits.EP2TXIF = 0; // clear interrupt
//     USBCSR1bits.EP3RXIF = 0; // clear interrupt

//     IPC33bits.USBIP = 7; // USB Interrupt Priority.
//     IPC33bits.USBIS = 1; // USB Interrupt Sub-Priority.

//     USBCSR0bits.FUNC = 0; // USB address = 0

//     IFS4bits.USBIF = 0; // Clear the USB interrupt flag.
//     IEC4bits.USBIE = 1; // Enable the USB interrupt.

//     USBCSR0bits.HSEN = 1;     // 1 = Enable High Speed (480Mbps) USB mode.
//     USBCSR0bits.SOFTCONN = 1; // USB D+/D- lines are enabled and active
// }

// void USBCDCDevice::Process()
// {
//     if (!ep3tbc)
//     {
//         unsigned cnt = 0;

//         while ((loop_ri != loop_wi) && (cnt < 16))
//             ep3data[cnt++] = loop[loop_ri++ & LOOP_MASK] + 1;

//         if (cnt)
//         {
//             ep3tbc = cnt;
//             ep3ptr = ep3data;
//             usb_ep3_tx();
//         }
//     }
// }

// void USBCDCDevice::Send(uint8_t *data, uint16_t length)
// {
//     ep3tbc = length;
//     ep3ptr = data;
//     usb_ep3_tx();
// }
