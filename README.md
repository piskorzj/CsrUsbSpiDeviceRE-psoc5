CsrUsbSpiDeviceRE - Cypress PSoC5
===
Description
---
This is a reverse engineered re-implementation of CSR's USB<->SPI Converter on the Cypress CY8C5888LTI-LP097. It is compatible with CSR's own drivers and BlueSuite tools, and should work on any BlueCore chip that supports programming through SPI.

Based on origional work by Frans-Willem for the Stellaris Launchpad - https://github.com/Frans-Willem/CsrUsbSpiDeviceRE
Based on origional work by Jeff Kent for the Sparkfun Pro Micro - https://github.com/jkent/CsrUsbSpiDeviceRE

Disclaimer
---
I make no guarantees about this code. For me it worked, but it might not for you. If you break your BlueCore module or anything else by using this software this is your own responsibility.

How to use
---
* Get 10$ development module http://www.cypress.com/documentation/development-kitsboards/cy8ckit-059-psoc-5lp-prototyping-kit-onboard-programmer-and
* Get PSoC Creator (Used version 4.1 - may be relevant)
* Import project to your workspace
* Check SPI pins placement in CsrUsbSpiDeviceRE.cydwr (change placement to whatever you need)
* Program the device
* Connect CSR module (PSoC voltage level is 5V, so to protect CSR inputs connect MOSI, CLK, SS/CE signals through resistor and 3V3 zener diode circuit. MISO can be connected directly)
```
MOSI/CLK/SS PSoC Side       R                    MOSI/CLK/SS CSR Side
--------------------------\/\/\/\----*-------------------------------
                                     |
                                     - D
                                     ^
                                     |
                                     |
                                    GND

R - some small value, I used 22 Ohm, can be more (up to 68 I guess, there are calculators online to get min/max values)
D - zener diode 3V3
```
* Device should be recognized, drivers and whole BlueSuite can be found on Internet
* Blue LED should blink/light during communication with CSR

Notes
---
I needed to program my CSR module, and after a while of search, I found Frans-Willem repository. Unfortunately, I didn't have Stellaris Launchpad in my inventory. I kept searching for different ports and found AVR ProMicro fork. However, also I was missing this chip. I decided to port it to some device I had (and it has USB support).

I managed to change an advertised name on my china CSR8645 APT-X Bluetooth 4.0 Audio Receiver using this project.



