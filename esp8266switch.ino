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

//current status of relay
int relayState = RELAY_OFF;

//maintains when the button was pressed
//action is taken on button release
//this allows for long press features
unsigned long buttomPressed = false; //time button was press, so must be unsigned long
long bounceTimeout = 0;

//configuration properties
char deviceName[20] = "light";
char locationName[20] = "bedroom";
char mqttServer[50] = "mqtt.local";
char hostname[41];
char commandTopic[70];
char statusTopic[70];
int relayPin = RELAY;
int ledPin = ONBOARD_LED;
int buttonPin = WIFI_BUTTON;
int maxOnTimer = 0;

//set when AP mode is enabled to indicating the
//config data needs to be saved
bool shouldSaveConfig = false;

long delayOffTime = 0;

double configVersion = 1.1;

//mqtt client
WiFiClient espClient;
PubSubClient mqClient(espClient);

void setup() {
  Serial.begin(115200);

//  factoryReset();
  
  configLoad();

//  const char version[] = "v1.0-beta";
  Serial.println("SmartHome Firmware");
  Serial.println(__DATE__ " " __TIME__);
  Serial.println(hostname);

  pinMode(ledPin, OUTPUT);
  pinMode(relayPin, OUTPUT);

  //slow ticker when starting up
  //switch to fast tick when in AP mode
  ticker.attach(0.6, tick);

  wifiSetup();

  mdnsInit();

  ticker.detach();

  //init over the air update server
  otaInit();

  //connect to mqtt servier
  mqttInit();

  digitalWrite(ledPin, HIGH);
}

void loop() {
  long now = millis();

  //check for delayed off timer
  if (delayOffTime > 0 && delayOffTime < now){
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
      toogleSwitch();

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
        delay(2000);
        factoryReset();
    }
  }

  //Connect or read message
  if (!mqClient.connected()) {
    mqttConnect();
  } else {
    mqClient.loop();
  }

  //Check for OTA Updates
  ArduinoOTA.handle();

}

void toogleSwitch() {
  if (relayState == RELAY_ON) {
    turnOff();
  } else {
    turnOn();
  }
}

void turnOn() {
  digitalWrite(relayPin, HIGH);
  digitalWrite(ledPin, LED_ON);
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
  digitalWrite(relayPin, LOW);
  digitalWrite(ledPin, LED_OFF);
  relayState = RELAY_OFF;

  //reset or cancel delayTimer
  delayOffTime = 0;

  sendCurrentStatus();
}

void sendCurrentStatus() {
  long remainingTimer  = delayOffTime - millis();
  char jsonStatusMsg[140];
  sprintf (jsonStatusMsg, "{\"status\":%s,\"delayOff\":\"%i\"}", relayState ? "\"ON\"" : "\"OFF\"", remainingTimer>0?remainingTimer:0);

  mqClient.publish((char *)statusTopic, (char *)jsonStatusMsg);
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

  wifiManager.setDebugOutput(false);

  WiFiManagerParameter wifiDeviceNameParam("device", "Name", deviceName, 20);
  WiFiManagerParameter wifiLocationNameParam("location", "Location", locationName, 20);
  WiFiManagerParameter wifiMqttServerParam("mqtt", "MQTT Server", mqttServer, 50);
  WiFiManagerParameter wifiRelayParam("relay", "Relay", "", 5);
  WiFiManagerParameter wifiLedParam("led", "LED", "", 5);
  WiFiManagerParameter wifimaxOnTimerParam("maxOnTimer", "Max On Timer", "", 5);

  wifiManager.setAPCallback(configModeCallback);
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  wifiManager.addParameter(&wifiDeviceNameParam);
  wifiManager.addParameter(&wifiLocationNameParam);
  wifiManager.addParameter(&wifiMqttServerParam);
  wifiManager.addParameter(&wifiRelayParam);
  wifiManager.addParameter(&wifiLedParam);
  wifiManager.addParameter(&wifimaxOnTimerParam);
  wifiManager.autoConnect("SmartSwitch");//-" + ESP.getChipId());

  if (shouldSaveConfig) {
    strncpy(deviceName, wifiDeviceNameParam.getValue(), 20);
    strncpy(locationName, wifiLocationNameParam.getValue(), 20);
    strncpy(mqttServer, wifiMqttServerParam.getValue(), 50);
    relayPin = atoi(wifiRelayParam.getValue());
    ledPin = atoi(wifiLedParam.getValue());
    maxOnTimer = atoi(wifimaxOnTimerParam.getValue());

    configSave();
  }
}

//call by WifiManager when entering AP mode
void configModeCallback (WiFiManager *myWiFiManager) {
  //fast ticker while waiting to config
  ticker.attach(0.2, tick);
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
    json.set("version",configVersion);
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
          configVersion = json.get<signed int>("version");
          strncpy(deviceName, json["device"], 20);
          strncpy(locationName, json["location"], 20);
          strncpy(mqttServer, json["mqttServer"], 50);
          sprintf (hostname, "%s-%s", locationName, deviceName);
          if (configVersion >= 1.1) {
            relayPin = json.get<signed int>("relay");
            ledPin = json.get<signed int>("led");
            maxOnTimer = json.get<signed int>("maxOnTimer");
          }

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
      Serial.println("Connected to MQTT Server");
      Serial.println(commandTopic);
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
    turnOn();
  } else if ((char)payload[0] == '0') {
    turnOff();
  } else if ((char)payload[0] == '2') {
    toogleSwitch();
  } else if ((char)payload[0] == '3') {
    sendCurrentStatus();
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

