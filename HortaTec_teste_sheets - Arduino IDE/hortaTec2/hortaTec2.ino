#include <DHT.h>
#include <Wire.h>
#include <SPI.h>
#include <PubSubClient.h>
#include "WiFi.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFiClientSecure.h>

#define DHT_SENSOR_PIN  11
#define DHT_SENSOR_TYPE DHT22
#define UV_SENSOR_PIN 8
#define TS_SENSOR_PIN 12
#define HS_SENSOR_PIN 20
#define NPK_SENSOR_DI 2
#define NPK_SENSOR_RO 3
#define NPK_SENSOR_RE 4
#define NPK_SENSOR_DE 5
#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  180        /* Time ESP32 will go to sleep (in seconds) */

RTC_DATA_ATTR int bootCount = 0;

//==================== WiFi and MQTT vars ====================================
  const char* ssid = "MEO-03B7A0";
  const char* password =  "ec6ca3a2bc";

//----------------------------------------Host & httpsPort
const char* host = "script.google.com";
const int httpsPort = 443;
//----------------------------------------

WiFiClientSecure client; //--> Create a WiFiClientSecure object.

String GAS_ID = "AKfycbyWy4r86H31y8ay5JqUzm5cTMA90nsL0mk1OJfP8biWm0zksKU"; //--> spreadsheet script ID



//==================== DHT vars ====================================
  float humi; // read humidity
  float tempC; // read temperature in Celsius
  DHT dht_sensor(DHT_SENSOR_PIN, DHT_SENSOR_TYPE);

//==================== UV vars =====================================
  float UVsensorVoltage; 
  float UVsensorValue;

//==================== Humidade Solo vars ==========================
  int soilMoistureValue;
  int percentage;

//==================== Temperatura Solo vars and setup =============
  const int oneWireBus = TS_SENSOR_PIN;
  OneWire oneWire(oneWireBus);
  DallasTemperature sensors(&oneWire);
  float ds18temperatureC;

//==================== NPK vars ==========================
  float nitrogen;
  float phosphorus;
  float potassium;
  

void setup() {
  Serial.begin(9600);
  
  WiFi.begin(ssid, password);
  Serial.println("Connecting to WiFi..");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("Connected to the WiFi network");
  
  dht_sensor.begin(); // initialize the DHT sensor
  
  sensors.begin(); // Start the DS18B20 sensor

  client.setInsecure();

  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);

  funcDHT();
  funcUV();
  funcHSolo();
  funcDS18();
  separador();
  sendData(tempC,humi,ds18temperatureC,UVsensorVoltage,percentage);

  esp_deep_sleep_start();
}

void funcDS18() {
  sensors.requestTemperatures(); 
  ds18temperatureC = sensors.getTempCByIndex(0);
  Serial.print("DS18 : ");
  Serial.print(ds18temperatureC);
  Serial.println("ºC");
}

void funcDHT(){
  // aux in case of error
  float auxh = humi;
  float auxt = tempC;
  // read humidity
  humi  = dht_sensor.readHumidity();
  // read temperature in Celsius
  tempC = dht_sensor.readTemperature();

  // check whether the reading is successful or not
  if ( isnan(tempC) || isnan(humi)) {
    Serial.println("Failed to read from DHT sensor!");
    //takes the previous value.
    tempC = auxt;
    humi = auxh;
  } else {
    Serial.print("Humidity: ");
    Serial.print(humi);
    Serial.print("%");
    Serial.print("  |  ");
    Serial.print("Temperature: ");
    Serial.print(tempC);
    Serial.println("°C");
  }
}

void funcUV(){ 
  UVsensorValue = analogRead(15);
  UVsensorVoltage = UVsensorValue/4095*3.3;
  Serial.print("UV sensor reading = ");
  Serial.print(UVsensorValue);
  Serial.println("");
  Serial.print("UV sensor voltage = ");
  Serial.print(UVsensorVoltage);
  Serial.println(" V");
}

void funcHSolo(){
  int maxV = 8200;
  int minV = 4200;
   
  soilMoistureValue = analogRead(HS_SENSOR_PIN);

  //percentage = ( 1 - (((float)soilMoistureValue - (float)minV) / ((float)maxV - (float)minV))) * 100;
  percentage = 32.322428-0.0033212*soilMoistureValue -8.7409*pow(10,-7)*pow(soilMoistureValue-5825.07,2)-1.3509*pow(10,-9)*pow(soilMoistureValue-5825.07,3);

  if(percentage < 0)
    percentage = 0;
  if(percentage > 100)
    percentage = 100;
  
  Serial.print("Humidade Solo: ");
  Serial.print(percentage);
  Serial.println(" %");
}

void funcNPK(){
  
}

void separador(){
  Serial.println();
  Serial.print("=======================================================");
  Serial.println();
}

// Subroutine for sending data to Google Sheets
void sendData(float temar, float humar, float temsolo, float uv, int humsolo) {
  Serial.println("==========");
  Serial.print("connecting to ");
  Serial.println(host);
  
  //----------------------------------------Connect to Google host
  if (!client.connect(host, httpsPort)) {
    Serial.println("connection failed");
    return;
  }
  //----------------------------------------

  //----------------------------------------Processing data and sending data
  String string_temperaturear =  String(temar, 1); 
  String string_humidityar =  String(humar, 1); 
  String string_temperaturesolo =  String(temsolo, 1);
  String string_uv =  String(uv, 1);
  String string_humsolo =  String(humsolo);
  String url = "/macros/s/" + GAS_ID + "/exec?temperaturear=" + string_temperaturear + "&humidityar=" + string_humidityar + "&temperaturesolo=" + string_temperaturesolo + "&uvindex=" + string_uv + "&humidadesolo=" + string_humsolo;
  Serial.print("requesting URL: ");
  Serial.println(url);

  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
         "Host: " + host + "\r\n" +
         "User-Agent: BuildFailureDetectorESP8266\r\n" +
         "Connection: close\r\n\r\n");

  Serial.println("request sent");
  //----------------------------------------

  //----------------------------------------Checking whether the data was sent successfully or not
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") {
      Serial.println("headers received");
      break;
    }
  }
  String line = client.readStringUntil('\n');
  if (line.startsWith("{\"state\":\"success\"")) {
    Serial.println("esp32s2/Arduino CI successfull!");
  } else {
    Serial.println("esp32s2/Arduino CI has failed");
  }
  Serial.print("reply was : ");
  Serial.println(line);
  Serial.println("closing connection");
  Serial.println("==========");
  Serial.println();
  //----------------------------------------
} 


void loop() {
  
}
