/*
 * File:        Lab3.c
 * 
 * DAC channel A has actual beam angle to be displayed as waveform
 * DAC channel B has motor control signal to be displayed as waveform
 * Author:      Henri Clarke (hxc2), Priya Kattappurath (psk92)
 * For use with Sean Carroll's Big Board
 * Target PIC:  PIC32MX250F128B
 */

////////////////////////////////////
// clock AND protoThreads configure!
// You MUST check this file!
#include "config_1_3_2.h"
// threading library
#include "pt_cornell_1_3_2.h"
// yup, the expander
#include "port_expander_brl4.h"

////////////////////////////////////
// graphics libraries
// SPI channel 1 connections to TFT
#include "tft_master.h"
#include "tft_gfx.h"
// need for rand function
#include <stdlib.h>
// need for sine function
#include <math.h>
// The fixed point types
#include <stdfix.h>
////////////////////////////////////

// lock out timer interrupt during spi comm to port expander
// This is necessary if you use the SPI2 channel in an ISR
#define start_spi2_critical_section INTEnable(INT_T2, 0);
#define end_spi2_critical_section INTEnable(INT_T2, 1);
/////

#define EnablePullUpB(bits) \
  CNPDBCLR = bits;          \
  CNPUBSET = bits;

////////////////////////////////////
// some precise, fixed, short delays
// to use for extending pulse durations on the keypad
// if behavior is erratic
#define NOP asm("nop");
// 20 cycles
#define wait20 \
  NOP;         \
  NOP;         \
  NOP;         \
  NOP;         \
  NOP;         \
  NOP;         \
  NOP;         \
  NOP;         \
  NOP;         \
  NOP;         \
  NOP;         \
  NOP;         \
  NOP;         \
  NOP;         \
  NOP;         \
  NOP;         \
  NOP;         \
  NOP;         \
  NOP;         \
  NOP;
// 40 cycles
#define wait40 \
  wait20;      \
  wait20;
////////////////////////////////////

/* Demo code for interfacing TFT (ILI9340 controller) to PIC32
 * The library has been modified from a similar Adafruit library
 */
// Adafruit data:
/***************************************************
  This is an example sketch for the Adafruit 2.2" SPI display.
  This library works with the Adafruit 2.2" TFT Breakout w/SD card
  ----> http://www.adafruit.com/products/1480

  Check out the links above for our tutorials and wiring diagrams
  These displays use SPI to communicate, 4 or 5 pins are required to
  interface (RST is optional)
  Adafruit invests time and resources providing this open source code,
  please support Adafruit and open-source hardware by purchasing
  products from Adafruit!

  Written by Limor Fried/Ladyada for Adafruit Industries.
  MIT license, all text above must be included in any redistribution
 ****************************************************/

// string buffer
char buffer[60];

////////////////////////////////////
// Audio DAC ISR
// A-channel, 1x, active
#define DAC_config_chan_A 0b0011000000000000
// B-channel, 1x, active
#define DAC_config_chan_B 0b1011000000000000

// === thread structures ============================================
// thread control structs

//print lock
static struct pt_sem print_sem;

// note that UART input and output are threads
static struct pt pt_cmd, pt_time, pt_input, pt_output, pt_DMA_output, pt_adc;

// system 1 second interval tick
int sys_time_seconds;

//print state variable
int printing = 0;

//== Timer 2 interrupt handler ===========================================
volatile unsigned int DAC_data_A, DAC_data_B; // output values
volatile SpiChannel spiChn = SPI_CHANNEL2;    // the SPI channel to use
volatile int spiClkDiv = 4;                   // 10 MHz max speed for port expander!!

