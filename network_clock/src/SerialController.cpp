#include "SerialController.h"

SerialController::SerialController(int latchPin, int dataPin, int clockPin){
    _latchPin = latchPin;
    _dataPin = dataPin;
    _clockPin = clockPin;

    //We need to manually control this.
    SPI.begin();
    pinMode(latchPin, OUTPUT);
}

void SerialController::writeBuffer(uint8_t data, uint8_t digit){
    _displayBuffer[digit] = data;

}

void SerialController::update(){
    SPI.begin();
    SPI.beginTransaction(SPISettings(14000000, LSBFIRST, SPI_MODE0));
    while(true)
    {
        SPI.transfer(_digit[_activeDigit]);
        SPI.transfer(_displayBuffer[_activeDigit]);
        delay(1);
        toggleLatch();

        _activeDigit = (_activeDigit + 1);
        if(_activeDigit >= 4){
            _activeDigit = 0;
        }
    }
    SPI.endTransaction();
    SPI.end();
}

void SerialController::toggleLatch(){
    digitalWrite(_latchPin, LOW);
    digitalWrite(_latchPin, HIGH);
}


