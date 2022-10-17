/**
    VidMax7219
    
    An Arduino driver for a 4x(8x8) => 32x8 LED panels driven by 4 Max7219 chip chained together.
    Tested on an Atmega328 Arduino.
    
    Allows to display images with 4 shades of red (0, 33, 66, 100) at arbitrary positions on the matrix.
    
    This code was originally part of the Tomori project, and is now ported as a standalone library.
    
    Requires and uses the MsTimer2 library.
    
    
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

#include "Arduino.h"
#include "vidmax7219.h"

/** Libraries we need */
#include <MsTimer2.h>

/** Default font */
#include "font.dat"

/** Stuff for development */
#ifndef __DEBUG__
#define __DEBUG__ 0
#endif


/**
    Constants, should be variables at some point
*/
#define FALSE 0
#define TRUE 1


#define VID_BUFSIZE 32
#define VID_DELAY 1

#define VID_DISPLAY_W 32
#define VID_DISPLAY_H 8

#define VID_PWR_MAX 0x0A08
#define VID_PWR_MIN 0x0A00
#define VID_INCR 1

#define VID_BUF_0a 0
#define VID_BUF_0b 32
#define VID_BUF_0c 64
#define VID_BUF_0d 96
#define VID_BUF_1a 128
#define VID_BUF_1b 128+32
#define VID_BUF_1c 128+64
#define VID_BUF_1d 128+96

/**
    Low-level macros
*/
#define vidStartWrite PORTB&=B11111100
#define vidStopWrite PORTB|=B10
#define NOP __asm__ __volatile__ ("nop\n\t")

/** 
    The buffers
    
    The blit buffer is the one displayed on the Max7219
    The draw buffer is the one on which the library draws pixels
    
    the function switchbuffers inverts them.
    
    Each buffer is made of 4 "subbuffers", this allows for creating shades of red (0, 25,50, 75, 100%).
*/
    
byte vid_buffer[VID_BUFSIZE*8];
volatile byte vid_blit_buffer_ref_a = VID_BUF_0a;
volatile byte vid_blit_buffer_ref_b = VID_BUF_0b;
volatile byte vid_blit_buffer_ref_c = VID_BUF_0c;
volatile byte vid_blit_buffer_ref_d = VID_BUF_0d;

volatile byte vid_draw_buffer_ref_a = VID_BUF_1a;
volatile byte vid_draw_buffer_ref_b = VID_BUF_1b;
volatile byte vid_draw_buffer_ref_c = VID_BUF_1c;
volatile byte vid_draw_buffer_ref_d = VID_BUF_1d;


/* This low-level function sends a word on the port that goes to the Max7219.
*/
static inline void vidWriteWord(unsigned int data) {
  int i = 15;
  while (i>0) {
    PORTB &= B11111010; // clock goes down as the bit is set
    PORTB |= (byte)(((data >> i--) & 1) << 2);
    // NOP; // 1 NOP = 62.5 ns > TDS (cf spec sheet of MX7219) 
    PORTB |= 1;
    // NOP; // 1 NOP > TCH
    // PORTB &= B11111110;
  }
  PORTB &= B11111010;
  PORTB |= (byte)((data & 1) << 2);
  // NOP; // TDS
  PORTB |= 1;
  // NOP; // TCH
}

/* This function redraws the whole screen and shifts to the next display buffer.

   A picture can have 4 levels of brightness, hence 4 sub-buffers are used to represent the levels,
   and the blit displays them in sequence.
   
   This should not be called manually unless for testing purposes, as it is called by MsTimer2.
*/
short int vid_tick=0;
static void vidBlit() {
  byte curbuf;
  switch (vid_tick) {
    case 0:
      curbuf = vid_blit_buffer_ref_a;
      break;
    case 1:
      curbuf = vid_blit_buffer_ref_b;
      break;
    case 2:
      curbuf = vid_blit_buffer_ref_c;
      break;
    case 3:
      curbuf = vid_blit_buffer_ref_d;
      break;
  }
  // vid_tick tourne en boucle de 3
  vid_tick = (vid_tick+1)%4;

  for (int i = 0 ; i < 32 ; i++) {
    if (i%4 == 0) { vidStopWrite; vidStartWrite; }
    vidWriteWord(((((i >> 2)+1)<<8) + vid_buffer[curbuf+i]));
  }
  vidStopWrite;
}


