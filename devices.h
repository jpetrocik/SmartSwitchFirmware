#ifndef Devices_h
#define Devices_h

typedef struct DEVICE_TMPLT {
  char        name[15];
  int         relayPin;
  int         ledPin;
};

const DEVICE_TMPLT devices[4] =  {
  {
      "Sonoff Basic",     
      GPIO12,
      GPIO13
  },
  {
      "Sonoff Socket",     
      GPIO14,
      GPIO13
  },
  {
      "Sonoff Dual R2",     
      GPIO14,
      GPIO13
  },
  {
      "KS602S Switch",     
      GPIO12,
      GPIO13
      //1Mb Memory
  }
};

#endif
