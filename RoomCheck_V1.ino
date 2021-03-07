#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SimpleDHT.h>
#include <SPI.h>
#include <Ethernet.h>
#include "HX711.h"
#include <FastLED.h>
#include <FirebaseESP8266.h>


#define gasSensor A0
#define pinLight 1 
#define pinDHT22 D4
#define pinHall D0
#define releuPTC D7
#define releuVentilator D8
#define LOADCELL_DOUT_PIN D2
#define LOADCELL_SCK_PIN D3
#define NUM_LEDS 16
#define WIFI_SSID "HUAWEI-W2Nw"
#define WIFI_PASSWORD "11223344"

#define FIREBASE_HOST "roomcheck-21820-default-rtdb.firebaseio.com"

/** The database secret is obsoleted, please use other authentication methods, 
 * see examples in the Authentications folder. 
*/
#define FIREBASE_AUTH "pKEnDYo2UAoINWJ7HwjZTssU9rSkjxzX2i1Emwh6"

//Define FirebaseESP8266 data object
FirebaseData firebaseData;

FirebaseJson json;

void printResult(FirebaseData &data);

unsigned long sendDataPrevMillis = 0;

uint16_t count = 0;

uint8_t i = 0;

void setup()
{

  Serial.begin(115200);

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
}



void loop()
{
  if (millis() - sendDataPrevMillis > 2000)
    {
        sendDataPrevMillis = millis();
        count++;

        String path = "/Room";

        FirebaseJson json1;

        FirebaseJsonData jsonObj;
    
        json1.set("sensor1/temp", 200);
        json1.set("sensor1/hum", 300);

        //Send JSON to RTDB
        Firebase.set(firebaseData, path + "/" + count, json1);

//        Serial.print(firebaseData.payload());

        //Get data from RTDB
        if (Firebase.getInt(firebaseData, path + "/" + 1 + "/" + "sensor1/temp")) {

          if (firebaseData.dataType() == "int") {
            Serial.println(firebaseData.intData());
          }
      
        } else {
          Serial.println(firebaseData.errorReason());
        }

        if (Firebase.getJSON(firebaseData, path + "/" + 1 + "/" + "sensor1")) {

          if (firebaseData.dataType() == "json")
          {
            Serial.println();
            FirebaseJson &json = firebaseData.jsonObject();
            //Print all object data
            Serial.println("Pretty printed JSON data:");
            String jsonStr;
            json.toString(jsonStr, true);
            Serial.println(jsonStr);
            Serial.println();
            Serial.println("Iterate JSON data:");
            Serial.println();
            size_t len = json.iteratorBegin();
            String key, value = "";
            int type = 0;
            for (size_t i = 0; i < len; i++)
            {
              json.iteratorGet(i, type, key, value);
              Serial.print(i);
              Serial.print(", ");
              Serial.print("Type: ");
              Serial.print(type == FirebaseJson::JSON_OBJECT ? "object" : "array");
              if (type == FirebaseJson::JSON_OBJECT)
              {
                Serial.print(", Key: ");
                Serial.print(key);
              }
              Serial.print(", Value: ");
              Serial.println(value);
            }
            json.iteratorEnd();
          }
      
        }

        if(Firebase.get(firebaseData, "/Room"))
        {
          //Success
          Serial.print("Get variant data success, type = ");
          Serial.println(firebaseData.dataType());
      
        }

        if (Firebase.getJSON(firebaseData, path)) {
          if (firebaseData.dataType() == "array")
          {
            Serial.println();
            //get array data from FirebaseData using FirebaseJsonArray object
            FirebaseJsonArray &arr = firebaseData.jsonArray();
            //Print all array values
            Serial.println("Pretty printed Array:");
            String arrStr;
            arr.toString(arrStr, true);
            Serial.println(arrStr);
            Serial.println();
            Serial.println("Iterate array values:");
            Serial.println();
            for (size_t i = 0; i < arr.size(); i++)
            {
              Serial.print(i);
              Serial.print(", Value: ");
        
              FirebaseJsonData &jsonData = firebaseData.jsonData();
              //Get the result data from FirebaseJsonArray object
              arr.get(jsonData, i);
              if (jsonData.typeNum == FirebaseJson::JSON_BOOL)
                Serial.println(jsonData.boolValue ? "true" : "false");
              else if (jsonData.typeNum == FirebaseJson::JSON_INT)
                Serial.println(jsonData.intValue);
              else if (jsonData.typeNum == FirebaseJson::JSON_FLOAT)
                Serial.println(jsonData.floatValue);
              else if (jsonData.typeNum == FirebaseJson::JSON_DOUBLE)
                printf("%.9lf\n", jsonData.doubleValue);
              else if (jsonData.typeNum == FirebaseJson::JSON_STRING ||
                       jsonData.typeNum == FirebaseJson::JSON_NULL ||
                       jsonData.typeNum == FirebaseJson::JSON_OBJECT ||
                       jsonData.typeNum == FirebaseJson::JSON_ARRAY)
                Serial.println(jsonData.stringValue);
            }
          }  
        }
    }
}
