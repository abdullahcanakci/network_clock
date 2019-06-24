#include <Arduino.h>
#include <SerialDriver.h>
#include <Ticker.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <RTClib.h>
#include <Bounce2.h>
#include <user_interface.h>
#include <EEPROM.h>

// ------------ PROTOTYPES ------------
// -------- NETWORK
void getNetworkConnection();
void sendNTPpacket(IPAddress& address);
void getUDPPacket();
void getWPSConnection();
void storeNetworkInfo(struct network_info *ni);
bool getNetworkInfo(struct network_info *ni);

// -------- DISPLAY
void updateDisplayBuffer();
void updateDisplay();

// -------- CLOCK
void getClock();
void parseClock(unsigned long epoch);
void updateClock();
// -------- INTERRUPTS
void activateTickerInts();
void deactivateTickerInts();

// -------- FLAGS
void setDisplayBufferFlag();
void setDisplayUpdateFlag();
void setUpdateClockFlag();
void setButtonReadFlag();
void setWPSFlag();
void setClockRefreshFlag();

/* 
#ifndef STASSID
#define STASSID "Ithilien"
#define STAPSK "147596328"
#endif
*/ //FUTURE  PROOFING

#define SSID_ADDRESS 0 //32 byte 0-31
#define PASSWORD_ADDRESS 32 // 64 bit  32-95
#define SSID_SIZE 32
#define PASSWORD_SIZE 64

#define DATA_PIN 13
#define CLOCK_PIN 14
#define LATCH_PIN 15

#define WPS_BUTTON_PIN 4
#define REFRESH_BUTTON_PIN 5


// ------------ NETWORK ---------------

unsigned int localPort = 2390;

IPAddress timeServerIP;
const char * ntpServerName = "time.nist.gov";

const int NTP_PACKET_SIZE = 48;
byte packetBuffer[NTP_PACKET_SIZE];

WiFiUDP udp;

// ------------ OBJECTS ------------

SerialDriver sc(DATA_PIN, CLOCK_PIN, LATCH_PIN);
RTC_Millis milliClock;
Bounce wpsButton = Bounce();
Bounce refreshButton = Bounce();



// ------------ TIMERS -------------

Ticker displayBufferUpdateTimer;
Ticker displayUpdateTimer;
Ticker networkRequestTimer;
Ticker updateTimeTimer;
Ticker buttonUpdateTimer;
//Clock refresh rate in hours
uint8_t clockRefreshRate = 6;
uint8_t clockKeeper = 6;

// ------------ STRUCTS --------------

struct network_info {
    char ssid[32];
    char password[64];
};


// ------------ VARIABLES ------------
//NETWORK
bool packetSent = false;
int gmtOffset = 3;
int timeToTry = 10; //Times to read UDP packet before sending another one

uint8_t displayBuffer[4] = {B01001110, B00011101, B00010101, B00010101};
bool dotStatus = true;