void __ISR(_TIMER_2_VECTOR, ipl2) Timer2Handler(void)
{
  // generate a trigger strobe for timing other events
  // mPORTBSetBits(BIT_0);
  // clear the timer interrupt flag
  mT2ClearIntFlag();
  //mPORTBClearBits(BIT_0);

  // ADC CALCULATIONS BEGIN
  // read the ADC AN11
  adc_9 = ReadADC10(0); // read the result of channel 9 conversion from the idle buffer
  AcquireADC10();       // not needed if ADC_AUTO_SAMPLING_ON below
  beam_angle = (adc_9); // multiply by 4 to output to DAC
  // ADC CALCULATIONS END

  // PID CONTROL BEGINS
  last_error_angle = error_angle;
  error_angle = desired_angle - beam_angle;

  // for differential control: differences between error terms over time
  last_error[0] = last_error[1];
  last_error[1] = last_error[2];
  last_error[2] = last_error[3];
  last_error[3] = error_angle - last_error_angle;

  // total of difference between error terms over time, used for averaging
  total_error = (last_error[0] + last_error[1] + last_error[2] + last_error[3]);

  // PID control terms
  proportional_cntl = P * error_angle;
  differential_cntl = D * (total_error);
  integral_cntl = integral_cntl + error_angle;

  pwm_on_time = proportional_cntl + differential_cntl + (int)(I * integral_cntl);

  // boundaries for error_angle - we don't want it to just STOP
  // error_angle < 0 : if the beam angle > desired angle
  // error_angle > -540 : compensate for the fact that we may go below an ADC value of 0
  //	which has a largely negative error term but is still physically behind desired angle
  //	ie. specific to our setup
  if (error_angle < 0 && error_angle > -540)
  { //if opposite signs
    integral_cntl = .98 * integral_cntl;
  }

  //bounds of PWM
  if (pwm_on_time < 0)
  {
    pwm_on_time = 0;
  }
  else if (pwm_on_time > 39999)
  {
    pwm_on_time = 39999;
  }

  array_pwm_on_time[0] = array_pwm_on_time[1];
  array_pwm_on_time[1] = array_pwm_on_time[2];
  array_pwm_on_time[2] = array_pwm_on_time[3];
  array_pwm_on_time[3] = pwm_on_time;

  average_output_DACB = (array_pwm_on_time[0] + array_pwm_on_time[1] + array_pwm_on_time[2] + array_pwm_on_time[3]) / 4;

  // sets hardware PWM signal using output-compare unit to control motor
  SetDCOC1PWM(pwm_on_time);

  // PID CONTROL ENDS

  // DAC OUTPUT
  DAC_data_A = (int)beam_angle << 2;
  DAC_data_B = average_output_DACB / 10;

  //OUTPUT TO DAC
  // test for ready
  while (TxBufFullSPI2())
    ;
  // reset spi mode to avoid conflict with expander
  SPI_Mode16();
  // DAC-A CS low to start transaction
  mPORTBClearBits(BIT_4); // start transaction
  // write to spi2
  WriteSPI2(DAC_config_chan_A | (DAC_data_A & 0xfff));
  // fold a couple of timer updates into the transmit time
  // test for done
  while (SPI2STATbits.SPIBUSY)
    ; // wait for end of transaction
  // MUST read to clear buffer for port expander elsewhere in code
  int junk = ReadSPI2();
  // CS high
  mPORTBSetBits(BIT_4); // end transaction

  // DAC-B CS low to start transaction
  mPORTBClearBits(BIT_4); // start transaction
  // write to spi2
  WriteSPI2(DAC_config_chan_B | (DAC_data_B & 0xfff));
  // test for done
  while (SPI2STATbits.SPIBUSY)
    ; // wait for end of transaction
  // MUST read to clear buffer for port expander elsewhere in code
  junk = ReadSPI2();
  // CS high
  mPORTBSetBits(BIT_4); // end transaction
                        //

  //
}

// === print a line on TFT =====================================================
// print a line on the TFT
// string buffer
char buffer[60];

void printLine(int line_number, char *print_buffer, short text_color, short back_color)
{
  // line number 0 to 31
  /// !!! assumes tft_setRotation(0);
  // print_buffer is the string to print
  int v_pos;
  v_pos = line_number * 10;
  // erase the pixels
  tft_fillRoundRect(0, v_pos, 239, 8, 1, back_color); // x,y,w,h,radius,color
  tft_setTextColor(text_color);
  tft_setCursor(0, v_pos);
  tft_setTextSize(1);
  tft_writeString(print_buffer);
}

void printLine2(int line_number, char *print_buffer, short text_color, short back_color)
{
  // line number 0 to 31
  /// !!! assumes tft_setRotation(0);
  // print_buffer is the string to print
  int v_pos;
  v_pos = line_number * 20;
  // erase the pixels
  tft_fillRoundRect(0, v_pos, 239, 16, 1, back_color); // x,y,w,h,radius,color
  tft_setTextColor(text_color);
  tft_setCursor(0, v_pos);
  tft_setTextSize(2);
  tft_writeString(print_buffer);
}