/**
    Constructor
    
    TODO : allow user to set ports for DIN, CS, CLK. Warning: may require changing the low-level functions that use "PORTA/PORTB" above.
*/
VidMax7219::VidMax7219()
{
  VID_DIN = 10;
  VID_CS  = 9;
  VID_CLK = 8;

  for (int i = 0 ; i < VID_BUFSIZE*8 ; i++) vid_buffer[i] = 0;
  // put your setup code here, to run once:
  pinMode(VID_DIN, OUTPUT);
  pinMode(VID_CS, OUTPUT);
  pinMode(VID_CLK, OUTPUT);

  // by default Cable Select is unset.
  digitalWrite(VID_CS, HIGH);

  MsTimer2::set(2, vidBlit);

  vidStartWrite;

  // we start by setupping the 4 displays
  // the chip should not decode to BCD
  // desactivate test mode
  vidWriteWord(0x0F00); // this is the display furthest away from the wires
  vidWriteWord(0x0F00);
  vidWriteWord(0x0F00);
  vidWriteWord(0x0F00);  // this is the closest
  vidStopWrite;

  // decode mode set to no decode
  vidStartWrite;
  vidWriteWord(0x0900);
  vidWriteWord(0x0900);
  vidWriteWord(0x0900);
  vidWriteWord(0x0900);
  vidStopWrite;

  // intensity set to medium !!!! OTHERWISE IT CRASHES THE CHIP
  vidSetPower(VID_PWR_MAX, FALSE);

  // scan limit set to all digits
  vidStartWrite;
  vidWriteWord(0x0B07);
  vidWriteWord(0x0B07);
  vidWriteWord(0x0B07);
  vidWriteWord(0x0B07);
  vidStopWrite;
}

/**
    Set the power to a fixed intensity, going from 0A00 to 0A08.
*/
void VidMax7219::vidSetPower(unsigned int intensity, bool pausetimer=TRUE) {
  if(pausetimer) MsTimer2::stop();
    // intensity set to medium !!!! OTHERWISE IT CRASHES THE CHIP
  vidStartWrite;
  vidWriteWord(intensity);
  vidWriteWord(intensity);
  vidWriteWord(intensity);
  vidWriteWord(intensity);
  vidStopWrite;
  if(pausetimer) MsTimer2::start();
}

/**
    This function uses the Shutdown register to turn the led panels on.
    It also starts the timer.
*/
void VidMax7219::vidOn() {   
  vidStartWrite;
  vidWriteWord(0x0C01);
  vidWriteWord(0x0C01);
  vidWriteWord(0x0C01);
  vidWriteWord(0x0C01);
  vidStopWrite;

  MsTimer2::start();
}

/**
    This function uses the Shutdown register to turn the led panels off.
    It also pauses the timer.
*/
void VidMax7219::vidOff() {
  MsTimer2::stop();
  
  vidStartWrite;
  vidWriteWord(0x0C00);
  vidWriteWord(0x0C00);
  vidWriteWord(0x0C00);
  vidWriteWord(0x0C00);
  vidStopWrite;
}

/**
    This function fills all the drawable buffers with 0s. The displayed buffers are not affected.
*/
void VidMax7219::vidClear() {
  for (byte i = 0 ; i < 128 ; i++) {
    vid_buffer[vid_draw_buffer_ref_a+i] = 0;
  }
}

/**
    This function replaces the display buffer by the drawing buffer. The consecutive calls to
    draw functions will then draw on the drawing buffer.
    
    The timer is stopped while this function runs. */
