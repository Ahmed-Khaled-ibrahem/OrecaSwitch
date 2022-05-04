/*
   Documentation
   -------------

   EEPROM 0 >> Device Type
   EEPROM 1 >> Loads Status location
   EEPROM 2 - 99 >> Loads Status Value // Changing every restart by 2 for not exceeding 100,000 write to ESP8266
   EEPROM 100 >> Number of Restarts occured
   EEPROM 101 - 104 >> Local IP
   EEPROM 104 - 108 >> Gateway IP
   EEPROM 200 >> Number of FACTORY RESETs
*/

#include <ESP8266WiFi.h>
#include <FirebaseESP8266.h>
#include <SoftwareSerial.h>
#include <WiFiUdp.h>
#include <EEPROM.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <WiFiClientSecure.h>
#include <time.h>

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

#define CS A0     // Same
#define SW1 16    // D0=GPIO16
#define SW2 5     // D1=GPIO5
#define L1 12     // D6=GPIO12
#define L2 14     // D5=GPIO14
#define Anode 4   // D2=GPIO4
#define Cathod 13 // D7=GPIO13
#define LED 2     // D4=GPIO2
#define RST 9     // SD2=GPIO9

//**************************************************************************************************************************************

const String FirmwareVer = {"1.0"};
#define URL_fw_Version "/ASa3ed/ESP12/master/version.txt"

#define URL_fw_Bin "https://raw.githubusercontent.com/ASa3ed/ESP12/master/newFW.bin"
const char* host = "raw.githubusercontent.com";

//#define URL_fw_Bin "https://firebasestorage.googleapis.com/v0/b/klix-2020.appspot.com/o/newFW.bin?alt=media&token=8bad48da-e4fb-45ea-ad52-45978dca07fe"
//const char* host = "firebasestorage.googleapis.com";

const int httpsPort = 443;

//**************************************************************************************************************************************

unsigned long previousMillis_2 = 0;
unsigned long previousMillis = 0;        // will store last time LED was updated
const long interval = 60000;             // Period to check Firmware Updates in "milliseconds"
const long mini_interval = 1000;

//**************************************************************************************************************************************

WiFiServer WifiServer(555);

IPAddress LocalIP;
IPAddress Gateway;
IPAddress Subnet(255, 255, 0, 0);
IPAddress DNS(8, 8, 8, 8);



String Status;
int Status_location;
String L1_status;
String L2_status;
int L1_stat;
int L2_stat;
String NodeStatus;
String LED_status;
String TcpOrder;
String FireOrder;
String Order[3];
String defaultOrder[3];
//String MasterOrder = 0;
bool FireFlag[4];
int RestartNumber;
int RESETNumber;
char devicetype;
// Change for many devices
// S, Shutter
// R, Load ,Load (Regular Use)
// L, L2 >Load , L1 >Dimming 
// D, L2 >Dimming , L1 >Load
// M, Dimming , Dimming

