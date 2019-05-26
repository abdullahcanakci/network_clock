#ifndef SerialController_h
#define SerialController_h

#include <Arduino.h>
#include <SPI.h>

class SerialController
{
public:
    SerialController(int latchPin, int dataPin, int clockPin, int d1, int d2, int d3, int d4);
    void writeBuffer(uint8_t data, uint8_t digit);
    void update();

private:
    void toggleLatch();
    void activateDigit(uint8_t digit);
    int _latchPin;
    int _dataPin;
    int _clockPin;
    uint8_t _displayBuffer[4] = {};
    int _digits[4] = {};
    uint8_t _activeDigit = 0;
};

#endif //SerialController