// Predefined colors definitions (from tft_master.h)
//#define	ILI9340_BLACK   0x0000
//#define	ILI9340_BLUE    0x001F
//#define	ILI9340_RED     0xF800
//#define	ILI9340_GREEN   0x07E0
//#define ILI9340_CYAN    0x07FF
//#define ILI9340_MAGENTA 0xF81F
//#define ILI9340_YELLOW  0xFFE0
//#define ILI9340_WHITE   0xFFFF

// === thread structures ============================================
// thread control structs
static struct pt pt_timer, pt_key;
;

// system 1 second interval tick
int sys_time_seconds;

// === Timer Thread =================================================
// update a 1 second tick counter

static PT_THREAD(protothread_timer(struct pt *pt))
{
  PT_BEGIN(pt);
  // timer readout
  //sprintf(buffer,"%s", "Time in sec since boot\n");
  //printLine(0, buffer, ILI9340_WHITE, ILI9340_BLACK);

  // set up LED to blink
  mPORTASetBits(BIT_0);           //Clear bits to ensure light is off.
  mPORTASetPinsDigitalOut(BIT_0); //Set port as output

  while (1)
  {
    // yield time 1 second
    PT_YIELD_TIME_msec(1000);
    sys_time_seconds++;
    // toggle the LED on the big board
    mPORTAToggleBits(BIT_0);

    // draw sys_time
    sprintf(buffer, "%d", sys_time_seconds);
    printLine(1, buffer, ILI9340_YELLOW, ILI9340_BLACK);

    sprintf(buffer, "ANGLE:%d", beam_angle);
    printLine(4, buffer, ILI9340_WHITE, ILI9340_BLACK);

    beep = mPORTBReadBits(BIT_3);

    sprintf(buffer, "%d", beep);
    printLine(5, buffer, ILI9340_YELLOW, ILI9340_BLACK);

    // !!!! NEVER exit while !!!!
  } // END WHILE(1)
  PT_END(pt);
} // timer thread

// === Command Thread ======================================================
// The serial interface
static char cmd[30];
static char hex_value[6];

static int cmyk[4] = [0; 0; 0; 0];
static int c = 0;
static int m = 0;
static int y = 0;
static int k = 0;

static char temp_bin_char;

// [hex_to_binary_char hex_char] is the binary string representation of hex_char
char hex_to_binary_char(hex_char)
{
  switch (hex_char)
  {
  case '0':
    return "0000";
  case '1':
    return "0001";
  case '2':
    return "0010";
  case '3':
    return "0011";
  case '4':
    return "0100";
  case '5':
    return "0101";
  case '6':
    return "0110";
  case '7':
    return "0111";
  case '8':
    return "1000";
  case '9':
    return "1001";
  case 'A':
  case 'a':
    return "1010";
  case 'B':
  case 'b':
    return "1011";
  case 'C':
  case 'c':
    return "1100";
  case 'D':
  case 'd':
    return "1101";
  case 'E':
  case 'e':
    return "1110";
  case 'F':
  case 'f':
    return "1111";
  }
}

int min_int(int a, int b)
{
  a < b ? a : b
}

static PT_THREAD(protothread_cmd(struct pt *pt))
{
  PT_BEGIN(pt);
  while (1)
  {
    // send the prompt via DMA to serial
    sprintf(PT_send_buffer, "hex #");
    // by spawning a print thread
    PT_SPAWN(pt, &pt_DMA_output, PT_DMA_PutSerialBuffer(&pt_DMA_output));

    //spawn a thread to handle terminal input
    // the input thread waits for input
    // -- BUT does NOT block other threads
    // string is returned in "PT_term_buffer"
    PT_SPAWN(pt, &pt_input, PT_GetSerialBuffer(&pt_input));
    // returns when the thread dies
    // in this case, when <enter> is pushed
    // now parse the string
    sscanf(PT_term_buffer, "%s", &hex_value);

    //TODO: ensure hex_value has length 6

    //binary values for r, g, b
    r = hex_to_binary_char(hex_value[0]) + hex_to_binary_char(hex_value[1]);
    g = hex_to_binary_char(hex_value[2]) + hex_to_binary_char(hex_value[3]);
    b = hex_to_binary_char(hex_value[4]) + hex_to_binary_char(hex_value[5]);

    if (r_scaled == 0 && g_scaled == 0 && b_scaled == 0)
    {
      k = 1;
      c = 0;
      m = 0;
      y = 0;
    }
    else
    {
      //computed c, m, y
      c = 1 - ((int)strtol(r, NULL, 2)) >> 8;
      m = 1 - ((int)strtol(g, NULL, 2)) >> 8;
      y = 1 - ((int)strtol(b, NULL, 2)) >> 8;

      minCMY = min(c, min(m, y));

      c = (c - minCMY) / (1 - minCMY);
      m = (m - minCMY) / (1 - minCMY);
      y = (y - minCMY) / (1 - minCMY);
      k = minCMY
    }

    // never exit while
  } // END WHILE(1)
  PT_END(pt);
}