void setup()
{
  Serial.begin(115200);
  Serial.println();
  EEPROM.begin(4096);

  pinMode(CS , INPUT);
  pinMode(SW1, INPUT);
  pinMode(SW2, INPUT);
  pinMode(RST, INPUT);
  pinMode(LED, OUTPUT);
  pinMode(L1, OUTPUT);
  pinMode(L2, OUTPUT);
  pinMode(Anode, OUTPUT);
  pinMode(Cathod, OUTPUT);

  //----------------------------------------------------- Get EEPROM -------------------------------------------------

  devicetype = char(EEPROM.read(0));
  Serial.print("DeviceType: ");
  Serial.println(devicetype);

  Status_location = int(EEPROM.read(1));
  L1_stat = int(EEPROM.read(Status_location));
  L2_stat = int(EEPROM.read(Status_location + 1));
  
  digitalWrite(L1, L1_stat);
  digitalWrite(L2, L2_stat);

  Serial.print("L1_stat: " + String(L1_stat) + " || L2_stat: " + String(L2_stat));
  
  RestartNumber = int(EEPROM.read(100));
  Serial.print("RestartNumber: "); Serial.println(RestartNumber);
  EEPROM.write(100, RestartNumber + 1);

  RESETNumber = int(EEPROM.read(200));
  Serial.print("RESETNumber: "); Serial.println(RESETNumber);

  
  if (Status_location < 98) {
    Status_location = Status_location + 2;
    EEPROM.write(Status_location, L1_stat);
    EEPROM.write(Status_location + 1, L2_stat);
    EEPROM.write(1, Status_location);
    EEPROM.commit();
  }
  else {
    Status_location = 2;
    EEPROM.write(Status_location, L1_stat);
    EEPROM.write(Status_location + 1, L2_stat);
    EEPROM.write(1, Status_location);
    EEPROM.commit();
  }

  //----------------------------------------------------- Connect to WiFi --------------------------------------------
  int wifiwait = 0;
  int count = 0;
  WiFi.mode(WIFI_STA);

  //    WL_NO_SHIELD        = 255,   // for compatibility with WiFi Shield library
  //    WL_IDLE_STATUS      = 0,
  //    WL_NO_SSID_AVAIL    = 1,
  //    WL_SCAN_COMPLETED   = 2,
  //    WL_CONNECTED        = 3,
  //    WL_CONNECT_FAILED   = 4,
  //    WL_CONNECTION_LOST  = 5,
  //    WL_DISCONNECTED     = 6

  if (WiFi.SSID() == 0) {
    while (WiFi.SSID() == 0) // ----------------------------- First Time >> No Credentials
    {
      Serial.println("First Time No Credentials.. Waiting for Smart Config ... ... ...");
      WiFi.beginSmartConfig();
      while (1) {
        delay(50);
        Devices(devicetype, defaultOrder, 0);
        if (WiFi.smartConfigDone()) {
          Serial.println("SmartConfig Success");
          break;
        }
      }
      WiFi.begin(WiFi.SSID(), WiFi.psk());

      while (WiFi.status() != WL_CONNECTED) {
        delay(20);
        Serial.print(".");
        Serial.print(WiFi.status());
        Devices(devicetype, defaultOrder, 0);
        if (digitalRead(RST) == HIGH) {
          FactoryReset();
        }
      }
    }
    // ----- Save IP address to EEPROM
    Serial.println("");
    Serial.print("Local ip : ");
    Serial.println(WiFi.localIP());


    LocalIP = WiFi.localIP();
    Gateway = WiFi.gatewayIP();


    for (int i = 101; i < 105; ++i)
    {
      EEPROM.write(i, LocalIP[i - 101]);
      Serial.print("Wrote: ");
      Serial.println(LocalIP[i - 101]);
    }
    for (int i = 105; i < 109; ++i)
    {
      EEPROM.write(i, Gateway[i - 105]);
      Serial.print("Wrote: ");
      Serial.println(Gateway[i - 105]);
    }

    EEPROM.commit();    //Store data to EEPROM
  } // -----------------------------------------------------------------------------------------

  else if (!WiFi.SSID() == 0) { // -------------------------------------- I have Credentials

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

    WiFi.config(LocalIP, DNS, Gateway, Subnet);
    //    if (!WiFi.config(LocalIP,Gateway,Subnet)) {
    //    Serial.println("STA Failed to configure");
    //    }


    //    WiFi.setDNS(dns_server1, dns_server2)

    WiFi.begin(WiFi.SSID(), WiFi.psk());

    while (WiFi.status() != WL_CONNECTED) {
      delay(20);
      Serial.print(".");
      Serial.print(WiFi.status());
      Devices(devicetype, defaultOrder, 0);
      if (digitalRead(RST) == HIGH) {
        FactoryReset();
      }
    }

  }

  Serial.println("");
  Serial.println("Connection Established");

  //----------------------------------------------------- Firebase Begin & Send DeviceType --------------------------------------------

  /* Assign the database URL and database secret(required) */
  config.database_url = DATABASE_URL;
  config.signer.tokens.legacy_token = DATABASE_SECRET;

  Firebase.reconnectWiFi(true);

  /* Initialize the library with the Firebase authen and config */
  Firebase.begin(&config, &auth);

  //Or use legacy authenticate method
  //Firebase.begin(DATABASE_URL, DATABASE_SECRET);

  // Send Devicetype & # of Restarts to Firebase & # of RESETS
  FireFlag[1] = 1;
  FireFlag[2] = 1;
  FireFlag[3] = 1;

  WifiServer.begin();

  if (digitalRead(RST) == HIGH)
  {
    FactoryReset();
  }

  //WiFi.printDiag(Serial);
  //PrintNuts();
  Serial.println("... Starting ...");
}
//---------------------------------------------------------------------------------------------------------
//----------------------------------------------------- LOOP ----------------------------------------------
//---------------------------------------------------------------------------------------------------------
void loop()
{
  yield(); // For ESP8266 to not dump *************************** SEARCH FOR STABILITY
  delay(1); // a work around for prevent WDT :(

  while (WiFi.status() != WL_CONNECTED) {
    delay(20);
    Serial.print(".");
    Serial.print(WiFi.status());
    Devices(devicetype, defaultOrder, 0);
  }

  WiFiClient client = WifiServer.available();

  if (client) {
    if (client.connected()) {
      if (client.available() > 0) {
        TcpOrder = (client.readStringUntil('\r'));
//        Serial.print("Packet: ");
//        Serial.println(TcpOrder);

        TcpOrder.remove(0, 5);
        TcpOrder = TcpOrder.substring(0, TcpOrder.indexOf(' '));
        Serial.println("Request : " + TcpOrder);

        client.println("HTTP/1.1 200 OK\nContent-type:text/html\nConnection: close\n");

        
        //DeviceType
        if (TcpOrder == "DeviceType") {
          client.println(devicetype);
        }

        //Status
        else if (TcpOrder == "NodeStatus") {
          Status = String(L1_stat) + "," + String(L2_stat);
          client.println(Status);
        }

        //RESET
        else if (TcpOrder == "RESET") {
          client.println("RESET");
          client.stop();  // Remove !!!
          FactoryReset();
        }
        else if (TcpOrder == "UPDATE") {
          client.println("UPDATE");
          client.stop();  // Remove !!!
          FirmwareUpdate();
        }
        //Action Operation
        else {
          client.println(TcpOrder);
          client.stop();  // Remove !!!
          // Re-Structuring Order S/D/L | 0/1/2 | Order
          SplitOrder(TcpOrder,Order);
          Serial.println("TCPOrder >>> " + Order[0]+" : "+Order[1]+" : "+Order[2]);
          
          Devices(devicetype, Order , 0);
        }
        client.stop();  // Remove !!!
      }
      TcpOrder = "";
      Devices(devicetype, defaultOrder, 0);
    }
  }




  // Smart Network Prototype
  //  MasterOrder = Firebase.getString(fbdo, WiFi.macAddress()+"/MasterOrder") ? fbdo.to<const char *>() : fbdo.errorReason().c_str();
  //  MasterOrder = String(MasterOrder);  // Node IP Only || OR || MacAddress,IP,FireOrder
  //
  //  split ip
  //  if(MasterOrder){
  //    client.write(IP,555);
  //    client.print(fireorder);
  //  }


  //  Serial.printf("Get string... %s\n", Firebase.getString(fbdo, WiFi.macAddress()+"/FireOrder") ? fbdo.to<const char *>() : fbdo.errorReason().c_str());
  // ----

  FireOrder = Firebase.getString(fbdo, WiFi.macAddress() + "/FireOrder") ? fbdo.to<const char *>() : fbdo.errorReason().c_str();

  FireOrder = String(FireOrder);

//   Serial.println(FireOrder);

  if (!(FireOrder == "0") && !(FireOrder == "connection lost") && !(FireOrder == "null") && !(FireOrder == "bad request")) {
    if (FireOrder == "RESET") {
      Serial.printf("Set RESET... %s\n", Firebase.setBool(fbdo, WiFi.macAddress() + "/ResetAck", 1) ? "ok" : fbdo.errorReason().c_str());
      FactoryReset();
    }
    if (FireOrder == "UPDATE") {
      Serial.printf("Set FW_UPDATE... %s\n", Firebase.setBool(fbdo, WiFi.macAddress() + "/FW_UPDATE", 1) ? "ok" : fbdo.errorReason().c_str());
      FirmwareUpdate();
    }

    //Action Operation
    else {
      // Re-Structuring Order S/D/L | 0/1/2 | Order
      Serial.println("FireOrder: " + FireOrder);
      SplitOrder(FireOrder,Order);
      Devices(devicetype, Order, 1);
      Status = String(L1_stat) + "," + String(L2_stat);
      Serial.printf("Set NodeStatus... %s\n", Firebase.setString(fbdo, WiFi.macAddress() + "/NodeStatus", Status) ? "ok" : fbdo.errorReason().c_str());
    }
    FireOrder = "0";   // Dont forget to clear from flutter also after check Acknoledgement on NodeStatus
  }

  //Devices IO
  Devices(devicetype, defaultOrder, 0);

  FireReport();

  if (digitalRead(RST) == HIGH) {
    FactoryReset();
  }
}
//---------------------------------------------------- Spliting Order -----------------------------------------
String *SplitOrder(String Order, String ArrOrder[3]){
  
  String DType = Order.substring(0, Order.indexOf(','));
  Order.remove(0, Order.indexOf(',')+1);
//  Serial.println(DType);
  String DChannel = Order.substring(0, Order.indexOf(','));
  Order.remove(0, Order.indexOf(',')+1);
//  Serial.println(DChannel);
  String DValue = Order.substring(0, Order.indexOf(','));
//  Serial.println(DValue);

  ArrOrder[0] = DType;
  ArrOrder[1] = DChannel;
  ArrOrder[2] = DValue;

  return ArrOrder;
}
//--------------------------------------------------- Firebase Reporter ---------------------------------------
void FireReport()
{
  yield();  // For ESP8266 to not dump
  Status = String(L1_stat) + "," + String(L2_stat);

  if (FireFlag[0] == 1)
  {
    if (Firebase.setString(fbdo, WiFi.macAddress() + "/NodeStatus", Status)) {
      FireFlag[0] = 0;
      Serial.println("FireReporter >>> Node Status : " + Status + " ... Reported");
    }
    else {
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
      Serial.println(fbdo.errorReason().c_str());
    }
  }

}

