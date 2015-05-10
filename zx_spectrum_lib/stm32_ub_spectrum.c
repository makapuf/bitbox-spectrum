//--------------------------------------------------------------
// File     : stm32_ub_spectrum.c
// Datum    : 30.01.2014
// Version  : 1.1
// Autor    : UB
// EMail    : mc-4u(@)t-online.de
// Web      : www.mikrocontroller-4u.de
// CPU      : STM32F429
// IDE      : CooCox CoIDE 1.7.4
// GCC      : 4.7 2012q4
// Module   : Z80, retarget_printf, TIM
// Funktion : ZX-Spectrum-Emulator per STM32F429
//
// Hinweis  : Z80-Emulator von "Marat Fayzullin" (17.08.2007)
//            Quelle = "http://fms.komkon.org/"
//
// Portpins : PB2 = Speaker-Out
//            PA9 = UART-TX, PA10 = UART-RX (115200 Baud, 8N1)
//   
// IO-Pins  : PA0  = IN  [an Port 0xAAAA,Bit0]  => (User-Button)
//            PG14 = OUT [an Port 0xAAAA,Bit0]  => (LED_RED)
//
// Keyboard : "ALT"+"ESC" = Reset (start ZX-Spectrum Basic)
//            "ALT"+"F1"  = Start Game Nr.1 from Flash (Z80-File)
//            "ALT"+"F1"  = Start Game Nr.2 from Flash (Z80-File)
//            "ALT"+"h"   = Help
//            "ALT"+"s"   = Save RAM over UART to PC (Z80-File)
//            "ALT"+"l"   = Load Basic-File over UART from PC (Z80-File)
//            "ALT"+"g"   = Load Game-File over UART from PC (Z80-File)
//
// Kempston-Joystick emulation :
//    Cursor_Left, Cursor_Right, Cursor_Up, Cursor_Down, Right_ALT
//--------------------------------------------------------------


//--------------------------------------------------------------
// Includes
//--------------------------------------------------------------
#include "stm32_ub_spectrum.h"


//---------------------------------------------------
// globale Variable
//---------------------------------------------------
char str_buf[20];


//---------------------------------------------------
// interne Funktionen
//---------------------------------------------------
void P_Spectrum_initIO(void);
void P_Spectrum_clearSpectrumRAM(void);
void P_Spectrum_clearTransferRAM(void);
void P_Spectrum_startFunktion(uint32_t nr);


//---------------------------------------------------
// Tastatur-DEBUG
//---------------------------------------------------
#if ZX_DEBUG_KEY==1
uint16_t dbg_k1,dbg_k2,dbg_s;
#endif



//--------------------------------------------------------------
// 64K Memory-MAP vom ZX-Spectrum (16k ROM + 48K RAM)
//
// 0x0000 - 0x3FFF : 0x4000 => ROM
// 0x4000 - 0x57FF : 0x1800 => Display RAM (nur Pixeldaten ohne Farben)
// 0x5800 - 0x5AFF : 0x0300 => Attribue RAM (Display Farbinformationen)
// 0x5B00 - 0xFFFF : 0xA500 => Data RAM
//--------------------------------------------------------------
extern byte Z80_ROM[];      // 16k ROM
byte Z80_RAM[0xC000];       // 48k RAM


//--------------------------------------------------------------
// Z80-CPU Struktur
//--------------------------------------------------------------
Z80 myCPU;


//--------------------------------------------------------------
// init vom ZX-Spectrum
// -reset der CPU
// -reset aller Speicher
// -init der Hardware
//--------------------------------------------------------------
void UB_Spectrum_init(void)
{
  uint32_t n;

  // init der Variabeln
  ZX_Spectrum.akt_mode=0;   // 0=BASIC
  ZX_Spectrum.cpu_delay=0;
  ZX_Spectrum.led_mode=0;   // 0=normal
  ZX_Spectrum.tr_buffer_len=0;

  // init aller IO-Pins
  P_Spectrum_initIO();

  // Reset der CPU
  ResetZ80(&myCPU);

  // loeschen vom Tastaturpuffer
  key_ram[0]=0xFF;
  key_ram[1]=0xFF;
  key_ram[2]=0xFF;
  key_ram[3]=0xFF;
  key_ram[4]=0xFF;
  key_ram[5]=0xFF;
  key_ram[6]=0xFF;
  key_ram[7]=0xFF;

  // loeschen vom Ausgabe RAM
  out_ram=0x00;
  // loeschen vom Kempston RAM
  kempston_ram=0x00;

  // init der Farben (bright=1)
  b1_colmap[0]=SPECTRUM_COL_00;
  b1_colmap[1]=SPECTRUM_COL_01;
  b1_colmap[2]=SPECTRUM_COL_02;
  b1_colmap[3]=SPECTRUM_COL_03;
  b1_colmap[4]=SPECTRUM_COL_04;
  b1_colmap[5]=SPECTRUM_COL_05;
  b1_colmap[6]=SPECTRUM_COL_06;
  b1_colmap[7]=SPECTRUM_COL_07;
  // init der Farben (bright=0)
  b0_colmap[0]=SPECTRUM_COL_00;
  b0_colmap[1]=SPECTRUM_COL_08;
  b0_colmap[2]=SPECTRUM_COL_09;
  b0_colmap[3]=SPECTRUM_COL_10;
  b0_colmap[4]=SPECTRUM_COL_11;
  b0_colmap[5]=SPECTRUM_COL_12;
  b0_colmap[6]=SPECTRUM_COL_13;
  b0_colmap[7]=SPECTRUM_COL_14;

  // loeschen vom Spectrum-RAM
  P_Spectrum_clearSpectrumRAM();

  // loeschen vom LCD-RAM
  for(n=0;n<SPECTRUM_PIXEL;n++) {
    lcd_ram[n]=0x00;
  }

  // loeschen vom Transfer-RAM
  P_Spectrum_clearTransferRAM();
}


