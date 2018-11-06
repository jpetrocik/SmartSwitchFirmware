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

int lastKnownDoorStatus = -1;

//configuration properties
char deviceName[20] = "opener";
char locationName[20] = "garage";
char mqttServer[50] = "mqtt.local";
char hostname[41];
char commandTopic[70];
char statusTopic[70];


//set when AP mode is enabled to indicating the
//config data needs to be saved
bool shouldSaveConfig = false;

//mqtt client
WiFiClient espClient;
PubSubClient mqClient(espClient);

void setup() {
  Serial.begin(115200);

  pinMode(ONBOARD_LED, OUTPUT);
  pinMode(RELAY, OUTPUT);
  pinMode(DOOR_STATUS, INPUT);


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

  const char version[] = "v1.0-beta" __DATE__ " " __TIME__;
  Serial.println("SmartGarage Firmware");
  Serial.println(version);
  Serial.println(hostname);
  Serial.println(commandTopic);
  Serial.println(statusTopic);

  lastKnownDoorStatus = digitalRead(DOOR_STATUS);
  digitalWrite(ONBOARD_LED, lastKnownDoorStatus);

}

void loop() {
  //Connect or read message
  if (!mqClient.connected()) {
    mqttConnect();
  } else {
    mqClient.loop();
  }

  sendDoorStatusOnChange();
  
  //Check for OTA Updates
  ArduinoOTA.handle();


  webServerLoop();
}

void openDoor() {
  //if (lastKnownDoorStatus == DOOR_CLOSED){
    toogleDoor();
  //} 
}

void closeDoor() {
//  if (lastKnownDoorStatus == DOOR_OPEN){
    toogleDoor();
//  } 
}

void toogleDoor(){
  digitalWrite(RELAY, HIGH);
  delay(500);
  digitalWrite(RELAY, LOW);
}

void sendCurrentDoorStatus() {
  int doorState = !digitalRead(DOOR_STATUS);

  char jsonStatusMsg[140];
  sprintf (jsonStatusMsg, "{\"status\":%s}", doorState ? "\"OFF\"" : "\"ON\"");
    
  mqClient.publish((char *)statusTopic, (char *)jsonStatusMsg);

}

void sendDoorStatusOnChange() {
  int doorStatus = digitalRead(DOOR_STATUS);

  if (doorStatus != lastKnownDoorStatus) {
    sendCurrentDoorStatus();
    lastKnownDoorStatus = doorStatus;
    digitalWrite(ONBOARD_LED, !doorStatus);
    
  }
}

void tick() {
  int state = digitalRead(ONBOARD_LED);
  digitalWrite(ONBOARD_LED, !state);
}

/******************************************
 * mDNS
 ******************************************/
void mdnsInit() {
  Serial.println("Starting mDNS server....");
  MDNS.begin(hostname);
  MDNS.addService("socket", "tcp", 80);
}


/******************************************
 * WifiManager
 ******************************************/
void wifiSetup() {
  Serial.println("Setting up wifi connection....");
  WiFiManager wifiManager;

  WiFiManagerParameter wifiDeviceNameParam("device", "Name", deviceName, 20);
  WiFiManagerParameter wifiLocationNameParam("location", "Location", locationName, 20);
  WiFiManagerParameter wifiMqttServerParam("mqtt", "MQTT Server", mqttServer, 50);

  wifiManager.setAPCallback(configModeCallback);
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  wifiManager.addParameter(&wifiDeviceNameParam);
  wifiManager.addParameter(&wifiLocationNameParam);
  wifiManager.addParameter(&wifiMqttServerParam);
  wifiManager.autoConnect("SmartHome");//-" + ESP.getChipId());

  strncpy(deviceName, wifiDeviceNameParam.getValue(), 20);
  strncpy(locationName, wifiLocationNameParam.getValue(), 20);
  strncpy(mqttServer, wifiMqttServerParam.getValue(), 50);
}

//call by WifiManager when entering AP mode
void configModeCallback (WiFiManager *myWiFiManager) {
}
//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Entering AP Mode....");
  shouldSaveConfig = true;
}

//Called to save the configuration data after
//the device goes into AP mode for configuration
void configSave() {
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["device"] = deviceName;
    json["location"] = locationName;
    json["mqttServer"] = mqttServer;
    json.set("relay",relayPin);
    json.set("led",ledPin);
    json.set("button",buttonPin);
    json.set("maxOnTimer",maxOnTimer);
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
          
          if (json.containsKey("relay")) {
            relayPin = json.get<signed int>("relay");
          }
            
          if (json.containsKey("led")) {
            ledPin = json.get<signed int>("led");
          }

          if (json.containsKey("button")) {
            buttonPin = json.get<signed int>("button");
          }

          if (json.containsKey("maxOnTimer")) {
            maxOnTimer = json.get<signed int>("maxOnTimer");
          }
          

        }
      }
    }
  }
}





//callback when a mqtt message is recieved
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  if ((char)payload[0] == '1') {
    openDoor();
  } else if ((char)payload[0] == '0') {
    closeDoor();
  } else if ((char)payload[0] == '3') {
    sendCurrentDoorStatus();
  }
}



