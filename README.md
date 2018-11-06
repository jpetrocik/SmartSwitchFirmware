# esp8266switch
Firmware for controlling ESP8266 based home automation or smart home devices via MQTT.  Most Chinese low cost smart plugs and switches are based on the esp8266 chip, like those from Sonoff, KYGNE, KCOOL, KMC, etc.  Supports Over The Air (OTA) updates using the ArudinoOTA library and mDNS discovery.

## Using Arduino IDE To Flash Firmware

There are many good tutorials out there on how to setup Arduino to flash ESP8266 chips.  Heres are good one https://randomnerdtutorials.com/how-to-install-esp8266-board-arduino-ide/. 

Once you have configured the Arudino IDE, you can flash your Sonoff, KCOOL, etc ESP8266 based devices.  Devices have different pinout for VCC, GND, RXD, and TXD, you must figure out the pinout for your device. I will add documentation for the devices I've used and know the pinouts for.  

## Using Firmware

Once you have flash the device, the device will go into AP mode which you can connect to with you computer.  Looks for SmartSwitch Wifi network.  Once connected you should be automatically direct to the Wifi configuration screen.  Enter your local wifi connection information

Once connected to the wifi the device is discoverable with mDNS.  Running the following command on Linux will list all the deivces you have configured on the network

avahi-browse -d local _socket._tcp

To get the ip address just ping the device

ping bedroom-light

Once you know the IP address you can configure the devices name, location and mqtt server isf you plan to use it with mqtt.

curl -X PUT "http://[ip address]/config?name=Lamp&location=Livingroom&server=mqtt.hostname.xx"

Other option available to configure the device with are:

relay: The pin number the relay is connected to
led: The pin number the led is connected to
button: The pin number the button is connected to

## REST Controlling the device

Current status:

curl "http://[ip address]/"

Turn light on

curl "http://[ip address]/on"

Turn light off

curl "http://[ip address]/off"

Turn toggle light

curl "http://[ip address]/toggle"

Restart device

curl -X POST "http://[ip address]/restart"

## MQTT

The device will subscribe to a command and status topic.  The device will subscribe to the command topic and publish status updates to the status topic.  The topics names are based on the name and location you configured.

The commands to publish to MQTT to control the device are simple numeric commands: 

* 0 - Turn Off
* 1 - Turn On
* 2 - Toggle 
* 3 - Status

Command Topic:
house/[location]/[name]/command

Status Topic:
house/[location]/[name]/status

Example using mosquitto client to get the current status

mosquitto_pub -t house/outside/landscaping/command -m '3'

This will cause the device to publish a status message to the status topic

mosquitto_sub -t house/outside/landscaping/status
