#include <Arduino.h>
#include <ArduinoJson.h>
#include <SerialDriver.h>
#include <Ticker.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>  
#include <FS.h>
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

bool loadCredentials();

void onIndex();
void handleApiExchange();
void handleApiInput();
void handleLogin();
void handleNotFound();
bool handleFileRead(String path);
bool isAuthenticated();

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

// -------- VARIOUS
bool loadDefaults();

String getDeviceName();
String getDevicePassword();

#define DEVICE_NAME_SIZE 12
#define DEVICE_PASS_SIZE 12
#define DEVICE_BRIGHTNESS_SIZE 1

#define SSID_SIZE 32
#define PASSWORD_SIZE 64

#define SSID_ADDRESS 0 //32 byte 0-31
#define PASSWORD_ADDRESS 32 // 64 bit  32-95
#define DEVICE_NAME_ADDRESS 96 //97-107
#define DEVICE_PASS_ADDRESS 108 //108-119
#define DEVICE_BRIGHTNESS_ADDRESS 120 //120-120
#define DEVICE_TIME_OFFSET_ADDRESS 121 //121-122;
#define DEVICE_FIRST_BOOT_ADDRESS 123 //123-123


#define DATA_PIN 13
#define CLOCK_PIN 14
#define LATCH_PIN 15

#define WPS_BUTTON_PIN 4
#define REFRESH_BUTTON_PIN 5
#define OFFSET_BUTTON_PIN 16

#define WPS_LED 16 //D3
#define CONN_LED 2 //D4


// ------------ NETWORK ---------------

unsigned int localPort = 2390;

IPAddress timeServerIP;
const char * ntpServerName = "time.nist.gov";

const int NTP_PACKET_SIZE = 48;
byte packetBuffer[NTP_PACKET_SIZE];

WiFiUDP udp;

// ------------ FILESYSTEM ----------

ESP8266WebServer server(80);

// ------------ OBJECTS ------------

SerialDriver sc(DATA_PIN, CLOCK_PIN, LATCH_PIN);
RTC_Millis milliClock;
Bounce wpsButton = Bounce();
Bounce refreshButton = Bounce();
Bounce offsetButton = Bounce();


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

typedef struct Device_Info_t {
  char ssid[32];
  char psk[64];
  char name[12];
  char loginName[12];
  char password[12];
  uint8_t brightness;
  int16_t timeOffset;
}Device_Info;

Device_Info_t deviceInfo;

// ------------ VARIABLES ------------
//NETWORK
bool packetSent = false;
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
bool flagClockOffset;


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

void clearEEPROM(){
  //CLEAR EEPROM
  // There is no such a thing as EEPROM on ESP12E.
  // But we are using part of a SPI flash as EEPROM.
  
  EEPROM.begin(512);
  for(int i = 0; i < 512; i++){
    EEPROM.write(i, 0);
  }
  EEPROM.end();
}

void initPeripherals(){
  wpsButton.attach(WPS_BUTTON_PIN, INPUT_PULLUP);
  wpsButton.interval(5);
  refreshButton.attach(REFRESH_BUTTON_PIN, INPUT_PULLUP);
  refreshButton.interval(5);
  offsetButton.attach(OFFSET_BUTTON_PIN, INPUT_PULLUP);
  offsetButton.interval(5);

  pinMode(WPS_LED, OUTPUT);
  digitalWrite(WPS_LED, LOW);

  pinMode(CONN_LED, OUTPUT);
  digitalWrite(CONN_LED, HIGH);
}

void initFileSystem(){
  SPIFFS.begin();
  Dir dir = SPIFFS.openDir("/");
  while(dir.next()){
    String fileName = dir.fileName();
    Serial.print("FS File: ");
    Serial.println(fileName);
  }
  SPIFFS.end();
}

void initServer(){
  server.on("/", HTTP_POST, handleApiInput);
  server.on("/api", HTTP_GET, handleApiExchange);
  server.on("/api", HTTP_POST, handleApiInput);
  server.on("/login", HTTP_POST, handleLogin);
  server.onNotFound(handleNotFound);


  const char * headerKeys[] = {"User-Agent", "Cookie"};
  size_t headerKeysSize = sizeof(headerKeys) / sizeof(char*);

  server.collectHeaders(headerKeys, headerKeysSize);

  server.begin();

  MDNS.begin("clock");
  Serial.print("Open http://");
  Serial.print("clock");
  Serial.println(".local/to see the file browser");
}

