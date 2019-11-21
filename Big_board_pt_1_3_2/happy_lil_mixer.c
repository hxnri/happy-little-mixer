/**
 * This is a very small example that shows how to use
 * === OUTPUT COMPARE  ===
/*
 * File:        Happy Little Mixer
 * Author:      Henri Clarke (hxc2), Michael Rivera (mr858), Priya Kattappurath (psk92)
 * Target PIC:  PIC32MX250F128B
 * Using sample code provided by Bruce Land.
 * ECE 4760, Fall 2019
 */

////////////////////////////////////
// clock AND protoThreads configure!
// You MUST check this file!
#include "config_1_3_2.h"
// threading library
#include "pt_cornell_1_3_2.h"
// port expander
#include "port_expander_brl4.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// === thread structures ============================================
// thread control structs

//print lock
static struct pt_sem print_sem;

// note that UART input and output are threads
static struct pt pt_servo, pt_cmd, pt_time, pt_input, pt_output, pt_DMA_output;

// system 1 second interval tick
int sys_time_seconds;

//The actual period of the wave
static int generate_period = (int)(((20.0 + 1.5) / 32.0) * 40000);
static int pwm_on_time = (int)(1.5 / (20.0 + 1.5) * 40000);
//print state variable
int printing = 0;

// == Timer 2 ISR =====================================================
// just toggles a pin for timeing strobe
void __ISR(_TIMER_2_VECTOR, ipl2) Timer2Handler(void)
{
  // generate a trigger strobe for timing other events
  mPORTBSetBits(BIT_0);
  // clear the timer interrupt flag
  mT2ClearIntFlag();
  mPORTBClearBits(BIT_0);
}

// === Command Thread ======================================================
// The serial interface
static char hex_value[6];

// rgb - from input hex
static int r = 0;
static int g = 0;
static int b = 0;

// cmyk
static int c = 0;
static int m = 0;
static int y = 0;
static int k = 0;

static char temp_bin_char;

/* 
  [hex_to_binary_char hex_char] is the binary string representation of 
  [hex_char]
*/
char hex_to_binary_char(char hex_char)
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

/*
  [min_int a b] is the minimum of a and b, an integer
*/
int min_int(int a, int b)
{
  return a < b ? a : b;
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

    //binary values for r, g, b
    r = hex_to_binary_char(hex_value[0]) + hex_to_binary_char(hex_value[1]);
    g = hex_to_binary_char(hex_value[2]) + hex_to_binary_char(hex_value[3]);
    b = hex_to_binary_char(hex_value[4]) + hex_to_binary_char(hex_value[5]);

    if (r == 0 && g == 0 && b == 0)
    {
      k = 1;
      c = 0;
      m = 0;
      y = 0;
    }
    else
    {
      //computed c, m, y
      // 1 - ([computed rgb value, in binary] / 255)
      int tempc = 1 - ((atoi(r)) >> 8);
      int tempm = 1 - ((atoi(g)) >> 8);
      int tempy = 1 - ((atoi(b)) >> 8);

      int minCMY = min(tempc, min(tempm, tempy));

      // updated cmyk values
      c = (tempc - minCMY) / (1 - minCMY);
      m = (tempm - minCMY) / (1 - minCMY);
      y = (tempy - minCMY) / (1 - minCMY);
      k = minCMY;
    }

    // never exit while
  } // END WHILE(1)
  PT_END(pt);
}

// === Servo Thread ======================================================
// The serial interface
static char cmd[16];
static int value;

