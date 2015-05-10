//--------------------------------------------------------------
// File     : zx_filetyp_z80.c
// Datum    : 27.01.2014
// Version  : 1.0
// Autor    : UB
// EMail    : mc-4u(@)t-online.de
// Web      : www.mikrocontroller-4u.de
// CPU      : STM32F429
// IDE      : CooCox CoIDE 1.7.4
// GCC      : 4.7 2012q4
// Module   : keine
// Funktion : Handle von ZX-Spectrum Files im Format "*.Z80"
//
// Hinweis  : Die Entpack-Funktion prueft NICHT das Fileformat
//            beim falschem Format event. Systemabsturz !!
//--------------------------------------------------------------


//--------------------------------------------------------------
// Includes
//--------------------------------------------------------------
#include "zx_filetyp_z80.h"


//-------------------------------------------------------------
extern byte out_ram;


//--------------------------------------------------------------
// interne Funktionen
//--------------------------------------------------------------
const uint8_t* p_decompFlashBlock(const uint8_t *block_adr);
uint8_t* p_decompRamBlock(uint8_t *block_adr);
uint8_t p_cntEqualBytes(uint8_t wert, uint32_t adr);



//--------------------------------------------------------------
// Daten von einem File (Typ = *.Z80) aus dem Flash entpacken
// und in den Speicher vom ZX-Spectrum kopieren
//
// data   = pointer auf den Start der Daten
// lenght = Anzahl der Bytes
//--------------------------------------------------------------
void ZX_ReadFromFlash_Z80(Z80 *R, const uint8_t *data, uint16_t length)
{
  const uint8_t *ptr;
  const uint8_t *akt_block,*next_block;
  uint8_t wert1,wert2;
  uint8_t flag_version=0;
  uint8_t flag_compressed=0;
  uint16_t header_len;
  uint16_t akt_adr;

  if(length==0) return;
  if(length>0xC020) return;

  //----------------------------------
  // Auswertung vom Header
  // Byte : [0...29]
  //----------------------------------

  // pointer auf Datenanfang setzen
  ptr=data;

  R->AF.B.h=*(ptr++); // A [0]
  R->AF.B.l=*(ptr++); // F [1]
  R->BC.B.l=*(ptr++); // C [2]
  R->BC.B.h=*(ptr++); // B [3]
  R->HL.B.l=*(ptr++); // L [4]
  R->HL.B.h=*(ptr++); // H [5]

  // PC [6+7]
  wert1=*(ptr++); 
  wert2=*(ptr++);
  R->PC.W=(wert2<<8)|wert1;
  if(R->PC.W==0x0000) {
    flag_version=1;
  }
  else {
    flag_version=0;
  } 

  // SP [8+9]
  wert1=*(ptr++);
  wert2=*(ptr++);
  R->SP.W=(wert2<<8)|wert1;

  R->I=*(ptr++); // I [10]
  R->R=*(ptr++); // R [11]
  
  // Comressed-Flag und Border [12]
  wert1=*(ptr++); 
  wert2=((wert1&0x0E)>>1);
  OutZ80(0xFE, wert2); // BorderColor
  if((wert1&0x20)!=0) {
    flag_compressed=1;
  }
  else {
    flag_compressed=0;
  }

  R->DE.B.l=*(ptr++); // E [13]
  R->DE.B.h=*(ptr++); // D [14]
  R->BC1.B.l=*(ptr++); // C1 [15]
  R->BC1.B.h=*(ptr++); // B1 [16]
  R->DE1.B.l=*(ptr++); // E1 [17]
  R->DE1.B.h=*(ptr++); // D1 [18]
  R->HL1.B.l=*(ptr++); // L1 [19]
  R->HL1.B.h=*(ptr++); // H1 [20]
  R->AF1.B.h=*(ptr++); // A1 [21]
  R->AF1.B.l=*(ptr++); // F1 [22]
  R->IY.B.l=*(ptr++); // Y [23]
  R->IY.B.h=*(ptr++); // I [24]
  R->IX.B.l=*(ptr++); // X [25]
  R->IX.B.h=*(ptr++); // I [26]

  // Interrupt-Flag [27]
  wert1=*(ptr++); 
  if(wert1!=0) {
    // EI
    R->IFF|=IFF_2|IFF_EI;
  }
  else {
    // DI
    R->IFF&=~(IFF_1|IFF_2|IFF_EI);
  }
  wert1=*(ptr++); // nc [28]
  // Interrupt-Mode [29]
  wert1=*(ptr++);  
  if((wert1&0x01)!=0) {
    R->IFF|=IFF_IM1;
  }
  else {
    R->IFF&=~IFF_IM1;
  }
  if((wert1&0x02)!=0) {
    R->IFF|=IFF_IM2;
  }
  else {
    R->IFF&=~IFF_IM2;
  }
  
  // restliche Register
  R->ICount   = R->IPeriod;
  R->IRequest = INT_NONE; 
  R->IBackup  = 0;

  //----------------------------------
  // speichern der Daten im RAM
  // Byte : [30...n]
  //----------------------------------

  akt_adr=0x4000; // start vom RAM
  

  if(flag_version==0) {
    //-----------------------
    // alte Version
    //-----------------------
    if(flag_compressed==1) {
      //-----------------------
      // komprimiert
      //-----------------------      
      while(ptr<(data+length)) {
        wert1=*(ptr++);
        if(wert1!=0xED) {
          WrZ80(akt_adr++, wert1);
        }
        else {
          wert2=*(ptr++);
          if(wert2!=0xED) { 
            WrZ80(akt_adr++, wert1);
            WrZ80(akt_adr++, wert2);
          }
          else {
            wert1=*(ptr++);
            wert2=*(ptr++);
            while(wert1--) {
              WrZ80(akt_adr++, wert2);
            }
          }
        }
      }
    }
    else {
      //-----------------------
      // nicht komprimiert
      //-----------------------
      while(ptr<(data+length)) {
        wert1=*(ptr++);
        WrZ80(akt_adr++, wert1);
      }
    }
  }
  else {
    //-----------------------
    // neue Version
    //-----------------------
    // Header Laenge [30+31]
    wert1=*(ptr++); 
    wert2=*(ptr++);
    header_len=(wert2<<8)|wert1;
    akt_block=(uint8_t*)(ptr+header_len); 
    // PC [32+33]
    wert1=*(ptr++); 
    wert2=*(ptr++);
    R->PC.W=(wert2<<8)|wert1;
    
    //------------------------
    // ersten block auswerten
    //------------------------
    next_block=p_decompFlashBlock(akt_block);
    //------------------------
    // alle anderen auswerten
    //------------------------
    while(next_block<data+length) {
      akt_block=next_block;
      next_block=p_decompFlashBlock(akt_block); 
    } 
  }
}


