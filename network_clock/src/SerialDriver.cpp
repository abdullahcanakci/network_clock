#include "SerialDriver.h"

SerialDriver::SerialDriver(uint8_t dataPin, uint8_t clockPin, uint8_t latchPin){
    _dataPin = dataPin;
    _clockPin = clockPin;
    _latchPin = latchPin;
    pinMode(_dataPin, OUTPUT);
    pinMode(_clockPin, OUTPUT);
    pinMode(_latchPin, OUTPUT);

    digitalWrite(_latchPin, HIGH);
}

void SerialDriver::DisplaySetup(){
    SetDisplays();
}
void SerialDriver::SetDisplays(){
    
    SerialSend(_addressShutdown, 1);
    //SerialSend(_addressDisplayTest, 1);
    SerialSend(_addressDisplayTest, 0);
    SerialSend(_addressScanLimit, 4);

    SerialSend(_addressDecode, 0x00);
    SerialSend(_addressIntensity, 8);

}

void SerialDriver::WriteDigit(uint8_t data, uint8_t digit){
    SerialSend(digit+1, data);
}

void SerialDriver::SerialSend(uint8_t regAddress, uint8_t data){
    SPI.begin();
    SPI.beginTransaction(SPISettings(14000000, MSBFIRST, SPI_MODE0));
    digitalWrite(_latchPin, LOW);
    // First send data
    
    SPI.transfer(regAddress);
    SPI.transfer(data);
    // Then send address    
    digitalWrite(_latchPin, HIGH);
    
    //EndTransaction
    SPI.end();
    SPI.endTransaction();

}