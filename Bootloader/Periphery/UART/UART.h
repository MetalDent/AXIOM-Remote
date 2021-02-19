#ifndef UART_H
#define UART_H

#include <stdint.h>

// NOTE: Using UART2 only at the moment
// NOTE: Removed destructor, as it's not required

using uartCallbackFunc = void(*)(volatile uint8_t* command, uint8_t size, uint8_t seqNo, uint8_t chip);

class UART
{
  public:
    void Initialize(uint32_t baudRate);
    
    explicit UART(uint32_t baudRate);

    void SendText(const uint8_t* message) const;
  
    void SetCallback(uartCallbackFunc callback);
    
    void Process();
    
    void loop_ch(uint8_t ch);
};

#endif // UART_H
