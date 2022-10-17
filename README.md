# A display driver for 4x(8x8) Max7219 controlled monochrome LED panels

## License

This is released by Mathieu Brèthes under the [Lesser General Public License](LICENSE).

The included font to display text is (c) [Nils Chertier](https://www.nilschertier.com/) and released under Creative Commons CC-BY. Check out his stuff, he's cool.

## Disclaimer

There is absolutely no warranty, as per the license.

## What it does

This library allows a sketch to control a row of 4 8x8 led panels, for a total of 256 leds. The LED panels are controlled by Max7219 controllers, chained together and connected to the following Arduino Ports :

- DIN : digital port 10;
- CS : digital port 9;
- CLK : digital port 8;

It offers functions to display arbitrarily-sized images with 4 shades of red (with some flicker) at any location of the panel (including negative coordinates), to set or get the intensity of a pixel on the panel.

It also has a function to display text with an included font, and a function to automatically rotate this text to the left forever.

It was tested on Arduino Mini and Arduino Pro (with ATMega328 microcontrollers).

## Requirements and limitations

- It uses MsTimer2, so you can't use that on your sketch.
- At this point you can't remove the font to save space, and you can't change the ports
- Therefore you can only have one instance of the class running.
- There is a bug if you try to display text at a negative coordinate.

## TODO

- Actual documentation
- more examples
- explanation on how to convert GIF images to the format used by the library
- allow to change ports
- allow for other panel configurations (more than 4 panels, square organization, etc.)
- (help me!) testing with ATMega168 and others

