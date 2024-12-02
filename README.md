# Tempeh temperer
This is a temperature regulator for making Tempeh or for fermenting food in general.

# Hardware

  * Arduino Pro Mini, 8 MHz (or a clone)
  * SSD1306 OLED display
  * DS18B20 waterproof one wire temperature probe
  * Heater: aluminium PCB heated bed from a small 3D printer
  * Enclosure: Eurobox with lid. Isolated with styrofoam on the inside

 The whole thing is powered by 5 V USB and draws up to 2 A. This is enough to keep the inside of the box at 32 deg C.

# TODO
During the first trial run, some problems were found.

I had the cooked soybeans in zip-lock bags and placed them in direct contact with the heating plate.

This led to a very poor heat distribution. The bottom of the bag was at the target temperature (31 degC) while the top and the rest of the box was 10 deg. lower.

I noticed this 10 h into the fermentation. I put a plastic tub as a spacer between the zip-lock bags and the heater. This improved the thermal distribution a bit, but it also made the closed loop regulation extremely slow and hard to control. I've measured the temperature directly on the top of the soy beans. It takes 10 minutes until the heat propagates there. For now I have the PID gains very low.

I also had to put 5 kitchen towels around the zip-lock bags, which fixed the poor temperature distribution.

Eventually I would like to have 2 temperature sensors: air temperature and Tempeh temperature. The PID will regulate the air temperature. There could be a second (much slower) control loop for the Tempeh temperature. Also there should be a fan in the box to circulate the air and improve the heat transfer.

The styrofoam isolation needs to be improved to be air tight (there's some large holes and gaps where the lid goes).