//--------------------------------------------------------------
// einige Befehlszyklen vom ZX-Spectrum abarbeiten
//
// cycles = Anzahl der CPU-Befehle die abgearbeitet werden sollen
//--------------------------------------------------------------
void UB_Spectrum_exec(uint16_t cycles)
{	
  ExecZ80(&myCPU,cycles);	
}


//--------------------------------------------------------------
// loest einen externen Interrupt vom ZX-Spectrum aus
// (muss alle 20ms aufgerufen werden !!)
//
// damit das ZX-Spectrum-Basic die Tastatur pollt
//--------------------------------------------------------------
void UB_Sepctrum_interrupt(void)
{
  IntZ80(&myCPU,INT_IRQ);
}


//--------------------------------------------------------------
// refresh vom LCD-Inhalt
//
// Hinweis : diese Funktion bremst das System und muss
//           so schnell wie moeglich abgearbeitet werden
//           (im Moment betraegt die Durchlaufzeit ca 12ms)
//
// ToDo : Funktion in Assembler programmieren ;-)
//
// 1. Display-RAM mit Bordercolor loeschen
// 2. Bildinhalt ins interne RAM kopieren
// 3. Daten vom interne RAM ins Display-RAM kopieren (per DMA2D)
// 4. Display-RAM anzeigen
//--------------------------------------------------------------
void UB_Spectrum_LCD_refresh(void)
{
   uint16_t xchar,ychar,ypixel;
   uint16_t adr1=0,adr2=0,vg_color=0x0000,bg_color=0xFFFF;
   uint8_t screen_wert,attribut,col_nr;
   static uint8_t flash_counter=0;
   uint32_t lcd_ram_adr=0;

   // Bordercolor bestimmen
   col_nr=(out_ram&0x07); // 0...7
   bg_color=b0_colmap[col_nr];

   //----------------------------
   // Display Screen mit
   // BorderColor loeschen
   //----------------------------
   UB_Graphic2D_ClearSreenDMA(bg_color);

   // Counter fuer Flash-Funktion
   flash_counter++;

   //----------------------------
   // kompletten Spectrum-Screen
   // ins interne RAM zeichnen
   //----------------------------
   adr1=0x00; // Start-Adr vom Screen-RAM

   // alle 192 Zeilen vom Screen durchgehen
   for(ypixel=0;ypixel<SPECTRUM_MAXY;ypixel++) {
     ychar=(ypixel>>3);
     adr2=0x1800+(ychar<<5);

     // start RAM adresse berechnen fuer eine Zeile
     lcd_ram_adr=SPECTRUM_MAXY-ypixel-1;

     // eine komplette Linie zeichnen (byteweise)
     for(xchar=0;xchar<SPECTRUM_CHAR_X;xchar++) {
       // Pixel-Wert (fuer 8Pixel) lesen
       screen_wert=Z80_RAM[adr1];
       adr1++;
       // Attribut-Wert (von einem Block) lesen
       attribut=Z80_RAM[adr2];
       adr2++;

       // bright_bit auswerten
       if((attribut&0x40)!=0) {
         // bright="1"
         col_nr=(attribut&0x07);
         vg_color=b1_colmap[col_nr];
         col_nr=((attribut&0x38)>>3);
         bg_color=b1_colmap[col_nr];
       }
       else {
         // bright="0"
         col_nr=(attribut&0x07);
         vg_color=b0_colmap[col_nr];
         col_nr=((attribut&0x38)>>3);
         bg_color=b0_colmap[col_nr];
       }
       // flash_bit auswerten
       if((attribut&0x80)!=0) {
         // flash="1"
         if((flash_counter&0x04)!=0) {
           // flash zeitpunkt ist erreicht
           screen_wert^=0xFF;
         }
       }

       // eine Linie von einem Byte zeichnen (8 Pixel)
       if((screen_wert&0x80)!=0) lcd_ram[lcd_ram_adr]=vg_color; else lcd_ram[lcd_ram_adr]=bg_color;
       lcd_ram_adr+=SPECTRUM_MAXY;
       if((screen_wert&0x40)!=0) lcd_ram[lcd_ram_adr]=vg_color; else lcd_ram[lcd_ram_adr]=bg_color;
       lcd_ram_adr+=SPECTRUM_MAXY;
       if((screen_wert&0x20)!=0) lcd_ram[lcd_ram_adr]=vg_color; else lcd_ram[lcd_ram_adr]=bg_color;
       lcd_ram_adr+=SPECTRUM_MAXY;
       if((screen_wert&0x10)!=0) lcd_ram[lcd_ram_adr]=vg_color; else lcd_ram[lcd_ram_adr]=bg_color;
       lcd_ram_adr+=SPECTRUM_MAXY;
       if((screen_wert&0x08)!=0) lcd_ram[lcd_ram_adr]=vg_color; else lcd_ram[lcd_ram_adr]=bg_color;
       lcd_ram_adr+=SPECTRUM_MAXY;
       if((screen_wert&0x04)!=0) lcd_ram[lcd_ram_adr]=vg_color; else lcd_ram[lcd_ram_adr]=bg_color;
       lcd_ram_adr+=SPECTRUM_MAXY;
       if((screen_wert&0x02)!=0) lcd_ram[lcd_ram_adr]=vg_color; else lcd_ram[lcd_ram_adr]=bg_color;
       lcd_ram_adr+=SPECTRUM_MAXY;
       if((screen_wert&0x01)!=0) lcd_ram[lcd_ram_adr]=vg_color; else lcd_ram[lcd_ram_adr]=bg_color;
       lcd_ram_adr+=SPECTRUM_MAXY;
     }
   }

   //----------------------------
   // Screen vom internen RAM ins
   // SD-RAM per DMA2D kopieren
   //----------------------------
   UB_Graphic2D_CopySpectrumRAM();

   // Debug Ausgabe der Tasten-Nummern
   #if ZX_DEBUG_KEY==1
   sprintf(str_buf,"K1=%d  K2=%d  S=%d ",(int)(dbg_k1),(int)(dbg_k2),(int)(dbg_s));
   UB_Font_DrawString(0,0,str_buf,&Arial_7x10,RGB_COL_BLACK,RGB_COL_WHITE);
   #endif


   //----------------------------
   // neuen Screen anzeigen
   //----------------------------
   UB_LCD_Refresh();
}


