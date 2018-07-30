
/******************************************
 * WifiManager
 ******************************************/
void wifiSetup() {
  Serial.println("Setting up wifi connection....");
  WiFiManager wifiManager;

  wifiManager.setDebugOutput(false);
  wifiManager.setAPCallback(wifiConfigModeCallback);
  wifiManager.autoConnect("SmartSwitch");//-" + ESP.getChipId());

}

//call by WifiManager when entering AP mode
void wifiConfigModeCallback (WiFiManager *myWiFiManager) {
  //fast ticker while waiting to config
  ticker.attach(0.2, tick);
}

