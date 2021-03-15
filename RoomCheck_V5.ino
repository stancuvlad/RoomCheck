//#include <ArduinoJson.h>
//#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SimpleDHT.h>
#include <SPI.h>
//#include <Ethernet.h>
#include "HX711.h"
#include <FastLED.h>
#include <FirebaseESP8266.h>

#define gasSensor A0
#define pinLight 1 
#define pinDHT22 2 //2
#define pinHall 16
#define releuPTC 13
#define releuVentilator 15
#define LOADCELL_DOUT_PIN 4 //4
#define LOADCELL_SCK_PIN 0 //0
#define NUM_LEDS 16

#define WIFI_SSID "HUAWEI-W2Nw"
#define WIFI_PASSWORD "11223344"

#define FIREBASE_HOST "roomcheck-21820-default-rtdb.firebaseio.com"

/** The database secret is obsoleted, please use other authentication methods, 
 * see examples in the Authentications folder. 
*/
#define FIREBASE_AUTH "pKEnDYo2UAoINWJ7HwjZTssU9rSkjxzX2i1Emwh6"

LiquidCrystal_I2C lcd(0x27, 16, 2);
SimpleDHT22 dht22;
CRGB leds[NUM_LEDS];
HX711 scale;

const long utcOffsetInSeconds = 7200;

