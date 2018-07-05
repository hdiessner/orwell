#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <BH1750.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_Sensor.h>
#include "Adafruit_BME680.h"
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>

#define UPDATE_HOST   "192.168.2.5"
#define UPDATE_PORT   2342
#define UPDATE_PATH   ""
#define BUILD_VERSION "Orwell-01"


#define MQTT_HOST     "192.168.2.5"
#define MQTT_PORT     1883

#define MOTION          10

WiFiClient      espClient;
PubSubClient    client(espClient);
Adafruit_BME680 bme;
BH1750          lightMeter(0x23);

bool     bmeOK              = false;
bool     BHOK               = false;
uint32_t lastStatusMessage  = 0;
uint32_t lastFirmwareCheck  = 0;
uint32_t lastMotion         = 0;
uint32_t lastBME680         = 0;
uint32_t lastBH1750         = 0;

char  topic[100];
char  msg[100];

void checkForNewFirmware(void){

    t_httpUpdate_return ret = ESPhttpUpdate.update(UPDATE_HOST, UPDATE_PORT, UPDATE_PATH, BUILD_VERSION);

    switch(ret) {
        case HTTP_UPDATE_FAILED:
            Serial.println("[update] Update failed.");
            break;
        case HTTP_UPDATE_NO_UPDATES:
            Serial.println("[update] No Update.");
            break;
        case HTTP_UPDATE_OK:
            Serial.println("[update] Update ok."); // may not called we reboot the ESP
            break;
    }

}

void connectToWifi(void){
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    String clientId = BUILD_VERSION;
    clientId += String(random(0xffff), HEX);
    Serial.println(clientId.c_str());
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      snprintf (topic, sizeof(topic), "orwell/test/startup");
      client.publish(topic, BUILD_VERSION);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void setup(){

    pinMode(MOTION, INPUT);

    Serial.begin(9600);

    for(uint8_t t = 5; t > 0; t--) {
        Serial.printf("[SETUP] WAIT %d...\n", t);
        Serial.flush();
        delay(500);
    }

    Serial.print("My version:");
    Serial.println(BUILD_VERSION);

    connectToWifi();

    client.setServer(MQTT_HOST, MQTT_PORT);
    reconnect();

    if (bme.begin()) {
        bmeOK = true;
        bme.setTemperatureOversampling(BME680_OS_8X);
        bme.setHumidityOversampling(BME680_OS_2X);
        bme.setPressureOversampling(BME680_OS_4X);
        bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
        bme.setGasHeater(320, 150); // 320*C for 150 ms
    } else {
      snprintf (topic, sizeof(topic), "orwell/test/error");
      client.publish(topic, "Error BME680 not present, deactivate");
    }

    if (lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE_2)) {
        BHOK = true;
    }else{
        BHOK = false;
        snprintf (topic, sizeof(topic), "orwell/test/error");
        client.publish(topic, "Error BH1750 not present, deactivate");
    }

}

void loop(){

  if (WiFi.status() != WL_CONNECTED) {
    connectToWifi();
  }

  if (!client.connected()) {
    reconnect();
  }

  client.loop();

  if (millis() < lastStatusMessage) {
    lastStatusMessage = 0;
  }

  if (millis() - lastStatusMessage > (23 * 1000)) {
    lastStatusMessage = millis();
    snprintf (topic, sizeof(topic), "orwell/test/status");
    snprintf (msg, sizeof(msg), "{\"uptime\":%lu, \"version\":\"%s\"}", millis(), BUILD_VERSION);
    client.publish(topic, msg);
  }

  if (millis() < lastFirmwareCheck) {
    lastFirmwareCheck = 0;
  }

  if (millis() - lastFirmwareCheck > (1 * 60 * 1000)) {
    lastFirmwareCheck = millis();
    checkForNewFirmware();
  }

  if (millis() < lastMotion) {
    lastMotion = 0;
  }

  if (digitalRead(MOTION)) {
    if (millis() - lastMotion > 1000){
      lastMotion = millis();
      snprintf (topic, sizeof(topic), "orwell/test/motion");
      snprintf (msg, sizeof(msg), "%ld", millis());
      client.publish(topic, msg);
    }
  }

  if (millis() < lastBME680) {
    lastBME680 = 0;
  }

  if (bmeOK){
    if (millis() - lastBME680 > 5000){
        lastBME680 = millis();

        if (! bme.performReading()) {
            client.publish("orwell/test/error", "can't read bme680");
        } else {
            snprintf (topic, sizeof(topic), "orwell/test/temperature");
            snprintf (msg, sizeof(msg), "%f", bme.temperature);
            client.publish(topic, msg);

            snprintf (topic, sizeof(topic), "orwell/test/pressure");
            snprintf (msg, sizeof(msg), "%f", bme.pressure / 100.0);
            client.publish(topic, msg);

            snprintf (topic, sizeof(topic), "orwell/test/humidity");
            snprintf (msg, sizeof(msg), "%f", bme.humidity);
            client.publish(topic, msg);

            snprintf (topic, sizeof(topic), "orwell/test/gas");
            snprintf (msg, sizeof(msg), "%f", bme.gas_resistance / 1000.0);
            client.publish(topic, msg);
        }
    }
  }

  if (millis() < lastBH1750) {
    lastBH1750 = 0;
  }

  if (BHOK){
    if (millis() - lastBH1750 > 1000){
        lastBH1750 = millis();

        snprintf (topic, sizeof(topic), "orwell/test/light");
        snprintf (msg, sizeof(msg), "%d", lightMeter.readLightLevel());
        client.publish(topic, msg);
    }
  }

  delay(10);

}