/*
 * Creates a network connection.
 */
void initNetwork(){

  bool niStored = deviceInfo.ssid != 0 && deviceInfo.psk != 0;

  WiFi.mode(WIFI_STA);

  if(niStored){
    //There is a network conn avaible in EEPROM

    Serial.print("Connecting to ");
    Serial.print(deviceInfo.ssid);
    Serial.println("");
  
    Serial.print(deviceInfo.psk);
    Serial.println("");
    WiFi.begin(deviceInfo.ssid, deviceInfo.psk);
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

  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  
  //Starting an UDP port for NTP connections.
  Serial.println("Starting UDP");
  udp.begin(localPort);
  Serial.print("Local port: ");
  Serial.println(udp.localPort());

  Serial.println();
}

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.println();

  loadCredentials();
  delay(500);

  //clearEEPROM();

  initPeripherals();

  /*
  * We are using compile/upload? time to init the RTC
  * So I don't have to determine whether or not the RTC needs to be 
  * init or adjust on clock acquire.
  */
  milliClock.begin(DateTime(F(__DATE__), F(__TIME__)));

  
  //Setup the MAX7219 for operation
  sc.DisplaySetup();
  sc.setBrightness(deviceInfo.brightness);
  // This method updates displayDriver registers with the buffer ones.
  // Default buffer contains "Conn". So on boot we can see the display activated.
  updateDisplay();


  initFileSystem();
  initNetwork();
  initServer();


  //Get Network Clock
  //updateClock();
  activateTickerInts();
  updateTimeTimer.attach(60*60, setUpdateClockFlag);

}