//--------------------------------------------------- FACTORY RESET ---------------------------------------
void FactoryReset()
{
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



//---------------------------------------------------- Read from EEPROM ----------------------------------------
String ReadEEPROM()
{
  String data;

  int datalength = EEPROM.read(0);
  for (int i = 1; i < datalength + 1 ; ++i)
  {
    data += char(EEPROM.read(i));
  }

  return data;
}

//--------------------------------------------------- Devices IO ------------------------------------------------
void Devices(char devicetype, String XOrder[3], bool FireOrTCP)
{
  yield();  // For ESP8266 to not dump


  if (devicetype == 'S') {               // ................................................. Roller Shutter

    if ((digitalRead(SW1) == HIGH || digitalRead(SW2) == HIGH)) // ------------------- Check Bouncing for 100ms
    {
      int i = 0;
      while (++i < 10) {
        delay(10);
        if (digitalRead(SW1) == HIGH && digitalRead(SW2) == HIGH) {
          Serial.println(i);
          break;
        }
      }
    }
    if ((digitalRead(SW1) == HIGH && digitalRead(SW2) == HIGH) || XOrder[2] == "STOP") // ------------------ Stop
    {
      //LED OFF
      digitalWrite(Anode , LOW);
      digitalWrite(Cathod, LOW);
      //      NodeStatus = "STOP";
      L1_stat = 0;
      L2_stat = 0;
      digitalWrite(L1 , LOW);
      digitalWrite(L2 , LOW);
      Serial.println("Stop");
      EEPROM.write(Status_location, L1_stat);
      EEPROM.write(Status_location + 1, L2_stat);
      EEPROM.commit();
      FireFlag[0] = 1;
      if (FireOrTCP == 1) {
        FireReport();
      }
      delay(500);
      while (digitalRead(SW1) == HIGH && digitalRead(SW2) == HIGH) {
        delay(1);
      }
    }
    else if (digitalRead(SW1) == HIGH || XOrder[2] == "UP") // ------------------------- UP
    {
      // NodeStatus = "UP";
      if (L2_stat == 1)
      {
        digitalWrite(L1 , LOW);
        digitalWrite(L2 , LOW);
        //LED RED
        digitalWrite(Anode , HIGH);
        digitalWrite(Cathod, LOW);
        delay(500);
      }
      //LED Yellow
      digitalWrite(Anode , LOW);
      digitalWrite(Cathod, HIGH);

      digitalWrite(L1 , HIGH);
      digitalWrite(L2 , LOW);
     
      L1_stat = 1;
      L2_stat = 0;
      Serial.println("Up");
      EEPROM.write(Status_location, L1_stat);
      EEPROM.write(Status_location + 1, L2_stat);
      EEPROM.commit();
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
      if (L1_stat == 1)
      {
        digitalWrite(L1 , LOW);
        digitalWrite(L2 , LOW);
        //LED RED
        digitalWrite(Anode , HIGH);
        digitalWrite(Cathod, LOW);
        delay(500);
      }
      //LED Yellow
      digitalWrite(Anode , LOW);
      digitalWrite(Cathod, HIGH);

      digitalWrite(L1 , LOW);
      digitalWrite(L2 , HIGH);

      L1_stat = 0;
      L2_stat = 1;
      Serial.println("Down");
      EEPROM.write(Status_location, L1_stat);
      EEPROM.write(Status_location + 1, L2_stat);
      EEPROM.commit();
      FireFlag[0] = 1;
      if (FireOrTCP == 1) {
        FireReport();
      }
      
      while (digitalRead(SW2) == HIGH) {
        delay(1);
      }
    }
  }
  if (devicetype == 'L') {                         // ............................ 2 switches/plugs/loads
    if (digitalRead(SW1) == HIGH || XOrder[2] == "OFF1" || XOrder[2] == "ON1") // ----------- First switch
    {
      if (L1_stat == 0 || XOrder[2] == "ON1")
      {
        //LED Yellow
        digitalWrite(Anode , LOW);
        digitalWrite(Cathod, HIGH);
        digitalWrite(L1 , HIGH);
        L1_stat = 1;
        EEPROM.write(Status_location, L1_stat);
        EEPROM.commit();
        Serial.println("Switch ONE ON");
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
        L1_stat = 0;
        EEPROM.write(Status_location, L1_stat);
        EEPROM.commit();
        Serial.println("Switch ONE OFF");
        FireFlag[0] = 1;
        if (FireOrTCP == 1) {
        FireReport();
        }
      }
      while (digitalRead(SW1) == HIGH) {
        delay(1);
      }
    }

    else if (digitalRead(SW2) == HIGH || XOrder[2] == "OFF2" || XOrder[2] == "ON2") // -------- Second switch
    {
      if (L2_stat == 0 || XOrder[2] == "ON2")
      {
        //LED Yellow
        digitalWrite(Anode, LOW);
        digitalWrite(Cathod, HIGH);
        digitalWrite(L2 , HIGH);
        L2_stat = 1;
        EEPROM.write(Status_location + 1, L2_stat);
        EEPROM.commit();
        Serial.println("Switch TWO ON");
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
        L2_stat = 0;
        EEPROM.write(Status_location + 1, L2_stat);
        EEPROM.commit();
        Serial.println("Switch TWO OFF");
        FireFlag[0] = 1;
        if (FireOrTCP == 1) {
        FireReport();
        }
      }
      while (digitalRead(SW2) == HIGH) {
        delay(1);
      }
    }
  }  
}

//--------------------------------------------------- Print Nuts ------------------------------------------------
void PrintNuts()
{
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
//--------------------------------------------------- Firmware Update Functions ------------------------------------------------
//--------------------------------------------------- Firmware Update Functions ------------------------------------------------
//--------------------------------------------------- Firmware Update Functions ------------------------------------------------
//------------------------------------------------- Firmware Update (Core_Function) --------------------------------------------
void FirmwareUpdate()
{
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
