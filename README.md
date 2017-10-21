![The Project so far](https://github.com/bneedhamia/WellDepthTemperature/blob/master/Project.jpg)
# WellDepthTemperature
A Sparkfun ESP8266 Thing Dev board Arduino Sketch that uses multiple MAX31820 temperature sensors to estimate the height of water in a well tank.

This is a work in progress (in fact, it's barely begun).

The side of the well tank is cooler below the level of the water than it is above that level. I plan to hot-glue a series of temperature sensors to the tank exterior, in a vertical line. Then I should be able to write code that will periodically measure the temperature at each sensor, estimate which sensors are below the water line, and translate that knowledge into an estimate of the level of the water in the tank.

I know that this method won't work when the ambient temperature of the well house is near the temperature of the water being pulled out of the well. I expect that won't be the case in the summer, when we usually run out of water.

I chose this method over more reliable and direct methods because, compared to them, it's non-intrusive: I don't have to put anything in the tank, on the lid inside the tank, or in the pipes to or from the tank.

The current Arduino Sketch requires a Sparkfun ESP8266 Thing Dev board, a few MAX31820 temperature sensors, and some discrete parts (diodes, resistor, etc.). Details are in the .ino Sketch PIN_ definitions and in BillOfMaterials.ods.

The state of the project: It currently connects to the local WiFi access point, reads the number of connected temperature sensors, and reports the 1-Wire address of one of them. There is a lot to do. See Diary.odt for details and progress.

Next steps: reporting the 1-Wire bus addresses of all the attached sensors, and reading the temperature from each of them.

## Files
* BillOfMaterials.ods = the parts list
* Diary.odt = my journal/diary of the project, with design details.
* LICENSE = the project GPL2 license file
* Project.jpg = a photo of the project so far
* README.md = this file
* WellDepth.ino = the ESP8266 Arduino Sketch
* More to come as I create them: fritzing diagram; mechanical notes; etc.
