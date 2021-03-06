#include "main.h"

void setup() {
  Serial.begin(115200);
  delay(500);
  //Init Peripherals. Buttons, displays etc
  initPeripherals();


  uint8_t bootState = checkBootState();
  /*
   * There are 3 boot states 
   * 0 - Regular boot
   *    Load credentials
   *    if avaible connect last known network
   * 
   * 1 - WPS boot
   *    Activate WPS
   *    try to connect, if failed show error and wait for 
   *    WPS button to pushed or rebooted
   *   
   * 2 - Access Point
   *    WPS is insecure, you may not have it enabled or don't want to use it
   *    This mode creates a access point to connect and configure it.
   * 
   * 3 - Reset
   *    User might corrupt or forget credentials etc. 
   *    This will reset credentials file with a master one.
   */


  /* We always have to load credentials. 
   * It has device name etc. and in case of WPS we will write updated credentials. to the file
   * if not loaded it will write empty.
   */
  //Load credentials from flash "/creds.txt" file
  loadCredentials();
  delay(500);
  sc.setBrightness(deviceInfo.brightness);
  switch (bootState)
  {
  case 0:
    initNetwork();
    //Get Network Clock
    milliClock.begin(DateTime(0));
    initInterrupts();
    break;

  case 1:
    getWPSConnection();
    if(WiFi.isConnected()){
      ESP.restart();
    }
    break;

  case 2:
    WiFi.softAP(deviceInfo.name, NULL);
    break;

  case 3:
    loadCredentials(true);
    //This will cause credentials to be red from master file and saved into the regular file.
    break;
  }

  initServer();
}

/*
 * Inits peripherals like buttons leds etc to their initial state
 */
void initPeripherals(){
  wpsButton.attach(WPS_BUTTON_PIN, INPUT_PULLUP);
  wpsButton.interval(5);
  refreshButton.attach(REFRESH_BUTTON_PIN, INPUT_PULLUP);
  refreshButton.interval(5);
  functionButton.attach(FUNCTION_BUTTON_PIN, INPUT_PULLUP);
  functionButton.interval(5);

  pinMode(WPS_LED, OUTPUT);
  digitalWrite(WPS_LED, LOW);

  pinMode(CONN_LED, OUTPUT);
  digitalWrite(CONN_LED, HIGH);

  //Init display
  sc.DisplaySetup();
  sc.setBrightness(deviceInfo.brightness);
  updateDisplay();

  Serial.println("Peripherals initiated");
}


void initServer(){
  server.on("/", HTTP_POST, handleApiInput);
  server.on("/api", HTTP_GET, handleApiExchange);
  server.on("/api", HTTP_POST, handleApiInput);
  server.on("/login", HTTP_POST, handleLogin);
  server.onNotFound(handleNotFound);


  //These are required to use cookie based auth
  const char * headerKeys[] = {"User-Agent", "Cookie"};
  size_t headerKeysSize = sizeof(headerKeys) / sizeof(char*);

  server.collectHeaders(headerKeys, headerKeysSize);

  server.begin();

  //init dns system
  MDNS.begin("clock");
  Serial.print("Open http://");
  Serial.print("clock");
  Serial.println(".local/to see the file browser");
}

/*
 * Creates a network connection.
 */
void initNetwork(){
  //If we have connected to a network connect it again
  bool niStored = deviceInfo.ssid != 0 && deviceInfo.psk != 0;

  WiFi.mode(WIFI_STA);

  if(niStored){
    Serial.printf("Network name: %s, Network password: %s", deviceInfo.ssid, deviceInfo.psk);
    WiFi.begin(deviceInfo.ssid, deviceInfo.psk);
    while(WiFi.status() != WL_CONNECTED){
      delay(500);
      Serial.print(".");
    }
  } else {
    //There is no preconfigured network.
    getWPSConnection();
  }

  if(WiFi.isConnected()){ 
    Serial.println("Connection established");
  }

  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  
  //Starting an UDP port for NTP connections.
  Serial.println("Starting UDP");
  udp.begin(localPort);
  Serial.print("Local port: ");
  Serial.println(udp.localPort());

  Serial.println();
}
uint32_t prevSeconds = 0;
bool isNetworkRequestActive = false;
uint32_t networkMillis = 0;
uint32_t buttonMillis = 0;