//--------------------------------------------------------------
// interne Funktion
// entpacken und speichern eines Block von Daten
// (vom Flash) der neuen Version
//--------------------------------------------------------------
const uint8_t* p_decompFlashBlock(const uint8_t *block_adr)
{
  const uint8_t *ptr;
  const uint8_t *next_block;
  uint8_t wert1,wert2;
  uint16_t block_len;
  uint8_t flag_compressed=0;
  uint8_t flag_page=0;
  uint16_t akt_adr=0;

  // pointer auf Blockanfang setzen
  ptr=block_adr;

  // Laenge vom Block
  wert1=*(ptr++);
  wert2=*(ptr++);
  block_len=(wert2<<8)|wert1;
  if(block_len==0xFFFF) {
    block_len=0x4000;
    flag_compressed=0;
  }
  else {
    flag_compressed=1;
  }
 
  // Page vom Block
  flag_page=*(ptr++);
  
  // next Block ausrechnen
  next_block=(uint8_t*)(ptr+block_len); 

  // Startadresse setzen
  if(flag_page==4) akt_adr=0x8000;
  if(flag_page==5) akt_adr=0xC000;
  if(flag_page==8) akt_adr=0x4000;

  if(flag_compressed==1) {
    //-----------------------
    // komprimiert
    //-----------------------
    while(ptr<(block_adr+3+block_len)) {
      wert1=*(ptr++);
      if(wert1!=0xED) {
        WrZ80(akt_adr++, wert1);
      }
      else {
        wert2=*(ptr++);
        if(wert2!=0xED) { 
          WrZ80(akt_adr++, wert1);
          WrZ80(akt_adr++, wert2);
        }
        else {
          wert1=*(ptr++);
          wert2=*(ptr++);
          while(wert1--) {
            WrZ80(akt_adr++, wert2);
          }
        }
      }
    }
  }
  else {
    //-----------------------
    // nicht komprimiert
    //-----------------------
    while(ptr<(block_adr+3+block_len)) {
      wert1=*(ptr++); 
      WrZ80(akt_adr++, wert1);  
    }
  }

  return(next_block); 
}


