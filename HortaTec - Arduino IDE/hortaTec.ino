#include <DHT.h>
#include <Wire.h>
#include <SPI.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFiClientSecure.h>
#include <SoftwareSerial.h>

#define DHT_SENSOR_PIN  40
#define DHT_SENSOR_TYPE DHT22
#define UV_SENSOR_PIN 8
#define TS_SENSOR_PIN 12
#define HS_SENSOR_PIN 20
#define RELAY_PIN 36
#define NPK_PIN_DI 2
#define NPK_PIN_RE 4
#define NPK_PIN_DE 5
#define NPK_PIN_RO 3
#define RELAY_PIN 36
#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  60        /* Time ESP32 will go to sleep (in seconds) */


//==================== WiFi and MQTT vars ====================================
  const char* ssid = "MEO-03B7A0";
  const char* password =  "ec6ca3a2bc";
  const char* mqttServer =  "192.168.1.149";
  const char* mqttUsername =  "mqtt";
  const char* mqttPassword =  "mqtt";
  WiFiClient espClient;
  PubSubClient client(espClient);


//==================== DHT vars ====================================
  float humi; // read humidity
  float tempC; // read temperature in Celsius
  DHT dht_sensor(DHT_SENSOR_PIN, DHT_SENSOR_TYPE);

//==================== UV vars =====================================
  float UVsensorIndex; 
  float UVsensorValue;

//==================== Humidade Solo vars ==========================
  int soilMoistureValue;
  int percentage;

//==================== Temperatura Solo vars and setup =============
  const int oneWireBus = TS_SENSOR_PIN;
  OneWire oneWire(oneWireBus);
  DallasTemperature sensors(&oneWire);
  float ds18temperatureC;

  //==================== NPK sensor vars and inquiry frames =============
  float nitrogenio;
  float potassio;
  float fosforo;
  const byte nitro[] = {0x01,0x03, 0x00, 0x1E, 0x00, 0x01, 0xE4, 0x0C};
  const byte phos[] = {0x01,0x03, 0x00, 0x1F, 0x00, 0x01, 0xB5, 0xCC};
  const byte pota[] = {0x01,0x03, 0x00, 0x20, 0x00, 0x01, 0x85, 0xC0};
  byte values[11];
  SoftwareSerial mod(NPK_PIN_RO,NPK_PIN_DI);
  

void reconnect() 
{
  // Loop until we're reconnected
  while (!client.connected()) 
  {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP32lient-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str(), mqttUsername, mqttPassword)) 
    {
      Serial.println("connected");
    } else 
    {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(9600);

  int countWifi = 0;

  pinMode(RELAY_PIN,OUTPUT);
  
  WiFi.begin(ssid, password);
  Serial.println("Connecting to WiFi..");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    countWifi++;
    if(countWifi > 20){
      esp_deep_sleep_start();
    }
  }
  Serial.println();
  Serial.println("Connected to the WiFi network");

  client.setServer(mqttServer, 1883);
  
  dht_sensor.begin(); // initialize the DHT sensor
  
  sensors.begin(); // Start the DS18B20 sensor

  mod.begin(9600);
  pinMode(NPK_PIN_RE, OUTPUT);
  pinMode(NPK_PIN_DE, OUTPUT);

  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);

  if (!client.connected()) 
  {
    reconnect();
  }
  client.loop();

  funcDHT();
  funcUV();
  funcHSolo();
  funcDS18();
  funcNPK();
  separador();

  esp_deep_sleep_start();
}

void funcDS18() {
  sensors.requestTemperatures(); 
  ds18temperatureC = sensors.getTempCByIndex(0);
  Serial.print("DS18 : ");
  Serial.print(ds18temperatureC);
  Serial.println("ºC");
  char tempString[8];
  dtostrf(ds18temperatureC, 1, 2, tempString);
  client.publish("esp32/DS18_sensor", tempString);
}

void funcDHT(){
  // aux in case of error
  float auxh = humi;
  float auxt = tempC;
  int countdht = 0;
  // read humidity
  humi  = dht_sensor.readHumidity();
  // read temperature in Celsius
  tempC = dht_sensor.readTemperature();

  // check whether the reading is successful or not
  if ( isnan(tempC) || isnan(humi)) {
    Serial.println("Failed to read from DHT sensor!");
    //takes the previous value.
    while(countdht < 5){
      Serial.println("tentativaDHT");
      humi  = dht_sensor.readHumidity();
      tempC = dht_sensor.readTemperature();
      countdht++;
      delay(2000);

      if (!(isnan(tempC) || isnan(humi))){
        Serial.print("Humidity: ");
        Serial.print(humi);
        Serial.print("%");
        Serial.print("  |  ");
        Serial.print("Temperature: ");
        Serial.print(tempC);
        Serial.println("°C");
        char tempString1[8];
        char tempString2[8];
        dtostrf(humi, 1, 2, tempString1);
        client.publish("esp32/Humidade_ar", tempString1);
        dtostrf(tempC, 1, 2, tempString2);
        client.publish("esp32/Temperatura", tempString2);
        break;
      }
    }
  } else {
    Serial.print("Humidity: ");
    Serial.print(humi);
    Serial.print("%");
    Serial.print("  |  ");
    Serial.print("Temperature: ");
    Serial.print(tempC);
    Serial.println("°C");
    char tempString1[8];
    char tempString2[8];
    dtostrf(humi, 1, 2, tempString1);
    client.publish("esp32/Humidade_ar", tempString1);
    dtostrf(tempC, 1, 2, tempString2);
    client.publish("esp32/Temperatura", tempString2);
  }
}