//Week Days
String weekDays[7]={"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

//Month names
String months[12]={"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};

//Define FirebaseESP8266 data object
FirebaseData firebaseData;

FirebaseJson json;

String path = "/Room/1";

//variabile globale
unsigned long sendDataPrevMillis = 0;
int contor = 1;
int count = 0;/////careful here
int ptcState = 1;
int ventilatorState = 1;
int lightState = 0;
int nrCarti = 0;
float weightValue = 1;
int initialHallValue = 0;
int initialWeightValue = 0;
int initialGasSensorValue = 0;
unsigned long timeForSendingData = 30000; 
unsigned long previousMillis = 0;
unsigned long previousM = 0;
unsigned long previousMili = 0;
float calibration_factor = -96650;

struct DateSenzori{
  float temperatura;
  float umiditate;
  int eroare;
};

struct Command{
  const char * stare_ptc;
  const char * stare_ventilator;
  const char * stare_led;
  int tmin;
  int tmax;
  int hmin;
  int hmax;
  int numar_carti;
  int httpCode;
};

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", utcOffsetInSeconds);

void setup()
{
    Serial.begin(115200);
    delay(10);
    
    WiFi.mode(WIFI_STA);
    
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("Connecting to Wi-Fi");
    while (WiFi.status() != WL_CONNECTED)
    {
      Serial.print(".");
      delay(300);
    }
    Serial.println();
    Serial.print("Connected with IP: ");
    Serial.println(WiFi.localIP());
    Serial.println();
    
    // Connect to WiFi network
    lcd.begin(14,12);
    lcd.backlight();
    lcd.setCursor(0,0);
    lcd.print(F("Connecting to"));
    lcd.setCursor(0,1);
    lcd.print(WIFI_SSID);
    
    lcd.clear();
    lcd.print(F("WiFi connected"));
    
    timeClient.begin();
    
    Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
    Firebase.reconnectWiFi(true);
    
    //Set the size of WiFi rx/tx buffers in the case where we want to work with large data.
    firebaseData.setBSSLBufferSize(1024, 1024);
    
    //Set the size of HTTP response buffers in the case where we want to work with large data.
    firebaseData.setResponseSize(1024);
    
    //Set database read timeout to 1 minute (max 15 minutes)
    Firebase.setReadTimeout(firebaseData, 1000 * 60);
    //tiny, small, medium, large and unlimited.
    //Size and its write timeout e.g. tiny (1s), small (10s), medium (30s) and large (60s).
    Firebase.setwriteSizeLimit(firebaseData, "tiny");
    
    //optional, set the decimal places for float and double data to be stored in database
    Firebase.setFloatDigits(2);
    Firebase.setDoubleDigits(6);
    
    /*
    This option allows get and delete functions (PUT and DELETE HTTP requests) works for device connected behind the
    Firewall that allows only GET and POST requests.
    
      Firebase.enableClassicRequest(firebaseData, true);
      */

    Firebase.setBool(firebaseData, path + "/relayPTC", false);
    Firebase.setBool(firebaseData, path + "/relayFan", false);
    Firebase.setBool(firebaseData, path + "/lightSwitch", false);
    Firebase.setBool(firebaseData, path + "/gasTrigger", false);
    Firebase.setBool(firebaseData, path + "/hallSensor", false);
    
    if (Firebase.getInt(firebaseData, path + "/data/count")){
      if (firebaseData.dataType() == "int"){
          int generalCount = firebaseData.intData();
          if (Firebase.getInt(firebaseData, path + "/data/" + (generalCount + 1) + "/" + "sensor1/count")) {    
              if (firebaseData.dataType() == "int"){
                  if(generalCount <= firebaseData.intData()){
                      count = firebaseData.intData();  
                  }
              }
          }else{
              count = generalCount;
          }
       }
    }
    
    pinMode(releuPTC, OUTPUT);
    pinMode(releuVentilator, OUTPUT);
    pinMode(pinLight, OUTPUT);
    pinMode(pinHall, INPUT);
    digitalWrite(releuPTC, 1);
    digitalWrite(releuVentilator, 1);
    digitalWrite(pinLight, 1);
    
    FastLED.addLeds<NEOPIXEL, pinLight>(leds, NUM_LEDS);
    FastLED.setBrightness(24);
    
    delay(1000);
   
    scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
    if (scale.is_ready()) {
      scale.tare();
      long zero_factor = scale.read_average();
    } else {
      Serial.println("HX711 not found.");
    }
    scale.set_scale();
    scale.set_scale(calibration_factor);
   
}



void loop()
{

  if (millis() - sendDataPrevMillis > 1000){
      int gasSensorValue = analogRead(gasSensor);
      //  delay(3);

      if(gasSensorValue == 1024){
          if(initialGasSensorValue != 1){
              initialGasSensorValue = 1;
              Firebase.setBool(firebaseData, path + "/gasTrigger", true);
          }
      }else if(gasSensorValue != 1024 && initialGasSensorValue != 0){
          initialGasSensorValue = 0;
      }

      int hallValue = digitalRead(pinHall);
      if(hallValue != initialHallValue){
          initialHallValue = hallValue;
          if(initialHallValue == 1){
              Firebase.setBool(firebaseData, path + "/hallSensor", true);  
          }else{
              Firebase.setBool(firebaseData, path + "/hallSensor", false);
          }
          
      }

    
      //Get data from RTDB
      if (Firebase.getBool(firebaseData, path + "/relayPTC")) {

        if (firebaseData.dataType() == "boolean") {
          if(firebaseData.boolData() == 1){
              ptcState = 0;
          }else if(firebaseData.boolData() == 0){
              ptcState = 1;
          }
          digitalWrite(releuPTC, ptcState);
        }
    
      } else {
        Serial.println(firebaseData.errorReason());
      }

      if (Firebase.getBool(firebaseData, path + "/relayFan")) {

        if (firebaseData.dataType() == "boolean") {
          Serial.println(firebaseData.boolData() == 1 ? "true" : "false");
          if(firebaseData.boolData() == 1){
              ventilatorState = 0;
          }else if(firebaseData.boolData() == 0){
              ventilatorState = 1;
          }
          digitalWrite(releuVentilator, ventilatorState);
        }
    
      } else {
        Serial.println(firebaseData.errorReason());
      }

//      if (Firebase.getBool(firebaseData, path + "/lightSwitch")) {
//
//        if (firebaseData.dataType() == "boolean") {
//          if(firebaseData.boolData() == 1){
//              lightMethod(1);
//          }else if(firebaseData.boolData() == 0){
//              lightMethod(0);
//          }
//        }
//    
//      } else {
//        Serial.println(firebaseData.errorReason());
//      }
      
  }

 
  if (millis() - sendDataPrevMillis > 3000)
    {
        sendDataPrevMillis = millis();
        count++;

        timeClient.update();

        unsigned long epochTime = timeClient.getEpochTime();
        
        String formattedTime = timeClient.getFormattedTime();
      
        int currentHour = timeClient.getHours();
      
        int currentMinute = timeClient.getMinutes();
         
        int currentSecond = timeClient.getSeconds();

//        String currentTime = String(currentHour) + ":" + String(currentMinute) + ":" + String(currentSecond);
      
        String weekDay = weekDays[timeClient.getDay()];  
      
        //Get a time structure
        struct tm *ptm = gmtime ((time_t *)&epochTime); 
      
        int monthDay = ptm->tm_mday;
      
        int currentMonth = ptm->tm_mon+1;
      
        String currentMonthName = months[currentMonth-1];
      
        int currentYear = ptm->tm_year+1900;
      
        //Print complete date:
        String currentDate = String(currentYear) + "-" + String(currentMonth) + "-" + String(monthDay);
        String finalTime = currentDate + " " + formattedTime;
        Serial.print("Current date: ");
        Serial.println(currentDate);

        FirebaseJson json1;

        FirebaseJsonData jsonObj;

        //read temp and hum and return them in date object
        DateSenzori date = Sensor();
        char float_str[8];
        char float_str2[8];
        char line0[21];
        char line1[21];
        dtostrf(date.temperatura,4,2,float_str);
        dtostrf(date.umiditate,4,2,float_str2); 
        sprintf(line0, "Temp: %-10s", float_str);
        sprintf(line1, "Umiditate: %-7s", float_str2);
        lcd.setCursor(0,0);
        lcd.print(line0);
        lcd.setCursor(0,1);
        lcd.print(line1);


        if (scale.is_ready()) {
            float weight = scale.get_units();
            Serial.print(weight, 3);
            Serial.print(F(" kg"));
            Serial.println();
            if(weight >= 0.100 && initialWeightValue == 0){
              initialWeightValue = 1;
              weightValue = weight;
    //          SendPostRequestBookAccess(initialWeightValue);
    //          SendPutRequest(1);
    //          contor++;
            }else if(weight >= 0.100 && (weight >= (contor*weightValue - weightValue/4)) && (weight <= (contor*weightValue + weightValue/4))){
    //          SendPostRequestBookAccess(initialWeightValue);
    //          SendPutRequest(contor);
    //          contor++;
            }else if((weight - (contor-1)*weightValue < -weightValue/4) && (weight + weightValue/4 >= (contor-1)*weight - weightValue) && contor > 2 && weight >= 0.100 ){
    //          SendPostRequestBookAccess(0);
    //          contor--;
    //          SendPutRequest(contor - 1);
            }else if(weight < 0.100 && initialWeightValue != 0){
              initialWeightValue = 0;
              weightValue = weight;
    //          SendPostRequestBookAccess(initialWeightValue);
    //          contor = 1;
    //          SendPutRequest(contor - 1);


            }

                    //        if(date.eroare != 101){
                json1.set("sensor1/temp", date.temperatura);
                json1.set("sensor1/hum", date.umiditate);
                json1.set("sensor1/timestamp", finalTime);
                json1.set("sensor1/count", count);
                json1.set("sensor1/state", weight);
        
                //Send JSON to RTDB
                Firebase.set(firebaseData, path + "/data/" + count, json1);
    //        }
        } else {
    //        if(date.eroare != 101){
                json1.set("sensor1/temp", date.temperatura);
                json1.set("sensor1/hum", date.umiditate);
                json1.set("sensor1/timestamp", finalTime);
                json1.set("sensor1/count", count);
                json1.set("sensor1/state", "bad");
        
                //Send JSON to RTDB
                Firebase.set(firebaseData, path + "/data/" + count, json1);
    //        }
        }


        //Set Count
        Firebase.set(firebaseData, path + "/data/count", count);
    }
}


void lightMethod(int trigger){
  if(trigger == 1){
      for (int i = 0; i <= 16; i++){
        //using the CRGB function we can set any LED to any color
        //using the parameters of Red, Green and Blue color
        leds[i] = CRGB(255,69,0);
        FastLED.show();
        delay(50);  
    }
  }else if(trigger == 0){
      for (int i = 0; i <= 16; i++){
        //using the CRGB function we can set any LED to any color
        //using the parameters of Red, Green and Blue color
        leds[i] = CRGB::Black;
        //set all leds to the given color
        FastLED.show();
        delay(50); 
    }
  }
}

struct DateSenzori Sensor(){
  // start working...
  Serial.println(F("================================="));
  Serial.println(F("Sample DHT22..."));
  
  struct DateSenzori dateSenzori;
  dateSenzori.eroare = 0;
  int err = SimpleDHTErrSuccess;
  if ((err = dht22.read2(pinDHT22, &dateSenzori.temperatura, &dateSenzori.umiditate, NULL)) != SimpleDHTErrSuccess) {
    Serial.print(F("Read DHT22 failed, err=")); Serial.println(err);
    dateSenzori.eroare = err;
  }

  Serial.print(F("Sample OK: "));
  Serial.print((float)dateSenzori.temperatura); Serial.print(F(" *C, "));
  Serial.print((float)dateSenzori.umiditate); Serial.println(F(" RH%"));

  return dateSenzori;
}


//void SendPutRequest(int numar_carti){
//  if (WiFi.status() == WL_CONNECTED) { 
// 
//    StaticJsonBuffer<300> JSONbuffer; 
//    JsonObject& JSONencoder = JSONbuffer.createObject(); 
//
//    JSONencoder["numar_carti"] = String(numar_carti);
//
//    char JSONmessageBuffer[300];
//    JSONencoder.prettyPrintTo(JSONmessageBuffer, sizeof(JSONmessageBuffer));
//    Serial.println(JSONmessageBuffer);
// 
//    HTTPClient http; 
// 
//    http.begin("http://lucrare-licenta.run.goorm.io/users/1/rooms/1/commands/1");      
//    http.addHeader("Content-Type", "application/json");
// 
//    int httpCode = http.PUT(JSONmessageBuffer);
//    String payload = http.getString();
// 
//    Serial.println(httpCode);
//    Serial.println(payload);
// 
//    http.end();
// 
//  } else {
//    Serial.println(F("Error in WiFi connection"));
//  }
//}
//
//Command SendGetRequest(){
//  if (WiFi.status() == WL_CONNECTED) { 
//    HTTPClient http;  
// 
//    http.begin("http://lucrare-licenta.run.goorm.io/users/1/rooms/1/commands/1");
//    http.addHeader("Content-Type", "application/json");  
//    struct Command command;
//    command.httpCode = http.GET();
//    const size_t capacity = JSON_OBJECT_SIZE(10) + 130;
//    DynamicJsonBuffer jsonBuffer(capacity);
//    JsonObject& root = jsonBuffer.parseObject(http.getString());
//    command.stare_ptc = root["stare_ptc"];
//    command.stare_ventilator = root["stare_ventilator"];
//    command.stare_led = root["stare_led"];
//    command.tmin = root["tmin"];
//    command.tmax = root["tmax"];
//    command.hmin = root["hmin"];
//    command.hmax = root["hmax"];
//    command.numar_carti = root["numar_carti"];
//    Serial.println(command.httpCode);
//    http.end();
//    return command;
//    
//  } else {
//    Serial.println(F("Error in WiFi connection"));
//  }
//}
