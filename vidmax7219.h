/**
    VidMax7219
    
    An Arduino driver for a 4x(8x8 led panels) => 32x8 LEDs driven by 4 Max7219 chip chained together.
    Tested on an Atmega328 Arduino.
    
    The Max7219 chain must be connected to Arduino digital outputs:
    DIN -->  10 
    CS  -->  9 
    CLK -->  8 
    
    Allows to display images with 4 shades of red (0, 25, 50, 75, 100) at arbitrary positions on the matrix.
    
    This code was originally part of the Tomori project, and is now ported as a standalone library.
    
    Requires and uses the MsTimer2 library.
    
    At this point, you can have only ONE INSTANCE of this driver running.

    Possible improvements if I ever find the time:
    - Make the library more configurable for led panels of different sizes and physical placement (ex. 16x16)
    - Allow to change the ports
    - Allow for running several instances of the driver for several Max7219 chains on different ports (requires
      thinking on how to call several functions from MsTimer2, requires getting the buffers in the class, requires
      getting rid of the functions that work outside of the scope of the class in vidmax7219.cpp).
    
    
    Copyright (C) 2017-2022  Mathieu Brethes

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#ifndef __VIDMAX7219__
#define __VIDMAX7219__

#include "Arduino.h"


class VidMax7219 {
    public:
        VidMax7219();
        void vidSetPower(unsigned int intensity, bool pausetimer);
        void vidOn();
        void vidOff();
        void vidClear();
        void vidSwitchBuffers();
        void vidDrawImage(int pos_x, int pos_y, byte img_w, byte img_h, const byte* img_a, const byte* img_b, const byte* img_c, const byte* img_d);
        void vidPrintText(char* text, int posX, int posY);
        void vidPrintRotateInit(char *text);
        void vidPrintRotate();
        byte vidGetPixel(int x, int y, byte img_w, byte img_h, const byte* img_a, const byte* img_b, const byte* img_c, const byte* img_d);
        void vidPutPixel(int pos_x, int pos_y, byte pixel);

                
    private:
        int _vpr_x;          // current location of the scrolling text
        int _vpr_sz;         // total length of the text, in pixels
        char *_vpr_t;        // the text that needs to be scrolled
        short int VID_DIN;  // datain port for Max7219 (output)
        short int VID_CS;   // cs port for Max7219 (output)
        short int VID_CLK;  // clock port for Max7219 (output)


};

static void vidBlit(); // only for debug purposes

#endif
