#include "SerialController.h"

//Ticker timeKeeper;
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

void SerialController::toggleDot(){
    _dot = !_dot;
}

void SerialController::update(){
    SPI.begin();
    SPI.beginTransaction(SPISettings(14000000, LSBFIRST, SPI_MODE0));
    toggleLatch();

    SPI.transfer(_digit[_activeDigit]);
    uint8_t temp =_displayBuffer[_activeDigit];
    if((_activeDigit == 1 || _activeDigit == 2) && _dot){
        temp = temp | B00000001;
    }
    SPI.transfer(temp);
    
    _activeDigit = (_activeDigit + 1) % 4;
    
    
    SPI.endTransaction();
    SPI.end();
}

void SerialController::toggleLatch(){
    digitalWrite(_latchPin, LOW);
    digitalWrite(_latchPin, HIGH);
}