//--------------------------------------------------------------
// Tastaturabfrage
// testet welche Tasten der USB-Tastatur gedrueckt sind
// und setzt die entsprechenden Bits vom ZX-Spectrum Tastatur-RAM
// bzw. vom Kemston-RAM (fuer Spiele)
// (Der ZX-Spectrum kann max zwei Tasten gleichzeitig auswerten)
//
// ueber Sondertasten werden hier auch Funktionen gestartet
// z.B. "Reset vom System" oder "Save per UART"
//--------------------------------------------------------------
void UB_Spectrum_KEY_scan(void)
{
  uint8_t akt_keyanz;
  uint8_t k1,k2;
  uint8_t akt_shift;
  static uint8_t reboot_flag=0;

  // gedrueckte Tasten abholen
  akt_keyanz=UB_USB_HID_HOST_GetKeyAnz();
  k1=USB_KEY_DATA.akt_key1;
  k2=USB_KEY_DATA.akt_key2;
  akt_shift=UB_USB_HID_HOST_GetShift();

  // Debug
  #if ZX_DEBUG_KEY==1
  dbg_k1=k1;
  dbg_k2=k2;
  dbg_s=akt_shift;
  #endif

  // alle Tasten auf "unbetaetigt"
  key_ram[0]=0xFF;
  key_ram[1]=0xFF;
  key_ram[2]=0xFF;
  key_ram[3]=0xFF;
  key_ram[4]=0xFF;
  key_ram[5]=0xFF;
  key_ram[6]=0xFF;
  key_ram[7]=0xFF;

  // Kempston-Joystick auf "unbetaetigt"
  kempston_ram=0x00;

  //----------------------------------------------
  if((akt_keyanz==0) && (akt_shift==0)) {
    reboot_flag=0;
    return; // keine Taste gedrueckt
  }

  //----------------------------------------------
  // Menu-Sondertasten (werden bevorzugt behandelt)
  // (nur die erste Taste wird ausgewertet)
  //----------------------------------------------
  if((akt_shift&0x08)!=0) { // left "ALT" ist betaetigt
    if(reboot_flag==0) {
      if(k1==110) { // "ESC" -> RESET
        reboot_flag=1;
        P_Spectrum_startFunktion(0);
        return;
      }
      if(k1==112) { // "F1" -> Spiel Nr.1 starten
        reboot_flag=1;
        P_Spectrum_startFunktion(1);
        return;
      }
      if(k1==113) { // "F2" -> Spiel Nr.2 starten
        reboot_flag=1;
        P_Spectrum_startFunktion(2);
        return;
      }
      if(k1==36) { // "h" -> Help
        reboot_flag=1;
        P_Spectrum_startFunktion(99);
        return;
      }
      if(k1==32) { // "s" -> Save RAM per UART
        reboot_flag=1;
        P_Spectrum_startFunktion(100);
        return;
      }
      if(k1==39) { // "l" -> Load BASIC per UART
        reboot_flag=1;
        P_Spectrum_startFunktion(101);
        return;
      }
      if(k1==35) { // "g" -> Load GAME per UART
        reboot_flag=1;
        P_Spectrum_startFunktion(102);
        return;
      }
    }
  }


  //----------------------------------------------
  // Mapped-Sondertasten (im BASIC-Mode)
  // (nur die erste Taste wird ausgewertet)
  //----------------------------------------------
  if(ZX_Spectrum.akt_mode==0) {
    if(k1==110) { // "ESC" -> BREAK
      key_ram[0]&=~0x01; // "caps shift"
      key_ram[7]&=~0x01; // "space"
    }
    if(k1==15) { // Delete
      key_ram[0]&=~0x01; // "caps shift"
      key_ram[4]&=~0x01; // "0"
      return;
    }
    if(k1==79) { // Cursor left
      key_ram[0]&=~0x01; // "caps shift"
      key_ram[3]&=~0x10; // "5"
      return;
    }
    if(k1==84) { // Cursor down
      key_ram[0]&=~0x01; // "caps shift"
      key_ram[4]&=~0x10; // "6"
      return;
    }
    if(k1==83) { // Cursor up
      key_ram[0]&=~0x01; // "caps shift"
      key_ram[4]&=~0x08; // "7"
      return;
    }
    if(k1==89) { // Cursor right
      key_ram[0]&=~0x01; // "caps shift"
      key_ram[4]&=~0x04; // "8"
      return;
    }
    if(k1==28) { // "+/*"
      if((akt_shift&0x01)==0) {
        key_ram[7]&=~0x02; // "symbol shift"
        key_ram[6]&=~0x04; // "k"
      }
      else {
        key_ram[7]&=~0x02; // "symbol shift"
        key_ram[7]&=~0x10; // "b"
      }
      return;
    }
    if(k1==29) { // "#/'"
      if((akt_shift&0x01)==0) {
        key_ram[7]&=~0x02; // "symbol shift"
        key_ram[3]&=~0x04; // "3"
      }
      else {
        key_ram[7]&=~0x02; // "symbol shift"
        key_ram[4]&=~0x08; // "7"
      }
      return;
    }
    if(k1==53) { // ",/;"
      if((akt_shift&0x01)==0) {
        key_ram[7]&=~0x02; // "symbol shift"
        key_ram[7]&=~0x08; // "n"
      }
      else {
        key_ram[7]&=~0x02; // "symbol shift"
        key_ram[5]&=~0x02; // "o"
      }
      return;
    }
    if(k1==54) { // "./:"
      if((akt_shift&0x01)==0) {
        key_ram[7]&=~0x02; // "symbol shift"
        key_ram[7]&=~0x04; // "m"
      }
      else {
        key_ram[7]&=~0x02; // "symbol shift"
        key_ram[0]&=~0x02; // "z"
      }
      return;
    }
    if(k1==55) { // "-/_"
      if((akt_shift&0x01)==0) {
        key_ram[7]&=~0x02; // "symbol shift"
        key_ram[6]&=~0x08; // "j"
      }
      else {
        key_ram[7]&=~0x02; // "symbol shift"
        key_ram[4]&=~0x01; // "0"
      }
      return;
    }
    if((akt_shift&0x04)!=0) { // left "STRG" ist betaetigt
      if(k1==2) { // "!"
        key_ram[7]&=~0x02; // "symbol shift"
        key_ram[3]&=~0x01; // "1"
        return;
      }
      if(k1==3) { // "
        key_ram[7]&=~0x02; // "symbol shift"
        key_ram[5]&=~0x01; // "p"
        return;
      }
      if(k1==5) { // "$"
        key_ram[7]&=~0x02; // "symbol shift"
        key_ram[3]&=~0x08; // "4"
        return;
      }
      if(k1==6) { // "%"
        key_ram[7]&=~0x02; // "symbol shift"
        key_ram[3]&=~0x10; // "5"
        return;
      }
      if(k1==7) { // "&"
        key_ram[7]&=~0x02; // "symbol shift"
        key_ram[4]&=~0x10; // "6"
        return;
      }
      if(k1==8) { // "/"
        key_ram[7]&=~0x02; // "symbol shift"
        key_ram[0]&=~0x10; // "v"
        return;
      }
      if(k1==9) { // "("
        key_ram[7]&=~0x02; // "symbol shift"
        key_ram[4]&=~0x04; // "8"
        return;
      }
      if(k1==10) { // ")"
        key_ram[7]&=~0x02; // "symbol shift"
        key_ram[4]&=~0x02; // "9"
        return;
      }
      if(k1==11) { // "="
        key_ram[7]&=~0x02; // "symbol shift"
        key_ram[6]&=~0x02; // "l"
        return;
      }
      if(k1==12) { // "?"
        key_ram[7]&=~0x02; // "symbol shift"
        key_ram[0]&=~0x08; // "c"
        return;
      }
    }
  }
  //----------------------------------------------
  // Sondertasten (im GAME-Mode)
  // Fuer "Kempston-Joystick"
  // 4 Richtungstasten + Feuer
  //----------------------------------------------
  if(ZX_Spectrum.akt_mode==1) {
    // Cursor left
    if((k1==79) || (k2==79)) kempston_ram|=0x02;
    // Cursor down
    if((k1==84) || (k2==84)) kempston_ram|=0x04;
    // Cursor up
    if((k1==83) || (k2==83)) kempston_ram|=0x08;
    // Cursor right
    if((k1==89) || (k2==89)) kempston_ram|=0x01;
    // right "ALT" (fire)
    if((akt_shift&0x10)!=0) kempston_ram|=0x10;
  }
 
  //----------------------------------------------
  // "normale" ZX-Spectrum Tastenbhandlung
  //----------------------------------------------

  // test ob zu viele Tasten gedrueckt sind
  if((akt_shift&0x01)!=0) akt_keyanz++;
  if((akt_shift&0x02)!=0) akt_keyanz++;
  if(akt_keyanz>2) {
    // mehr als zwei Tasten sind nicht erlaubt
    return; 
  } 

  if((k1==49) || (k2==49)) key_ram[0]&=~0x10; // "v"
  if((k1==48) || (k2==48)) key_ram[0]&=~0x08; // "c"
  if((k1==47) || (k2==47)) key_ram[0]&=~0x04; // "x"
  if((k1==22) || (k2==22)) key_ram[0]&=~0x02; // "z"
  if((akt_shift&0x01)!=0) key_ram[0]&=~0x01;  // "caps shift"

  if((k1==35) || (k2==35)) key_ram[1]&=~0x10; // "g"
  if((k1==34) || (k2==34)) key_ram[1]&=~0x08; // "f"
  if((k1==33) || (k2==33)) key_ram[1]&=~0x04; // "d"
  if((k1==32) || (k2==32)) key_ram[1]&=~0x02; // "s"
  if((k1==31) || (k2==31)) key_ram[1]&=~0x01; // "a"

  if((k1==21) || (k2==21)) key_ram[2]&=~0x10; // "t"
  if((k1==20) || (k2==20)) key_ram[2]&=~0x08; // "r"
  if((k1==19) || (k2==19)) key_ram[2]&=~0x04; // "e"
  if((k1==18) || (k2==18)) key_ram[2]&=~0x02; // "w"
  if((k1==17) || (k2==17)) key_ram[2]&=~0x01; // "q"

  if((k1==6) || (k2==6)) key_ram[3]&=~0x10;   // "5"
  if((k1==5) || (k2==5)) key_ram[3]&=~0x08;   // "4"
  if((k1==4) || (k2==4)) key_ram[3]&=~0x04;   // "3"
  if((k1==3) || (k2==3)) key_ram[3]&=~0x02;   // "2"
  if((k1==2) || (k2==2)) key_ram[3]&=~0x01;   // "1"

  if((k1==7) || (k2==7)) key_ram[4]&=~0x10;   // "6"
  if((k1==8) || (k2==8)) key_ram[4]&=~0x08;   // "7"
  if((k1==9) || (k2==9)) key_ram[4]&=~0x04;   // "8"
  if((k1==10) || (k2==10)) key_ram[4]&=~0x02; // "9"
  if((k1==11) || (k2==11)) key_ram[4]&=~0x01; // "0"

  if((k1==46) || (k2==46)) key_ram[5]&=~0x10; // "y"
  if((k1==23) || (k2==23)) key_ram[5]&=~0x08; // "u"
  if((k1==24) || (k2==24)) key_ram[5]&=~0x04; // "i"
  if((k1==25) || (k2==25)) key_ram[5]&=~0x02; // "o"
  if((k1==26) || (k2==26)) key_ram[5]&=~0x01; // "p"

  if((k1==36) || (k2==36)) key_ram[6]&=~0x10; // "h"
  if((k1==37) || (k2==37)) key_ram[6]&=~0x08; // "j"
  if((k1==38) || (k2==38)) key_ram[6]&=~0x04; // "k"
  if((k1==39) || (k2==39)) key_ram[6]&=~0x02; // "l"
  if((k1==43) || (k2==43)) key_ram[6]&=~0x01; // "enter"

  if((k1==50) || (k2==50)) key_ram[7]&=~0x10; // "b"
  if((k1==51) || (k2==51)) key_ram[7]&=~0x08; // "n"
  if((k1==52) || (k2==52)) key_ram[7]&=~0x04; // "m"
  if((akt_shift&0x02)!=0) key_ram[7]&=~0x02;  // "symbol shift"
  if((k1==61) || (k2==61)) key_ram[7]&=~0x01; // "space"
}


