#ifndef SERIALDRIVER_H
#define SERIALDRIVER_H

#include <Arduino.h>
#include <SPI.h>

class SerialDriver{
public:
    SerialDriver(uint8_t dataPin, uint8_t clockPin, uint8_t latchPin);
    void DisplaySetup();
    void WriteDigit(uint8_t data, uint8_t digit);


private:
    void SerialSend(uint8_t address, uint8_t data);
    void SetDisplays();

    uint8_t _dataPin, _clockPin, _latchPin;
    uint8_t _digitCount;
    uint8_t _numberOfChips;
    uint8_t _scanLimit = 1;
    uint8_t _intensity;

    uint8_t _addressNoop = 0;
    uint8_t _addressDecode = 9;
    uint8_t _addressIntensity = 10;
    uint8_t _addressScanLimit = 11;
    uint8_t _addressShutdown = 12;
    uint8_t _addressDisplayTest = 15;

    boolean _shutdown = false;
};

#endif