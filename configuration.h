#ifndef Configuration_h
#define Configuration_h


//#define SONOFF_BASIC
//#define KMC_SMART_PLUG
//#define SONOFF_DUAL_R2
#define KS602S_SWITCH

#ifdef SONOFF_BASIC
  #define RELAY_PIN 12
  #define LED_PIN 13
  #define LED_INVERTED !
  #define RELAY_INVERTED !
#endif

#ifdef KMC_SMART_PLUG
  #define RELAY_PIN 14
  #define LED_PIN 13
  #define BUTTON_PIN 0
  #define LED_INVERTED !
  #define RELAY_INVERTED !
#endif

#ifdef SONOFF_DUAL_R2
  #define RELAY_PIN 14
  #define LED_PIN 13
  #define BUTTON_PIN 0
  #define LED_INVERTED !
  #define RELAY_INVERTED !
#endif

#ifdef KS602S_SWITCH
  #define RELAY_PIN 12
  #define LED_PIN 13
  #define BUTTON_PIN 0
  #define RELAY_INVERTED !
#endif

//Relay state
#ifdef RELAY_INVERTED
  #define RELAY_CLOSE 1
  #define RELAY_OPEN 0
#else
  #define RELAY_CLOSE 0
  #define RELAY_OPEN 1
#endif

//Led state
#ifdef RELAY_INVERTED
  #define LED_ON 0
  #define LED_OFF 1
#else
  #define LED_ON 1
  #define LED_OFF 0
#endif




#endif