// === Key Thread =============================================
volatile int start_time = 99999999;
volatile int has_pushed = 0;
volatile int time_5 = 99999999;
volatile int time_10 = 99999999;
volatile int time_15 = 99999999;

static PT_THREAD(protothread_key(struct pt *pt))
{
  PT_BEGIN(pt);

  while (1)
  {
    // yield time
    PT_YIELD_TIME_msec(30);

    boop = mPORTBReadBits(BIT_3);

    //DEBOUNCING FSM
    switch (PushState)
    {
    case NoPush: //No Push
      sprintf(buffer, "NoPush");
      printLine(8, buffer, ILI9340_YELLOW, ILI9340_BLACK);
      //sprintf(buffer,"%s", "Case 0");
      if (has_pushed)
      {
        start_time = sys_time_seconds;
        time_5 = start_time + 5;
        time_10 = start_time + 10;
        time_15 = start_time + 15;
        desired_angle = (int)((0 + 90) * 2.939);
        has_pushed = 0;
      }
      if (boop == 0)
      {
        PushState = MaybePush;
      }
      else
        PushState = 0;
      break;

    case MaybePush: // Maybe Push
      sprintf(buffer, "MaybePush");
      printLine(8, buffer, ILI9340_YELLOW, ILI9340_BLACK);
      if (boop == 0)
      {
        PushState = Pushed;
      }
      else
        PushState = NoPush;
      break;

    case Pushed: // Pushed
      sprintf(buffer, "Pushed");
      printLine(8, buffer, ILI9340_YELLOW, ILI9340_BLACK);
      has_pushed = 1;
      desired_angle = 0;
      if (boop == 0)
      {
        PushState = Pushed;
      }
      else
        PushState = MaybeNoPush;
      break;

    case MaybeNoPush: // Maybe No Push
      sprintf(buffer, "MaybeNoPush");
      printLine(8, buffer, ILI9340_YELLOW, ILI9340_BLACK);
      if (boop == 0)
      {
        PushState = Pushed;
      }
      else
        PushState = NoPush;
      break;
    }

    sprintf(buffer, "%d", start_time);
    printLine(2, buffer, ILI9340_YELLOW, ILI9340_BLACK);

    sprintf(buffer, "%d", has_pushed);
    printLine(9, buffer, ILI9340_YELLOW, ILI9340_BLACK);

    sprintf(buffer, "%d", desired_angle);
    printLine(10, buffer, ILI9340_YELLOW, ILI9340_BLACK);

    if (sys_time_seconds == time_5)
    {
      desired_angle = (int)((30 + 90) * 2.939);
    }
    else if (sys_time_seconds == time_10)
    {
      desired_angle = (int)((-30 + 90) * 2.939);
    }
    else if (sys_time_seconds == time_15)
    {
      desired_angle = (int)((0 + 90) * 2.939);
    }

    // !!!! NEVER exit while !!!!
  } // END WHILE(1)
  PT_END(pt);
} // keypad thread

// === Main  ======================================================

