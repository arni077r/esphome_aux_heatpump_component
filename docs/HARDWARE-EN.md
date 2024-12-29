## Hardware ##
I tested it with an esp32 . Minimal scheme:  
![scheme](https://github.com/GrKoR/esphome_aux_ac_component/blob/master/images/scheme.png?raw=true)
 
At the first time in addition to scheme above IO0 (GPIO0) must be pulled down to GND at the boot and ESPHome can be uploaded through UART0. If your ESPHome configuration contains OTA you can pull up IO0 or leave it floating. All further updates can be uploaded over-the-air.  
I leave GPIO0 in air cause I don't see any reason to solder additional components for single use.

ESP-12E before DC-DC and air conditioner connected:  
![esp-12e minimal photo](https://github.com/GrKoR/esphome_aux_ac_component/blob/master/images/esp-12e.jpg?raw=true)
 
Air conditioner internal block has a 5-wire or a 4-wire (pseudo-USB) connection to the wifi-module. There are another types of connection too. For example AUX Aegean Sea ( 爱琴海 ), check [issue #71](https://github.com/GrKoR/esphome_aux_ac_component/issues/71) for details.

## 6-wire connection
It use [JST SM](https://www.jst-mfg.com/product/pdf/eng/eSM.pdf) connector for 6-wire connection.

### Pinout ###
1. Yellow: +12V..+14V DC. Measured +14.70V max and +13.70V min. Service manual declares up to +16V.
2. Black: ground.
3. White: +5V DC (max: +5.63V; min: +4.43V) Enable signal for the 3V3 buck regulator on the OEM module. It goes directly to the air conditioner microcontroller through resistor 1kOhm. It's non used with the ESP module.
4. Not used
5. Blue: TX of air conditioner. High is +5V.
6. Red: RX of air conditioner. High is +5V.

You should feed your ESP **from +12V..+14V line only**! It is prohibited to use +5V line for this purpose.  
+5V line is digital signal line and directly goes to conditioner's controller. It can't provide enough power. In worst scenario you probably can burn down your air conditioner controller.

### Pinout ###
<img src="https://github.com/GrKoR/esphome_aux_ac_component/blob/master/images/plyta.jpg?raw=true" width="400">


Big thanks to [@diabl0](https://github.com/diabl0) for this pinout in [issue #70](https://github.com/GrKoR/esphome_aux_ac_component/issues/70).  


## Power supply

For power supply it is possible to use any kind of suitable modules. I use this:  
![power module](https://github.com/GrKoR/esphome_aux_ac_component/blob/master/images/DD4012SA.jpg?raw=true). 

## Connections ##
Black wire of AC's connector goes to the middle pin of the power module and to the GND pin of esp.  
Yellow wire is connected to the Vin pin of the power module.  
Blue wire is connected to the RXD pin of esp.  
Red wire is connected to the TXD pin of esp.  

**ATTENTION!** In case you are using board like NodeMCU instead of clean esp8266/esp32 module, you shouldn't connect RX & TX wires of air conditioner to TX & RX pins of board! *(TXD1/RXD1, TXD2/RXD2 are also most likely not suitable.)* Use any other digital pins for UART connection. It doesn't matter if your board will use hardware or software UART. All UART types are working well.  
The usage of alternate pins for NodeMCU-like boards is necessary cause RX & TX lines of this boards are often have additional components like resistors or USB-TTL converters connected. This components are violate esp-to-ac UART connection.