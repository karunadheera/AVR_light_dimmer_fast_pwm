
ATTINY85V LED DIMMER FAST PWM
=============================

AVR based LED (12V strip) dimmer controllable by any infrared remote. The code is based on IRMP library. http://www.mikrocontroller.net/articles/IRMP#IRMP_-_Infrarot-Multiprotokoll-Decoder

The code is configurable to support almost any infrared remote. Currently programmed for Apple and SONY remotes.

To enable support for your remote, first enable it's protocol in irmpconfig.h. Then configure enum with comment IR PROTOCOLs to support your own remote.

Features:
	* Uses fast pwm signals to dim the LED light, so that no flicker bothers the user.
	* Automatically persists the dimming level and other statuses in the AVR's EEPROM for resume purposes.
	
