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
#include <Bounce2.h>

#include "configuration.h"
#include "devices.h"

Ticker ticker;

//current status of relay
int relayState = RELAY_OFF;

//configuration properties
char deviceName[20] = "fan";
char locationName[20] = "hallway";
char hostname[41] = "fan-hallway";
char mqttServer[50];

int highRelayPin = 13;
int lowRelayPin = 15;
int highButtonPin = 12;
int lowButtonPin = 5;
int offButtonPin = 3;
int maxOnTimer = 0;

//set when AP mode is enabled to indicating the
//config data needs to be saved
bool shouldSaveConfig = false;

//time after which light will turn off
long delayOffTime = 0;

//contains last json status message
char jsonStatusMsg[140];

Bounce highSwitch = Bounce();
Bounce lowSwitch = Bounce();
Bounce offSwitch = Bounce();

void setup() {
  Serial.begin(115200);

  configLoad();

  Serial.println("SmartSwitch Firmware");
  Serial.println(__DATE__ " " __TIME__);
  Serial.println(hostname);

  pinMode(highRelayPin, OUTPUT);
  pinMode(lowRelayPin, OUTPUT);

  digitalWrite(highRelayPin, RELAY_OFF);
  digitalWrite(lowRelayPin, RELAY_OFF);

  pinMode(highButtonPin, INPUT_PULLUP);
  pinMode(lowButtonPin, INPUT_PULLUP);
  pinMode(offButtonPin, INPUT_PULLUP);

  highSwitch.attach(highButtonPin);
  highSwitch.interval(5);

  lowSwitch.attach(lowButtonPin);
  lowSwitch.interval(5);
  
  offSwitch.attach(offButtonPin);
  offSwitch.interval(5);
  
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

}

void loop() {
  long now = millis();

  //check for delayed off timer
  if (delayOffTime > 0 && delayOffTime < now){
    Serial.println("Delayed turning off");
    turnOff();
  }

  highSwitch.update();
  lowSwitch.update();
  offSwitch.update();
  
  if (highSwitch.fell()) {
      mqttSendDebug("High button pressed....");
      turnOnHigh();

  } else if (lowSwitch.fell()) {
      mqttSendDebug("Low button pressed....");
      turnOnLow();

  } else if (offSwitch.fell()) {
      mqttSendDebug("Off button pressed....");
      turnOff();

  }

  mqttLoop();

  otaLoop();

  webServerLoop();
}

void turnOnHigh() {
  turnOnSafely(highRelayPin);
}

void turnOnLow() {
  turnOnSafely(lowRelayPin);
}

void turnOnSafely(int relayPin) {

  //if changing between high or low, first turn off the relay and wait to ensure relay turns off
  turnOffQuitely();
  delay(50);

  digitalWrite(relayPin, RELAY_ON);
  relayState = RELAY_ON;
  
  //cancel delayTimer
  delayOffTime = 0;

  if(maxOnTimer > 0) {
    long now = millis();
    delayOffTime = now + (maxOnTimer * 60 * 1000);
  }     
  
  sendCurrentStatus();
}

void turnOff() {
  turnOffQuitely();
  sendCurrentStatus();
}

void turnOffQuitely() {
  digitalWrite(highRelayPin, RELAY_OFF);
  digitalWrite(lowRelayPin, RELAY_OFF);
  relayState = RELAY_OFF;

  //reset or cancel delayTimer
  delayOffTime = 0;

}

void sendCurrentStatus() {
  long remainingTimer  = delayOffTime - millis();
  sprintf (jsonStatusMsg, "{\"status\":%s,\"delayOff\":\"%i\"}", relayState ? "\"ON\"" : "\"OFF\"", remainingTimer>0?remainingTimer:0);

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
  //there is no available led to control, they are connected directly to the relay
  //int state = digitalRead(ledPin);
  //digitalWrite(ledPin, !state);
}

//Called to save the configuration data after
//the device goes into AP mode for configuration
void configSave() {
  DynamicJsonDocument jsonDoc(1024);
  JsonObject json = jsonDoc.to<JsonObject>();

    json["device"] = deviceName;
    json["location"] = locationName;
    json["mqttServer"] = mqttServer;
    json["highRelay"] = highRelayPin;
    json["lowRelay"] = lowRelayPin;
    json["highButton"] = highButtonPin;
    json["lowButton"] = lowButtonPin;
    json["offButton"] = offButtonPin;
    json["maxOnTimer"] =  maxOnTimer;
    sprintf (hostname, "%s-%s", locationName, deviceName);

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
                 
          if (json.containsKey("location")) {
            strncpy(locationName, json["location"], 20);
          }

          sprintf (hostname, "%s-%s", locationName, deviceName);

          if (json.containsKey("mqttServer")) {
            strncpy(mqttServer, json["mqttServer"], 50);
          } else {
            mqttServer[0]=0;
          }
          
          if (json.containsKey("highRelay")) {
            highRelayPin = json["highRelay"];
          }
            
          if (json.containsKey("lowRelay")) {
            lowRelayPin = json["lowRelay"];
          }
          
          if (json.containsKey("highButton")) {
            highButtonPin = json["highButton"];
          }

          if (json.containsKey("lowButton")) {
            lowButtonPin = json["lowButton"];
          }

          if (json.containsKey("offButton")) {
            offButtonPin["offButton"];
          }

          if (json.containsKey("maxOnTimer")) {
            maxOnTimer = json["maxOnTimer"];
          }
          

      }
    }
  }
}








