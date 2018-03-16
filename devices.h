#ifndef Devices_h
#define Devices_h

#define SONOFF_SMARTSWITCH
//#define SONOFF_SMARTPLUG
//#define KS602S

//LED Status light constants
#define LED_ON LOW
#define LED_OFF HIGH

//Relay state
#define RELAY_OPEN 0
#define RELAY_CLOSED 1

#if defined (SONOFF_SMARTPLUG)
  #define ONBOARD_LED 13
  #define RELAY 14
  #define WIFI_BUTTON 0

#endif

#if defined (SONOFF_SMARTSWITCH)
  #define ONBOARD_LED 13
  #define RELAY 12
  #define WIFI_BUTTON 0
#endif

#if defined (KS602S)
  #define ONBOARD_LED 13
  #define RELAY 12
  #define WIFI_BUTTON 0

  #define LED_ON HIGH
  #define LED_OFF HIGH

#endif


#endif
