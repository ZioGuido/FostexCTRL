# FostexCTRL
Project for controlling Fostex R8 and compatible recorders using Arduino

This project is about controlling the Fostex R8 reel-to-reel tape recorder, or other compatible recorders, using an Arduino board.
This is made possible thanks to the SYNCHRO port of this recorder that accepts simple commands for transport and other tape functions.

By interfacing an Arduino board to this port, I could control the recorder transport function and also implement basic auto-locator functions.
Controlling the unit can be done either via MIDI MMC (Midi Machine Control) commands or using an old Alesis LRC, which is a small controller originally dedicated to ADAT machines. Its very clever design consists of a series of buttons that intervene on a resistor divider network that changes the overall resistance at the output jack. By reading this resistance value using the Arduino board's ADC ports, we can determine which button was depressed on the LRC and operate functions on the tape machine.

