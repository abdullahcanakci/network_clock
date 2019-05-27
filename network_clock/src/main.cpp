#include <Arduino.h>
#include <SerialController.h>
#include <Ticker.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <RTCLib.h>

// ------------ PROTOTYPES ------------

void sendNTPpacket(IPAddress& address);
void updateDisplayBuffer();
void updateDisplayDot();
void updateDisplay();
void updateClock();
void activateTickerInts();
void deactivateTickerInts();
void getClock();
void parseClock(unsigned long epoch);
void getUDPPacket();

// create a global shift register object
// parameters: (number of shift registers, data pin, clock pin, latch pin)
int dataPin = 13, clockPin = 14, latchPin = 15;

#ifndef STASSID
#define STASSID "Ithilien"
#define STAPSK "147596328"
#endif



// ------------ NETWORK ---------------
const char * ssid = STASSID;
const char * pass = STAPSK;

unsigned int localPort = 2390;

IPAddress timeServerIP;
const char * ntpServerName = "time.nist.gov";

const int NTP_PACKET_SIZE = 48;
byte packetBuffer[NTP_PACKET_SIZE];

WiFiUDP udp;

// ------------ OBJECTS ------------

SerialController sc(latchPin, dataPin, clockPin);
RTC_Millis milliClock;


// ------------ TIMERS -------------

Ticker timeKeeper;
Ticker networkKeeper;
Ticker dotKeeper;
Ticker displayKeeper;

// ------------ VARIABLES ------------

boolean packetSent = false;
int gmtOffset = 3;
int timeToTry = 10; //Times to read UDP packet before sending another one
 
// ------------ NUM REF TABLE ---------------
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
  Serial.begin(115200);

  Serial.println();
  Serial.println();

  milliClock.begin(DateTime(F(__DATE__), F(__TIME__)));

  Serial.print("Connecting to:");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
  while(WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print(".");
  }
  Serial.println("");

  Serial.println("Starting UDP");
  udp.begin(localPort);
  Serial.print("Local port: ");
  Serial.print(udp.localPort());

  sc.writeBuffer(numTable[2], 0);
  sc.writeBuffer(numTable[4], 0);
  sc.writeBuffer(numTable[6], 0);
  sc.writeBuffer(numTable[8], 0);
  delay(1000);
  updateClock();

}

void updateDisplay(){
  sc.update();
}

void updateDisplayDot(){
  sc.toggleDot();
}

void updateDisplayBuffer(){
  int dig0 = 0, dig1 = 0, dig2 = 0, dig3 = 0;
  DateTime now = milliClock.now();
  dig0 = now.hour() / 10;
  dig1 = now.hour() % 10;
  dig2 = now.minute() / 10;
  dig3 = now.minute() % 10;
  sc.writeBuffer(numTable[dig0],0);
  sc.writeBuffer(numTable[dig1],1);
  sc.writeBuffer(numTable[dig2],2);
  sc.writeBuffer(numTable[dig3],3);
}

void loop() {
}
void updateClock() {
  //deactivate Interrupts so the WiFi can work on it's own.
  deactivateTickerInts();
  sc.writeBuffer(numTable[2], 0);
  sc.writeBuffer(numTable[4], 0);
  sc.writeBuffer(numTable[6], 0);
  sc.writeBuffer(numTable[8], 0);
  sc.update();
  sc.update();
  networkKeeper.attach(0.5, getClock);
  //getClock();
  //Clear buffer so it stays empty while connecting

}

void activateTickerInts(){
  //Timer to update display buffer
  timeKeeper.attach(5, updateDisplayBuffer);

  //Timer to update display seconds dot.
  dotKeeper.attach(1, updateDisplayDot);

  //Timer to update display scan
  displayKeeper.attach_ms(4, updateDisplay);
}

void deactivateTickerInts(){
  timeKeeper.detach();
  dotKeeper.detach();
  displayKeeper.detach();
}

void getClock(){
  //Get random ip from pool
  WiFi.hostByName(ntpServerName, timeServerIP);
  if(!packetSent){
    sendNTPpacket(timeServerIP);
    packetSent = true;
    return;
  }

  int cb = udp.parsePacket();
  if(cb == 0) {
    Serial.println("No packet received.");
    timeToTry--;
    if(timeToTry > 0){
      return;
    } else {
      packetSent = false;
      timeToTry = 10;
    }
  } else {
    
    networkKeeper.detach();
    timeToTry = 10;
    packetSent=false;
    Serial.print("Packet received, length=");
    Serial.println(cb);

    udp.read(packetBuffer, NTP_PACKET_SIZE);

    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);

    unsigned long secsSince1900 = highWord << 16 | lowWord;

    Serial.print("Seconds from 1900: ");
    Serial.println(secsSince1900);

    // now convert NTP time into everyday time:
    Serial.print("Unix time = ");
    // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
    const unsigned long seventyYears = 2208988800UL;
    // subtract seventy years:
    unsigned long epoch = secsSince1900 - seventyYears;
    // print Unix time:
    Serial.println(epoch);

    parseClock(epoch);
    updateDisplayBuffer();
    activateTickerInts();
  }

}

void parseClock(unsigned long epoch){
  DateTime time = DateTime(epoch);
  time = time + TimeSpan(0, gmtOffset, 0,0);
  /*if(gmtOffset > 0){
    time = time + TimeSpan(0, gmtOffset, 0,0);
  }else if(gmtOffset < 0){
    time = time + TimeSpan(0, -gmtOffset, 0,0);
  }*/

  Serial.print("The GMT time is ");
  Serial.print(time.hour());
  Serial.print(':');
  if (time.minute() < 10) {
    Serial.print('0');
  }
  Serial.print(time.minute());
  Serial.print(':');
  if (time.second() < 10) {
    Serial.print('0');
  }
  Serial.println(time.second()); 
  milliClock.adjust(time);
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress& address) {
  Serial.println("sending NTP packet...");
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 2;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;

  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  udp.beginPacket(address, 123); //NTP requests are to port 123
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  udp.endPacket();
}