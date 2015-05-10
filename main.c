//--------------------------------------------------------------
// File     : main.c
// Datum    : 30.01.2014
// Version  : 1.1
// Autor    : UB
// EMail    : mc-4u(@)t-online.de
// Web      : www.mikrocontroller-4u.de
// CPU      : STM32F429
// IDE      : CooCox CoIDE 1.7.4
// GCC      : 4.7 2012q4
// Module   : CMSIS_BOOT, M4_CMSIS_CORE
// Funktion : ZX-Spectrum Emulator (STM32F429-Disco-Board)
// Hinweis  : Diese zwei Files muessen auf 8MHz stehen
//              "cmsis_boot/stm32f4xx.h"
//              "cmsis_boot/system_stm32f4xx.c"
// In Configuration diese Define hinzufügen :
// "STM32F429_439xx" , "__ASSEMBLY__" , "USE_STDPERIPH_DRIVER"
//--------------------------------------------------------------

#include "main.h"
#include "stm32_ub_systick.h"
#include "stm32_ub_spectrum.h"
#include "stm32_ub_led.h"
#include "stm32f4xx_tim.h"




//--------------------------------------------------------------
// interne Funktionen
//--------------------------------------------------------------
void P_initTimer(void);



//--------------------------------------------------------------
// MAIN vom ZX-Spectrum-Emulator
//--------------------------------------------------------------
int main(void)
{
  DMA2D_Koord k;

  SystemInit(); // Quarz Einstellungen aktivieren


  //------------------------------
  // init der LEDs
  //------------------------------
  UB_Led_Init();

  //------------------------------
  // init vom LCD [im Landscape-Mode]
  //------------------------------
  UB_LCD_Init();
  UB_LCD_SetMode(LANDSCAPE);
  UB_LCD_LayerInit_Fullscreen();
  // auf Vordergrund schalten
  UB_LCD_SetLayer_2();
  UB_LCD_FillLayer(RGB_COL_GREEN);

  //------------------------------
  // init vom USB-Host
  // fuer USB-Keyboard
  //------------------------------
  UB_USB_HID_HOST_Init();

  //------------------------------
  // init vom Systick
  // (muss nach USB gemacht werden)
  //------------------------------
  UB_Systick_Init();

  //------------------------------
  // gruene LED einschalten
  //------------------------------
  UB_Led_On(LED_GREEN);

  //------------------------------
  // init und Reset vom Spectrum
  //------------------------------
  UB_Spectrum_init();

  //------------------------------
  // init vom Timer
  //------------------------------
  P_initTimer();

  //------------------------------
  // init der UART
  //------------------------------
  UB_Uart_Init();
  //UB_Uart_SendString(COM1,"ZX-Spectrum Emulator",LFCR);
  //UB_Uart_SendString(COM1,"V:1.1 / 2014 / UB",LFCR);

  //------------------------------
  // Startscreen anzeigen
  //------------------------------
  k.dest_xp=0;k.dest_yp=0;
  k.source_h=320;k.source_w=240;
  k.source_xp=0;k.source_yp=0;
  UB_Graphic2D_CopyImgDMA(&ZX_Picture,k);
  UB_Font_DrawString(5,5,"V:1.1 / 30.01.2014",&Arial_7x10,RGB_COL_BLACK,SPECTRUM_COL_14);
  // kurze Pause
  UB_Systick_Pause_ms(2000);
  UB_Font_DrawString(5,5,"press 'ALT'+'h' for 'help'",&Arial_7x10,RGB_COL_BLACK,RGB_COL_GREEN);
  // kurze Pause
  UB_Systick_Pause_ms(3000);

  if(usb_status!=USB_HID_KEYBOARD_CONNECTED) {
    ZX_Spectrum.akt_mode=99; // error
  }

  //------------------------------
  while(1)
  {
    if((ZX_Spectrum.akt_mode==0) || (ZX_Spectrum.akt_mode==1)) {
      //------------------------------
      // Mode = BASIC oder GAME
      //------------------------------

      //------------------------------
      // einige Befehlszyklen vom ZX-Spectrum abarbeiten
      //------------------------------
      if(ZX_Spectrum.akt_mode==0) {
        // im Basic-Mode kein Delay
        UB_Spectrum_exec(1000);
      }
      else {
        // Delay (sonst ist die CPU zu schnell)
        if(ZX_Spectrum.cpu_delay>=Z80_CPU_DELAY) {          
          ZX_Spectrum.cpu_delay=0;
          UB_Spectrum_exec(1000);
        }
      }


      //------------------------------
      // Timer fuer Tastatur-Interrupt
      //------------------------------	
      if(INT_Timer_ms==0) {
        INT_Timer_ms=SPECTRUM_INTERRUPT_TIME;
        UB_Sepctrum_interrupt(); // muss VOR dem KEY_SCAN gemacht werden !!
        UB_Spectrum_KEY_scan();
      }


      //------------------------------
      // Timer fuer LED
      //------------------------------
      if(LED_Timer_ms==0) {
        LED_Timer_ms=LED_BLINK_RATE;
        if(usb_status==USB_HID_KEYBOARD_CONNECTED) {
          UB_Led_Toggle(LED_GREEN);
          if(ZX_Spectrum.led_mode==0) UB_Led_Off(LED_RED);
        }
        else {
          if(ZX_Spectrum.led_mode==0) UB_Led_Toggle(LED_RED);
          UB_Led_Off(LED_GREEN);
        }
      }


      //------------------------------
      // Timer fuer LCD
      //------------------------------
      if(LCD_Timer_ms==0) {
        LCD_Timer_ms=LCD_REFRESH_RATE;
        UB_Spectrum_LCD_refresh();
      }

    }
    else if(ZX_Spectrum.akt_mode==2) {
      //------------------------------
      // Mode = SAVE
      //------------------------------
      UB_Spectrum_Send_Buffer();
      // kurze Pause
      UB_Systick_Pause_ms(2000);
    }
    else if((ZX_Spectrum.akt_mode==3) || (ZX_Spectrum.akt_mode==4)) {
      //------------------------------
      // Mode = LOAD (Basic oder Game)
      //------------------------------
      UB_Spectrum_Load_Buffer();
      // kurze Pause
      UB_Systick_Pause_ms(2000);
    }
    else if(ZX_Spectrum.akt_mode==5) {
      //------------------------------
      // Mode = HELP
      //------------------------------
      UB_Spectrum_Help();
      // kurze Pause
      UB_Systick_Pause_ms(8000);
    }
    else {
      //------------------------------
      // Mode = Error
      //------------------------------
      UB_LCD_SetLayer_2();
      UB_LCD_SetTransparency(255);
      UB_LCD_FillLayer(RGB_COL_RED);
      UB_Font_DrawString(200,20," Error : please connect an USB-Keyboard ",&Arial_7x10,RGB_COL_WHITE,RGB_COL_BLACK);
      UB_Font_DrawString(180,20,"         on CN6 and make a 'restart'    ",&Arial_7x10,RGB_COL_WHITE,RGB_COL_BLACK);
      UB_Led_On(LED_RED);
      UB_Led_Off(LED_GREEN);
      while(1);
    }
  }
}