void VidMax7219::vidSwitchBuffers() {
  MsTimer2::stop();
  byte tmp_a = vid_blit_buffer_ref_a;
  byte tmp_b = vid_blit_buffer_ref_b;
  byte tmp_c = vid_blit_buffer_ref_c;
  byte tmp_d = vid_blit_buffer_ref_d;
  vid_blit_buffer_ref_a = vid_draw_buffer_ref_a;
  vid_draw_buffer_ref_a = tmp_a;  
  vid_blit_buffer_ref_b = vid_draw_buffer_ref_b;
  vid_draw_buffer_ref_b = tmp_b;  
  vid_blit_buffer_ref_c = vid_draw_buffer_ref_d;
  vid_draw_buffer_ref_c = tmp_d;  
  vid_blit_buffer_ref_d = vid_draw_buffer_ref_d;
  vid_draw_buffer_ref_d = tmp_d;  
  MsTimer2::start();
}

/** A generic function to draw an image at a defined location of the screen.
 *
 *  ALPHA 2 : now allows negative coordinates.
 *  
 *  STORE ALL IMAGES IN PROGMEM
 *  https://www.arduino.cc/reference/en/language/variables/utilities/progmem/
 *  
 *  Algorithm
 *  
 *  1. We calculate in which memory cell the image begins
 *  2. at the beginning, we apply a beginning mask
 *  3. bits are copied
 *  4. at the end, the end mask is applied.
 */
