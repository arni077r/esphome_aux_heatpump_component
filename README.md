# this project is based on GrKoR (https://github.com/GrKoR/esphome_aux_ac_component) project and changed to work with HeatPumps
 
For issues or feature requests, please go to [the issue section](https://github.com/GrKoR/esphome_aux_ac_component/issues). It will be perfect if you attach log to your issue. Log you can collect with [this python script](https://github.com/GrKoR/ac_python_logger). It helps you to save all data frames from the UART bus to a csv-file. This log combined with the detailed situation description will significantly speed up bug correction.
There is also a [detailed instruction describing how to properly request a feature](docs/HOW_TO_FEATURE_REQUEST-EN.md).
 
## DISCLAIMER ##
1. All data of this project (software, firmware, schemes, 3d-models etc.) are provided **'AS IS'**. Everything you do with your devices, you are doing at your own risk. If you don't strongly understand what you are doing, just buy Wi-Fi module from your air conditioner manufacturer.
2. I am not a programmer. So source code is certainly not optimal and badly decorated (but there are a lot of comments in it; sorry, a significant part of it is in Russian). Also, code may be written unsafe. I tried to test all parts of the code, but I'm sure I missed a lot of things. So treat it with suspicion, expect a trick from it, and if you discover something wrong write an issue here.
3. Russian and English readme files are substantially identical in meaning. But in case of differences, the [Russian](https://github.com/GrKoR/esphome_aux_ac_component#readme) version is more significant.
 
## Short description ##
This custom component allows you to control your heat pump through Wi-Fi if it is made in the AUX factory.<br />
Component tested with ESPHome 1.18.0 and AUX Split HeatPump.
 
 
## Supported heatpumps ##
None but working on it

### List of compatible ACs (tested) ###

## How to use it ##
For correct component operation, you need hardware and firmware. The hardware description is located [in a separate file](docs/HARDWARE-EN.md).

### Firmware: Integration aux_ac to your configuration ###
You need [ESPHome](https://esphome.io) v.1.18.0 or above. `External_components` have appeared in this version. But it is better to use ESPHome v.1.20.4 or above, cause there were a lot of `external_components` errors corrected before this version.

## Installing ##
## Simple example ##
The source code of this example is located in the [aux_ac_simple.yaml](https://github.com/GrKoR/esphome_aux_ac_component/blob/master/examples/simple/aux_ac_simple.yaml) file.

All settings in it is trivial. Just copy the file to your local folder, specify your Wi-Fi settings and compile YAML with ESPHome.
