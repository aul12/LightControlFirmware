# LightControlFirmware
Firmware running on an ESP8266 to control my LED-Strips. The firmware supports special animations and setting colors, all controlled via TCP.
For accurate colors gamma correction and dithering is implemented.

## Communication
Accepts commands via TCP on port 1337 using [rcLib](https://github.com/ToolboxPlane/RadioControlProtocol). 
All packages are required to be 10 bit, 4 channels with the following information

| Channel | Interpretation |
| --- | --- |
| 0 | Command |
| 1 | Red (10 bit) |
| 2 | Green (10 bit) |
| 3 | Blue (10 bit) |

### Commands
The following commands are available:


| Command | Interpretation |
| --- | --- |
| 0 | Set color |
| 1 | Sunrise animation |
| 2 | Continuous Fade |
