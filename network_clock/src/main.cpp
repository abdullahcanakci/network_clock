#include <Arduino.h>
#include <SerialDriver.h>
#include <Ticker.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <RTClib.h>

// ------------ PROTOTYPES ------------

void sendNTPpacket(IPAddress& address);
void updateDisplayBuffer();
void updateDisplay();
void updateClock();
void activateTickerInts();
void deactivateTickerInts();
//Turns out doing long works on interrupts wasn't a bright idea.
void setDisplayBufferFlag();
void setDisplayUpdateFlag();
void setUpdateClockFlag();
void getNetworkConnection();
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

SerialDriver sc(dataPin, clockPin, latchPin);
RTC_Millis milliClock;


// ------------ TIMERS -------------

Ticker displayBufferUpdateTimer;
Ticker displayUpdateTimer;
Ticker networkRequestTimer;
Ticker updateTimeTimer;
//Clock refresh rate in hours
uint8_t clockRefreshRate = 6;
uint8_t clockKeeper = 6;

// ------------ VARIABLES ------------
//NETWORK
boolean packetSent = false;
int gmtOffset = 3;
int timeToTry = 10; //Times to read UDP packet before sending another one

uint8_t displayBuffer[4] = {B01001110, B00011101, B00010101, B00010101};
boolean dotStatus = true;

// ------------ FLAGS -----------
boolean flagDisplayBufferUpdate;
boolean flagDisplayUpdate;
boolean flagNetworkRequest;
boolean flagClockUpdate;

// ------------ NUM REF TABLE ---------------
uint8_t numTable[] = {
  B01111110,
  B00110000,
  B01101101,
  B01111001,
  B00110011,
  B01011011,
  B00011111,
  B01110000,
  B01111111,
  B01111011,
};


void setup() {
  Serial.begin(115200);

  Serial.println();
  Serial.println();

  /*
  * We are using compile/upload? time to init the RTC
  * So I don't have to determine whether or not the RTC needs to be 
  * init or adjust on clock acquire.
  */
  milliClock.begin(DateTime(F(__DATE__), F(__TIME__)));

  //Setup the MAX7219 for operation
  sc.DisplaySetup();
  // This method updates displayDriver registers with the buffer ones.
  // Default buffer contains "Conn". So on boot we can see the display activated.
  updateDisplay();

  //Connect to a network
  getNetworkConnection();
 
  //Get Network Clock
  updateClock();
  updateTimeTimer.attach(60*60, setUpdateClockFlag);
}

// Actions are based on Ticker interrupts. No need for loop()
void loop() {
  if(flagDisplayBufferUpdate){
    flagDisplayBufferUpdate = false;
    updateDisplayBuffer();
  }
  if(flagDisplayUpdate){
    flagDisplayUpdate = false;
    updateDisplay();
  }
  if(flagNetworkRequest){
    flagNetworkRequest = false;
    getClock();
  }
  if(flagClockUpdate){
    flagClockUpdate = false;
    updateClock();
  }
}

void setDisplayBufferFlag(){
  flagDisplayBufferUpdate = true;
}
void setDisplayUpdateFlag(){
  flagDisplayUpdate = true;
}
void setUpdateClockFlag(){
  flagClockUpdate = true;
}
void setNetworkRequestFlag(){
  flagNetworkRequest = true;
}

void activateTickerInts(){
  //Timer to update display buffer
  displayBufferUpdateTimer.attach(5, setDisplayBufferFlag);

  displayUpdateTimer.attach(1, setDisplayUpdateFlag);
}

//As I know wifi creaupdates problems when there are interrupts shorter than 2ms
// We are disabling all short term interrupts so they won't bother network stack
void deactivateTickerInts(){
  displayBufferUpdateTimer.detach();
  displayUpdateTimer.detach();
}

/*
* Connects to a network and creates a port for connections.
*/

void getNetworkConnection(){
  Serial.print("Connecting to:");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
  while(WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print(".");
  }
  Serial.println("");

  //Starting an UDP port for NTP connections.
  Serial.println("Starting UDP");
  udp.begin(localPort);
  Serial.print("Local port: ");
  Serial.println(udp.localPort());
}

/*
 *  Updates display buffer with new time values
 */
void updateDisplayBuffer(){
  DateTime now = milliClock.now();
  displayBuffer[0] = numTable[now.hour() / 10];
  displayBuffer[1] = numTable[now.hour() % 10];
  displayBuffer[2] = numTable[now.minute() / 10];
  displayBuffer[3] = numTable[now.minute() % 10];
}

/*
* Passes display buffer values into the drivers registers.
*/
void updateDisplay(){
  for(int i = 0; i < 4; i++){
    if((i == 1 || i == 2) && dotStatus ){
    
        sc.WriteDigit(displayBuffer[i] | B10000000, i);
      
    } else {
      sc.WriteDigit(displayBuffer[i], i);
    }
  }
  dotStatus = !dotStatus;
}

// This methods will be called intervals to get clock from network and update local one.
void updateClock() {
  if(clockKeeper < clockRefreshRate){
    return;
  }
  clockKeeper = 0;

  //deactivate Interrupts so the WiFi can work on it's own.
  deactivateTickerInts();
  networkRequestTimer.attach(0.5, setNetworkRequestFlag);
}

//Gets clock from network source and provides soft rtc with the results.
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
    
    networkRequestTimer.detach();
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
    setDisplayBufferFlag();
    activateTickerInts();
  }

}

//Parse unix epoch time to soft rtc and logs it out
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