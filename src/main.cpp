#include "WiFi.h"
#include "NTPClient.h"
#include "WiFiUdp.h"
#include <Firebase_ESP_Client.h>
#include "TFT_eSPI.h"
#include "Logo_Scoutify.h" 
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
#define WIFI_SSID "TEKDUS"
#define WIFI_PASSWORD  "08116605951"
#define API_KEY "AIzaSyDlHt2E6aaTFxda9JuQnYqqhEZqUGBlMYg"
#define DATABASE_URL "https://scoutify-iot-default-rtdb.asia-southeast1.firebasedatabase.app/"
#define SOS D2
#define BATTERYTRIGGERDISPLAY D1

/*------------------------------------------------
| WIRING TO TFT DISPLAY | USB C KE ARAH ATAS     |
--------------------------------------------------
CS       to  D3  (GPIO5)  (A3)    |  Pin 4 kiri  |
DC       to  D4  (GPIO6)  (SDA)   |  Pin 5 kiri  |
RES/RST  to  D5  (GPIO7)  (SCL)   |  Pin 6 kiri  |
SDA/MOSI to  D10 (GPIO10) (MOSI)  |  Pin 4 kanan |
SCL/SCK  to  D8  (GPIO8)  (SCK)   |  Pin 6 kanan |
VCC      to  3,3v                 |  Pin 3 kanan |
GND      to  GND                  |  Pin 2 kanan |
--------------------------------------------------*/

/*------------------------------------------------------------------
| WIRING TO A9G BOARD | MICRO USB KE ARAH ATAS                     |
--------------------------------------------------------------------
AT_TX   |  Pin 14 kiri  |   to  D6  (GPIO21) (TX)   |  Pin 7 kiri  |
AT_RX   |  Pin 13 kiri  |   to  D7  (GPIO20) (RX)   |  Pin 7 kanan |
IO25    |  Pin 1 kanan  |   to  D2  (GPIO4)  (A2)   |  Pin 3 kiri  |
VUSB    |  Pin 2 kiri   |   to  5v                  |  Pin 1 kanan |
GND     |  Pin 1  kiri  |   to  GND                 |  Pin 2 kanan |
--------------------------------------------------------------------*/


WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
TFT_eSPI tft= TFT_eSPI();
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
FirebaseJson json;

int screenW=160;
int screenH=80;
int sendCount = 1;
bool signupOK = false;
bool manual;
bool manualtemp;
String formattedDate;
String dayStamp;
String timeStamp;
String databasePath;
String parentPath;
String combinepath;
String uid;
String latPath = "/latitude";
String longPath = "/longitude";
String manualPath = "/manual";
String manualtempPath = "/manual temp";
String uidPath = "/user id"; 
String timePath = "/timestamp";
String lat;
String longi;

String SOS_NUM = "+6285811096232";
boolean stringComplete = false;
String inputString = "";
String fromGSM = "";
bool CALL_END = 1;
char* response = "";
String res = "";
int c = 0;

int batteryLevelDisplay = 0;
int batteryLevel = 0;
unsigned long previousMillis = 0;
unsigned long interval = 10000;
unsigned long sendDataPrevMillis = 0;
unsigned long timerDelay = 90000;


void initWiFi() {
  btStop();
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
 delay(1000);
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("RRSI: ");
  Serial.println(WiFi.RSSI());
}

