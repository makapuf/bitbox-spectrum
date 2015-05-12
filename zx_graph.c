// display of the spectrum screen
// uses zx_couples data 

/* spectrum screen is located in RAM as beginning of RAM (0x1800 bytes), and 768 B attributes at 

see http://wordpress.animatez.co.uk/computers/zx-spectrum/screen-memory-layout/

*/
#include <stdint.h>
#include "bitbox.h"

extern uint8_t Z80_RAM[]; 
extern uint32_t zx_couples[]; 
extern uint8_t out_ram;
extern uint8_t *VRAM; // by default start of RAM, can change for effects

extern volatile int timer_sync;
extern volatile int timer_interrupt;

void graph_line() {
    // sync timers to line timer for a 30kHz timer
    timer_sync++;
    timer_interrupt++;

    // -- First lines of horizontal borders
    if (vga_line/2==0 || vga_line/2 == 216/2) {
        // vertical borders on a 320x240 screen for the 256 x 192 lines are 0-23 and 216-239.
        // bgcolor is output 0xfe bits 0,1,2. No need to draw it on vertical margins because it should not have been overwritten.
        // draw 2 lines as we're alternating buffers
        uint32_t bgcol = zx_couples[(out_ram & 0x07)*4 + 3]; // BGBG for attribute 0000CCC
        for (uint32_t *dst=(uint32_t*)&draw_buffer[0];dst<(uint32_t*) &draw_buffer[320];dst++)
            *dst=bgcol;
        return;
    }

    // -- Other lines of borders
    if (vga_line<=23 || vga_line >= 216) 
        return; // buffers already written

    // -- Blitting a "real" line
    // Pixel addr is y7y6y2y1y0 - y5y4y3x7x6x5x4x3 (lower x bits addr are within the byte), startint at 24
    uint8_t y=vga_line-24;
    uint8_t *attr = &VRAM[0x1800 + (y/8 ) * 32];

    uint8_t ypix = (y&0xc0) | (y&7)<<3 | (y&0x38)>>3;
    uint8_t *pixel = &VRAM[ ypix * 32 ]; // 32 bytes per line

    uint32_t *dst = (uint32_t*) &draw_buffer[(320-256)/2]; // skip left margin
    for (int i=0;i<32;i++) {
        uint8_t atr = *attr++;
        // handle Flash bit
        if (atr&0x80 && vga_frame%32) 
            atr ^=0x7f;
        atr &= 0x7f;
        // 4 couples blit
        *dst++ = zx_couples[atr * 4 + ((*pixel>>6) & 3)];
        *dst++ = zx_couples[atr * 4 + ((*pixel>>4) & 3)];
        *dst++ = zx_couples[atr * 4 + ((*pixel>>2) & 3)];
        *dst++ = zx_couples[atr * 4 + ((*pixel>>0) & 3)];
        // next character
        pixel++;
    }
}

void graph_frame() { 
}