//--------------------------------------------------------------
// Daten von einem File (Typ = *.Z80) aus dem Transfer-RAM entpacken
// und in den Speicher vom ZX-Spectrum kopieren
//
// data   = pointer auf den Start der Daten
// lenght = Anzahl der Bytes
//--------------------------------------------------------------
void ZX_ReadFromTransfer_Z80(Z80 *R, uint8_t *data, uint16_t length)
{
  uint8_t *ptr;
  uint8_t *akt_block,*next_block;
  uint8_t wert1,wert2;
  uint8_t flag_version=0;
  uint8_t flag_compressed=0;
  uint16_t header_len;
  uint16_t akt_adr;

  if(length==0) return;
  if(length>0xC020) return;

  //----------------------------------
  // Auswertung vom Header
  // Byte : [0...29]
  //----------------------------------

  // pointer auf Datenanfang setzen
  ptr=data;

  R->AF.B.h=*(ptr++); // A [0]
  R->AF.B.l=*(ptr++); // F [1]
  R->BC.B.l=*(ptr++); // C [2]
  R->BC.B.h=*(ptr++); // B [3]
  R->HL.B.l=*(ptr++); // L [4]
  R->HL.B.h=*(ptr++); // H [5]

  // PC [6+7]
  wert1=*(ptr++); 
  wert2=*(ptr++);
  R->PC.W=(wert2<<8)|wert1;
  if(R->PC.W==0x0000) {
    flag_version=1;
  }
  else {
    flag_version=0;
  } 

  // SP [8+9]
  wert1=*(ptr++);
  wert2=*(ptr++);
  R->SP.W=(wert2<<8)|wert1;

  R->I=*(ptr++); // I [10]
  R->R=*(ptr++); // R [11]
  
  // Comressed-Flag und Border [12]
  wert1=*(ptr++); 
  wert2=((wert1&0x0E)>>1);
  OutZ80(0xFE, wert2); // BorderColor
  if((wert1&0x20)!=0) {
    flag_compressed=1;
  }
  else {
    flag_compressed=0;
  }

  R->DE.B.l=*(ptr++); // E [13]
  R->DE.B.h=*(ptr++); // D [14]
  R->BC1.B.l=*(ptr++); // C1 [15]
  R->BC1.B.h=*(ptr++); // B1 [16]
  R->DE1.B.l=*(ptr++); // E1 [17]
  R->DE1.B.h=*(ptr++); // D1 [18]
  R->HL1.B.l=*(ptr++); // L1 [19]
  R->HL1.B.h=*(ptr++); // H1 [20]
  R->AF1.B.h=*(ptr++); // A1 [21]
  R->AF1.B.l=*(ptr++); // F1 [22]
  R->IY.B.l=*(ptr++); // Y [23]
  R->IY.B.h=*(ptr++); // I [24]
  R->IX.B.l=*(ptr++); // X [25]
  R->IX.B.h=*(ptr++); // I [26]

  // Interrupt-Flag [27]
  wert1=*(ptr++); 
  if(wert1!=0) {
    // EI
    R->IFF|=IFF_2|IFF_EI;
  }
  else {
    // DI
    R->IFF&=~(IFF_1|IFF_2|IFF_EI);
  }
  wert1=*(ptr++); // nc [28]
  // Interrupt-Mode [29]
  wert1=*(ptr++);  
  if((wert1&0x01)!=0) {
    R->IFF|=IFF_IM1;
  }
  else {
    R->IFF&=~IFF_IM1;
  }
  if((wert1&0x02)!=0) {
    R->IFF|=IFF_IM2;
  }
  else {
    R->IFF&=~IFF_IM2;
  }
  
  // restliche Register
  R->ICount   = R->IPeriod;
  R->IRequest = INT_NONE; 
  R->IBackup  = 0;

  //----------------------------------
  // speichern der Daten im RAM
  // Byte : [30...n]
  //----------------------------------

  akt_adr=0x4000; // start vom RAM
  

  if(flag_version==0) {
    //-----------------------
    // alte Version
    //-----------------------
    if(flag_compressed==1) {
      //-----------------------
      // komprimiert
      //-----------------------      
      while(ptr<(data+length)) {
        wert1=*(ptr++);
        if(wert1!=0xED) {
          WrZ80(akt_adr++, wert1);
        }
        else {
          wert2=*(ptr++);
          if(wert2!=0xED) { 
            WrZ80(akt_adr++, wert1);
            WrZ80(akt_adr++, wert2);
          }
          else {
            wert1=*(ptr++);
            wert2=*(ptr++);
            while(wert1--) {
              WrZ80(akt_adr++, wert2);
            }
          }
        }
      }
    }
    else {
      //-----------------------
      // nicht komprimiert
      //-----------------------
      while(ptr<(data+length)) {
        wert1=*(ptr++);
        WrZ80(akt_adr++, wert1);
      }
    }
  }
  else {
    //-----------------------
    // neue Version
    //-----------------------
    // Header Laenge [30+31]
    wert1=*(ptr++); 
    wert2=*(ptr++);
    header_len=(wert2<<8)|wert1;
    akt_block=(uint8_t*)(ptr+header_len); 
    // PC [32+33]
    wert1=*(ptr++); 
    wert2=*(ptr++);
    R->PC.W=(wert2<<8)|wert1;
    
    //------------------------
    // ersten block auswerten
    //------------------------
    next_block=p_decompRamBlock(akt_block);
    //------------------------
    // alle anderen auswerten
    //------------------------
    while(next_block<data+length) {
      akt_block=next_block;
      next_block=p_decompRamBlock(akt_block); 
    } 
  }
}