// ------------ FLAGS -----------
bool flagDisplayBufferUpdate;
bool flagDisplayUpdate;
bool flagNetworkRequest;
bool flagClockUpdate;
bool flagWpsConnect;
bool flagClockRefresh;
bool flagButtonRead;


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

  /* EEPROM.begin(512);
  for(int i = 0; i < 512; i++){
    EEPROM.write(i, 0);
  }
  EEPROM.end();*/


  /*
  * We are using compile/upload? time to init the RTC
  * So I don't have to determine whether or not the RTC needs to be 
  * init or adjust on clock acquire.
  */
  milliClock.begin(DateTime(F(__DATE__), F(__TIME__)));

  wpsButton.attach(WPS_BUTTON_PIN, INPUT_PULLUP);
  wpsButton.interval(5);
  refreshButton.attach(REFRESH_BUTTON_PIN, INPUT_PULLUP);
  refreshButton.interval(5);

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
  if(flagButtonRead){
    wpsButton.update();
    refreshButton.update();
    if(wpsButton.rose()){
      flagWpsConnect = true;
    }
    if(refreshButton.rose()){
      flagClockRefresh = true;
    }
  }

  if(flagClockRefresh){
    Serial.println("Clock refresh");
    flagClockRefresh = false;
    clockKeeper = clockRefreshRate;
    flagClockUpdate = true;
  }

  if(flagWpsConnect){
    Serial.println("Wps connect");
    getWPSConnection();
    clockKeeper = clockRefreshRate+1;
    setUpdateClockFlag();
    flagWpsConnect = false;
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

void setButtonReadFlag() {
  flagButtonRead = true;
}

void setWPSFlag(){ 
  flagWpsConnect = true; 
}

void setClockRefreshFlag(){ 
  flagClockRefresh = true; 
}

void activateTickerInts(){
  //Timer to update display buffer
  displayBufferUpdateTimer.attach(5, setDisplayBufferFlag);

  displayUpdateTimer.attach(1, setDisplayUpdateFlag);

  buttonUpdateTimer.attach_ms(5, setButtonReadFlag);
}

//As I know wifi creaupdates problems when there are interrupts shorter than 2ms
// We are disabling all short term interrupts so they won't bother network stack
void deactivateTickerInts(){
  displayBufferUpdateTimer.detach();
  displayUpdateTimer.detach();
  buttonUpdateTimer.detach();
}

/*
 * Reads Network info from EEPROM if no network info is present returns false
 */
bool getNetworkInfo(struct network_info *ni){
  EEPROM.begin(512);
  char ssid[SSID_SIZE];
  char pass[PASSWORD_SIZE];

  int index = 0;
  //This counter used for tracking number of zeroes in SSID or PASSWORD
  //If they are equal to SSID or PASSWORD size we can assume the we didn't write anything EEPROM
  //For this to work reliably we have to prefill our EEPROM with zeroes. Otherwise trash data will break this.

  uint8_t counter = 0;
  for(int address = SSID_ADDRESS; address < SSID_ADDRESS + SSID_SIZE; address++){
    ssid[index] = EEPROM.read(address);
    if(ssid[index] == 0){
      counter ++;
    }
    index++;
  }
  if (counter == 32){
    EEPROM.end();
    return false;
  }

  index = 0;
  counter = 0;
  for(int address = PASSWORD_ADDRESS; address < PASSWORD_ADDRESS + PASSWORD_SIZE - 1 ; address++){
    pass[index] = EEPROM.read(address);
    if(pass[index] == 0){
      counter++;
    }
    index++;
  }
  if(counter == 64){
    EEPROM.end();
    return false;
  }

  EEPROM.end();


  memcpy(ni->ssid, ssid, sizeof(ssid));
  memcpy(ni->password, pass, sizeof(pass));
  return true;
}

/*
 * Store provided network info to the EEPROM.
 */
void storeNetworkInfo(struct network_info *ni){
  EEPROM.begin(512);
  int index = 0;
  for(int address = SSID_ADDRESS; address < SSID_ADDRESS + SSID_SIZE; address++){
    EEPROM.write(address, ni->ssid[index]);
    index++;
  }
  index = 0;
  for(int address = PASSWORD_ADDRESS; address < PASSWORD_ADDRESS + PASSWORD_SIZE - 1 ; address++){
    EEPROM.write(address, ni->password[index]);
    index++;
  }
  EEPROM.commit();
  EEPROM.end();
}

void getWPSConnection(){
    Serial.print("Waiting WPS connection");
    WiFi.beginWPSConfig();
    while(WiFi.status() != WL_CONNECTED){
      delay(500);
      Serial.print(".");
    }

    Serial.println("");
    Serial.println("WPS connected");

    struct station_config conf;
    wifi_station_get_config(&conf);

    struct network_info ni;
    // Save network info to EEPROM
    memcpy(ni.ssid, conf.ssid, sizeof(conf.ssid));
    memcpy(ni.password, conf.password, sizeof(conf.password));

    storeNetworkInfo(&ni);
}

/*
* Connects to a network and creates a port for connections.
*/

void getNetworkConnection(){

  struct network_info ni;
  bool niStored = getNetworkInfo(&ni);

  WiFi.mode(WIFI_STA);

  if(niStored){
    //There is a network conn avaible in EEPROM
    Serial.print("Connecting to ");
    Serial.println(ni.ssid);
    Serial.println(ni.password);
    WiFi.begin(ni.ssid, ni.password);
    while(WiFi.status() != WL_CONNECTED){
      delay(500);
      Serial.print(".");
    }
  } else {
    //There is no preconfigured network.
    getWPSConnection();
  }

  Serial.println("");
  Serial.println("Connection established");
  
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
  networkRequestTimer.attach(0.1, setNetworkRequestFlag);
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