//--------------------------------------------------------------
// interne Z80-CPU-Funktion
// ein Byte ins RAM schreiben
//
// ADDR  : [0x4000 - 0xFFFF]
// Value : [0x00 - 0xFF]
//--------------------------------------------------------------
void WrZ80(register word Addr,register byte Value)
{
  uint16_t a1,a2,a3;

  if(Addr<0x4000) { // [0x0000 - 0x3FFF]
    //ROM
    // ins ROM kann nicht geschrieben werden
  }
  else if(Addr<0x5800) { // [0x4000 - 0x57FF]
    // Bildschirm RAM
    // Hinweis : um eine bessere performance zu bekommen,
    // wird das Screen-Ram hier umformatiert
    a1=(Addr&0x181F);
    a2=((Addr&0x0700)>>3);
    a3=((Addr&0x00E0)<<3);
    Addr=a1|a2|a3;
    Z80_RAM[Addr]=Value;
  }
  else if(Addr<0x5B00) { // [0x5800 - 0x5AFF]
    // Attribute RAM
    Z80_RAM[Addr-0x4000]=Value;
  }
  else { // [0x5B00 - 0xFFFF]
    // Data RAM
    Z80_RAM[Addr-0x4000]=Value;
  }
}


//--------------------------------------------------------------
// interne Z80-CPU-Funktion
// ein Byte vom ROM oder RAM lesen
//
// ADDR     : [0x0000 - 0xFFFF]
// ret_wert : [0x00 - 0xFF]
//--------------------------------------------------------------
byte RdZ80(register word Addr)
{
  byte ret_wert=0x00;
  uint16_t a1,a2,a3;

  if(Addr<0x4000) { // [0x0000 - 0x3FFF]
    //ROM
    ret_wert=Z80_ROM[Addr];
  }
  else if(Addr<0x5800) { // [0x4000 - 0x57FF]
    // Bildschirm RAM
    // Hinweis : um eine bessere performance zu bekommen,
    // wird das Screen-Ram hier umformatiert
    a1=(Addr&0x181F);
    a2=((Addr&0x0700)>>3);
    a3=((Addr&0x00E0)<<3);
    Addr=a1|a2|a3;
    ret_wert=Z80_RAM[Addr];
  }
  else if(Addr<0x5B00) { // [0x5800 - 0x5AFF]
    // Attribute RAM
    ret_wert=Z80_RAM[Addr-0x4000];
  }
  else { // [0x5B00 - 0xFFFF]
    // Data RAM
    ret_wert=Z80_RAM[Addr-0x4000];
  }

  return(ret_wert);
}


