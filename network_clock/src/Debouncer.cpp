#include "Debouncer.h"

void handleButtonInterrupt();

long _stateChangeTime;    
uint8_t _buttonPin;
uint8_t _pressTime;
boolean _buttonPressed = false;

volatile byte interruptCounter = 0;
int numberOfInterrupts = 0;

Debouncer::Debouncer(uint8_t buttonPin){
    _buttonPin = buttonPin;

    pinMode(_buttonPin, INPUT_PULLUP);

    attachInterrupt(digitalPinToInterrupt(_buttonPin), handleButtonInterrupt, FALLING);
}

boolean interrupt = false;

void ICACHE_RAM_ATTR handleButtonInterrupt(){
    noInterrupts();
    if(!interrupt){
        interrupt = true;
        _stateChangeTime = millis();
    }
    long interruptTime = millis();

    if(interruptTime - _stateChangeTime > 150){
        Serial.println("Button press");
        interrupt = false;
    }
    interrupts();
}