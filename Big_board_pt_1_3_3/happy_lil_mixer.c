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
#include "config_1_3_3.h"
// threading library
#include "pt_cornell_1_3_3.h"
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
static struct pt pt_servo, pt_cmd, pt_time, pt_input, pt_output, pt_DMA_output, pt_serial_test, pt_DMA_output_aux, pt_input_aux;

// system 1 second interval tick
int sys_time_seconds;

//The actual period of the wave
volatile int generate_period2 = (int)(((20.0 + 0.75) / 32.0) * 40000);
volatile int pwm_on_time2 = (int)((0.75 / (20.0 + 0.75)) * ((20.0 + 0.75) / 32.0) *40000);
volatile int pwm_on_time3 = (int)((0.75 / (20.0 + 0.75)) * ((20.0 + 0.75) / 32.0) *40000);
volatile int pwm_on_time4 = (int)((0.75 / (20.0 + 0.75)) * ((20.0 + 0.75) / 32.0) *40000);
volatile int pwm_on_time5 = (int)((0.75 / (20.0 + 0.75)) * ((20.0 + 0.75) / 32.0) *40000);
//print state variable
volatile int printing = 0;
volatile int i = 0;

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
char hex_value[6] = {};

// cmyk
volatile float final_c = 0;
volatile float final_m = 0;
volatile float final_y = 0;
volatile float final_k = 0;
volatile float error_c = 0;
volatile float error_m = 0;
volatile float error_y = 0;
volatile float error_k = 0;

int count;

/* 
  [hex_to_dec hex_char] is the decimal int representation of 
  char [hex_char]
*/
int hex_to_dec(char hex_char)
{
  switch (hex_char)
  {
  case '0':
    return 0;
    break;
  case '1':
    return 1;  
    break;
  case '2':
    return 2;
    break;
  case '3':
    return 3;
    break;
  case '4':
    return 4;
    break;
  case '5':
    return 5;
      break;
  case '6':
    return 6;
    break;
  case '7':
    return 7;
    break;
  case '8':
    return 8;
    break;
  case '9':
    return 9;
    break;
  case 'A':
      return 10;
      break;
  case 'a':
    return 10;
      break;
  case 'B':
      return 11;
    break;
  case 'b':
    return 11;
    break;
  case 'C':
      return 12;
    break;
  case 'c':
    return 12;
    break;
  case 'D':
      return 13;
    break;
  case 'd':
    return 13;
    break;
  case 'E':
      return 14;
    break;
  case 'e':
    return 14;
    break;
  case 'F':
      return 15;
    break;
  case 'f':
    return 15;
    break;
  }
}