//--------------------------------------------------------------
// interne Z80-CPU-Funktion
// ein Byte an die Peripherie ausgeben
//
// Port  : [0x0000 - 0xFFFF]
// Value : [0x00 - 0xFF]
//--------------------------------------------------------------
void OutZ80(register word Port,register byte Value)
{
  if((Port&0xFF)==0xFE) {
    // BorderColor und Ton-Out speichern
    out_ram=Value;
    // Ton-Signal schalten
    if((Value&0x10)==0) {
      SPEAKER_PORT->BSRRH = SPEAKER_PIN; // Bit auf Lo
    }
    else {
      SPEAKER_PORT->BSRRL = SPEAKER_PIN; // Bit auf Hi
    }
  }
  if(Port==0xAAAA) {
    // 1bit Digital-OUT [Test von UB]
    ZX_Spectrum.led_mode=1;
    if((Value&0x01)==0) {
      LED_RED_PORT->BSRRH = LED_RED_PIN; // Bit auf Lo
    }
    else {
      LED_RED_PORT->BSRRL = LED_RED_PIN; // Bit auf Hi
    }
  }
}


//--------------------------------------------------------------
// interne Z80-CPU-Funktion
// ein Byte von der Peripherie lesen
//
// Port     : [0x0000 - 0xFFFF]
// ret_wert : [0x00 - 0xFF]
//--------------------------------------------------------------
byte InZ80(register word Port)
{
  byte ret_wert=0xFF;

  if((Port&0xFF)==0xFE) {
    // Abfrage der Tastatur und Ton-IN
    if(Port==0xFEFE) ret_wert=key_ram[0];
    if(Port==0xFDFE) ret_wert=key_ram[1];
    if(Port==0xFBFE) ret_wert=key_ram[2];
    if(Port==0xF7FE) ret_wert=key_ram[3];
    if(Port==0xEFFE) ret_wert=key_ram[4];
    if(Port==0xDFFE) ret_wert=key_ram[5];
    if(Port==0xBFFE) ret_wert=key_ram[6];
    if(Port==0x7FFE) ret_wert=key_ram[7];
  }
  else if((Port&0xFF)==0x1F) {
    // Abfrage vom Kempston-Joystick
    ret_wert=kempston_ram;
  }
  else if(Port==0xAAAA) {
    // 1bit Digital-IN [Test von UB]
    if(GPIO_ReadInputDataBit(BUTTON_PORT, BUTTON_PIN)==Bit_RESET) {
      ret_wert=0x00;
    }
    else {
      ret_wert=0x01;
    }
  }

  return(ret_wert);
}


