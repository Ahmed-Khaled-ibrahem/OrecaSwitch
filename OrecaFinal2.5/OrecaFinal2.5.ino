/*
   Documentation
   -------------
   EEPROM Locations

   EEPROM 0 >> Device Type
   EEPROM 1 >> Loads Status location
   EEPROM 2 - 99 >> Loads Status Value // Changing every restart by 2 for not exceeding 100,000 write to ESP8266
   EEPROM 100 >> Number of Restarts occured
   EEPROM 101 - 104 >> Local IP
   EEPROM 104 - 108 >> Gateway IP
   EEPROM 200 >> Number of FACTORY RESETs



    WIFI Status

    WL_NO_SHIELD        = 255,   // for compatibility with WiFi Shield library
    WL_IDLE_STATUS      = 0,
    WL_NO_SSID_AVAIL    = 1,
    WL_SCAN_COMPLETED   = 2,
    WL_CONNECTED        = 3,
    WL_CONNECT_FAILED   = 4,
    WL_CONNECTION_LOST  = 5,
    WL_DISCONNECTED     = 6


    Devices types

    S, Shutter
    R, Load ,Load (Regular Use)
    L, L2 >Load , L1 >Dimming
    D, L2 >Dimming , L1 >Load
    M, Dimming , Dimming
*/

#include <ESP8266WiFi.h>
#include <FirebaseESP8266.h>
//#include <SoftwareSerial.h>
//#include <WiFiUdp.h>
#include <EEPROM.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <WiFiClientSecure.h>
//#include <time.h>
#include <ESP8266Ping.h>

// *******************************************************************************************************************************
#include <addons/RTDBHelper.h>

/* If work with RTDB, define the RTDB URL and database secret */
#define DATABASE_URL "klix-2020.firebaseio.com" //<databaseName>.firebaseio.com or <databaseName>.<region>.firebasedatabase.app
#define DATABASE_SECRET "Tk5c4JryptxOBawjqV956Ud4rXTwipYufJo4AcSF"

///* If work with RTDB, define the RTDB URL and database secret */
//#define DATABASE_URL "new-work-54a1f-default-rtdb.firebaseio.com"
//#define DATABASE_SECRET "bhV9QwIM5hpUxtEy2JRm7PQ8oMihEMa2xy8dZ4Q4"

/* Define the Firebase Data object */
FirebaseData fbdo;

/* Define the FirebaseAuth data for authentication data */
FirebaseAuth auth;

/* Define the FirebaseConfig data for config data */
FirebaseConfig config;

//**************************************************************************************************************************************

//#define CS A0     // Same
#define SW1 16      // D0=GPIO16
#define SW2 5       // D1=GPIO5
#define L1 4        // D2=GPIO4
#define L2 0        // D5=GPIO14  => D3=GPIO0
#define Anode 12    // D6=GPIO12
#define Cathod 13   // D7=GPIO13
//#define LED 2     // D4=GPIO2
#define RST 10      // SD2=GPIO9  => D5=GPIO14 

byte Loads[] = {'S', L1, L2};

//**************************************************************************************************************************************

const String FirmwareVer = {"1.0"};
#define URL_fw_Version "/ASa3ed/ESP12/master/version.txt"

//#define URL_fw_Bin "https://raw.githubusercontent.com/ASa3ed/ESP12/master/newFW.bin"
//const char* host = "raw.githubusercontent.com";

#define URL_fw_Bin "https://firebasestorage.googleapis.com/v0/b/klix-2020.appspot.com/o/v2.bin?alt=media&token=8bad48da-e4fb-45ea-ad52-45978dca07fe"
const char* host = "firebasestorage.googleapis.com";

const int httpsPort = 443;

//**************************************************************************************************************************************

WiFiServer WifiServer(555);

IPAddress LocalIP;
IPAddress Gateway;
IPAddress Subnet(255, 255, 0, 0);
IPAddress DNS(8, 8, 8, 8);