void VidMax7219::vidDrawImage(int pos_x, int pos_y, byte img_w, byte img_h, const byte* img_a, const byte* img_b, const byte* img_c, const byte* img_d) {
    byte oldPosAVal;
    byte oldPosBVal;
    byte oldPosCVal;
    byte oldPosDVal;
    byte curPosA;
    byte curPosB;
    byte curPosC;
    byte curPosD;

  byte shift_x = (pos_x >= 0 ? pos_x % 8 : 8- (-pos_x) % 8);
  byte shift_enter_mask = 0xFF << (8 - shift_x);
  byte shift_leave_mask = 0xFF >> ((pos_x + img_w) % 8);
  
  byte img_wbyte = (img_w >> 3) + (img_w % 8 ? 1 : 0); 


#if __DEBUG__ >= 2
  Serial.print("IMG_WBYTE is : ");
  Serial.println(img_wbyte);
  Serial.print("mask is : ");
  Serial.print(shift_enter_mask, HEX);
  Serial.print(" ");
  Serial.println(shift_leave_mask, HEX);
  Serial.print("SHIFT_X is :");
  Serial.println(shift_x);
#endif

  for(int y = 0 ; y < img_h ; y++) {
    if (pos_y + y >= VID_DISPLAY_H) break;
    if (pos_y + y < 0) continue;
    int x = 0;
    // the Y beginning mask is only applied if the X coordinate is >= 0
    if (pos_x >= 0) {
      curPosA = vid_draw_buffer_ref_a+(((pos_y+y)*VID_DISPLAY_W+pos_x)>>3);
      curPosB = vid_draw_buffer_ref_b+(((pos_y+y)*VID_DISPLAY_W+pos_x)>>3);
      curPosC = vid_draw_buffer_ref_c+(((pos_y+y)*VID_DISPLAY_W+pos_x)>>3);
      curPosD = vid_draw_buffer_ref_d+(((pos_y+y)*VID_DISPLAY_W+pos_x)>>3);
      vid_buffer[curPosA] &= shift_enter_mask;
      vid_buffer[curPosB] &= shift_enter_mask;
      vid_buffer[curPosC] &= shift_enter_mask;
      vid_buffer[curPosD] &= shift_enter_mask;
    }
    for ( ; x <= ((img_w%8 ? ((img_w+8)>>3) : img_w>>3)<<3) ; x+= 8) {
      if (pos_x + x < 0) continue;
      if (pos_x + x >= VID_DISPLAY_W) break;

      curPosA = vid_draw_buffer_ref_a+(((pos_y+y)*VID_DISPLAY_W+pos_x+x)>>3);
      curPosB = vid_draw_buffer_ref_b+(((pos_y+y)*VID_DISPLAY_W+pos_x+x)>>3);
      curPosC = vid_draw_buffer_ref_c+(((pos_y+y)*VID_DISPLAY_W+pos_x+x)>>3);
      curPosD = vid_draw_buffer_ref_d+(((pos_y+y)*VID_DISPLAY_W+pos_x+x)>>3);

      oldPosAVal = vid_buffer[curPosA];
      oldPosBVal = vid_buffer[curPosB];
      oldPosCVal = vid_buffer[curPosC];
      oldPosDVal = vid_buffer[curPosD];




#if __DEBUG__ >= 2
        Serial.print(x);
        Serial.print(" ");
        Serial.println(y);
        Serial.print("Mem ref vbuf :");
        Serial.println(curPosA);
        Serial.print("Mem ref imgbuf :");
        Serial.println((x>>3)+(y*img_wbyte));
#endif
        
        vid_buffer[curPosA] |= (x<img_w?pgm_read_byte_near(img_a + (x>>3)+(y*img_wbyte))>>shift_x:0)+(x==0?0:pgm_read_byte_near(img_a + ((x-8)>>3)+(y*img_wbyte))<<(8-shift_x));      
        vid_buffer[curPosB] |= (x<img_w?pgm_read_byte_near(img_b + (x>>3)+(y*img_wbyte))>>shift_x:0)+(x==0?0:pgm_read_byte_near(img_b + ((x-8)>>3)+(y*img_wbyte))<<(8-shift_x));      
        vid_buffer[curPosC] |= (x<img_w?pgm_read_byte_near(img_c + (x>>3)+(y*img_wbyte))>>shift_x:0)+(x==0?0:pgm_read_byte_near(img_c + ((x-8)>>3)+(y*img_wbyte))<<(8-shift_x));      
        vid_buffer[curPosD] |= (x<img_w?pgm_read_byte_near(img_d + (x>>3)+(y*img_wbyte))>>shift_x:0)+(x==0?0:pgm_read_byte_near(img_d + ((x-8)>>3)+(y*img_wbyte))<<(8-shift_x));      
#if __DEBUG__ >= 2
        Serial.print("buffer content ");
        Serial.println(vid_buffer[curPosA], HEX);
#endif
    }
    
    if(pos_x + x < VID_DISPLAY_W) {
      vid_buffer[curPosA] |= oldPosAVal & shift_leave_mask;
      vid_buffer[curPosB] |= oldPosBVal & shift_leave_mask;
      vid_buffer[curPosC] |= oldPosCVal & shift_leave_mask;
      vid_buffer[curPosD] |= oldPosDVal & shift_leave_mask;
    }
  }
}

/* This function prints a series of texts. It requires a font, that is currently implemented as a global array included in another file.

   TODO : allow the user to choose his own font.
   TODO : there is still a bug when posX or posY are < 0
   
   Character accepted : ascii A-Z, numbers 0-9, special characters !?-/, and space.
   All unknown characters will be converted to white space.
*/
void VidMax7219::vidPrintText(char* text, int posX, int posY) {
  int i;  
  for (i = 0 ; text[i] != 0; i++) {
    if (posX >= -7) {
      int index;
      if (text[i] >= 'A' && text[i] <= 'Z') index = text[i] - 'A';
      else if (text[i] >= '0' && text[i] <= '9') index = (text[i] - '0') + 26;
      else {
        switch(text[i]) {
          case '!':
            index = 36;
            break;
          case '?':
            index = 37;
            break;
          case '-':
            index = 38;
            break;
          case '/':
            index = 39;
            break;
          default:
            // espace
            index = 40;     
        }    
      }
      vidDrawImage(posX, posY, police_x, police_y, pgm_read_word_near(&police_names_a[index]), pgm_read_word_near(&police_names_b[index]), pgm_read_word_near(&police_names_c[index]), pgm_read_word_near(&police_names_d[index]));
    }
    posX += police_x;
    if (posX > 32) break;
  }
}