//--------------------------------------------------------------
// interne Z80-CPU-Funktion
//--------------------------------------------------------------
void PatchZ80(register Z80 *R)
{
  // nothing to do
}


//--------------------------------------------------------------
// interne Z80-CPU-Funktion
//--------------------------------------------------------------
word LoopZ80(register Z80 *R)
{
  word ret_wert=0;

  // kein Interrupt wurde ausgeloest
  ret_wert=INT_NONE;

  return(ret_wert);
}


//--------------------------------------------------------------
// interne Funktion
// init aller IO-Pins
//--------------------------------------------------------------
void P_Spectrum_initIO(void)
{
  GPIO_InitTypeDef  GPIO_InitStructure;

  //---------------------------------
  // init vom Speaker-Pin
  //---------------------------------
  RCC_AHB1PeriphClockCmd(SPEAKER_CLOCK, ENABLE);

  // Config als Digital-Ausgang
  GPIO_InitStructure.GPIO_Pin = SPEAKER_PIN;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_Init(SPEAKER_PORT, &GPIO_InitStructure);

  // Bit auf Lo
  SPEAKER_PORT->BSRRH = SPEAKER_PIN;


  //---------------------------------
  // init vom Button-Pin
  //---------------------------------
  RCC_AHB1PeriphClockCmd(BUTTON_CLOCK, ENABLE);

  // Config als Digital-Eingang
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
  GPIO_InitStructure.GPIO_Pin = BUTTON_PIN;
  GPIO_Init(BUTTON_PORT, &GPIO_InitStructure);
}


//--------------------------------------------------------------
// interne Funktion
// loescht das 48k RAM vom Spectrum
//--------------------------------------------------------------
void P_Spectrum_clearSpectrumRAM(void)
{
  uint32_t n;

  // loeschen vom SpectrumRAM
  for(n=0;n<0xC000;n++) {
    Z80_RAM[n]=0x00;
  }
}


//--------------------------------------------------------------
// interne Funktion
// loescht das 48k Transfer-RAM
//--------------------------------------------------------------
void P_Spectrum_clearTransferRAM(void)
{
  uint32_t n;

  // loeschen vom TransferRAM
  for(n=0;n<TR_BUFFER_SIZE;n++) {
    *(volatile uint8_t*)(TR_BUFFER_START_ADR + n) = 0x00;
  }
}