void funcUV(){ 
  UVsensorValue = analogRead(UV_SENSOR_PIN);
  
  if(UVsensorValue < 275){
    UVsensorIndex = 0;
  }
  else if(UVsensorValue < 725){
    UVsensorIndex = 1;
  }
  else if(UVsensorValue < 1225){
    UVsensorIndex = 2;
  }
  else if(UVsensorValue < 1500){
    UVsensorIndex = 3;
  }
  else if(UVsensorValue < 1750){
    UVsensorIndex = 4;
  }
  else if(UVsensorValue < 2000){
    UVsensorIndex = 5;
  }
  else if(UVsensorValue < 2300){
    UVsensorIndex = 6;
  }
  else if(UVsensorValue < 2580){
    UVsensorIndex = 7;
  }
  else if(UVsensorValue < 2780){
    UVsensorIndex = 8;
  }
  else if(UVsensorValue < 3080){
    UVsensorIndex = 9;
  }
  else if(UVsensorValue < 3280){
    UVsensorIndex = 10;
  }
  else if(UVsensorValue >= 3280){
    UVsensorIndex = 11;
  }
  
  Serial.print("UV sensor reading = ");
  Serial.print(UVsensorValue);
  Serial.println("");
  Serial.print("UV sensor index = ");
  Serial.println(UVsensorIndex);

  char tempString[8];
  dtostrf(UVsensorIndex, 1, 0, tempString);
  client.publish("esp32/UV_sensor", tempString);
}

void funcHSolo(){
  
  soilMoistureValue = analogRead(HS_SENSOR_PIN);

  percentage = 32.322428-0.0033212*soilMoistureValue -8.7409*pow(10,-7)*pow(soilMoistureValue-5825.07,2)-1.3509*pow(10,-9)*pow(soilMoistureValue-5825.07,3);

  if(percentage < 0)
    percentage = 0;
  if(percentage > 100)
    percentage = 100;
  
  Serial.print("Humidade Solo: ");
  Serial.print(percentage);
  Serial.println(" %");

  char tempString[8];
  dtostrf(soilMoistureValue, 1, 2, tempString);
  client.publish("esp32/Humidade_solo", tempString);
}

byte nitrogen(){
  digitalWrite(NPK_PIN_DE,HIGH);
  digitalWrite(NPK_PIN_RE,HIGH);
  delay(10);
  if(mod.write(nitro,sizeof(nitro))==8){
    digitalWrite(NPK_PIN_DE,LOW);
    digitalWrite(NPK_PIN_RE,LOW);
    for(byte i=0;i<7;i++){
      values[i] = mod.read();
      //Serial.print(mod.read(),HEX);
      Serial.print(values[i],HEX);
    }
    Serial.println();
  }
  delay(100);
  return values[4];
}
 
byte phosphorous(){
  digitalWrite(NPK_PIN_DE,HIGH);
  digitalWrite(NPK_PIN_RE,HIGH);
  delay(10);
  if(mod.write(phos,sizeof(phos))==8){
    digitalWrite(NPK_PIN_DE,LOW);
    digitalWrite(NPK_PIN_RE,LOW);
    for(byte i=0;i<7;i++){
    //Serial.print(mod.read(),HEX);
    values[i] = mod.read();
    Serial.print(values[i],HEX);
    }
    Serial.println();
  }
  delay(100);
  return values[4];
}
 
byte potassium(){
  digitalWrite(NPK_PIN_DE,HIGH);
  digitalWrite(NPK_PIN_RE,HIGH);
  delay(10);
  if(mod.write(pota,sizeof(pota))==8){
    digitalWrite(NPK_PIN_DE,LOW);
    digitalWrite(NPK_PIN_RE,LOW);
    for(byte i=0;i<7;i++){
    //Serial.print(mod.read(),HEX);
    values[i] = mod.read();
    Serial.print(values[i],HEX);
    }
    Serial.println();
  }
  delay(100);
  return values[4];
}

void funcNPK(){
  int countNPK = 0;
  do{
    nitrogenio = nitrogen();
    delay(1000);
    fosforo = phosphorous();
    delay(1000);
    potassio = potassium();
    delay(1000);
    if(nitrogenio != 255 && fosforo != 255 && potassio != 255){
      break;
    }
    countNPK++;
  }while(countNPK<=4);

  char tempString1[8];
  char tempString2[8];
  char tempString3[8];
  dtostrf(nitrogenio, 1, 2, tempString1);
  client.publish("esp32/Nitrogenio", tempString1);
  dtostrf(fosforo, 1, 2, tempString2);
  client.publish("esp32/Fosforo", tempString2);
  dtostrf(potassio, 1, 2, tempString3);
  client.publish("esp32/Potassio", tempString3);
}

void separador(){
  Serial.println();
  Serial.print("=======================================================");
  Serial.println();
}

void loop() {
  
}
