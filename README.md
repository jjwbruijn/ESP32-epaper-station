# ESP32-epaper-station
An E-paper shelf label (electronic shelf label or ESL) base station based on an Electronic Shelf Label and an ESP32. One tag is used as a zigbee radio to send packets to other tags/labels/esl's. It's based on the work of epaper/tag heroes listed below

## Disclaimer
- By using this software you acknowledge that I am in no way liable if you burn down your PC, set your hair on fire or other potential things that may or may not happen due to the use of this software.
- THIS IS PRETTY MUCH IN ALPHA STATE RIGHT NOW - IT KINDA WORKS, BUT IT IS SOMEWHAT UNSTABLE AND VERY MUCH UNSUPPORTED - 

## Big thanks
- [Dmitry](https://dmitry.gr/?r=05.Projects&proj=29.%20eInk%20Price%20Tags)
- [ATC1441](https://github.com/atc1441/ZBS_Flasher)
- [DanielKucera](https://github.com/danielkucera/epaper-station)
- My incredibly patient wife & children
- The lovely people in the ATC1441 discord

## Howto
### You're going to need a couple of things
- ESP32 (whatever board, a minimum of two GPIO pins for a serial connection)
- Solum ZBS243 SOC-based ESL (I tried and built it for the 2.9"/1.54" board). The EPD panel can be disconnected or removed
    - [ZBS_Flasher](https://github.com/atc1441/ZBS_Flasher)- ESP32/ESP8266/Arduino Nano based flasher for the ESL. We'll try to integrate this into the first ESP32 in the future
- Webserver running PHP
- Shelf labels running the custom firmware (originally version from dmitry)

### Getting stuff up and running
- Flash the shelf label/tag with the binary provided in the firmware_zigbee_base directory. Guides to flash label can be found here
- Open the ESP32 firmware and edit settings.h, make sure your webserver is filled in correctly.
- Build the ESP32 firmware and flash to your ESP32 board. I like to use PlatformIO. Whatever works for you!
- Connect the base-station-ESL to the ESP32 using the designated pins. Defaults are 12/13, this can be changed in serial.cpp
- Connect the ESL VCC and ground pins to the ESP32. Make sure to hook it up to 3.3v
- Add the wwwdir folder to your webserver, rename to esl or whatever you've entered in settings.h
- Boot the ESP32 and use WifiManager/config portal to connect the ESP32 to your network   

## Next steps / errata
Omg where to start... It's unstable, only happy flow is implemented, there is almost 0 error handling. These tags aren't meant to be used as base stations, the radio interface may be unstable and can stop/drop out. Your milage may vary, and is very much dependent on network congestion and RF environment

### Todo
- Improve radio latency/speed
- Built-in flasher
- Error handling
- Web interface
- Improved PHP side - database for tags?
- Associate message to the webserver
- Remove crypto from both custom firmware/ESP32 should improve speed
- Use the display of an ESL as a base-station status screen
- Wifi stability
- Code clean-up
- ...

## FAQ
- Q: OMG Your code is a hot mess / unmitigated dumpster fire / whatever
  - A: Yeah I know. This isn't my day gig, feel free to clean it up and hit me up with a PR

- Q: Can the base-station run on microcontroller board X
  - A: Maybe. Feel free to port it and hit me up with a PR when you do. I wouldn't recommend it per-se, ESP32's are cheap and highly effective for something like this

- Q: Does this work for other labels/tags?
  - A: Not that I'm aware of. They need to be running custom firmware anyway; this base station currently uses dmitry-style communication with tags

- Q: Can you help me build this?
  - A: Sorry, no. My time is very much limited. If you can't figure this out on your own, you are probably not the intended audience. Once again, sorry.

- Q: When will X be done?
  - A: Maybe tomorrow, maybe never