//--------------------------------------------------------------
// Timer und NVIC einstellen (auf 100us)
// TIM2 FRQ = 2*APB1 = 84MHz
// TIM2 = 84MHz / (83+1) / (99+1) = 10kHz => 100us
//--------------------------------------------------------------
void P_initTimer(void)
{
  TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
  NVIC_InitTypeDef NVIC_InitStructure;

  //------------------------------
  // Timer einstellen
  //------------------------------
  // Clock enable
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

  // Timer disable
  TIM_Cmd(TIM2, DISABLE);

  // Timer init (auf 100us)
  TIM_TimeBaseStructure.TIM_Period =  99;
  TIM_TimeBaseStructure.TIM_Prescaler = 83;
  TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
  TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
  TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);

  // Timer preload enable
  TIM_ARRPreloadConfig(TIM2, ENABLE);

  //------------------------------
  // NVIC einstellen
  //------------------------------

  // Update Interrupt enable
  TIM_ITConfig(TIM2,TIM_IT_Update,ENABLE);

  // NVIC konfig
  NVIC_InitStructure.NVIC_IRQChannel = TIM2_IRQn;
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
  NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
  NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
  NVIC_Init(&NVIC_InitStructure);

  // Timer enable
  TIM_Cmd(TIM2, ENABLE);
}


//--------------------------------------------------------------
// Timer-Interrupt
// wird beim erreichen vom Counterwert aufgerufen
// (nach 100us)
//--------------------------------------------------------------
void TIM2_IRQHandler(void)
{
  if (TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET) {
    // wenn Interrupt aufgetreten
    TIM_ClearITPendingBit(TIM2, TIM_IT_Update);

    ZX_Spectrum.cpu_delay+=100; // 100us sind um
  }
}
