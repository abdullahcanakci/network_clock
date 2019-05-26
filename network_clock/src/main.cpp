#include <Arduino.h>
#include <SPI.h>

// create a global shift register object
// parameters: (number of shift registers, data pin, clock pin, latch pin)
int dataPin = 13, clockPin = 14, latchPin = 15;
uint8_t LED_Pin = D3; 



 
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
  // put your setup code here, to run once:
    Serial.begin(9600);
    pinMode(LED_Pin, OUTPUT);   // Initialize the LED pin as an output
    //pinMode(dataPin, OUTPUT);
    //pinMode(clockPin, OUTPUT);
    SPI.begin();
    SPI.beginTransaction(SPISettings(14000000, LSBFIRST, SPI_MODE0));
    pinMode(latchPin, OUTPUT);

}

void loop() {
  // put your main code here, to run repeatedly:
  //SPI.beginTransaction(SPISettings(14000000, LSBFIRST, SPI_MODE0));
  int i;
  for(i = 0; i< 10; i = i+1){
    //shiftOut(dataPin, clockPin, MSBFIRST, numTable[i]);
 
    SPI.transfer(numTable[i]);
    Serial.print(""+ i);
    digitalWrite(latchPin, HIGH);
    digitalWrite(latchPin, LOW); 


    delay(250);
  }
  digitalWrite(LED_Pin, LOW); // Turn the LED on
  delay(1000);                // Wait for a second
  digitalWrite(LED_Pin, HIGH);// Turn the LED off
  delay(1000);                // Wait for a second
}