String Status;

int Stat_Loc[3]; // Two Channel
int Stat_Val[3]; // Two Channel

String NodeStatus;
String LED_status;
String TcpOrder;
String FireOrder;
String Order[3];
String defaultOrder[3];
bool FireFlag[4] = {1,1,1,1};
int RestartNumber;
int RESETNumber;
char devicetype;

bool Connection_Quality = false;

void setup()
{
  Serial.begin(115200);
  Serial.println();
  EEPROM.begin(4096);

  //pinMode(CS , INPUT);
  pinMode(SW1, INPUT);
  pinMode(SW2, INPUT);
  pinMode(RST, INPUT);
  //pinMode(LED, OUTPUT);
  pinMode(L1, OUTPUT);
  pinMode(L2, OUTPUT);
  pinMode(Anode, OUTPUT);
  pinMode(Cathod, OUTPUT);

  GetEEPROM();

  WiFi.mode(WIFI_STA);

  if ( WiFi.SSID() == 0 ) {             // ----------------------------- First Time >> No Credentials
      Connect_using_Smart_Config();
      WiFi.begin( WiFi.SSID(), WiFi.psk());
      Check_Wifi_Conectivity();
    
    Serial.println("");
    Serial.print("Local ip : ");
    Serial.println(WiFi.localIP());

    LocalIP = WiFi.localIP();
    Gateway = WiFi.gatewayIP();

    Write_IP_EEPROM();

  }
  else {  // -------------------------------------- I have Credentials
    WiFi.config(LocalIP, DNS, Gateway, Subnet);
    WiFi.begin(WiFi.SSID(), WiFi.psk());
    Check_Wifi_Conectivity();
  }

  Serial.println("\n Connection Established");

  //--------------------------- Firebase Begin & Send DeviceType --------------------------------------

  /* Assign the database URL and database secret(required) */
  config.database_url = DATABASE_URL;
  config.signer.tokens.legacy_token = DATABASE_SECRET;
  Firebase.reconnectWiFi(true);
  /* Initialize the library with the Firebase authen and config */
  Firebase.begin(&config, &auth);
  //Or use legacy authenticate method
  //Firebase.begin(DATABASE_URL, DATABASE_SECRET);
  // Send Devicetype & # of Restarts to Firebase & # of RESETS

  WifiServer.begin();

  //WiFi.printDiag(Serial);
  //PrintNuts();
  Serial.println("... Starting ...");
}

void loop()
{
  yield(); // For ESP8266 to not dump *************************** SEARCH FOR STABILITY
  delay(1); // a work around for prevent WDT :(

  Check_Wifi_Conectivity();
  TCPOrederHandler();
  firebaseOrderHandler();
  Devices(devicetype, defaultOrder, 0);
  FireReport();
}

//----------------------------------------------------# Functions # -----------------------------------------

String *SplitOrder(String Order, String ArrOrder[3]) {

  String DType = Order.substring(0, Order.indexOf(','));
  Order.remove(0, Order.indexOf(',') + 1);
  //  Serial.println(DType);
  String DChannel = Order.substring(0, Order.indexOf(','));
  Order.remove(0, Order.indexOf(',') + 1);
  //  Serial.println(DChannel);
  String DValue = Order.substring(0, Order.indexOf(','));
  //  Serial.println(DValue);

  ArrOrder[0] = DType;
  ArrOrder[1] = DChannel;
  ArrOrder[2] = DValue;

  return ArrOrder;
}

