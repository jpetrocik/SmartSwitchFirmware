bool shouldSaveConfig = false;

/******************************************
 * WifiManager
 ******************************************/
void wifiSetup() {
  Serial.println("Setting up wifi connection....");
  WiFiManager wifiManager;

  WiFiManagerParameter custom_registered_phone("registered_phone", "Mobile Phone", registeredPhone, 15, "required type=\"tel\" pattern=\"[0-9]{11}\" placeholder=\"15625551234\" required title=\"Please enter a valid phone number, e.g. 15625551234\"");
  wifiManager.addParameter(&custom_registered_phone);
  WiFiManagerParameter custom_device_name("device_name", "Device Name", deviceName, 20, "required");
  wifiManager.addParameter(&custom_device_name);
  
  wifiManager.setConfigPortalTimeout(300);
  wifiManager.setDebugOutput(false);
  wifiManager.setAPCallback(wifiConfigModeCallback);
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  if (!wifiManager.autoConnect("SmartGarage")) { //-" + ESP.getChipId())) {
    Serial.println("Failed to connect, trying again...");
    ESP.restart();
  }

  if (shouldSaveConfig) {
    strncpy(registeredPhone, custom_registered_phone.getValue(), 15);
    strncpy(deviceName, custom_device_name.getValue(), 20);
    configSave();
  }

}

//call by WifiManager when entering AP mode
void wifiConfigModeCallback (WiFiManager *myWiFiManager) {
  //fast ticker while waiting to config
  ticker.attach(0.2, tick);
}

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Will save config");
  shouldSaveConfig = true;
}