//--------------------------------------------------------------
// interne Funktion
// entpacken und speichern eines Block von Daten
// (vom Transfer-RAM) der neuen Version
//--------------------------------------------------------------
uint8_t* p_decompRamBlock(uint8_t *block_adr)
{
  uint8_t *ptr;
  uint8_t *next_block;
  uint8_t wert1,wert2;
  uint16_t block_len;
  uint8_t flag_compressed=0;
  uint8_t flag_page=0;
  uint16_t akt_adr=0;

  // pointer auf Blockanfang setzen
  ptr=block_adr;

  // Laenge vom Block
  wert1=*(ptr++);
  wert2=*(ptr++);
  block_len=(wert2<<8)|wert1;
  if(block_len==0xFFFF) {
    block_len=0x4000;
    flag_compressed=0;
  }
  else {
    flag_compressed=1;
  }
 
  // Page vom Block
  flag_page=*(ptr++);
  
  // next Block ausrechnen
  next_block=(uint8_t*)(ptr+block_len); 

  // Startadresse setzen
  if(flag_page==4) akt_adr=0x8000;
  if(flag_page==5) akt_adr=0xC000;
  if(flag_page==8) akt_adr=0x4000;

  if(flag_compressed==1) {
    //-----------------------
    // komprimiert
    //-----------------------
    while(ptr<(block_adr+3+block_len)) {
      wert1=*(ptr++);
      if(wert1!=0xED) {
        WrZ80(akt_adr++, wert1);
      }
      else {
        wert2=*(ptr++);
        if(wert2!=0xED) { 
          WrZ80(akt_adr++, wert1);
          WrZ80(akt_adr++, wert2);
        }
        else {
          wert1=*(ptr++);
          wert2=*(ptr++);
          while(wert1--) {
            WrZ80(akt_adr++, wert2);
          }
        }
      }
    }
  }
  else {
    //-----------------------
    // nicht komprimiert
    //-----------------------
    while(ptr<(block_adr+3+block_len)) {
      wert1=*(ptr++); 
      WrZ80(akt_adr++, wert1);  
    }
  }

  return(next_block);
}