void FireReport(){
  yield();  // For ESP8266 to not dump

  if (FireFlag[0] == 1)
  {
    Status = String(Stat_Val[1]) + "," + String(Stat_Val[2]);

    if (Firebase.setString(fbdo, WiFi.macAddress() + "/NodeStatus", Status)) {
      FireFlag[0] = 0;
      Serial.println("FireReporter >>> Node Status : " + Status + " ... Reported");
    }
    else {
      Serial.print("FireReporter Error : ");
      Serial.println(fbdo.errorReason().c_str());
    }
  }
  if (FireFlag[1] == 1)
  {
    if (Firebase.setString(fbdo, WiFi.macAddress() + "/DeviceType", String(devicetype))) {
      FireFlag[1] = 0;
      Serial.println("FireReporter >>> DeviceType : " + String(devicetype) + " ... Reported");
    }
    else {
      Serial.print("FireReporter Error : ");
      Serial.println(fbdo.errorReason().c_str());
    }
  }

  if (FireFlag[2] == 1)
  {
    if (Firebase.setInt(fbdo, WiFi.macAddress() + "/RestartNumber", RestartNumber)) {
      FireFlag[2] = 0;
      Serial.println("FireReporter >>> RestartNumber : " + String(RestartNumber) + " ... Reported");
    }
    else {
      Serial.print("FireReporter Error : ");
      Serial.println(fbdo.errorReason().c_str());
    }
  }

  if (FireFlag[3] == 1)
  {
    if (Firebase.setInt(fbdo, WiFi.macAddress() + "/RESET", RESETNumber)) {
      FireFlag[3] = 0;
      Serial.println("FireReporter >>> RESET : " + String(RESETNumber) + " ... Reported");
    }
    else {
      Serial.print("FireReporter Error : ");
      Serial.println(fbdo.errorReason().c_str());
    }
  }

}

void FactoryReset(){
  Serial.println("**************** Starting FACTORY RESET ****************");
  digitalWrite(Anode, LOW);
  digitalWrite(Cathod, HIGH);
  delay(1000);

  EEPROM.write(200, RESETNumber + 1);

  WiFi.disconnect();
  ESP.eraseConfig();

  EEPROM.write(1, 2); // Status_location

  for (int i = 2; i < 200; ++i)   // From 1 if you need store devicetype
  {
    EEPROM.write(i, 0);
    Serial.print("Wrote: ");
    Serial.println(i);
  }

  EEPROM.commit();    //Store data to EEPROM
  //  ESP.reset();
  ESP.restart();    // just restart

}

void GetEEPROM(){
  devicetype = char(EEPROM.read(0));
  Serial.print("DeviceType: ");
  Serial.println(devicetype);

  //  Status_location = int(EEPROM.read(1));
  //  L1_stat = int(EEPROM.read(Status_location));
  //  L2_stat = int(EEPROM.read(Status_location) + 1);

  Stat_Loc[1] = int(EEPROM.read(1));
  Stat_Loc[2] = Stat_Loc[1] + 1;
  Stat_Val[1] = int(EEPROM.read(Stat_Loc[1]));
  Stat_Val[2] = int(EEPROM.read(Stat_Loc[2]));

  digitalWrite(L1, Stat_Val[1]);
  digitalWrite(L2, Stat_Val[2]);

  Serial.println("L1_stat: " + String(Stat_Val[1]) + " || L2_stat: " + String(Stat_Val[2]));

  RestartNumber = int(EEPROM.read(100));
  Serial.println("RestartNumber: " + String(RestartNumber));
  EEPROM.write(100, RestartNumber + 1);

  RESETNumber = int(EEPROM.read(200));
  Serial.println("RESETNumber: " + String(RESETNumber));

  if (Stat_Loc[1] < 98) {
    Stat_Loc[1] = Stat_Loc[1] + 2;
    Stat_Loc[2] = Stat_Loc[2] + 2;
    EEPROM.write(1, Stat_Loc[1]);
    EEPROM.write(Stat_Loc[1], Stat_Val[1]);
    EEPROM.write(Stat_Loc[2], Stat_Val[2]);
    EEPROM.commit();
  }
  else {
    Stat_Loc[1] = 2;
    Stat_Loc[2] = 3;
    EEPROM.write(1, Stat_Loc[1]);
    EEPROM.write(Stat_Loc[1], Stat_Val[1]);
    EEPROM.write(Stat_Loc[2], Stat_Val[2]);

    EEPROM.commit();
  }

  // -------- Read IP  --------
  for (int i = 101; i < 105 ; ++i)
  {
    LocalIP[i - 101] = EEPROM.read(i);
  }
  for (int i = 105; i < 109 ; ++i)
  {
    Gateway[i - 105] = EEPROM.read(i);
  }
  Serial.println(LocalIP);
  Serial.println(Gateway);
}

