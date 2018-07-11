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

  configLoad();

  wifiSetup();

  if (shouldSaveConfig) {
    configSave();
  }

  mdnsInit();

  ticker.detach();
  
  //init over the air update server
  otaInit();

  //connect to mqtt servier
  mqttInit();

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
    sprintf (hostname, "%s-%s", locationName, deviceName);

    File configFile = SPIFFS.open("/config.json", "w");
    if (configFile) {
      Serial.println("Saving config data....");
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
          strncpy(deviceName, json["device"], 20);
          strncpy(locationName, json["location"], 20);
          strncpy(mqttServer, json["mqttServer"], 50);
          sprintf (hostname, "%s-%s", locationName, deviceName);

        }
      }
    }
  }
}


/******************************************
 * MQTT
 ******************************************/
void mqttInit() {
  Serial.println("Connecting to MQTT Server....");
  mqClient.setServer(mqttServer, 1883);
  mqClient.setCallback(mqttCallback);

  sprintf (commandTopic, "house/%s/%s/command", locationName, deviceName);
  sprintf (statusTopic, "house/%s/%s/status", locationName, deviceName);
}

int reconnectAttemptCounter = 0;
long nextReconnectAttempt = 0;
void mqttConnect() {
  if(!mqClient.connected() && nextReconnectAttempt < millis() ) {
    
    if (mqClient.connect(hostname)) {
      Serial.println("connected");
      mqClient.subscribe(commandTopic);
      mqClient.subscribe("house/command");
      
      reconnectAttemptCounter = 0;
      nextReconnectAttempt=0;
      
    } else {
      Serial.print("Failed to connect to ");
      Serial.println(mqttServer);


      reconnectAttemptCounter++;
      nextReconnectAttempt = sq(reconnectAttemptCounter) * 1000;
      if (nextReconnectAttempt > 30000) nextReconnectAttempt = 30000;
      
      Serial.print("Will reattempt to connect in ");
      Serial.print(nextReconnectAttempt);
      Serial.println(" seconds");

      nextReconnectAttempt += millis();
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


/******************************************
 * Arduino OTA
 ******************************************/
void otaInit() { 
  Serial.println("Enabling OTA Updates");

  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  ArduinoOTA.setHostname(hostname);

  // No authentication by default
  // ArduinoOTA.setPassword((const char *)"123");

  ArduinoOTA.onStart([]() {
    ticker.attach(0.6, tick);
    Serial.println("OTA Update Start....");
  });
  ArduinoOTA.onEnd([]() {
    ticker.attach(0.2, tick);
    Serial.println("\nOTA Update Finished");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
}