void initFirebase() {
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  if (Firebase.signUp(&config, &auth, "", "")){
    signupOK = true;
  }
  else  {
  Serial.printf("%s\n", config.signer.signupError.message.c_str());
  }
  config.token_status_callback = tokenStatusCallback; 
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

void uidName() {
   while ((auth.token.uid) == "") {
    Serial.print('.');
    delay(1000);
  }
  uid = auth.token.uid.c_str();
}

void initTime() {
  timeClient.begin();
  timeClient.setTimeOffset(25200);
}

void timeSend() {
  while(!timeClient.update()) {
  timeClient.forceUpdate();
  }
  formattedDate = timeClient.getFormattedDate();
  int splitT = formattedDate.indexOf("T");
  dayStamp = formattedDate.substring(0, splitT);
  timeStamp = formattedDate.substring(splitT+1, formattedDate.length()-1);
}

void childName(){
  String childName = "Packet " + String(sendCount) + " from " + String(uid);
  combinepath = String(childName);
  sendCount++;
}

void initA9G(void * parameter) {
  Serial1.begin(115200, SERIAL_8N1, D6, D7);
  pinMode(SOS, INPUT_PULLUP);

  Serial1.println("AT");               
  delay(1000);

  Serial1.println("AT+GPS = 1");      
  delay(1000);

  Serial1.println("AT+GPSLP = 2");      
  delay(1000);

  Serial1.println("AT+CMGF = 1");
  delay(1000);

  Serial1.println("AT+CSMP  = 17,167,0,0 ");
  delay(1000);

  Serial1.println("AT+CPMS = \"SM\",\"ME\",\"SM\" ");
  delay(1000);
  vTaskDelete(NULL);
  }

void convertstring() {
   if (Serial1.available())
    {
      char inChar = Serial1.read();
      if (inChar == '\n') {
        if (fromGSM == "SEND LOCATION\r")
        {
        }
        Serial.println(fromGSM);
        fromGSM = "";
      }
      else
      {
        fromGSM += inChar;
      }
      delay(20);
    }
}

void AutoTracking() {
  Serial1.println("AT+LOCATION = 2");  

  while (!Serial1.available());
  while (Serial1.available())
  {
    char add = Serial1.read();
    res = res + String(add);
    delay(50);
  }

  res = res.substring(17, 38);
  response = &res[0];
  Serial.print("Received Data - "); 
  Serial.println(response);
  Serial.println("\n");

  if (strstr(response, "GPS NOT"))
  {
    Serial.println("No Location data");
    json.set(latPath, String(lat));
    json.set(longPath, String(longi));
  }
  else
  {
    int i = 0;
    while (response[i] != ',')
      i++;

    String location = (String)response;
    String lat = location.substring(2, i);
    String longi = location.substring(i + 1);
    json.set(latPath, String(lat));
    json.set(longPath, String(longi));
    Serial.println(lat);
    Serial.println(longi);

  }
  response = "";
  res = "";
}

void manualTracking() {

 Serial1.println("AT+LOCATION = 2");  

  while (!Serial1.available());
  while (Serial1.available())
  {
    char add = Serial1.read();
    res = res + String(add);
    delay(50);
  }

  res = res.substring(17, 38);
  response = &res[0];
  Serial.print("Received Data - "); 
  Serial.println(response);
  Serial.println("\n");

  if (strstr(response, "GPS NOT"))
  {
    Serial.println("No Location data");
    json.set(latPath, String(lat));
    json.set(longPath, String(longi));
    Serial1.println("AT+CMGF=1");
    delay(1000);
    Serial1.println("AT+CMGS=\"" + SOS_NUM + "\"\r");
    delay(1000);

    Serial1.println ("Unable to fetch location. Please try again");
    delay(1000);
    Serial1.println((char)26);
    delay(1000);
  }
  else
  {
    int i = 0;
    while (response[i] != ',')
      i++;

    String location = (String)response;
    String lat = location.substring(2, i);
    String longi = location.substring(i + 1);
    json.set(latPath, String(lat));
    json.set(longPath, String(longi));
    Serial.println(lat);
    Serial.println(longi);
    String Gmaps_link = ( "http://maps.google.com/maps?q=" + lat + "+" + longi); 
    Serial1.println("AT+CMGF=1");
    delay(1000);
    Serial1.println("AT+CMGS=\"" + SOS_NUM + "\"\r");
    delay(1000);

    Serial1.println ("I'm here " + Gmaps_link);
    delay(1000);

    Serial1.println((char)26);
    delay(1000);

    Serial1.println("AT+CMGD=1,4"); 
    delay(5000);
  }
  response = "";
  res = "";
}

  void batteryDisplay() {
  tft.fillScreen(TFT_BLACK);
  int batteryWidth = 65;
  int batteryHeight = 26;
  int batteryX = (tft.width() - batteryWidth) / 2;
  int batteryY = (tft.height() - batteryHeight) / 2;
  tft.drawRect(batteryX, batteryY, batteryWidth, batteryHeight, TFT_WHITE);
  int batteryLevelWidth = map(0, 0, 100, 0, batteryWidth);
  tft.fillRect(batteryX, batteryY, batteryLevelWidth, batteryHeight, TFT_WHITE);
  }

  void displayData() {
  int batteryX = (tft.width() - 65) / 2;
  int batteryY = (tft.height() - 26) / 2;

  tft.setCursor(batteryX, batteryY + 10); //X,Y
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2.5);
  tft.printf(" %d", 100);
  tft.println("%");
  }

void setup() {
  Serial.begin(115200);
   xTaskCreatePinnedToCore(initA9G, "Initialize A9G Board", 10000, NULL, 1, NULL, 0);
  pinMode(BATTERYTRIGGERDISPLAY, INPUT_PULLUP);
  tft.init();
  tft.setRotation(3);
  tft.setSwapBytes(true);
  tft.fillScreen(TFT_BLACK);
  tft.pushImage(0,-3,screenW,screenH,Logo_Scoutify);
  initWiFi();
  initFirebase();
  uidName();
  initTime();
}

void loop() {
convertstring();
 if (digitalRead(SOS) == LOW) {
    Serial.println("SOS Button Pressed");
    timeSend();
    String Stampwaktu = dayStamp + " " + timeStamp;
    childName();
    manualTracking();
    json.set(manualPath, bool(manual = true));
    json.set(manualtempPath, bool(manualtemp = true));
    json.set(uidPath, String(uid));
    json.set(timePath, String(Stampwaktu));
    databasePath = "/locations/" + combinepath;
    parentPath= databasePath;
    Serial.printf("", Firebase.RTDB.setJSON(&fbdo, parentPath.c_str(), &json) ? "ok" : fbdo.errorReason().c_str());
    delay(10000);
  } else {
  if (Firebase.ready() && (millis() - sendDataPrevMillis > timerDelay)){
    sendDataPrevMillis = millis();
    timeSend();
    String Stampwaktu = dayStamp + " " + timeStamp;
    childName();
    AutoTracking();
    json.set(manualPath, bool(manual = false));
    json.set(manualtempPath, bool(manualtemp = false));
    json.set(uidPath, String(uid));
    json.set(timePath, String(Stampwaktu));
    databasePath = "/locations/" + combinepath;
    parentPath= databasePath;
    Serial.printf("", Firebase.RTDB.setJSON(&fbdo, parentPath.c_str(), &json) ? "ok" : fbdo.errorReason().c_str());
   } }
 
if (digitalRead(BATTERYTRIGGERDISPLAY) == LOW) {
    Serial.println("Battery Triggered");
    batteryDisplay();
    displayData();
    delay(2000);
  } else {
    tft.fillScreen(TFT_BLACK);
    tft.pushImage(0,-3,screenW,screenH,Logo_Scoutify);
    delay(2500);
  }

  if ((WiFi.status() != WL_CONNECTED) && (millis() - previousMillis >=interval)) {
    Serial.print(millis());
    Serial.println("Reconnecting to WiFi...");
    WiFi.disconnect();
    WiFi.reconnect();
    previousMillis = millis();
    }
  } 
  
