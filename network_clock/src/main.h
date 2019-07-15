#ifndef MAIN_H
#define MAIN_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <SerialDriver.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>  
#include <FS.h>
#include <WiFiUdp.h>
#include <RTClib.h>
#include <Bounce2.h>
#include <user_interface.h>

#define DEVICE_NAME_SIZE 12
#define DEVICE_PASS_SIZE 12

#define SSID_SIZE 32
#define PASSWORD_SIZE 64

#define DATA_PIN 13
#define CLOCK_PIN 14
#define LATCH_PIN 15

#define WPS_BUTTON_PIN 4
#define REFRESH_BUTTON_PIN 5
#define OFFSET_BUTTON_PIN 16

#define WPS_LED 16 //D3
#define CONN_LED 2 //D4



// -------- NETWORK
void getNetworkConnection();
void sendNTPpacket(IPAddress& address);
void getUDPPacket();
void getWPSConnection();

// -------- SERVER

void onIndex();
void handleLogin();
bool isAuthenticated();
void handleApiExchange();
void handleApiInput();
void handleNotFound();
bool handleFileRead(String path);

// -------- DISPLAY
uint32_t updateDisplayBuffer();
uint32_t updateDisplay();

// -------- CLOCK
void getClock();
void parseClock(unsigned long epoch);
uint32_t updateClock();

// -------- INTERRUPTS
void activateTickerInts();
void deactivateTickerInts();


struct Node* addInterrupt(uint32_t (*function)(void));
bool removeInterrupt(struct Node* n);

// -------- FLAGS
void setDisplayBufferFlag();
void setDisplayUpdateFlag();
void setUpdateClockFlag();
void setButtonReadFlag();
void setWPSFlag();
void setClockRefreshFlag();

// -------- VARIOUS
bool loadCredentials();
void saveCredentials();

void initPeripherals();
void initNetwork();
void initServer();
void initInterrupts();


/*
 *
 * -----------VARIABLES
 *
 */

// ------NETWORK
// ------------ NETWORK ---------------

// NTP -------------
unsigned int localPort = 2390;

IPAddress timeServerIP;
const char * ntpServerName = "time.nist.gov";

const int NTP_PACKET_SIZE = 48;
byte packetBuffer[NTP_PACKET_SIZE];

WiFiUDP udp;

bool packetSent = false;
int timeToTry = 10; //Times to read UDP packet before sending another one

uint8_t displayBuffer[4] = {B01001110, B00011101, B00010101, B00010101};
bool dotStatus = true;


// FILESYSTEM ----------

ESP8266WebServer server(80);

// OBJECTS ------------
SerialDriver sc(DATA_PIN, CLOCK_PIN, LATCH_PIN);
RTC_Millis milliClock;
Bounce wpsButton = Bounce();
Bounce refreshButton = Bounce();
Bounce offsetButton = Bounce();

// ------------ STRUCTS --------------

typedef struct Device_Info_t {
  char ssid[32];
  char psk[64];
  char name[12];
  char loginName[12];
  char password[12];
  uint8_t brightness;
  int16_t timeOffset;
}Device_Info;

typedef struct Node {
  uint32_t time;
  uint32_t (*function)(void);
  bool isActive = true;
  struct Node *next = nullptr;
}Node;

struct List {
  struct Node *head = nullptr;
  struct Node *tail = nullptr;
  uint8_t length = 0;
  struct Node *index = nullptr;

  void reset(){
    index = nullptr;
  }

  struct Node* getCurrent(){
    return index;
  }
  void setIndex(struct Node* n){
    struct Node* temp = head;
    while(temp->next != nullptr){
      temp = temp->next;
    }
    index = temp;
  }
  
  bool advance(){
    if(index == nullptr){
      if(head == nullptr){
        return false;
      }
      index = head;
      return true;
    }
    if(index == tail){
      index = nullptr;
      return false;
    }
    if(index->next != nullptr){
      index = index->next;
      return true;
    }
    return true;
  }
};

Device_Info_t deviceInfo;
struct List *interruptList = (struct List*)malloc(sizeof(struct List));

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

#endif