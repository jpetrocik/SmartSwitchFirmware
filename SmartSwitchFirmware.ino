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
#include "devices.h"

Ticker ticker;

//current status of relay
int relayState = RELAY_OFF;

//maintains when the button was pressed
//action is taken on button release
//this allows for long press features
unsigned long buttomPressed = false; //time button was press, so must be unsigned long
long bounceTimeout = 0;

//configuration properties
char deviceName[20] = "light";
char roomName[20] = "bedroom";
char locationName[20] = "house";
char hostname[41] = "light-bedroom";
char mqttServer[50];
int relayPin = GPIO12;
int ledPin = GPIO13;
int buttonPin = GPIO00;
int maxOnTimer = 0;

//set when AP mode is enabled to indicating the
//config data needs to be saved
bool shouldSaveConfig = false;

//time after which light will turn off
long delayOffTime = 0;

//contains last json status message
char jsonStatusMsg[140];

void setup() {
  Serial.begin(115200);

  configLoad();

  Serial.println("SmartHome Firmware");
  Serial.println(__DATE__ " " __TIME__);
  Serial.println(hostname);

  pinMode(ledPin, OUTPUT);
  pinMode(relayPin, OUTPUT);
  pinMode(buttonPin, INPUT);

  digitalWrite(relayPin, RELAY_OFF);

  //slow ticker when starting up
  //switch to fast tick when in AP mode
  ticker.attach(0.6, tick);

  wifiSetup();

  mdnsSetup();

  ticker.detach();

  //init over the air update server
  otaSetup();

  //connect to mqtt servier
  mqttSetup();

  webServerSetup();

  digitalWrite(ledPin, LED_OFF);
}

void loop() {
  long now = millis();

  //check for delayed off timer
  if (delayOffTime > 0 && delayOffTime < now) {
    Serial.println("Delayed turning off");
    turnOff();
  }

  //action is taken when button is released. any
  //button pressed within 100ms is consider a bounce and
  //ignored.
  //
  //<1.5sec: toggle relay
  //>1.5: delay off sec * minutes
  if (now - bounceTimeout  > 100 ) {

    //button initially pressed
    if (!digitalRead(buttonPin) && !buttomPressed) {
      Serial.println("Button pressed....");
      buttomPressed = now;
      bounceTimeout = now;
      toogle();

      //button is released
    } else if (digitalRead(buttonPin) && buttomPressed) {
      Serial.println("Button released....");

      //delayed off timer
      if ((now - buttomPressed > 1000) && (relayState == RELAY_ON)) {
        delayOffTime = (now - buttomPressed) * 60;
        Serial.print("Delay timer set for ");
        Serial.println(delayOffTime);
        delayOffTime += now;
      }

      buttomPressed = false;
      bounceTimeout = now;

      //after 30sec perfrom factory reset
    } else if (buttomPressed && (now - buttomPressed) > 30000 ) {
      ticker.attach(0.2, tick);
      delay(5000);
      factoryReset();
    }
  }

  mqttLoop();

  otaLoop();

  webServerLoop();
}

void toogle() {
  if (relayState == RELAY_ON) {
    turnOff();
  } else {
    turnOn();
  }
}

void turnOn() {
  digitalWrite(relayPin, RELAY_ON);
  digitalWrite(ledPin, LED_ON);
  relayState = RELAY_ON;

  //cancel delayTimer
  delayOffTime = 0;

  if (maxOnTimer > 0) {
    long now = millis();
    delayOffTime = now + (maxOnTimer * 60 * 1000);
  }

  sendCurrentStatus();
}

void turnOff() {
  digitalWrite(relayPin, RELAY_OFF);
  digitalWrite(ledPin, LED_OFF);
  relayState = RELAY_OFF;

  //reset or cancel delayTimer
  delayOffTime = 0;

  sendCurrentStatus();
}

void sendCurrentStatus() {
  long remainingTimer  = delayOffTime - millis();
  sprintf (jsonStatusMsg, "{\"status\":%s,\"delayOff\":\"%i\"}", relayState ? "\"ON\"" : "\"OFF\"", remainingTimer > 0 ? remainingTimer : 0);

  mqttSendStatus();
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
  int state = digitalRead(ledPin);
  digitalWrite(ledPin, !state);
}

//Called to save the configuration data after
//the device goes into AP mode for configuration
void configSave() {
  DynamicJsonDocument jsonDoc(1024);
  JsonObject json = jsonDoc.to<JsonObject>();

  json["device"] = deviceName;
  json["room"] = roomName;
  json["location"] = locationName;
  json["mqttServer"] = mqttServer;
  json["relay"] = relayPin;
  json["led"] = ledPin;
  json["button"] = buttonPin;
  json["maxOnTimer"] = maxOnTimer;
  sprintf (hostname, "%s-%s", roomName, deviceName);

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

        DynamicJsonDocument jsonDoc(size);
        DeserializationError error = deserializeJson(jsonDoc, buf.get());
        serializeJsonPretty(jsonDoc, Serial);

        JsonObject json = jsonDoc.as<JsonObject>();

        if (json.containsKey("device")) {
          strncpy(deviceName, json["device"], 20);
        }

        if (json.containsKey("room")) {
          strncpy(roomName, json["room"], 20);
        }

        if (json.containsKey("location")) {
          strncpy(locationName, json["location"], 20);
        }

        sprintf (hostname, "%s-%s", roomName, deviceName);

        if (json.containsKey("mqttServer")) {
          strncpy(mqttServer, json["mqttServer"], 50);
        } else {
          mqttServer[0] = 0;
        }

        if (json.containsKey("relay")) {
          relayPin = json["relay"];
        }

        if (json.containsKey("led")) {
          ledPin = json["led"];
        }

        if (json.containsKey("button")) {
          buttonPin = json["button"];
        }

        if (json.containsKey("maxOnTimer")) {
          maxOnTimer = json["maxOnTimer"];
        }

      }
    }
  }
}








