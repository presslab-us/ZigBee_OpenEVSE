# ZigBee_OpenEVSE
ZigBee add-on module for OpenEVSE  

## Parts required
CC2530 SZ2 wireless module with Axis RFX2401C LNA, goodluckbuy.com  
MCP1702-3302E regulator for 3.3V, digikey.com  
10uF tantalum, digikey.com  
0.001uF capacitor, digikey.com  

## Connections
OpenEVSE pin 1 to module pin 1,2  
OpenEVSE pin 3 to reg pin 2  
OpenEVSE pin 4 to module pin 20  
OpenEVSE pin 5 to 1.0k resistor to module pin 21  

Reg pin 1 to module pin 1,2  
Reg pin 3 to module pin 3  

Jumper on module pins 13,15 for LNA  
10uF tantalum on 3.3V/GND  
0.001uF capacitor on module pin 24 to GND  

# Building
Install Z-STACK-HOME 1.2.2a  
Copy OpenEVSE to C:\Texas Instruments\Z-Stack Home 1.2.2a.44539\Projects\zstack\HomeAutomation\OpenEVSE  

# Programming
HEX file located in OpenEVSE\CC2530DB\RouterEB\Exe\OpenEVSE.hex  