void Devices(char devicetype, String XOrder[3], bool FireOrTCP){
  yield();  // For ESP8266 to not dump

  if (devicetype == 'S' || XOrder[0] == "S") {  // --------------------------------- Roller Shutter

    if ( digitalRead(SW1) == HIGH || digitalRead(SW2) == HIGH ) // ------------------- Check Bouncing for 100ms
    {
      int i = 0;
      while (++i < 10) {
        delay(10);
        if ( digitalRead(SW1) == HIGH && digitalRead(SW2) == HIGH ) {
          Serial.println(i);
          break;
        }
      }
    }
    if ((digitalRead(SW1) == HIGH && digitalRead(SW2) == HIGH) || XOrder[2] == "STOP") // ------------------ Stop
    {
      //LED OFF
      LED_Note(0);
      
      Turn_Load( 1, 0 );
      Turn_Load( 2, 0 );
      
      FireFlag[0] = 1;
      
      if (FireOrTCP == 1) {
        FireReport();
      }
      //delay(500);
      while (digitalRead(SW1) == HIGH && digitalRead(SW2) == HIGH) {
        delay(1);
      }
    }
    else if (digitalRead(SW1) == HIGH || XOrder[2] == "UP")  // ------------------------- UP
    {
      if (Stat_Val[2] == 1)
      {
        digitalWrite(L1 , LOW);
        digitalWrite(L2 , LOW);
        
        //LED RED
        LED_Note(2);
        delay(500);
      }
      
      //LED Yellow
      LED_Note(1);
      
      Turn_Load( 1, 1 );
      Turn_Load( 2, 0 );

      FireFlag[0] = 1;
      
      if (FireOrTCP == 1) {
        FireReport();
      }

      while (digitalRead(SW1) == HIGH) {
        delay(1);
      }
    }

    else if (digitalRead(SW2) == HIGH || XOrder[2] == "DOWN") // -------------------- Down
    {
      //      NodeStatus = "DOWN";
      if (Stat_Val[1] == 1)
      {
        digitalWrite(L1 , LOW);
        digitalWrite(L2 , LOW);
        //LED RED
        LED_Note(2);
        delay(500);
      }
      //LED Yellow
      LED_Note(1);

      Turn_Load( 1, 0 );
      Turn_Load( 2, 1 );

      FireFlag[0] = 1;
      
      if (FireOrTCP == 1) {
        FireReport();
      }

      while (digitalRead(SW2) == HIGH) {
        delay(1);
      }
    }
  
   else if (digitalRead(RST) == LOW) {
      FactoryReset();
    }
  }

  if (XOrder[0] == "D") // ---------------------------------------- Digital Outputs
  {
    String LoadNo = XOrder[1].substring(1);
    Serial.println("Load number: " + LoadNo);

     //LED Yellow
      LED_Note(1);
      
      //Turn_Load( 1, 1 );
      //Turn_Load( 2, 0 );

      Turn_Load( LoadNo.toInt(), XOrder[2].toInt() );
    //digitalWrite( Loads[LoadNo.toInt()] , XOrder[2].toInt());
  
    Serial.println("Load >> " + XOrder[1] + " Value: " + XOrder[2] + " In Location: " + String(Stat_Loc[LoadNo.toInt()]));
    FireFlag[0] = 1;
    
    if (FireOrTCP == 1) {
      FireReport();
    }
  }

  else if (XOrder[0] == "A") // ----------------------------------- Analog Outputs
  {
    String LoadNo = XOrder[1].substring(1);
    Serial.println("Load number: " + LoadNo);

    //LED Yellow
    LED_Note(1);
    
    analogWrite(Loads[LoadNo.toInt()] , XOrder[2].toInt());
    Stat_Val[LoadNo.toInt()] = XOrder[2].toInt();
    EEPROM.write(Stat_Loc[LoadNo.toInt()], Stat_Val[LoadNo.toInt()]);
    EEPROM.commit();
    Serial.println("Load >> " + XOrder[1] + " Value: " + XOrder[2] + " In Location: " + String(Stat_Loc[LoadNo.toInt()]));
    FireFlag[0] = 1;
    
    if (FireOrTCP == 1) {
      FireReport();
    }
  }

  else {        // ----------------------------------------------- 2 switches/plugs/loads
    if (digitalRead(SW1) == HIGH) // ----------- First switch
    {
      if (Stat_Val[1] == 0)
      {
        //LED Yellow
        digitalWrite(Anode , LOW);
        digitalWrite(Cathod, HIGH);
        digitalWrite(L1 , HIGH);
        Stat_Val[1] = 1;
        EEPROM.write(Stat_Loc[1], Stat_Val[1]);
        EEPROM.commit();
        Serial.println("Load ONE ON In Location: " + String(Stat_Loc[1]));
        FireFlag[0] = 1;
        if (FireOrTCP == 1) {
          FireReport();
        }
      }
      else
      {
        //LED OFF
        digitalWrite(Anode, LOW);
        digitalWrite(Cathod, LOW);
        digitalWrite(L1 , LOW);
        Stat_Val[1] = 0;
        EEPROM.write(Stat_Loc[1], Stat_Val[1]);
        EEPROM.commit();
        Serial.println("Load ONE OFF In Location: " + String(Stat_Loc[1]));
        FireFlag[0] = 1;
        if (FireOrTCP == 1) {
          FireReport();
        }
      }
      while (digitalRead(SW1) == HIGH) {
        delay(1);
      }
    }

    else if (digitalRead(SW2) == HIGH) // -------- Second switch
    {
      if (Stat_Val[2] == 0)
      {
        //LED Yellow
        digitalWrite(Anode, LOW);
        digitalWrite(Cathod, HIGH);
        digitalWrite(L2 , HIGH);
        Stat_Val[2] = 1;
        EEPROM.write(Stat_Loc[2], Stat_Val[2]);
        EEPROM.commit();
        Serial.println("Load TWO ON In Location: " + String(Stat_Loc[2]));
        FireFlag[0] = 1;
        if (FireOrTCP == 1) {
          FireReport();
        }
      }
      else
      {
        //LED OFF
        digitalWrite(Anode, LOW);
        digitalWrite(Cathod, LOW);
        digitalWrite(L2 , LOW);
        Stat_Val[2] = 0;
        EEPROM.write(Stat_Loc[2], Stat_Val[2]);
        EEPROM.commit();
        Serial.println("Load TWO OFF In Location: " + String(Stat_Loc[2]));
        FireFlag[0] = 1;
        if (FireOrTCP == 1) {
          FireReport();
        }
      }
      while (digitalRead(SW2) == HIGH) {
        delay(1);
      }
    }

    else if (digitalRead(RST) == LOW) {
      FactoryReset();
    }
  }
}

