// #include "stm32_ub_spectrum.h"
#include <string.h> // memcpy
#include <stdint.h>

#include "z80_lib/Z80.h"

extern uint8_t title_scr[];
extern const int title_scr_len;
void zx_keyboard(void);

//--------------------------------------------------------------
// 64K map of ZX-Spectrum (16k ROM + 48K RAM)
//
// 0x0000 - 0x3FFF : 0x4000 => ROM
// 0x4000 - 0x57FF : 0x1800 => Display RAM (pixels, no color)
// 0x5800 - 0x5AFF : 0x0300 => Attribute RAM (attributes)
// 0x5B00 - 0xFFFF : 0xA500 => Data RAM
//--------------------------------------------------------------
#define INT_TIME 30000/50 // 50Hz interrupts at 30kHz 

uint8_t Z80_RAM[0xC000];  // 48k RAM
Z80 myCPU;
uint8_t key_ram[8]={0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff};                  // Keyboard buffer
uint8_t out_ram;                     // Output (fe port)
uint8_t kempston_ram;                // Kempston-Joystick Buffer

volatile int timer_sync;             // synchronization timer 
volatile int timer_interrupt;        // 50Hz interrupt timer 

int turbo=0;                         // set slow for games, by example, high in basic
uint8_t * volatile VRAM=Z80_RAM;     // What will be displayed. Generally ZX VRAM, can be changed for alt screens.
// -------------------------------------------------------------------------------------------------

int bitbox_main(void)
{
    // bitbox should already be initialized here

    // show init screen art
    VRAM = title_scr;
    while (timer_interrupt<5*30000); // small delay

    VRAM = Z80_RAM;
    memset(Z80_RAM, 0, sizeof(Z80_RAM));
    ResetZ80(&myCPU);

    while (1) {
        ExecZ80(&myCPU,3500*6/30); // 3.5MHz ticks for 6 lines @ 30 kHz = 700 cycles

        // emulation sync every 6 lines
        while (!turbo && timer_sync <= 6);
        timer_sync=0;

        if (timer_interrupt>INT_TIME) {

            IntZ80(&myCPU,INT_IRQ); // must be called every 20ms
            zx_keyboard();
            // check menu
            // refresh frame emu ?
            timer_interrupt=0;
        }
    }
}

// if menu : ... display ce qu'il faut, loade, setup pointer/reset & "continue" ; mode turbo 
// prendre la fonte en ROM :)
