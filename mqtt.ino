WiFiClient _espClient;
PubSubClient _mqClient(_espClient);

int _reconnectAttemptCounter = 0;
long _nextReconnectAttempt = 0;

char _commandTopic[70];
char _statusTopic[70];
char _locationTopic[70];
char _debugTopic[70];

void mqttSetup() {
  if(strlen(mqttServer) == 0)
    return;
    
  Serial.println("Connecting to MQTT Server....");
  _mqClient.setServer(mqttServer, 1883);
  _mqClient.setCallback(mqttCallback);

  sprintf (_commandTopic, "%s/%s/%s/command", locationName, roomName, deviceName);
  sprintf (_statusTopic, "%s/%s/%s/status", locationName, roomName, deviceName);
  sprintf (_debugTopic, "house/%s/%s/debug", locationName, deviceName);

}

void mqttLoop(){
  if (!_mqClient.connected()) {
    mqttConnect();
  } else {
    _mqClient.loop();
  }
}

void mqttConnect() {
  if(!_mqClient.connected() && _nextReconnectAttempt < millis() ) {
    
    if (_mqClient.connect(hostname)) {
      Serial.println("Connected to MQTT Server");
      Serial.println(_commandTopic);
      _mqClient.subscribe(_commandTopic);
      _mqClient.subscribe(_locationTopic);

      _reconnectAttemptCounter = 0;
      _nextReconnectAttempt=0;
      
    } else {
      Serial.print("Failed to connect to ");
      Serial.println(mqttServer);

      _reconnectAttemptCounter++;
      _nextReconnectAttempt = sq(_reconnectAttemptCounter) * 1000;
      if (_nextReconnectAttempt > 30000) _nextReconnectAttempt = 30000;
      
      Serial.print("Will reattempt to connect in ");
      Serial.print(_nextReconnectAttempt);
      Serial.println(" seconds");

      _nextReconnectAttempt += millis();
    }
  }
}

//callback when a mqtt message is recieved
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  if ((char)payload[0] == '0') {
    turnOff();
  } else if ((char)payload[0] == '1') {
    turnOnHigh();
  } else if ((char)payload[0] == '2') {
    turnOnLow();
  } else if ((char)payload[0] == '3') {
    sendCurrentStatus();
  }
}

void mqttSendStatus() {
    if (_mqClient.connected()) {
      _mqClient.publish((char *)_statusTopic, (char *)jsonStatusMsg);
    }
}

void mqttSendDebug(char* msg) {
    if (_mqClient.connected()) {
      _mqClient.publish((char *)_debugTopic, msg);
    }
}