//--------------------------------------------------------------
// interne Funktion
// startet eine Menu-Funktion (auserhalb vom Spectrum-Basic)
//
// nr : 0 = loescht das Spectrum-RAM und macht einen Reset der CPU
//      1 = loescht das Spectrum-RAM und startet ein Spiel vom Flash
//      2 = loescht das Spectrum-RAM und startet ein Spiel vom Flash
//     99 = zeigt Hilfe-Seite an
//    100 = kopiert das Spectrum-RAM in das Transfer-RAM (als Z80-File)
//          und sendet dann dieses per UART
//    101 = empfängt ein Basic-File per UART (als Z80-File) ins Transfer-RAM
//          und kopiert dieses dann ins Spectrum-RAM
//    102 = empfängt ein Game-File per UART (als Z80-File) ins Transfer-RAM
//          und kopiert dieses dann ins Spectrum-RAM
//--------------------------------------------------------------
void P_Spectrum_startFunktion(uint32_t nr)
{
  if(nr==1) {
    // komplettes Spectrum-RAM loeschen
    P_Spectrum_clearSpectrumRAM();
    // kopiert das Spiel "pssst" vom Flash
    ZX_ReadFromFlash_Z80(&myCPU,(const uint8_t*)ZX_Game_PSSST.table, ZX_Game_PSSST.len);
    // sprung zum aktuellen ProgramCounter  
    JumpZ80(R->PC.W);
    // Mode umstellen
    ZX_Spectrum.akt_mode=1; // 1=GAME
  }
  else if(nr==2) {
    // komplettes Spectrum-RAM loeschen
    P_Spectrum_clearSpectrumRAM();
    // kopiert das Spiel "AticAtac" vom Flash
    ZX_ReadFromFlash_Z80(&myCPU,(const uint8_t*)ZX_Game_AticAtac.table, ZX_Game_AticAtac.len);
    // sprung zum aktuellen ProgramCounter
    JumpZ80(R->PC.W);
    // Mode umstellen
    ZX_Spectrum.akt_mode=1; // 1=GAME
  }
  else if(nr==99) {
    // Mode umstellen
    ZX_Spectrum.akt_mode=5; // 5=HELP
  }
  else if(nr==100) {
    // komplettes Transfer-RAM loeschen
    P_Spectrum_clearTransferRAM();
    // Spectrum-RAM als Z80-File packen
    // und in Transfer-RAM kopieren
    ZX_Spectrum.tr_buffer_len=ZX_WriteToTransfer_Z80(&myCPU,(uint8_t*)(TR_BUFFER_START_ADR));
    // Mode umstellen
    ZX_Spectrum.akt_mode=2; // 2=SAVE
  }
  else if(nr==101) {
    // komplettes Transfer-RAM loeschen
    P_Spectrum_clearTransferRAM();
    ZX_Spectrum.tr_buffer_len=0;
    // Mode umstellen
    ZX_Spectrum.akt_mode=3; // 3=LOAD BASIC
  }
  else if(nr==102) {
    // komplettes Transfer-RAM loeschen
    P_Spectrum_clearTransferRAM();
    ZX_Spectrum.tr_buffer_len=0;
    // Mode umstellen
    ZX_Spectrum.akt_mode=4; // 4=LOAD GAME
  }
  else {
    // komplettes Spectrum-RAM loeschen
    P_Spectrum_clearSpectrumRAM();
    // Reset der CPU
    ResetZ80(&myCPU);
    // Mode umstellen
    ZX_Spectrum.akt_mode=0; // 0=BASIC
  }
}


//--------------------------------------------------------------
// Transfer-Buffer per UART senden (als ".Z80"-File)
//--------------------------------------------------------------
void UB_Spectrum_Send_Buffer(void)
{
  uint32_t n;
  uint8_t wert;

  // Screen loeschen und Text anzeigen
  UB_LCD_SetLayer_2();
  UB_LCD_SetTransparency(255);
  UB_LCD_FillLayer(RGB_COL_WHITE);
  UB_Font_DrawString(200,50," Send Buffer (Z80-File) ",&Arial_7x10,RGB_COL_WHITE,RGB_COL_RED);
  UB_Font_DrawString(160,20,"TX-Pin = PA9",&Arial_7x10,RGB_COL_BLACK,RGB_COL_WHITE);
  UB_Font_DrawString(140,20,"Setting = 115200 Baud, 8N1",&Arial_7x10,RGB_COL_BLACK,RGB_COL_WHITE);
  sprintf(str_buf,"Bytes = %d",(int)(ZX_Spectrum.tr_buffer_len));
  UB_Font_DrawString(120,20,str_buf,&Arial_7x10,RGB_COL_BLACK,RGB_COL_WHITE);
  UB_Font_DrawString(80,50,"sending...",&Arial_7x10,RGB_COL_BLACK,RGB_COL_WHITE);

  // kompletten Transfer-Buffer per UART senden
  for(n=0;n<ZX_Spectrum.tr_buffer_len;n++) {
    wert=*(volatile uint8_t*)(TR_BUFFER_START_ADR + n);
    UB_Uart_SendByte(COM1, wert);
  }

  UB_Font_DrawString(80,50,"  ready.  ",&Arial_7x10,RGB_COL_BLACK,RGB_COL_GREEN);

  // Mode umstellen
  ZX_Spectrum.akt_mode=0; // 0=BASIC
}


