#ifndef USBCDCDEVICE_H
#define USBCDCDEVICE_H

#include <stdint.h>

#include "IUSBDevice.h"

using usbCallbackFunc = void(*)(volatile uint8_t* command, int size, int seqNo);

class USBCDCDevice : public IUSBDevice
{
public:
    void Initialize();

    void Process();

    void Send(uint8_t *data, uint16_t length) override;

    void SetCallback(usbCallbackFunc callback);
};

#endif //USBCDCDEVICE_H