void loop() {
  server.handleClient();
  MDNS.update();

  uint32_t time = milliClock.now().unixtime();
  interruptList->reset();
  if(time - prevSeconds >= 1 && !packetSent){
    prevSeconds = time;
    while (interruptList->advance())
    {
      struct Node *temp = interruptList->getCurrent();
      if (time >= temp->time)
      { 
        uint32_t ans = temp->function();
        if (ans == 0)
        {
          //We are calling remove interrupt which also uses advance and
          //getCurrent functions. I relocate our pointer to previous one so our while
          // will use advance to the right pointer
          struct Node *t = temp->next;
          removeInterrupt(temp);
          interruptList->setIndex(t);
        }
        else
        {
          temp->time = ans;
        }
      }
    }
  }

  if(packetSent){
    if(millis() - networkMillis > 200){
      getClock();
      networkMillis = millis();
    }
  } 

  if(millis() - buttonMillis >= 5){
    wpsButton.update();
    refreshButton.update();
    functionButton.update();
    if(wpsButton.rose()){
      getWPSConnection();
    }
    if(refreshButton.rose()){
      updateClock();
    }
    if(functionButton.rose()){
      //TODO set offset
    }
    buttonMillis = millis();
  }

  if(WiFi.isConnected()){
    digitalWrite(CONN_LED, LOW);
  }else{
    digitalWrite(CONN_LED, HIGH);
  }
}

void initInterrupts(){
  struct Node* temp = addInterrupt(updateDisplay);
  temp->time = milliClock.now().unixtime() + 5;

  struct Node* temp1 = addInterrupt(updateDisplayBuffer);
  temp1->time = milliClock.now().unixtime() + 5;


  struct Node *temp2 = addInterrupt(updateClock);
  temp2->time = milliClock.now().unixtime() + 5;

  interruptList->reset();
}

/*
 * Adds and interrupt to the table.
 */
struct Node* addInterrupt(uint32_t (*function) (void)){
  struct Node* newNode = (struct Node*)malloc(sizeof(struct Node));

  newNode->function = function;
  interruptList->reset();
  if(interruptList->head ==nullptr){
    //No interrupts registered
    interruptList->head = newNode;
    interruptList->tail = newNode;
  } else {
    interruptList->tail->next = newNode;
    interruptList->tail = newNode;
  }
  return newNode;
}

bool removeInterrupt(struct Node* n){
  struct Node* prev = nullptr;
  struct Node* temp = nullptr;
  interruptList->reset();
  while(interruptList->advance()){
    temp = interruptList->getCurrent();
    if(temp != n){
      prev = temp;
      temp = temp->next;
    }
    if(temp == interruptList->tail && n != interruptList->tail){
      return false;
      // We are at tail, and couldn't find provided node
    }else {
      interruptList->tail = prev;
      //We remove tail.
    }
  }
  if(prev != nullptr && temp != nullptr){
    prev->next = temp->next;
    return false;
  }
  free(n);
  return true;
}

/*
 * Loads credentials from flash.
 */
bool loadCredentials(bool reset){
  SPIFFS.begin();
  File credFile;
  if(reset){
    Serial.println("Reloading credentials.");
    credFile = SPIFFS.open("/creds_master.txt", "r");
  } else {
    Serial.println("Reading credentials.");
    credFile = SPIFFS.open("/creds.txt", "r");
  }

  if(credFile){
    Serial.println("No file found");
  }

  char buffer[64];
  int index = 0;
  while (credFile.available()) {
    int l = credFile.readBytesUntil(',', buffer, sizeof(buffer));
    buffer[l] = '\0';
    Serial.println(buffer);
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
  if(reset){
    saveCredentials();
    delay(500);
    Serial.println("Credentials restore");
    ESP.restart();
  }
  Serial.println("Credentials loaded");
  return true;
}

/*
 * Saves credentials to flash with CSV pattern
 */
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
  Serial.println("Credentials saved.");
}

/*
 * Connects to a open WPS connection
 * TODO: More testing required
 */