void PrintNuts(){
  Serial.println("*****************************************");
  Serial.println("************* Hello ESP8266 *************");
  Serial.println("*****************************************");
  WiFi.printDiag(Serial);
  Serial.printf("SSID: %s\n", WiFi.SSID().c_str());
  Serial.printf("Password: %s\n", WiFi.psk().c_str());
  Serial.printf("MacAddress: %s\n", WiFi.macAddress().c_str());
  Serial.printf("BSSID: %s\n", WiFi.BSSIDstr().c_str());
  Serial.printf("RSSI: %d dBm\n", WiFi.RSSI());
  Serial.printf("Gataway IP: %s\n", WiFi.gatewayIP().toString().c_str());
  Serial.print("DNS #1, #2 IP: ");
  WiFi.dnsIP().printTo(Serial);
  Serial.print(", ");
  WiFi.dnsIP(1).printTo(Serial);
  Serial.println();
  Serial.print("Local ip : ");
  Serial.println(WiFi.localIP());
  Serial.print("Subnet: ");
  //Serial.println(WiFi.subnet());
  Serial.printf("Default hostname: %s\n", WiFi.hostname().c_str());
  WiFi.hostname("OrecaTech");
  Serial.printf("New hostname: %s\n", WiFi.hostname().c_str());
  Serial.printf("Flash Chip Real Size : %d\n", ESP.getFlashChipRealSize());
  Serial.println("*****************************************");
  Serial.println("***************** Nuts ******************");
  Serial.println("*****************************************");
}

