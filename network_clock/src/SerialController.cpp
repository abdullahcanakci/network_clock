#include "SerialController.h"

SerialController::SerialController(int latchPin, int dataPin, int clockPin){
    _latchPin = latchPin;
    _dataPin = dataPin;
    _clockPin = clockPin;

    //We need to manually control this.
    pinMode(latchPin, OUTPUT);
}

void SerialController::writeBuffer(uint8_t data, uint8_t digit){
    _displayBuffer[digit] = data;

}

void SerialController::update(){
    SPI.beginTransaction(SPISettings(14000000, LSBFIRST, SPI_MODE0));
    SPI.transfer(_displayBuffer[_activeDigit]);
    SPI.endTransaction();
    _activeDigit = (_activeDigit + 1) % 4;
}

