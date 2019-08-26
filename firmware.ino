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
#include <ESP8266httpUpdate.h>
#include <Bounce2.h>

#include "configuration.h"
#include "settings.h"

Ticker ticker;

Bounce doorSwitch = Bounce();

char jsonStatusMsg[140];

//configuration properties
char deviceName[20] = "Garage Door";
char deviceToken[40] = "";
char registeredPhone[15] = "";

char hostname[20] = "garage";

void setup() {
  Serial.begin(115200);

  pinMode(ONBOARD_LED, OUTPUT);
  pinMode(RELAY, OUTPUT);

  //setup pin read with bounce protection
  doorSwitch.attach(DOOR_STATUS, INPUT_PULLUP); 
  doorSwitch.interval(500);

  //sprintf (hostname, "garage_%08X", ESP.getChipId());

  //slow ticker when starting up
  //switch to fast tick when in AP mode
  ticker.attach(0.6, tick);

  configLoad();

  wifiSetup();

  mdnsSetup();

#ifdef OTA_ENABLED
  otaSetup();
#endif

  mqttSetup();

  webServerSetup();

  ticker.detach();

  digitalWrite(ONBOARD_LED, !doorSwitch.read());

  Serial.println("SmartGarage Firmware");
  Serial.println(__DATE__ " " __TIME__);
  Serial.println(deviceToken);

}

void loop() {

  mqttLoop();

#ifdef OTA_ENABLED
  otaLoop();
#endif

  webServerLoop();

  sendDoorStatusOnChange();
}

void openDoor() {
  if (!doorSwitch.read()) {
    toogleDoor();
  }
}

void closeDoor() {
  if (doorSwitch.read()) {
    toogleDoor();
  }
}

void toogleDoor() {
  digitalWrite(RELAY, HIGH);
  delay(500);
  digitalWrite(RELAY, LOW);
}

void sendCurrentDoorStatus() {
  int doorState = !doorSwitch.read();
  sprintf (jsonStatusMsg, "{\"status\":%s}", doorState ? "\"OFF\"" : "\"ON\"");

  mqttSendMsg(jsonStatusMsg);
}

void sendDoorStatusOnChange() {
  boolean changed = doorSwitch.update();

  //if the button state has changed, record when and current state
  if (changed) {
    sendCurrentDoorStatus();
    int doorState = !doorSwitch.read();
    digitalWrite(ONBOARD_LED, doorState);
  }

}

void factoryReset() {
  Serial.println("Restoring Factory Setting....");
  WiFi.disconnect();
  SPIFFS.format();
  ESP.eraseConfig();
  Serial.println("Restarting....");
  delay(500);
  ESP.restart();
}

void updateFirmware() {
  WiFiClient wifiClient;
  ESPhttpUpdate.setLedPin(ONBOARD_LED, LOW);
  ESPhttpUpdate.update(wifiClient, "http://petrocik.net/~john/garage.bin");
}

void tick() {
  int state = digitalRead(ONBOARD_LED);
  digitalWrite(ONBOARD_LED, !state);
}

//Called to save the configuration data after
//the device goes into AP mode for configuration
void configSave() {
  DynamicJsonDocument doc(1024);
  JsonObject json = doc.to<JsonObject>();
  json["deviceName"] = deviceName;
  json["registeredPhone"] = registeredPhone;

  if (strlen(deviceToken) >= 20)
    json["deviceToken"] = deviceToken;

  File configFile = SPIFFS.open("/config.json", "w");
  if (configFile) {
    Serial.println("Saving config data....");
    serializeJson(json, Serial);
    Serial.println();
    serializeJson(json, configFile);
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

        DynamicJsonDocument jsonDoc(1024);
        DeserializationError error = deserializeJson(jsonDoc, buf.get());
        serializeJsonPretty(jsonDoc, Serial);

        JsonObject json = jsonDoc.as<JsonObject>();
        if (json.containsKey("deviceToken")) {
          strncpy(deviceToken, json["deviceToken"], 40);
        }

        if (json.containsKey("deviceName")) {
          strncpy(deviceName, json["deviceName"], 20);
        }

        if (json.containsKey("registeredPhone")) {
          strncpy(registeredPhone, json["registeredPhone"], 15);
        }

      }
    }
  }
}