void FirmwareUpdate(){
  yield();
  delay(1);

  WiFiClientSecure client;
  //  client.setTrustAnchors(&cert);
  client.setInsecure();
  if (!client.connect(host, httpsPort)) {
    Serial.println("Connection failed");
    return;
  }
  client.print(String("GET ") + URL_fw_Version + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "User-Agent: BuildFailureDetectorESP8266\r\n" +
               "Connection: close\r\n\r\n");
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") {
      Serial.println("Headers received");
      break;
    }
  }
  String payload = client.readStringUntil('\n');

  payload.trim();
  if (payload.equals(FirmwareVer) )
  {
    Serial.println("Device already on latest firmware version");
  }
  else
  {
    Serial.println("New firmware detected");
    ESPhttpUpdate.setLedPin(LED_BUILTIN, LOW);
    t_httpUpdate_return ret = ESPhttpUpdate.update(client, URL_fw_Bin);

    switch (ret) {
      case HTTP_UPDATE_FAILED:
        Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s\n", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
        break;

      case HTTP_UPDATE_NO_UPDATES:
        Serial.println("HTTP_UPDATE_NO_UPDATES");
        break;

      case HTTP_UPDATE_OK:
        Serial.println("HTTP_UPDATE_OK");
        break;
    }
  }
}

void TCPOrederHandler() {

  WiFiClient client = WifiServer.available();

  if (client) {
    if (client.connected()) {
      if (client.available() > 0) {
        
        TcpOrder = client.readStringUntil('\r');
        TcpOrder.remove(0, 5);
        TcpOrder = TcpOrder.substring(0, TcpOrder.indexOf(' '));
        Serial.println("Request : " + TcpOrder);

        client.println("HTTP/1.1 200 OK\nContent-type:text/html\nConnection: close\n");

        if (TcpOrder == "DeviceType") {
          client.println(devicetype);
          client.stop();
          // Take care here if TCP FAILED code will continue anyway ! without acknowledgment to User !!!
          // Hint client.print() returns the number of bytes written,
          // #ofBytes = client.println("");
          // if #ofBytes then continue
        }
        else if (TcpOrder == "NodeStatus") {
          Status = String(Stat_Val[1]) + "," + String(Stat_Val[1]);
          client.println(Status);
          client.stop();
          // Take care here if TCP FAILED code will continue anyway ! without acknowledgment to User !!!
          // Hint client.print() returns the number of bytes written,
          // #ofBytes = client.println("");
          // if #ofBytes then continue
        }
        else if (TcpOrder == "RESET") {
          client.println("RESET");
          client.stop();
          // Take care here if TCP FAILED code will continue anyway ! without acknowledgment to User !!!
          // Hint client.print() returns the number of bytes written,
          // #ofBytes = client.println("");
          // if #ofBytes then continue
          FactoryReset();
        }
        else if (TcpOrder == "UPDATE") {
          client.println("UPDATE");
          client.stop();
          // Take care here if TCP FAILED code will continue anyway ! without acknowledgment to User !!!
          // Hint client.print() returns the number of bytes written,
          // #ofBytes = client.println("");
          // if #ofBytes then continue
          FirmwareUpdate();
        }
        //Action Operation
        else {
          client.println(TcpOrder);
          client.stop();
          // Take care here if TCP FAILED code will continue anyway ! without acknowledgment to User !!!
          // Hint client.print() returns the number of bytes written,
          // #ofBytes = client.println("");
          // if #ofBytes then continue

          SplitOrder(TcpOrder, Order);
          Serial.println("TCPOrder >>> " + Order[0] + " : " + Order[1] + " : " + Order[2]);
          Devices(devicetype, Order , 0);
        }
      }
      TcpOrder = "";
      Devices(devicetype, defaultOrder, 0);
    }
  }
}

