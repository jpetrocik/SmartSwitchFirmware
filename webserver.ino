ESP8266WebServer server(80); 


void webServerSetup() {
  Serial.println("Starting web server on port 80");
   server.on("/", handleStatus); 
   server.on("/on", handleTurnOn); 
   server.on("/off", handleTurnOff); 
   server.on("/toggle", handleToggle); 
   server.on("/restart", HTTP_POST, handleRestart); 
   server.on("/factoryreset", HTTP_POST, handleFactoryReset); 
   server.on("/config", HTTP_GET, handleConfigureDevice); 
   server.on("/config", HTTP_PUT, handleSaveConfigureDevice);

   server.begin();
}

void webServerLoop() {
  server.handleClient();
}

void handleTurnOn() {
  turnOn();
  server.send(200, "application/json",  (char *)jsonStatusMsg);
}

void handleTurnOff() {
  turnOff();
  server.send(200, "application/json",  (char *)jsonStatusMsg);
}

void handleToggle() {
  toogle();
  server.send(200, "application/json",  (char *)jsonStatusMsg);
}

void handleStatus() {
  server.send(200, "application/json",  (char *)jsonStatusMsg);
}

void handleRestart() {
  server.send(200, "application/json",  "{\"message\":\"Restarting\"}");
  delay(1000);
  ESP.restart();
}

void handleConfigureDevice() {
  Serial.println("Loading config data....");
  if (SPIFFS.exists("/config.json")) {
    //file exists, reading and loading
    File configFile = SPIFFS.open("/config.json", "r");
    if (configFile) {
      size_t size = configFile.size();
      std::unique_ptr<char[]> buf(new char[size]);

      configFile.readBytes(buf.get(), size);

      DynamicJsonDocument json(1024);
      DeserializationError error = deserializeJson(json, buf.get());

      if (!error) {
        String result;
        serializeJsonPretty(json, result);
        server.send(200, "application/json",  result);
        return;
      }
    }
  }
  server.send(200, "application/json",  "{}");

}

void handleFactoryReset() {
  factoryReset();
  delay(1000);
  server.send(200, "application/json",  "{\"message\":\"Factory Reset\"}");
}

void handleSaveConfigureDevice() {
  int argCount = server.args();
  for (int i=0; i<argCount; i++){
    String argName = server.argName(i);
    String argValue = server.arg(i);

    if (argName == "name") {
      argValue.toCharArray(deviceName, 20);
    } else if (argName == "location"){
      argValue.toCharArray(locationName, 20);
    } else if (argName == "relay"){
      relayPin = argValue.toInt();
    } else if (argName == "maxOnTimer"){
      maxOnTimer = argValue.toInt();
    } else if (argName == "led"){
      ledPin = argValue.toInt();
    } else if (argName == "button"){
      buttonPin = argValue.toInt();
    } else if (argName == "server") {
      argValue.toCharArray(mqttServer, 50);
    }

  }

  configSave();

  handleConfigureDevice();
}