void getWPSConnection(){
    Serial.println("Waiting WPS connection");
    displayBuffer[0] = B00111110;
    displayBuffer[1] = B01100111;
    displayBuffer[2] = B01011011;
    displayBuffer[3] = B00000000;

    //Activate WPS LED
    digitalWrite(WPS_LED, HIGH);
    WiFi.mode(WIFI_STA);
    int timeToTry  = 1;
    while (timeToTry < 6){
      displayBuffer[3] = numTable[timeToTry];
      updateDisplay();
      if(WiFi.beginWPSConfig()){
        Serial.println("WPS connection established.");
        WiFi.printDiag(Serial);
        break;
      }
      Serial.print("WPS connection failed. Tryin again ...");
      Serial.println(timeToTry, DEC);
      timeToTry += 1;
    }

    if(WiFi.status() != WL_CONNECTED){
      displayBuffer[0] = B01000111;
      displayBuffer[1] = B01110111;
      displayBuffer[2] = B00000110;
      displayBuffer[3] = B00001110;
      updateDisplay();
      return;
    }

    //Disable WPS LED
    digitalWrite(WPS_LED, LOW);

    Serial.println("");
    Serial.println("WPS connected");

    struct station_config conf;
    wifi_station_get_config(&conf);

    memcpy(deviceInfo.ssid, conf.ssid, sizeof(conf.ssid));
    memcpy(deviceInfo.psk, conf.password, sizeof(conf.password));

    saveCredentials();
    updateClock();
}

/*
 *  Updates display buffer with new time values
 */
uint32_t updateDisplayBuffer(){
  DateTime now = milliClock.now();
  if(deviceInfo.timeOffset > 0){
    now = now + TimeSpan(0, deviceInfo.timeOffset / 60, deviceInfo.timeOffset % 60,0);
  }else if(deviceInfo.timeOffset < 0){
    now = now - TimeSpan(0, (-deviceInfo.timeOffset) / 60, (-deviceInfo.timeOffset) % 60,0);
  }

  displayBuffer[0] = numTable[now.hour() / 10];
  displayBuffer[1] = numTable[now.hour() % 10];
  displayBuffer[2] = numTable[now.minute() / 10];
  displayBuffer[3] = numTable[now.minute() % 10];

  return milliClock.now().unixtime() + 5;
}

/*
* Passes display buffer values into the drivers registers.
*/
uint32_t updateDisplay(){
  for(int i = 0; i < 4; i++){
    if((i == 1 || i == 2) && dotStatus){
        sc.WriteDigit(displayBuffer[i] | B10000000, i);
      
    } else {
      sc.WriteDigit(displayBuffer[i], i);
    }
  }
  dotStatus = !dotStatus;
  return milliClock.now().unixtime() + 1;
}

/*
 * Handles filetype to HTML content type conversion
 */
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

/*
 * Main Api response.
 * Builds main api response to clients.
 * Checks auth.
 */
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

/*
 * Handles reading and uploading of requested files from clients.
 * Blocks access to "/creds.txt"
 */
bool handleFileRead(String path){
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

    return true;
  }
  SPIFFS.end();
  return false;
}

/*
 * If a page not found we came here.
 * Should update with a real webpage
 */
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

/*
 * Handles API post request.
 * Parsing, determining key:value pairs etc are all done inside.
 */
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
  // 0 - Brightness
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
    deviceInfo.timeOffset = timezone;

    break;
  }
  case 3:
  {
    break;
  }
  }

  server.send ( 200, "text/json", "{success:true}" );
  saveCredentials();
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

/*
 * Handles checking of credentials, cookies etc.
 */
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
uint32_t updateClock() {
  //deactivate Interrupts so the WiFi can work on it's own.

  getClock();
  return (milliClock.now() + TimeSpan(0,6,0,0)).unixtime();
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
    Serial.print(".");
    if(timeToTry > 0){
      return;
    } else {
      packetSent = false;
      timeToTry = 10;
      getClock();
    }
  } else {
    
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
  }
}

//Parse unix epoch time to soft rtc and logs it out
void parseClock(unsigned long epoch){
  DateTime time = DateTime(epoch);

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