static PT_THREAD(protothread_cmd(struct pt *pt))
{
  PT_BEGIN(pt);

  while (1)
  {
    if(printing == 0){
        float c, m, y, k;
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
    
        int r = (hex_to_dec(hex_value[0]))*16 + hex_to_dec(hex_value[1]);
        int g = (hex_to_dec(hex_value[2]))*16 + hex_to_dec(hex_value[3]);
        int b = (hex_to_dec(hex_value[4]))*16 + hex_to_dec(hex_value[5]);

        if (r == 0 && g == 0 && b == 0){
            k = 1;
            c = 0;
            m = 0;
            y = 0;
        }else{
            float tempc = 1 - ((float)r)/255.0;
            float tempm = 1 - ((float)g)/255.0;
            float tempy = 1 - ((float)b)/255.0;
      
            float minCMY = min(tempc, min(tempm, tempy));

            // updated cmyk values
            c = (tempc - minCMY) / (1 - minCMY);
            m = (tempm - minCMY) / (1 - minCMY);
            y = (tempy - minCMY) / (1 - minCMY);
            k = minCMY;
    
            final_c = c;
            final_m = m;
            final_y = y;
            final_k = k;
        }
    
        sprintf(PT_send_buffer, "%f,%f,%f,%f", final_c, final_m, final_y, final_k);
        PT_SPAWN(pt, &pt_DMA_output, PT_DMA_PutSerialBuffer(&pt_DMA_output));
        final_c = (int)(100*final_c);
        final_m = (int)(100*final_m);
        final_y = (int)(100*final_y);
        final_k = (int)(100*final_k);
        error_c = final_c;
        error_m = final_m;
        error_y = final_y;        
        error_k = final_k;
        printing = 1;
    }
    PT_YIELD_TIME_msec(200);
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
    PT_YIELD_TIME_msec(200);
    //sprintf(PT_send_buffer, "%f,%f,%f,%f,%d", final_c, final_m, final_y, final_k,printing);
    //PT_SPAWN(pt, &pt_DMA_output, PT_DMA_PutSerialBuffer(&pt_DMA_output));
    if (printing != 1)
    {
        pwm_on_time2 = (int)((1.5 / (20.0 + 1.5)) * ((20.0 + 1.5) / 32.0) *40000);
        pwm_on_time3 = (int)((1.25 / (20.0 + 1.25)) * ((20.0 + 1.25) / 32.0) *40000);
        pwm_on_time4 = (int)((1.25 / (20.0 + 1.25)) * ((20.0 + 1.25) / 32.0) *40000);
        pwm_on_time5 = (int)((1.0 / (20.0 + 1.0)) * ((20.0 + 1.0) / 32.0) *40000);
        SetDCOC2PWM(pwm_on_time2);
        SetDCOC3PWM(pwm_on_time3);
        SetDCOC4PWM(pwm_on_time4);
        SetDCOC5PWM(pwm_on_time5);
        
    } else {
        
        while(i < error_c){
            generate_period2 = (int)(((20.0 + 1.0) / 32.0) * 40000);
            pwm_on_time2 = (int)((1.0 / (20.0 + 1.0)) * ((20.0 + 1.0) / 32.0) *40000);
            WritePeriod2(generate_period2);
            SetDCOC2PWM(pwm_on_time2);
            PT_YIELD_TIME_msec(100);
            generate_period2 = (int)(((20.0 + 0.75) / 32.0) * 40000);
            pwm_on_time2 = (int)((0.75 / (20.0 + 0.75)) * ((20.0 + 0.75) / 32.0) *40000);
            WritePeriod2(generate_period2);
            SetDCOC2PWM(pwm_on_time2);
            PT_YIELD_TIME_msec(100);
            sprintf(PT_send_buffer, "\n C: %d,%f", i,final_c);
            PT_SPAWN(pt, &pt_DMA_output, PT_DMA_PutSerialBuffer(&pt_DMA_output));
            i = i + 1;
        }
        i = 0;
        while(i < error_m){
            generate_period2 = (int)(((20.0 + 1.0) / 32.0) * 40000);
            pwm_on_time3 = (int)((1.0 / (20.0 + 1.0)) * ((20.0 + 1.0) / 32.0) *40000);
            WritePeriod2(generate_period2);
            SetDCOC3PWM(pwm_on_time3);
            PT_YIELD_TIME_msec(100);
            generate_period2 = (int)(((20.0 + 0.75) / 32.0) * 40000);
            pwm_on_time3 = (int)((0.75 / (20.0 + 0.75)) * ((20.0 + 0.75) / 32.0) *40000);
            WritePeriod2(generate_period2);
            SetDCOC3PWM(pwm_on_time3);
            PT_YIELD_TIME_msec(100);
            sprintf(PT_send_buffer, "\n M: %d,%f", i,final_m);
            PT_SPAWN(pt, &pt_DMA_output, PT_DMA_PutSerialBuffer(&pt_DMA_output));
            i = i + 1;
        }
        i = 0;
        while(i < error_y){
            generate_period2 = (int)(((20.0 + 1.0) / 32.0) * 40000);
            pwm_on_time4 = (int)((1.0 / (20.0 + 1.0)) * ((20.0 + 1.0) / 32.0) *40000);
            WritePeriod2(generate_period2);
            SetDCOC4PWM(pwm_on_time4);
            PT_YIELD_TIME_msec(100);
            generate_period2 = (int)(((20.0 + 0.75) / 32.0) * 40000);
            pwm_on_time4 = (int)((0.75 / (20.0 + 0.75)) * ((20.0 + 0.75) / 32.0) *40000);
            WritePeriod2(generate_period2);
            SetDCOC4PWM(pwm_on_time4);
            PT_YIELD_TIME_msec(100);
            sprintf(PT_send_buffer, "\n Y: %d,%f", i,final_y);
            PT_SPAWN(pt, &pt_DMA_output, PT_DMA_PutSerialBuffer(&pt_DMA_output));
            i = i + 1;
        }
        i = i + 1;
        while(i < error_k){
            generate_period2 = (int)(((20.0 + 1.0) / 32.0) * 40000);
            pwm_on_time5 = (int)((1.0 / (20.0 + 1.0)) * ((20.0 + 1.0) / 32.0) *40000);
            WritePeriod2(generate_period2);
            SetDCOC5PWM(pwm_on_time5);
            PT_YIELD_TIME_msec(100);
            generate_period2 = (int)(((20.0 + 0.75) / 32.0) * 40000);
            pwm_on_time5 = (int)((0.75 / (20.0 + 0.75)) * ((20.0 + 0.75) / 32.0) *40000);
            WritePeriod2(generate_period2);
            SetDCOC5PWM(pwm_on_time5);
            PT_YIELD_TIME_msec(100);
            sprintf(PT_send_buffer, "\n K: %d,%f", i,final_k);
            PT_SPAWN(pt, &pt_DMA_output, PT_DMA_PutSerialBuffer(&pt_DMA_output));
            i = i + 1;
        }
        i = 0;
        printing = 2;
    }
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
    sys_time_seconds = (sys_time_seconds + 1);
    // NEVER exit while
  } // END WHILE(1)

  PT_END(pt);
} // thread 4

