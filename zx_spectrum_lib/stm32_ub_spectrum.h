//--------------------------------------------------------------
// File     : stm32_ub_spectrum.h
//--------------------------------------------------------------

//--------------------------------------------------------------
#ifndef __STM32F4_UB_SPECTRUM_H
#define __STM32F4_UB_SPECTRUM_H


//--------------------------------------------------------------
// Includes
//--------------------------------------------------------------
#include "stm32f4xx.h"
#include "z80.h"
#include "stm32_ub_lcd_ili9341.h"
#include "stm32_ub_font.h"
#include "stm32_ub_graphic2d.h"
#include "stm32_ub_usb_hid_host.h"
#include "zx_filetyp_z80.h"
#include "stm32_ub_uart.h"


//--------------------------------------------------------------
// Debug-Schalter (enweder auf 1 oder 0 einstellen)
//--------------------------------------------------------------
//#define  ZX_DEBUG_KEY  1  // KEY-Code wird auf dem Display angezeigt
#define  ZX_DEBUG_KEY  0  // ohne Key-Debug
//--------------------------------------------------------------



//--------------------------------------------------------------
// IO-Pins
//--------------------------------------------------------------
#define   SPEAKER_CLOCK     RCC_AHB1Periph_GPIOB
#define   SPEAKER_PIN       GPIO_Pin_2
#define   SPEAKER_PORT      GPIOB

#define   BUTTON_CLOCK      RCC_AHB1Periph_GPIOA
#define   BUTTON_PIN        GPIO_Pin_0
#define   BUTTON_PORT       GPIOA

#define   LED_RED_CLOCK     RCC_AHB1Periph_GPIOG
#define   LED_RED_PIN       GPIO_Pin_14
#define   LED_RED_PORT      GPIOG


//--------------------------------------------------------------
// Display Defines (nicht aendern !!)
//--------------------------------------------------------------
#define   SPECTRUM_MAXX  256  // Pixel in X-Richtung [0...255]
#define   SPECTRUM_MAXY  192  // Pixel in Y-Richtung [0...191]
#define   SPECTRUM_PIXEL  SPECTRUM_MAXX*SPECTRUM_MAXY


#define   SPECTRUM_CHAR_X  32 // Zeichen in X-Richtung [0...31]
#define   SPECTRUM_CHAR_Y  24 // Zeichen in X-Richtung [0...23]

#define   SPECTRUM_YSTART  32 // Display startposition (y)
#define   SPECTRUM_XSTART  24 // Display startposition (x)


//--------------------------------------------------------------
// Color Defines [b=bright]
//--------------------------------------------------------------
#define   SPECTRUM_COL_00    0x0000  // schwarz [b=1 und b=0]
#define   SPECTRUM_COL_01    0x001F  // blau [b=1]
#define   SPECTRUM_COL_02    0xF800  // rot [b=1]
#define   SPECTRUM_COL_03    0xF81F  // magenta [b=1]
#define   SPECTRUM_COL_04    0x07E0  // gruen [b=1]
#define   SPECTRUM_COL_05    0x07FF  // cyan [b=1]
#define   SPECTRUM_COL_06    0xFFE0  // gelb [b=1]
#define   SPECTRUM_COL_07    0xFFFF  // weiss [b=1]

#define   SPECTRUM_COL_08    0x0019  // blau [b=0]
#define   SPECTRUM_COL_09    0xC800  // rot [b=0]
#define   SPECTRUM_COL_10    0xC819  // magenta [b=0]
#define   SPECTRUM_COL_11    0x0660  // gruen [b=0]
#define   SPECTRUM_COL_12    0x0679  // cyan [b=0]
#define   SPECTRUM_COL_13    0xCE60  // gelb [b=0]
#define   SPECTRUM_COL_14    0xCE79  // weiss [b=0]


//--------------------------------------------------------------
// sonstige Defines
//--------------------------------------------------------------
#define   LED_BLINK_RATE             250   // 250ms
#define   LCD_REFRESH_RATE            75   // (>50ms)
#define   SPECTRUM_INTERRUPT_TIME     20   // 20ms => 50 Hz (nicht aendern)


//--------------------------------------------------------------
// Delay fuer Z80 CPU (@ 3,5Mhz)
// 1000 Takte bei 3,5MHz = ca. 285us
//--------------------------------------------------------------
#define   Z80_CPU_DELAY              300   // 300us


//--------------------------------------------------------------
// diverse RAM Arrays (nicht aendern !!)
//--------------------------------------------------------------
byte key_ram[8];                  // 8Bytes als Tastaturpuffer
byte out_ram;                     // Ausgabepuffer
uint16_t b1_colmap[8];            // colormap (bright=1)
uint16_t b0_colmap[8];            // colormap (bright=0)
uint16_t lcd_ram[SPECTRUM_PIXEL]; // Puffer fuer Screen
byte kempston_ram;                // Kempston-Joystick Buffer


//--------------------------------------------------------------
// Transfer-Buffer fuer Datenaustausch im externen SD-RAM
// etwas mehr als 48kByte (Header+Daten)
//--------------------------------------------------------------
#define  TR_BUFFER_SIZE   0xC020
#define  TR_BUFFER_START_ADR    ((uint32_t)0xD0100000)



//--------------------------------------------------------------
// Struktur vom ZX-Spectrum
//--------------------------------------------------------------
typedef struct {
  uint32_t akt_mode;      // [0=Basic, 1=GAME, 2=SAVE, 3+4=LOAD, 5=HELP]
  uint32_t cpu_delay;     // Delay im Game-Mode
  uint32_t led_mode;      // [0=normal, 1=per Basic]
  uint32_t tr_buffer_len; // Transfer-Buffer laenge
}ZX_Spectrum_t;
ZX_Spectrum_t ZX_Spectrum;




//--------------------------------------------------------------
// Struktur von einem ZX-Spectrum File
//--------------------------------------------------------------
typedef struct UB_ZX_Game_t {
  const uint8_t *table;  // Tabelle mit den Daten
  uint16_t len;          // Anzahl der Bytes
}UB_ZX_File;



//--------------------------------------------------------------
// Files die im Programm benutzt werden
//--------------------------------------------------------------
extern UB_ZX_File   ZX_Game_PSSST;
extern UB_ZX_File   ZX_Game_AticAtac;




//--------------------------------------------------------------
// Globale Funktionen
//--------------------------------------------------------------
void UB_Spectrum_init(void);
void UB_Spectrum_exec(uint16_t cycles);
void UB_Sepctrum_interrupt(void);
void UB_Spectrum_LCD_refresh(void);
void UB_Spectrum_KEY_scan(void);
void UB_Spectrum_Send_Buffer(void);
void UB_Spectrum_Load_Buffer(void);
void UB_Spectrum_Help(void);




//--------------------------------------------------------------
#endif // __STM32F4_UB_SPECTRUM_H