/* This function initializes the rotating text print system. You need to call it once, then
   just call vidPrintRotate() repeatedly until you are bored of seeing your text scrolling from right to left on your display.
   */
void VidMax7219::vidPrintRotateInit(char *text) {
  int letters=0;
  for (letters=0; text[letters] != 0 ; letters++);
  _vpr_x = 32;
  _vpr_sz = letters * 8;
  _vpr_t = text;
}

/* This function rotates the text setup by vidPrintRotateInit, forever. It can be called in a for or while loop.

   it includes a delay of 20 millis.
   
   TODO : remove the delay and let the user put his own.
*/
void VidMax7219::vidPrintRotate() {
    vidClear();
    vidPrintText(_vpr_t, _vpr_x--, 0);
    vidSwitchBuffers();
    if (_vpr_x < (- (_vpr_sz))) _vpr_x = 32;
    delay(20);
}

/* This function returns a byte field corresponding to the value of the pixel set at coordinates X, Y.

   You need to add the values of the 4 lowmost bits to get the percentage of brightness (0=0, 1=25, 2=50, 3=75, 4=100).
*/
byte VidMax7219::vidGetPixel(int x, int y, byte img_w, byte img_h, const byte* img_a, const byte* img_b, const byte* img_c, const byte* img_d) {
  byte img_wbyte = (img_w >> 3) + (img_w % 8 ? 1 : 0); 
  byte shift = 7 - (x % 8);
  return (0x08&((pgm_read_byte_near(img_a + (x>>3)+(y*img_wbyte)) >> shift) << 3))
       + (0x04&((pgm_read_byte_near(img_b + (x>>3)+(y*img_wbyte)) >> shift) << 2))
       + (0x02&((pgm_read_byte_near(img_c + (x>>3)+(y*img_wbyte)) >> shift) << 1))
       + (1&(pgm_read_byte_near(img_d + (x>>3)+(y*img_wbyte)) >> shift));
}


/* This function draws a pixel on the drawable frame buffer (not the displayed one).

   The pixel is a byte that must follow 0000xxxx where xxxx can be set to any combination
   of 0 or 1 --> all 1 mean 100% brightness, all 0 mean 0% brightness
   
   This function does not work if pos_x or pos_y are not in the range 0, 31 and 0, 7 respectively.
*/
void VidMax7219::vidPutPixel(int pos_x, int pos_y, byte pixel) {
  byte curPos;

  byte shift = 7 - (pos_x % 8);
  byte mask = (~((byte)1<<shift));
  

  curPos = vid_draw_buffer_ref_a+((pos_y*VID_DISPLAY_W+pos_x)>>3);
  vid_buffer[curPos] &= mask; // on vide d'abord le pixel qui nous intéresse
  vid_buffer[curPos] += ((pixel >> 3) << shift); // et on le reremplit potentiellement
  curPos = vid_draw_buffer_ref_b+((pos_y*VID_DISPLAY_W+pos_x)>>3);
  vid_buffer[curPos] &= mask;
  vid_buffer[curPos] += (((0x04&pixel) >> 2) << shift);
  curPos = vid_draw_buffer_ref_c+((pos_y*VID_DISPLAY_W+pos_x)>>3);
  vid_buffer[curPos] &= mask;
  vid_buffer[curPos] += (((0x02&pixel) >> 1) << shift);
  curPos = vid_draw_buffer_ref_d+((pos_y*VID_DISPLAY_W+pos_x)>>3);
  vid_buffer[curPos] &= mask;
  vid_buffer[curPos] += ((1&pixel) << shift);
}



