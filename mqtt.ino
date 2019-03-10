WiFiClientSecure _espClient;
//PubSubClient _mqClient(_espClient);
PubSubClient _mqClient(mqttServer, mqttServerPort, mqttCallback, _espClient);

int _reconnectAttemptCounter = 0;
long _nextReconnectAttempt = 0;

char _commandTopic[70];
char _statusTopic[70];
char _regToken[20];
char _regTopic[70];

//callback when a mqtt message is recieved
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message received on ");
  Serial.println (topic);
  if (strcmp(topic, _commandTopic) == 0) {
    if ((char)payload[0] == '1') {
      toogleDoor();
    } else if ((char)payload[0] == '0') {
      toogleDoor();
    } else if ((char)payload[0] == '3') {
      sendCurrentDoorStatus();
    } else if ((char)payload[0] == 'A') {
      sendCurrentAddress();
    }
  } else if (strcmp(topic, _regTopic) == 0) {
    Serial.println("Device registered");
    memcpy(deviceToken, payload, length);
    configSave();
    mqttSubscribe();
    _mqClient.unsubscribe(_regTopic);
  }
}

void mqttSetup() {
  //load cert to connect to server
  //_espClient.setCertificate (ca_bin_crt, ca_bin_crt_len);

  //  _mqClient.setServer(MQTT_SERVER, MQTT_PORT);
  //  _mqClient.setCallback(mqttCallback);
}

void mqttLoop() {
  if (!_mqClient.connected()) {
    mqttConnect();
  } else {
    _mqClient.loop();
  }
}

void mqttConnect() {
  if (!_mqClient.connected() && _nextReconnectAttempt < millis() ) {
    Serial.println("Connecting to MQTT Server....");
    if (_mqClient.connect(hostname, mqttUsername, mqttPassword)) {
      Serial.println("Connected to MQTT Server");

      //check for required registration
      if (!mqttIsRegistered()) {
        mqttRegister();
      } else {
        mqttSubscribe();
      }

      _reconnectAttemptCounter = 0;
      _nextReconnectAttempt = 0;

    } else {
      Serial.print("Failed to connect to ");
      Serial.println(MQTT_SERVER);

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



void mqttSendMsg(char * msg) {
  if (_mqClient.connected() && (strlen(_statusTopic) > 0)) {
    _mqClient.publish((char *)_statusTopic, msg);
  }
}

boolean mqttIsRegistered() {
  if (strlen(deviceToken) == 0)
    return false;
  return true;
}

void mqttRegister() {
  Serial.println("Registering device...");

  if (strlen(registeredPhone) == 0) {
    Serial.println("No registered phone number");
    factoryReset();
    return;
  }

  mqttGenerateRegistrationTopic();

  //construct json messaage
  DynamicJsonBuffer jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();
  json["name"] = deviceName;
  json["phone"] = registeredPhone;
  json["regToken"] = _regToken;

  //send message
  char message[100];
  json.printTo(message, sizeof(message));

  _mqClient.publish("garage/register", message);
}

void mqttGenerateRegistrationTopic() {
  for (int i = 0; i < 20; i++) {
    byte randomValue = random(0, 26);
    _regToken[i] = randomValue + 'a';
  }

  sprintf(_regTopic, "garage/%s", _regToken);
  _mqClient.subscribe(_regTopic);
}


void mqttSubscribe() {
  sprintf (_commandTopic, "garage/%s/command", deviceToken);
  Serial.println(_commandTopic);

  sprintf (_statusTopic, "garage/%s/status", deviceToken);
  Serial.println(_statusTopic);

  _mqClient.subscribe(_commandTopic);
}

