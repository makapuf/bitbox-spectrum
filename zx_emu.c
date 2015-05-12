// emulator defined routines
#include <string.h> // memcpy
#include <stdint.h>
#include "bitbox.h"

#include "z80_lib/Z80.h"
#include "zx_filetyp_z80.h"

extern const uint8_t rom_zx48_rom[]; // 16k ROM
extern uint8_t Z80_RAM[]; 
extern uint8_t key_ram[8];                  // Keyboard buffer
extern uint8_t out_ram;                     // Output (fe port)
extern uint8_t kempston_ram;                // Kempston-Joystick Buffer
extern Z80 myCPU;

extern const uint8_t rom_aticatac_z80[];
extern const uint8_t rom_knightlore_z80[];
extern const uint8_t rom_lunar_z80[];
extern const uint8_t rom_manicm_z80[];
extern const uint8_t rom_tintin_z80[]; 

extern const uint16_t rom_aticatac_z80_len; 
extern const uint16_t rom_knightlore_z80_len; 
extern const uint16_t rom_lunar_z80_len; 
extern const uint16_t rom_manicm_z80_len; 
extern const uint16_t rom_tintin_z80_len; 


#define BASERAM 0x4000
#define MYIOPORT 0xAAAA

void WrZ80(register word Addr,register byte Value)
{
  if (Addr >= BASERAM)
    Z80_RAM[Addr-BASERAM]=Value;
}

uint8_t RdZ80(register word Addr)
{
  if (Addr<BASERAM) 
    return rom_zx48_rom[Addr];
  else
    return Z80_RAM[Addr-BASERAM];
}

#define DAC_ADDR 0x40007428 // 2xu8 output of DAC.
void OutZ80(register uint16_t Port,register uint8_t Value)
{
    if((Port&0xFF)==0xFE) {
        out_ram=Value; // update it
        // output to DAC 
        //*(uint16_t*)(DAC_ADDR) = (Value&0x10)==0 ? 0xffff: 0; 
    }
    if(Port==MYIOPORT) {
        set_led(Value);
    }
}


byte InZ80(register uint16_t port)
{
    if((port&0xFF)==0xFE) {
        // keyboard / in
        switch(port>>8) {
            case 0xFE : return key_ram[0]; break;
            case 0xFD : return key_ram[1]; break;
            case 0xFB : return key_ram[2]; break;
            case 0xF7 : return key_ram[3]; break;
            case 0xEF : return key_ram[4]; break;
            case 0xDF : return key_ram[5]; break;
            case 0xBF : return key_ram[6]; break;
            case 0x7F : return key_ram[7]; break;
        }
    } else if((port&0xFF)==0x1F) {
        // kempston RAM
        return kempston_ram;
    } else if(port==MYIOPORT) {
        return button_state();
    }
    return 0xFF;
}

void PatchZ80(register Z80 *R)
{
  // nothing to do
}


word LoopZ80(register Z80 *R)
{
  // no interrupt triggered
  return INT_NONE;
}


void load_file(char id) {
    memset(Z80_RAM, 0, 0xC000);
    switch (id) {
        case 'a' : ZX_ReadFromFlash_Z80(&myCPU, rom_aticatac_z80,rom_aticatac_z80_len); break;
        case 'k' : ZX_ReadFromFlash_Z80(&myCPU, rom_knightlore_z80,rom_knightlore_z80_len); break;
        case 'l' : ZX_ReadFromFlash_Z80(&myCPU, rom_lunar_z80,rom_lunar_z80_len); break;
        case 'm' : ZX_ReadFromFlash_Z80(&myCPU, rom_manicm_z80,rom_manicm_z80_len); break;
        case 't' : ZX_ReadFromFlash_Z80(&myCPU, rom_tintin_z80,rom_tintin_z80_len); break;
    }
    JumpZ80(R->PC.W);
}

// kbd to kempston bits F (right Ctrl) + UDLR 
const uint8_t kbd_gamepad[] = {79,80,81,82,228}; 

const uint8_t map_qw[8][5] = {
    {25, 6,27,29,224}, // vcxz<caps shift=Lshift>
    {10, 9, 7,22, 4}, // gfdsa
    {23,21, 8,26,20}, // trewq
    {34,33,32,31,30}, // 54321
    {35,36,37,38,39}, // 67890
    {28,24,12,18,19}, // yuiop
    {11,13,14,15,40}, // hjkl<enter>
    { 5,17,16,225,44}, // bnm <symbshift=RSHift> <space>
};

void zx_keyboard() 
{
    // matrix of keys in descending 0x10-0x01 order
    // already mapped. 

    // reset state
    for (int i=0;i<8;i++) 
        key_ram[i] = 0xff;

    // qwerty map
    int nb_keys=0;

    struct event e;
    do {
        e = event_get();
        if (e.type == evt_keyboard_press ) {
            // scan all possibilities
            for (int j=0;j<8;j++) 
                for(int i=0;i<5;i++)
                    if (e.kbd.key == map_qw[j][i]) {
                        key_ram[j] &= ~ (1<<(4-i));
                        nb_keys++;
                    }

            // Speccy modifier keys 
            if (e.kbd.mod & 0x02) { // Shift Caps
                key_ram[0]&= ~1;
                nb_keys++;
            }
            if (e.kbd.mod & 0x01) { // Ctrl = Symbol
                key_ram[7]&= ~2;
                nb_keys++;
            }

            // emulator special keys
            if (e.kbd.mod & 0x04) { // ctrl-alt + ?
                // turbo ? 
                switch(e.kbd.key) {
                    case 76 : ResetZ80(&myCPU); break; // del : reset
                    case 4  : load_file('a'); break;
                    case 14 : load_file('k'); break;
                    case 15 : load_file('l'); break;
                    case 16 : load_file('m'); break;
                    case 23 : load_file('t'); break;
                }


                // A : load attic attac.
                if (e.kbd.key==4) 
                    load_file('a');
            }
            // gamepad (emulated by keyboard : press)
            for (int i=0;i<5;i++)
                if (e.kbd.key == kbd_gamepad[i]) 
                    kempston_ram |= 1<<i;
        } else if (e.type==evt_keyboard_release) {
            // gamepad (emulated by keyboard : release)
            for (int i=0;i<5;i++)
                if (e.kbd.key == kbd_gamepad[i]) 
                    kempston_ram &= ~(1<<i);           
        }
        

    } while ((nb_keys<2) && e.type ); 

    event_clear();

    // gamepad (real)



}

int printf ( const char * format, ... )
{
    return 10; // empty.
}