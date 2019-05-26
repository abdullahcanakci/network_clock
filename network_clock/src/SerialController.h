#ifndef SerialController_h
#define SerialController_h

#include <Arduino.h>
#include <SPI.h>

class SerialController
{
public:
    SerialController(int latchPin, int dataPin, int clockPin);
    void writeBuffer(uint8_t data, uint8_t digit);
    void update();

private:
    static int _latchPin;
    static int _dataPin;
    static int _clockPin;
    uint8_t _displayBuffer[4];
    uint8_t _activeDigit = 0;
};

#endif //SerialController