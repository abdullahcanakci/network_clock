#include "SerialController.h"

SerialController::SerialController(int latchPin, int dataPin, int clockPin, int d1, int d2, int d3, int d4){
    _latchPin = latchPin;
    _dataPin = dataPin;
    _clockPin = clockPin;

    _digits[0] = d1;
    _digits[1] = d2;
    _digits[2] = d3;
    _digits[3] = d4;

    //We need to manually control this.
    SPI.begin();
    pinMode(latchPin, OUTPUT);
    pinMode(d1, OUTPUT);
    pinMode(d2, OUTPUT);
    pinMode(d3, OUTPUT);
    pinMode(d4, OUTPUT);
    digitalWrite(d1, LOW);
    digitalWrite(d2, LOW);
    digitalWrite(d3, LOW);
    digitalWrite(d4, LOW);
}

void SerialController::writeBuffer(uint8_t data, uint8_t digit){
    _displayBuffer[digit] = data;

}

void SerialController::update(){
    SPI.begin();
    SPI.beginTransaction(SPISettings(14000000, LSBFIRST, SPI_MODE0));
    while(true)
    {
        SPI.transfer(_displayBuffer[_activeDigit]);
        activateDigit(_activeDigit);
        toggleLatch();
        delay(250);

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

void SerialController::activateDigit(uint8_t digit){
    for(int i = 0; i < 4; i++){
        if(i == digit){
            digitalWrite(_digits[i], HIGH);
        }else{
            digitalWrite(_digits[i], LOW);
        }
    }
}

