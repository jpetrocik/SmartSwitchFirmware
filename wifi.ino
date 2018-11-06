
/******************************************
 * WifiManager
 ******************************************/
void wifiSetup() {
  Serial.println("Setting up wifi connection....");
  WiFiManager wifiManager;

  wifiManager.setConfigPortalTimeout(300);
  wifiManager.setDebugOutput(false);
  wifiManager.setAPCallback(wifiConfigModeCallback);
  if (!wifiManager.autoConnect("SmartGarage")) { //-" + ESP.getChipId())) {
    Serial.println("Failed to connect, trying again...");
    ESP.restart();
  }

}

//call by WifiManager when entering AP mode
void wifiConfigModeCallback (WiFiManager *myWiFiManager) {
  //fast ticker while waiting to config
  ticker.attach(0.2, tick);
}

