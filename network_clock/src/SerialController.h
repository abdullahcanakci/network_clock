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
    void toggleLatch();
    int _latchPin;
    int _dataPin;
    int _clockPin;
    uint8_t _displayBuffer[4] = {};
    uint8_t _activeDigit = 0;
    uint8_t _digit[4] = {B10000000, B01000000, B00100000, B00010000};
};

#endif //SerialController