void main(void)
{
  //SYSTEMConfigPerformance(PBCLK);

  ANSELA = 0;
  ANSELB = 0;

  // set up DAC on big board
  // timer interrupt //////////////////////////
  // Set up timer2 on,  interrupts, internal clock, prescalar 1, toggle rate
  // at 40 MHz PB clock
  OpenTimer2(T2_ON | T2_SOURCE_INT | T2_PS_1_1, generate_period);
  // set up the timer interrupt with a priority of 2
  ConfigIntTimer2(T2_INT_ON | T2_INT_PRIOR_2);
  mT2ClearIntFlag(); // and clear the interrupt flag

  // set pulse to go high at 1/4 of the timer period and drop again at 1/2 the timer period
  OpenOC1(OC_ON | OC_TIMER2_SRC | OC_PWM_FAULT_PIN_DISABLE, 0, 0);
  // OC2 is PPS group 2, map to RPB5 (pin 14)
  PPSOutput(1, RPB7, OC1);

  SetDCOC1PWM(20000);

  // control CS for DAC
  mPORTBSetPinsDigitalOut(BIT_4);
  mPORTBSetBits(BIT_4);
  PPSOutput(2, RPB5, SDO2);

  // divide Fpb by 2, configure the I/O ports. Not using SS in this example
  // 16 bit transfer CKP=1 CKE=1
  // possibles SPI_OPEN_CKP_HIGH;   SPI_OPEN_SMP_END;  SPI_OPEN_CKE_REV
  // For any given peripherial, you will need to match these
  // clk divider set to 4 for 10 MHz
  SpiChnOpen(SPI_CHANNEL2, SPI_OPEN_ON | SPI_OPEN_MODE16 | SPI_OPEN_MSTEN | SPI_OPEN_CKE_REV, 4);
  // end DAC setup

  // init the display
  // NOTE that this init assumes SPI channel 1 connections
  tft_init_hw();
  tft_begin();
  tft_fillScreen(ILI9340_BLACK);
  //240x320 vertical display
  tft_setRotation(0); // Use tft_setRotation(1) for 320x240

  // === setup system wide interrupts  ========
  INTEnableSystemMultiVectoredInt();

  // the ADC ///////////////////////////////////////
  // configure and enable the ADC
  CloseADC10(); // ensure the ADC is off before setting the configuration

  // define setup parameters for OpenADC10
  // Turn module on | ouput in integer | trigger mode auto | enable autosample
  // ADC_CLK_AUTO -- Internal counter ends sampling and starts conversion (Auto convert)
  // ADC_AUTO_SAMPLING_ON -- Sampling begins immediately after last conversion completes; SAMP bit is automatically set
  // ADC_AUTO_SAMPLING_OFF -- Sampling begins with AcquireADC10();
#define PARAM1 ADC_FORMAT_INTG16 | ADC_CLK_AUTO | ADC_AUTO_SAMPLING_OFF //

  // define setup parameters for OpenADC10
  // ADC ref external  | disable offset test | disable scan mode | do 1 sample | use single buf | alternate mode off
#define PARAM2 ADC_VREF_AVDD_AVSS | ADC_OFFSET_CAL_DISABLE | ADC_SCAN_OFF | ADC_SAMPLES_PER_INT_1 | ADC_ALT_BUF_OFF | ADC_ALT_INPUT_OFF
  //
  // Define setup parameters for OpenADC10
  // use peripherial bus clock | set sample time | set ADC clock divider
  // ADC_CONV_CLK_Tcy2 means divide CLK_PB by 2 (max speed)
  // ADC_SAMPLE_TIME_5 seems to work with a source resistance < 1kohm
#define PARAM3 ADC_CONV_CLK_PB | ADC_SAMPLE_TIME_5 | ADC_CONV_CLK_Tcy2 //ADC_SAMPLE_TIME_15| ADC_CONV_CLK_Tcy2

  // define setup parameters for OpenADC10
  // set AN11 and  as analog inputs
#define PARAM4 ENABLE_AN11_ANA // pin 24

  // define setup parameters for OpenADC10
  // do not assign channels to scan
#define PARAM5 SKIP_SCAN_ALL

  // use ground as neg ref for A | use AN11 for input A
  // configure to sample AN11
  SetChanADC10(ADC_CH0_NEG_SAMPLEA_NVREF | ADC_CH0_POS_SAMPLEA_AN11); // configure to sample AN11
  OpenADC10(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5);                  // configure ADC using the parameters defined above

  EnableADC10(); // Enable the ADC
  ///////////////////////////////////////////////////////

  // === config threads ==========
  // turns OFF UART support and debugger pin, unless defines are set
  PT_setup();

  //enable button
  mPORTBSetPinsDigitalIn(BIT_3);
  EnablePullUpB(BIT_3);

  // init the threads
  PT_INIT(&pt_timer);
  PT_INIT(&pt_cmd);
  PT_INIT(&pt_adc);
  PT_INIT(&pt_key);

  // round-robin scheduler for threads
  while (1)
  {
    PT_SCHEDULE(protothread_timer(&pt_timer));
    PT_SCHEDULE(protothread_cmd(&pt_cmd));
    PT_SCHEDULE(protothread_key(&pt_key));
    //PT_SCHEDULE(protothread_adc(&pt_adc));
  }
} // main

// === end  ======================================================