static PT_THREAD(protothread_serial_test(struct pt *pt))
{
  PT_BEGIN(pt);

  while (1)
  {
    // yield time 1 second
    //sprintf(PT_send_buffer, "hi ");
    // by spawning a print thread
    //PT_SPAWN(pt, &pt_DMA_output, PT_DMA_PutSerialBuffer(&pt_DMA_output));
    //sprintf(PT_send_buffer, "\nRunning\n");
    //PT_SPAWN(pt, &pt_DMA_output, PT_DMA_PutSerialBuffer(&pt_DMA_output));
    if(printing == 2){
        //PIC32 Request
        sprintf(PT_send_buffer_aux, "q");
        PT_SPAWN(pt, &pt_DMA_output_aux, PT_DMA_PutSerialBuffer_aux(&pt_DMA_output_aux));
    
        //PIC32 Receive
        //sprintf(PT_send_buffer, "Receiving: ");
        //PT_SPAWN(pt, &pt_DMA_output, PT_DMA_PutSerialBuffer(&pt_DMA_output));
        PT_SPAWN(pt, &pt_input_aux, PT_GetSerialBuffer_aux(&pt_input_aux));
        sscanf(PT_term_buffer_aux, "%s", &hex_value);
    
    
        float c, m, y, k;
        int r = (hex_to_dec(hex_value[0]))*16 + hex_to_dec(hex_value[1]);
        int g = (hex_to_dec(hex_value[2]))*16 + hex_to_dec(hex_value[3]);
        int b = (hex_to_dec(hex_value[4]))*16 + hex_to_dec(hex_value[5]);

        if (r == 0 && g == 0 && b == 0){
            k = 1;
            c = 0;
            m = 0;
            y = 0;
        }else{
            float tempc = 1 - ((float)r)/255.0;
            float tempm = 1 - ((float)g)/255.0;
            float tempy = 1 - ((float)b)/255.0;
      
            float minCMY = min(tempc, min(tempm, tempy));

            // updated cmyk values
            c = (tempc - minCMY) / (1 - minCMY);
            m = (tempm - minCMY) / (1 - minCMY);
            y = (tempy - minCMY) / (1 - minCMY);
            k = minCMY;
            c = (int)(100*c);
            m = (int)(100*m);
            y = (int)(100*y);
            k = (int)(100*k);
            error_c = 0;
            error_m = 0;
            error_y = 0;
            error_k = 0;
            if(abs(final_c - c) + abs(final_m - m) + abs(final_y - y) >= 30){
                if(final_c == 0){
                    if(y > final_y){
                        error_m += y - final_y;
                    }else{
                        error_y += final_y - y;
                    }
                    if(m > final_m){
                        error_y += m - final_m;
                    }else{
                        error_m += final_m - m;
                    }
                }else if(final_m == 0){
                    if(y > final_y){
                        error_c += y - final_y;
                    }else{
                        error_y += final_y - y;
                    }
                    if(c > final_c){
                        error_y += c - final_c;
                    }else{
                        error_c += final_c - c;
                    }
                }else{
                    if(c > final_c){
                        error_m += c - final_c;
                    }else{
                        error_c += final_c - c;
                    }
                    if(m > final_m){
                        error_c += m - final_m;
                    }else{
                        error_m += final_m - m;
                    }
                }
                final_c += error_c;
                final_m += error_m;
                final_y += error_y;        
                final_k += error_k;
                printing = 1;
            }else{
                printing = 0;
            }
        }
    }
    //PIC32 Display Request
    //sprintf(PT_send_buffer, hex_value);
    //PT_SPAWN(pt, &pt_DMA_output, PT_DMA_PutSerialBuffer(&pt_DMA_output));
    PT_YIELD_TIME_msec(200);
    // NEVER exit while
  } // END WHILE(1)

  PT_END(pt);
}