static PT_THREAD(protothread_servo(struct pt *pt))
{
  PT_BEGIN(pt);
  while (1)
  {
    PT_YIELD_TIME_msec(100);
    if (sys_time_seconds == 0)
    {
      // update the timer period
      // update the pulse start/stop
      generate_period = (int)(((20.0 + 1.5) / 32.0) * 40000);
      pwm_on_time = (int)(1.5 / (20.0 + 1.5) * 40000);
      WritePeriod2(generate_period);
      SetDCOC2PWM(pwm_on_time);
      SetDCOC3PWM(pwm_on_time);
      SetDCOC4PWM(pwm_on_time);
      SetDCOC5PWM(pwm_on_time);
    }

    if (sys_time_seconds == 1)
    {
      //generate_period = 1.5 * 40000;
      generate_period = (int)(((20.0 + 1.25) / 32.0) * 40000);
      pwm_on_time = (int)(1.25 / (20.0 + 1.25) * 40000);
      WritePeriod2(generate_period);
      SetDCOC2PWM(pwm_on_time);
      SetDCOC3PWM(pwm_on_time);
      SetDCOC4PWM(pwm_on_time);
      SetDCOC5PWM(pwm_on_time);
    } //

    if (sys_time_seconds == 2)
    {
      //generate_period = 1.5 * 40000;
      generate_period = (int)(((20.0 + 1.0) / 32.0) * 40000);
      pwm_on_time = (int)(1.0 / (20.0 + 1.0) * 40000);
      WritePeriod2(generate_period);
      SetDCOC2PWM(pwm_on_time);
      SetDCOC3PWM(pwm_on_time);
      SetDCOC4PWM(pwm_on_time);
      SetDCOC5PWM(pwm_on_time);
    } //
      // never exit while
  }   // END WHILE(1)
  PT_END(pt);
} // thread 3

// === One second Thread ======================================================
// update a 1 second tick counter
static PT_THREAD(protothread_time(struct pt *pt))
{
  PT_BEGIN(pt);

  while (1)
  {
    // yield time 1 second
    PT_YIELD_TIME_msec(1000);
    sys_time_seconds = (sys_time_seconds + 1) % 3;
    // NEVER exit while
  } // END WHILE(1)

  PT_END(pt);
} // thread 4

// === Main  ======================================================

int main(void)
{
  // === Config timer and output compares to make pulses ========
  // set up timer2 to generate the wave period
  OpenTimer2(T2_ON | T2_SOURCE_INT | T2_PS_1_32, generate_period);
  ConfigIntTimer2(T2_INT_ON | T2_INT_PRIOR_2);
  mT2ClearIntFlag(); // and clear the interrupt flag

  // set up compare3 for PWM mode
  OpenOC3(OC_ON | OC_TIMER2_SRC | OC_PWM_FAULT_PIN_DISABLE, pwm_on_time, pwm_on_time); //
  //OpenOC3(OC_ON | OC_TIMER2_SRC | OC_PWM_FAULT_PIN_ENABLE , pwm_on_time, pwm_on_time); //
  // OC3 is PPS group 4, map to RPB9 (pin 18)
  PPSOutput(4, RPB9, OC3);

  // set pulse to go high at 1/4 of the timer period and drop again at 1/2 the timer period
  OpenOC2(OC_ON | OC_TIMER2_SRC | OC_PWM_FAULT_PIN_DISABLE, pwm_on_time, pwm_on_time);
  // OC2 is PPS group 2, map to RPB5 (pin 14)
  PPSOutput(2, RPB5, OC2);

  OpenOC4(OC_ON | OC_TIMER2_SRC | OC_PWM_FAULT_PIN_DISABLE, pwm_on_time, pwm_on_time);
  // OC2 is PPS group 2, map to RPB5 (pin 14)
  PPSOutput(3, RPB2, OC4);

  OpenOC5(OC_ON | OC_TIMER2_SRC | OC_PWM_FAULT_PIN_DISABLE, pwm_on_time, pwm_on_time);
  // OC2 is PPS group 2, map to RPB5 (pin 14)
  PPSOutput(3, RPA2, OC5);

  // === config the uart, DMA, vref, timer5 ISR ===========
  PT_setup();

  // === setup system wide interrupts  ====================
  INTEnableSystemMultiVectoredInt();

  // === set up i/o port pin ===============================
  mPORTBSetPinsDigitalOut(BIT_0); //Set port as output

  // === now the threads ===================================

  // init the threads
  PT_INIT(&pt_servo);
  PT_INIT(&pt_cmd);
  PT_INIT(&pt_time);

  // schedule the threads
  while (1)
  {
    PT_SCHEDULE(protothread_cmd(&pt_servo));
    PT_SCHEDULE(protothread_cmd(&pt_cmd));
    PT_SCHEDULE(protothread_time(&pt_time));
  }
} // main