void loop() {
  server.handleClient();
  MDNS.update();

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
    offsetButton.update();
    if(wpsButton.rose()){
      flagWpsConnect = true;
    }
    if(refreshButton.rose()){
      flagClockRefresh = true;
    }
    if(offsetButton.rose()){
      flagClockOffset = true;
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

  if(flagClockOffset){
    Serial.println("Clock offset");
    flagClockOffset = false;
  }

  if(WiFi.isConnected()){
    digitalWrite(CONN_LED, LOW);
  }else{
    digitalWrite(CONN_LED, HIGH);
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

bool loadCredentials(){
  SPIFFS.begin();
  File credFile = SPIFFS.open("/creds.txt", "r");
  if(!credFile){
    Serial.println("Credentials file can't be opened");
  }

  char buffer[64];
  int index = 0;
  while (credFile.available()) {
    int l = credFile.readBytesUntil(',', buffer, sizeof(buffer));
    buffer[l] = '\0';

    switch (index)
    {
    case 0:{
      memcpy(deviceInfo.ssid, buffer, l);
      break;
    }
    case 1:
    {
      memcpy(deviceInfo.psk, buffer, l);
      break;
    }
    case 2:
    {
      memcpy(deviceInfo.name , buffer, l);
      break;
    }
    case 3:
    {
      memcpy(deviceInfo.loginName, buffer, l);
      break;
      break;
    }
    case 4:
    {
      memcpy(deviceInfo.password, buffer, l);
      break;
    }
    case 5:
    {
      String s = buffer;
      uint8_t b = s.toInt();
      s.~String();
      deviceInfo.brightness = b;
      break;
    }
    case 6:
    {
      String s = buffer;
      uint16_t b = s.toInt();
      s.~String();
      deviceInfo.timeOffset = b;
      break;
    }
    }

    index++;
  }
  credFile.close();
  SPIFFS.end();
  return true;
}

void saveCredentials(){
  SPIFFS.begin();
  File creds = SPIFFS.open("/creds.txt", "w");

  creds.print(deviceInfo.ssid);
  creds.print(',');
  creds.print(deviceInfo.psk);
  creds.print(',');
  creds.print(deviceInfo.name);
  creds.print(',');
  creds.print(deviceInfo.loginName);
  creds.print(',');
  creds.print(deviceInfo.password);
  creds.print(',');
  creds.print(deviceInfo.brightness);
  creds.print(',');
  creds.print(deviceInfo.timeOffset);
  creds.print(',');

  creds.close();
  SPIFFS.end();



  SPIFFS.end();
}


void getWPSConnection(){
    Serial.println("Waiting WPS connection");

    //Activate WPS LED
    digitalWrite(WPS_LED, HIGH);

    int timeToTry  = 5;
    while (timeToTry > 0){
      if(WiFi.beginWPSConfig()){
        Serial.println("WPS connection established.");
        break;
      }
      Serial.print("WPS connection failed. Tryin again ...");
      Serial.println(timeToTry, DEC);
      timeToTry -= 1;
    }

    //Disable WPS LED
    digitalWrite(WPS_LED, LOW);

    Serial.println("");
    Serial.println("WPS connected");

    struct station_config conf;
    wifi_station_get_config(&conf);

    // Save network info to EEPROM
    memcpy(deviceInfo.ssid, conf.ssid, sizeof(conf.ssid));
    memcpy(deviceInfo.psk, conf.password, sizeof(conf.password));

    saveCredentials();
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
    if((i == 1 || i == 2) && dotStatus){
        sc.WriteDigit(displayBuffer[i] | B10000000, i);
      
    } else {
      sc.WriteDigit(displayBuffer[i], i);
    }
  }
  dotStatus = !dotStatus;
}

void buildJsonAnswer(char *output){
  StaticJsonBuffer<400> buffer;
  JsonObject& root = buffer.createObject();

  bool auth = isAuthenticated();
  if(!auth){
    root["auth"] = false;
  } else {
    root["auth"] = true;
    root["ssid"] = deviceInfo.ssid; //32 Byte + 4 byte
    root["psk"] = deviceInfo.psk; //64 Byte + 4 byte

    root["dname"] = deviceInfo.name; //20Byte
    root["lname"] = deviceInfo.loginName;
    root["dpass"] = deviceInfo.password; //20byte
    root["bright"] = deviceInfo.brightness; //byte

    root["time"] = milliClock.now().unixtime();
    root["timezone"] = deviceInfo.timeOffset;
  }
  root.printTo(output, 400);
}

String getContentType(String filename){
  if (filename.endsWith(".html")) {
    return "text/html";
  } else if (filename.endsWith(".css")) {
    return "text/css";
  } else if (filename.endsWith(".js")) {
    return "application/javascript";
  } else if (filename.endsWith(".svg")){
    return "image/svg+xml";
  }
  return "text";
}

bool handleFileRead(String path){
  //deactivateTickerInts();
  SPIFFS.begin();
  Serial.println("Handle file read");
  if(path.endsWith("/")){
    path += "index.html";
  }
  if(path.endsWith("/creds.txt")){
    SPIFFS.end();
    return false;
  }
  String contentType = getContentType(path);
  String pathWithGz = path + ".gz";
  if(SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)){
    if(SPIFFS.exists(pathWithGz)){
      path += ".gz";
    }
    File file = SPIFFS.open(path ,"r");
    Serial.print("File open: ");
    Serial.print(path);
    server.streamFile(file, contentType);
    file.close();
    Serial.println("File closed.");

    //activateTickerInts();
    return true;
  }
  //activateTickerInts();
  SPIFFS.end();
  return false;
}

void handleNotFound(){
  if (!handleFileRead(server.uri())) {        // check if the file exists in the flash memory (SPIFFS), if so, send it
    server.send(404, "text/plain", "404: File Not Found");
  }
}
void handleApiExchange(){
  char buffer[400];
  buildJsonAnswer(buffer);
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", buffer);
}

void handleApiInput(){
  StaticJsonBuffer<400> newBuffer;
  JsonObject& root = newBuffer.parseObject(server.arg("plain"));
  Serial.println(server.arg("plain"));

  if(!root.success()){
    Serial.println("parseObject() failed");
    return;
  }
  uint8_t type = root["type"];
  Serial.print("Request Type: ");
  Serial.println(type);
  //Types are
  // 1 - Network
  // 2 - Device
  // 3 - Server

  switch (type)
  {
  case 0:
  {
    uint8_t brightness = root["bright"];
    sc.setBrightness(brightness);
    deviceInfo.brightness = brightness;
    break;
  }

  case 1:
  {
    const char *ssid = root["ssid"];
    Serial.print("SSID: ");
    Serial.println(ssid);

    const char *psk = root["psk"];
    Serial.print("PSK: ");
    Serial.println(psk);
    if(!(WiFi.SSID().equals(deviceInfo.ssid)) || (!WiFi.psk().equals(deviceInfo.psk))){
      //We have updated credentials. Update them
      memcpy(deviceInfo.ssid, ssid, SSID_SIZE);
      memcpy(deviceInfo.psk, psk, PASSWORD_SIZE);
    }
    break;
  }
  case 2:
  {

    const char *deviceName = root["dname"];
    const char *loginName = root["lname"];
    const char *devicePass = root["dpass"];

    int16_t timezone = root["timezone"];
    uint8_t brightness = root["bright"];

    memcpy(deviceInfo.name, deviceName, DEVICE_NAME_SIZE);
    memcpy(deviceInfo.loginName, loginName, 12);
    memcpy(deviceInfo.password, devicePass, DEVICE_PASS_SIZE);
    deviceInfo.brightness = brightness;
    if(deviceInfo.timeOffset != timezone){
      DateTime now = milliClock.now();
      uint32_t epoch = now.unixtime();
      epoch -= deviceInfo.timeOffset * 60;
      deviceInfo.timeOffset = timezone;
      parseClock(epoch);
    }
    break;
  }
  case 3:
  {
    break;
  }
  }

  server.send ( 200, "text/json", "{success:true}" );
  saveCredentials();
  delay(1000);
}

bool isAuthenticated() {
  Serial.println("Enter isAuthenticated");
  if (server.hasHeader("Cookie")) {
    Serial.print("Found cookie: ");
    String cookie = server.header("Cookie");
    Serial.println(cookie);
    if (cookie.indexOf("ESPSESSIONID=1") != -1) {
      Serial.println("Authentication Successful");
      return true;
    }
  }
  Serial.println("Authentication Failed");
  return false;
}


void handleLogin(){
  if(server.hasHeader("Cookie")){
    Serial.println("Found cookie:");
    Serial.println(server.header("Cookie"));
  }
  StaticJsonBuffer<100> newBuffer;
  JsonObject& root = newBuffer.parseObject(server.arg("plain"));
  Serial.println(server.arg("plain"));
  bool dc = false;
  dc = root["DISCONNECTED"];

  if(dc){
    server.sendHeader("Cache-Control", "no-cache");
    server.sendHeader("Set-Cookie", "ESPSESSIONID=0");
    server.send(301);
    return;
  }
  String id = root["USERNAME"];
  String pw = root["PASSWORD"];

  if(id != NULL && pw != NULL){
    if(id.equals(deviceInfo.loginName) && pw.equals(deviceInfo.password)){
      server.sendHeader("Cache-Control", "no-cache");
      server.sendHeader("Set-Cookie", "ESPSESSIONID=1");
      server.send(301);
    }
  }
  id.~String();
  pw.~String();
}

// This methods will be called intervals to get clock from network and update local one.
void updateClock() {
  if(clockKeeper < clockRefreshRate){
    return;
  }
  clockKeeper = 0;

  //deactivate Interrupts so the WiFi can work on it's own.
  deactivateTickerInts();
  networkRequestTimer.attach(0.3, setNetworkRequestFlag);
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
    updateDisplayBuffer();
    activateTickerInts();
  }

}

//Parse unix epoch time to soft rtc and logs it out
void parseClock(unsigned long epoch){
  DateTime time = DateTime(epoch);
  if(deviceInfo.timeOffset > 0){
    time = time + TimeSpan(0, deviceInfo.timeOffset / 60, deviceInfo.timeOffset % 60,0);
  }else if(deviceInfo.timeOffset < 0){
    time = time - TimeSpan(0, (-deviceInfo.timeOffset) / 60, (-deviceInfo.timeOffset) % 60,0);
  }

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