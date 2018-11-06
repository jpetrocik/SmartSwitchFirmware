#include <FS.h>

#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino

//needed for library
#include <ArduinoOTA.h>
#include <DNSServer.h>
#include <WiFiManager.h>         //https://github.com/tzapu/WiFiManager
#include <Ticker.h>
#include <ArduinoJson.h>
#include <ESP8266mDNS.h>
#include <PubSubClient.h>

#include <WiFiClient.h>
#include <ESP8266WebServer.h>

#include "configuration.h"

Ticker ticker;

int lastKnownDoorStatus = -1;

char jsonStatusMsg[140];

//configuration properties
char deviceName[20] = "door";
char locationName[20] = "garage";
char mqttServer[50] = "mqtt.local";
char hostname[41];
char commandTopic[70];
char statusTopic[70];

void setup() {
  Serial.begin(115200);

  pinMode(ONBOARD_LED, OUTPUT);
  pinMode(RELAY, OUTPUT);
  pinMode(DOOR_STATUS, INPUT);


  //slow ticker when starting up
  //switch to fast tick when in AP mode
  ticker.attach(0.6, tick);

  configLoad();

  wifiSetup();

  mdnsSetup();

  otaSetup();
  
  mqttSetup();

  webServerSetup();

  ticker.detach();
  
  Serial.println("SmartGarage Firmware");
  Serial.println(__DATE__ " " __TIME__);
  Serial.println(hostname);

  lastKnownDoorStatus = digitalRead(DOOR_STATUS);
  digitalWrite(ONBOARD_LED, !lastKnownDoorStatus);
}

void loop() {
  mqttLoop();

  otaLoop();

  webServerLoop();

  sendDoorStatusOnChange();
}

void toogleDoor(){
  digitalWrite(RELAY, HIGH);
  delay(500);
  digitalWrite(RELAY, LOW);
}

void sendCurrentDoorStatus() {
  int doorState = !digitalRead(DOOR_STATUS);
  sprintf (jsonStatusMsg, "{\"status\":%s}", doorState ? "\"OFF\"" : "\"ON\"");

  mqttSendStatus();
}

void sendDoorStatusOnChange() {
  int doorStatus = digitalRead(DOOR_STATUS);

  if (doorStatus != lastKnownDoorStatus) {
    sendCurrentDoorStatus();
    lastKnownDoorStatus = doorStatus;
    digitalWrite(ONBOARD_LED, !doorStatus);
  }
}

void factoryReset() {
  Serial.println("Restoring Factory Setting....");
  WiFi.disconnect();
  SPIFFS.format();
  delay(500);
  Serial.println("Restarting....");
  ESP.restart();
}

void tick() {
  int state = digitalRead(ONBOARD_LED);
  digitalWrite(ONBOARD_LED, !state);
}

//Called to save the configuration data after
//the device goes into AP mode for configuration
void configSave() {
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["device"] = deviceName;
    json["location"] = locationName;
    json["mqttServer"] = mqttServer;
    sprintf (hostname, "%s-%s", locationName, deviceName);

    File configFile = SPIFFS.open("/config.json", "w");
    if (configFile) {
      Serial.println("Saving config data....");
      json.prettyPrintTo(Serial);
      json.printTo(configFile);
      configFile.close();
    }
}

//Loads the configuration data on start up
void configLoad() {
  Serial.println("Loading config data....");
  if (SPIFFS.begin()) {
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        size_t size = configFile.size();
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);

        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        if (json.success()) {
          json.prettyPrintTo(Serial);

          if (json.containsKey("device")) {
            strncpy(deviceName, json["device"], 20);
          } 
                 
          if (json.containsKey("location")) {
            strncpy(locationName, json["location"], 20);
          }

          sprintf (hostname, "%s-%s", locationName, deviceName);

          if (json.containsKey("mqttServer")) {
            strncpy(mqttServer, json["mqttServer"], 50);
          } else {
            mqttServer[0]=0;
          }
        }
      }
    }
  }
}

