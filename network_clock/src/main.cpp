#include <Arduino.h>
#include <SPI.h>
#include <SerialController.h>


// create a global shift register object
// parameters: (number of shift registers, data pin, clock pin, latch pin)
int dataPin = 13, clockPin = 14, latchPin = 15;
int d1 = 5, d2 = 4, d3 = 2, d4 = 16;


SerialController sc(latchPin, dataPin, clockPin, d1,d2,d3,d4);
 
// Num table
uint8_t numTable[] = {
  B11111100,
  B01100000,
  B11011010,
  B11110010,
  B01100110,
  B10110110,
  B00111110,
  B11100000,
  B11111110,
  B11110110,
};

void setup() {
    Serial.begin(9600);
    sc.writeBuffer(numTable[2], 0);
    sc.writeBuffer(numTable[4], 1);
    sc.writeBuffer(numTable[6], 2);
    sc.writeBuffer(numTable[8], 3);

}

void loop() {
  sc.update();
}

