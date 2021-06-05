# Wio_Terminal_Rfm69_Receiver_Eyes

Wio Terminal receives and displays sensor data transmitted via Adafruit Rfm69HCW radio.
Uses LowPowerLab Rfm69 library.

Electrical connection: See folder pictures.

Sensor data are sent by two different nodes. One node sends three temperature values from a solar plant (collector-, storage- and watertemperature).
The other node sends values read from a smartmeter (Current, Power and Work) as well as on/off states of the solar plant pump).
The values are transmitted in a very special non standard data format.
The actual values are displayed as numbers on the Wio Terminal display.

A red dot in the upper right corner of the display signals that the last sending of the node was missed. Green signals o.k.

The first rectangle on the buttom of the screen turns red when a reinitialization of the Rfm69 was performed (no power readings for more than 30 min).
The second rectangle on the buttom of the screen turns red when a watchdog reset was performed (then Today kWh and Watt min and Watt max are not valid) 

Pressing the 5-way button of the Wio Terminal for more than 2 sec toogles the display between power and temperature values. Pressing the 5-way buuton
sends a command to the node to refresh the readings from the smartmeter.
Backlight reduction can be reverted by noise (hand clapping)
As a funny extension this App has an "Uncanny Eyes" animation when awaking from backlight reduction mode
A simple version of this App without animation is: https://github.com/RoSchmi/Wio_Terminal_Rfm69_Receiver