//--------------------------------------------------------------
// ".Z80"-File per UART empfangen (in Transfer-Buffer)
// entweder ein Basic-File oder ein Game-File
//--------------------------------------------------------------
void UB_Spectrum_Load_Buffer(void)
{
  uint32_t tim_cnt=0,timeout=0;
  uint32_t old_len=0;

  // Screen loeschen und Text anzeigen
  UB_LCD_SetLayer_2();
  UB_LCD_SetTransparency(255);
  UB_LCD_FillLayer(RGB_COL_WHITE);
  if(ZX_Spectrum.akt_mode==3) {
    UB_Font_DrawString(200,50," Receive Basic-File (Z80-File) ",&Arial_7x10,RGB_COL_WHITE,RGB_COL_RED);
  }
  else {
    UB_Font_DrawString(200,50," Receive Game-File (Z80-File) ",&Arial_7x10,RGB_COL_WHITE,RGB_COL_RED);
  }
  UB_Font_DrawString(160,20,"RX-Pin = PA10",&Arial_7x10,RGB_COL_BLACK,RGB_COL_WHITE);
  UB_Font_DrawString(140,20,"Setting = 115200 Baud, 8N1",&Arial_7x10,RGB_COL_BLACK,RGB_COL_WHITE);
  UB_Font_DrawString(80,50,"waiting...",&Arial_7x10,RGB_COL_BLACK,RGB_COL_WHITE);

  // warten bis alle Bytes per UART in den Transfer-Buffer geladen wurden
  timeout=8000; // ca 5sec timeout
  do {
    sprintf(str_buf,"Bytes = %d",(int)(ZX_Spectrum.tr_buffer_len));
    UB_Font_DrawString(120,20,str_buf,&Arial_7x10,RGB_COL_BLACK,RGB_COL_WHITE);
    tim_cnt++;
    if(ZX_Spectrum.tr_buffer_len!=old_len) {
      old_len=ZX_Spectrum.tr_buffer_len;
      tim_cnt=0;
      timeout=1000; // timeout verkürzen
    }
  }
  while(tim_cnt<timeout);

  if(ZX_Spectrum.tr_buffer_len==0) {
    UB_Font_DrawString(80,50," timeout  ",&Arial_7x10,RGB_COL_BLACK,RGB_COL_GREEN);
  }
  else {
    UB_Font_DrawString(80,50,"  ready.  ",&Arial_7x10,RGB_COL_BLACK,RGB_COL_GREEN);
  }

  // entpacken vom File und kopieren ins Spectrum-RAM
  if(ZX_Spectrum.tr_buffer_len>0) {
    // kopiert das geladene File vom Transfer-RAM
    ZX_ReadFromTransfer_Z80(&myCPU,(uint8_t*)(TR_BUFFER_START_ADR),ZX_Spectrum.tr_buffer_len);
    // sprung zum aktuellen ProgramCounter
    JumpZ80(R->PC.W);
  }

  // Mode umstellen
  if(ZX_Spectrum.akt_mode==3) {
    ZX_Spectrum.akt_mode=0; // 0=BASIC
  }
  else {
    ZX_Spectrum.akt_mode=1; // 1=GAME
  }
}


//--------------------------------------------------------------
// Hilfe Seite anzeigen
//--------------------------------------------------------------
void UB_Spectrum_Help(void)
{
  // Screen loeschen und Text anzeigen
  UB_LCD_SetLayer_2();
  UB_LCD_SetTransparency(255);
  UB_LCD_FillLayer(RGB_COL_WHITE);

  UB_Font_DrawString(220,50," HELP for ZX-Spectrum-Emulator ",&Arial_7x10,RGB_COL_WHITE,RGB_COL_RED);
  UB_Font_DrawString(190,5 ,"'ALT+ESC' = Reset (start ZX-Spectrum Basic)",&Arial_7x10,RGB_COL_BLACK,RGB_COL_WHITE);
  UB_Font_DrawString(170,5 ,"'ALT+F1'  = Start Game 'PSSST' from Flash",&Arial_7x10,RGB_COL_BLACK,RGB_COL_WHITE);
  UB_Font_DrawString(150,5 ,"'ALT+F2'  = Start Game 'AticAtac' from Flash",&Arial_7x10,RGB_COL_BLACK,RGB_COL_WHITE);
  UB_Font_DrawString(130,5 ,"'ALT+s'   = Send Basic-RAM to UART [PA9]",&Arial_7x10,RGB_COL_BLACK,RGB_COL_WHITE);
  UB_Font_DrawString(110,5 ,"'ALT+l'   = Load Basic-File from UART [PA10]",&Arial_7x10,RGB_COL_BLACK,RGB_COL_WHITE);
  UB_Font_DrawString(90 ,5 ,"'ALT+g'   = Load Game-File from UART [PA10]",&Arial_7x10,RGB_COL_BLACK,RGB_COL_WHITE);
  UB_Font_DrawString(70 ,5 ,"Kempston-Joystick = 'Cursor' and 'right ALT'",&Arial_7x10,RGB_COL_BLACK,RGB_COL_WHITE);
  UB_Font_DrawString(50 ,5 ,"Speaker-Out = [PB2]",&Arial_7x10,RGB_COL_BLACK,RGB_COL_WHITE);
  UB_Font_DrawString(20 ,50," www.Mikrocontroller.bplaced.net ",&Arial_7x10,RGB_COL_BLACK,RGB_COL_GREEN);

  // Mode umstellen
  ZX_Spectrum.akt_mode=0; // 0=BASIC
}