void firebaseOrderHandler() {

if( !Connection_Quality ){
  Connection_Quality = Ping.ping("firebase.google.com");
  }

if( Connection_Quality ){
  if ( Firebase.getString(fbdo, WiFi.macAddress() + "/FireOrder") )
  {
    FireOrder = fbdo.to<const char *>();
    FireOrder = String(FireOrder);
  }
  else
  {
    Connection_Quality = false;
    Serial.print("FireOrder Error: ");
    Serial.println(fbdo.errorReason().c_str());
  }
}

  if (!(FireOrder == "0")) {
    if (FireOrder == "RESET") {
      Serial.printf("Set RESET... %s\n", Firebase.setBool(fbdo, WiFi.macAddress() + "/ResetAck", 1) ? "ok" : fbdo.errorReason().c_str());
      // Take care here if set firebase FAILED code will continue anyway ! without acknowledgment to User !!!
      FactoryReset();
    }
    if (FireOrder == "UPDATE") {
      Serial.printf("Set FW_UPDATE... %s\n", Firebase.setBool(fbdo, WiFi.macAddress() + "/FW_UPDATE", 1) ? "ok" : fbdo.errorReason().c_str());
      // Take care here if set firebase FAILED code will continue anyway ! without acknowledgment to User !!!
      FirmwareUpdate();
    }
    //Action Operation
    else {
      Serial.println("FireOrder: " + FireOrder);
      SplitOrder( FireOrder, Order);
      Devices( devicetype, Order, 1);
    }
    FireOrder = "0";   // Dont forget to clear from flutter also after check Acknoledgement on NodeStatus
  }

}

void Write_IP_EEPROM() {

  for (int i = 101; i < 105; ++i)
  {
    EEPROM.write(i, LocalIP[i - 101]);
  }
  for (int i = 105; i < 109; ++i)
  {
    EEPROM.write(i, Gateway[i - 105]);
  }
  EEPROM.commit();
}

void Connect_using_Smart_Config() {

  Serial.println("\n First Time No Credentials.. Waiting for Smart Config ... ... ...");
  WiFi.beginSmartConfig();
  while (1) {
    delay(50);
    Devices(devicetype, defaultOrder, 0);
    if (WiFi.smartConfigDone()) {
      Serial.println("SmartConfig Success");
      break;
    }
  }
}

void Check_Wifi_Conectivity() {

  while ( WiFi.status() != WL_CONNECTED ) {
    delay(20);
    Serial.print(".");
    Serial.print(WiFi.status());
    Devices(devicetype, defaultOrder, 0);
  }
}

void LED_Note( byte state ){
  if(state == 0){
      digitalWrite(Anode , 0);
      digitalWrite(Cathod, 0);
    }
    else if(state == 1){
        digitalWrite(Anode , 0);
      digitalWrite(Cathod, 1);
      }
      else{
          digitalWrite(Anode , 1);
      digitalWrite(Cathod, 0);
        }
  }

void Turn_Load( byte LN, bool state){
  
      Stat_Val[LN] = state;
      digitalWrite(Loads[LN] , state);
      EEPROM.write( Stat_Loc[LN], state );
      EEPROM.commit();
  }
  