//--------------------------------------------------------------
// Daten aus dem Speicher vom ZX-Spectrum lesen, packen und
// als File-Typ (Typ = *.Z80) in einen Transferpuffer kopieren
//
// buffer_start = pointer auf den Start vom Transferpuffer
// return_wert  = Anzahl der Bytes im Transferpuffer
//
// Hinweis : Z80-Version = alte Version , Komprimierung = EIN
//--------------------------------------------------------------
uint16_t ZX_WriteToTransfer_Z80(Z80 *R, uint8_t *buffer_start)
{
  uint16_t ret_wert=0;
  uint8_t *ptr;
  uint8_t wert,anz;
  uint32_t akt_adr;  // muss 32bit sein !!

  //----------------------------------
  // Schreiben vom Header
  // Byte : [0...29]
  //----------------------------------

  // Startadresse setzen
  ptr=buffer_start;

  *(ptr++)=R->AF.B.h; // A [0]
  *(ptr++)=R->AF.B.l; // F [1]
  *(ptr++)=R->BC.B.l; // C [2]
  *(ptr++)=R->BC.B.h; // B [3]
  *(ptr++)=R->HL.B.l; // L [4]
  *(ptr++)=R->HL.B.h; // H [5]

  // PC [6+7] => fuer Version=0
  *(ptr++)=R->PC.B.l;
  *(ptr++)=R->PC.B.h;

  // SP [8+9]
  *(ptr++)=R->SP.B.l;
  *(ptr++)=R->SP.B.h;

  *(ptr++)=R->I; // I [10]
  *(ptr++)=R->R; // R [11]

  // Comressed-Flag und Border [12]
  wert=((out_ram&0x07)<<1);
  wert|=0x20; // fuer Compressed=1
  *(ptr++)=wert;

  *(ptr++)=R->DE.B.l; // E [13]
  *(ptr++)=R->DE.B.h; // D [14]
  *(ptr++)=R->BC1.B.l; // C1 [15]
  *(ptr++)=R->BC1.B.h; // B1 [16]
  *(ptr++)=R->DE1.B.l; // E1 [17]
  *(ptr++)=R->DE1.B.h; // D1 [18]
  *(ptr++)=R->HL1.B.l; // L1 [19]
  *(ptr++)=R->HL1.B.h; // H1 [20]
  *(ptr++)=R->AF1.B.h; // A1 [21]
  *(ptr++)=R->AF1.B.l; // F1 [22]
  *(ptr++)=R->IY.B.l; // Y [23]
  *(ptr++)=R->IY.B.h; // I [24]
  *(ptr++)=R->IX.B.l; // X [25]
  *(ptr++)=R->IX.B.h; // I [26]

  // Interrupt-Flag [27]
  if((R->IFF&IFF_EI)!=0) {
    // EI
    wert=0x01;
  }
  else {
    // DI
    wert=0x00;
  }
  *(ptr++)=wert;
  *(ptr++)=0x00; // nc [28]
  // Interrupt-Mode [29]
  wert=0x00;
  if((R->IFF&IFF_IM1)!=0) wert|=01;
  if((R->IFF&IFF_IM2)!=0) wert|=02;
  *(ptr++)=wert;

  //----------------------------------
  // schreiben der Daten vom RAM
  // Byte : [30...n]
  //----------------------------------

  // Startadresse setzen
  akt_adr=0x4000; // start vom RAM

  //-----------------------
  // alte Version
  // komprimiert
  //-----------------------

  do {
    wert=RdZ80(akt_adr);
    if(wert==0xED) {
      anz=p_cntEqualBytes(wert,akt_adr);
      if(anz>=2) {
        *(ptr++)=0xED;
        *(ptr++)=0xED;
        *(ptr++)=anz;
        *(ptr++)=wert;
        akt_adr+=anz;
      }
      else {
        *(ptr++)=wert;
        akt_adr++;
        if(akt_adr<=0xFFFF) {
          wert=RdZ80(akt_adr);
          *(ptr++)=wert;
          akt_adr++;
        }
      }
    }
    else {
      anz=p_cntEqualBytes(wert,akt_adr);
      if(anz>=5) {
        *(ptr++)=0xED;
        *(ptr++)=0xED;
        *(ptr++)=anz;
        *(ptr++)=wert;
        akt_adr+=anz;
      }
      else {
        *(ptr++)=wert;
        akt_adr++;
      }
    }
  }while(akt_adr<=0xFFFF);

  // Endekennung speichern
  *(ptr++)=0x00;
  *(ptr++)=0xED;
  *(ptr++)=0xED;
  *(ptr++)=0x00;

  // Anzahl der Bytes ausrechnen (Header+Daten)
  ret_wert=ptr-buffer_start;

  return(ret_wert);
}


//--------------------------------------------------------------
// interne Funktion
// anzahl von gleichen Bytes bestimmen
//--------------------------------------------------------------
uint8_t p_cntEqualBytes(uint8_t wert, uint32_t adr)
{
  uint8_t ret_wert=1;
  uint8_t n,test;

  // max 255 gleiche Werte
  for(n=0;n<254;n++) {
    adr++;
    if(adr>=0xFFFF) break;
    test=RdZ80(adr);
    if(test==wert) {
      ret_wert++;
    }
    else {
      break;
    }
  }

  return(ret_wert);
}