// === Main  ======================================================

int main(void)
{
  // === Config timer and output compares to make pulses ========
  // set up timer2 to generate the wave period
  OpenTimer2(T2_ON | T2_SOURCE_INT | T2_PS_1_32, generate_period2);
  ConfigIntTimer2(T2_INT_ON | T2_INT_PRIOR_2);
  mT2ClearIntFlag(); // and clear the interrupt flag
  
  // set up compare3 for PWM mode
  OpenOC3(OC_ON | OC_TIMER2_SRC | OC_PWM_FAULT_PIN_DISABLE, pwm_on_time3, pwm_on_time3); //
  //OpenOC3(OC_ON | OC_TIMER2_SRC | OC_PWM_FAULT_PIN_ENABLE , pwm_on_time, pwm_on_time); //
  // OC3 is PPS group 4, map to RPB9 (pin 18)
  PPSOutput(4, RPB9, OC3);

  // set pulse to go high at 1/4 of the timer period and drop again at 1/2 the timer period
  OpenOC2(OC_ON | OC_TIMER2_SRC | OC_PWM_FAULT_PIN_DISABLE, pwm_on_time2, pwm_on_time2);
  // OC2 is PPS group 2, map to RPB5 (pin 14)
  PPSOutput(2, RPB5, OC2);

  OpenOC4(OC_ON | OC_TIMER2_SRC | OC_PWM_FAULT_PIN_DISABLE, pwm_on_time4, pwm_on_time4);
  // OC2 is PPS group 2, map to RPB5 (pin 14)
  PPSOutput(3, RPB2, OC4);

  OpenOC5(OC_ON | OC_TIMER2_SRC | OC_PWM_FAULT_PIN_DISABLE, pwm_on_time5, pwm_on_time5);
  // OC2 is PPS group 2, map to RPB5 (pin 14)
  PPSOutput(3, RPA2, OC5);

  //PPSInput(2, U2RX, RPB11);
  
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
  PT_INIT(&pt_serial_test);

  // schedule the threads
  while (1)
  {
    PT_SCHEDULE(protothread_servo(&pt_servo));
    PT_SCHEDULE(protothread_cmd(&pt_cmd));
    PT_SCHEDULE(protothread_time(&pt_time));
    PT_SCHEDULE(protothread_serial_test(&pt_serial_test));
  }
} // main
