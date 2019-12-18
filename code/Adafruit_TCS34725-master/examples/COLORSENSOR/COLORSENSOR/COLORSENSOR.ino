#include <Wire.h>
#include "Adafruit_TCS34725.h"


/* Modified from example code for the Adafruit TCS34725 breakout library */

/* Connect SCL    to analog 5
   Connect SDA    to analog 4
   Connect VDD    to 3.3V DC
   Connect GROUND to common ground
   Connect LED to common ground when not in use,
                         leave open when in use. */

/* Initialise with default values (int time = 2.4ms, gain = 1x) */
// Adafruit_TCS34725 tcs = Adafruit_TCS34725();

/* Initialise with specific int time and gain values */
Adafruit_TCS34725 tcs = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_700MS, TCS34725_GAIN_1X);

//global variables used to compare values
uint16_t oldR, oldG, oldB, is_ready;


char receivedChar;
int receivedInt = 0;
boolean newData = false;

void recvOneChar() {
  if (Serial.available() > 0) {
    receivedChar = Serial.read();
    //Serial.println(receivedChar);
    newData = true;
  }
}

String conHex(int test){
  String upper = String(test / 16);
  String lower = String(test % 16);
  int u = test / 16;
  int l = test % 16;
  if(u > 9){
    if(u == 10){
      upper = 'A';
    }else if(u == 11){
      upper = 'B';
    }else if(u == 12){
      upper = 'C';
    }else if(u == 13){
      upper = 'D';
    }else if(u == 14){
      upper = 'E';
    }else{
      upper = 'F';
    }
  }
  if(l > 9){
    if(l == 10){
      lower = 'A';
    }else if(l == 11){
      lower = 'B';
    }else if(l == 12){
      lower = 'C';
    }else if(l == 13){
      lower = 'D';
    }else if(l == 14){
      lower = 'E';
    }else{
      lower = 'F';
    }
  }
  return upper + lower;
}


void setup(void) {
  Serial.begin(9600);

  if (tcs.begin()) {
    Serial.println("Found sensor");
  }else{
    Serial.println("No TCS34725 found ... check your connections");
  }
  
  //initialize globals
  oldR = 0;
  oldG = 0;
  oldB = 0;
  is_ready = 0;
  // Now we're ready to get readings!
}

void loop(void) {
  while (is_ready == 0)
  {
    recvOneChar();
    if (receivedChar == 'q') {
      is_ready = 1;
    }
  }
  while (is_ready)
  {
    //r,g,b are the raw ADC values, range from 0-65535
    //max is used to scale those values if they are too big
    //R,G,B are the final values that will be sent back to the PIC32
    uint16_t r, g, b, c, MAX, R, G, B, len_StringRGB;
    //raw values scaled from 0-2550
    float scaledR, scaledG, scaledB;

    uint8_t binR, binG, binB;
    String StringR, StringG, StringB, StringRGB;

    tcs.getRawData(&r, &g, &b, &c);

    scaledR = ((float)r / 256) * 10;
    scaledG = ((float)g / 256) * 10;
    scaledB = ((float)b / 256) * 10;

    if (scaledR > 255 | scaledG > 255 | scaledB > 255) {
      MAX = scaledR;
      if (scaledG > MAX) {
        MAX = scaledG;
      }
      if (scaledB > MAX) {
        MAX = scaledB;
      }
      // scale the values to integer 0-255
      R = 255 * scaledR / MAX;
      G = 255 * scaledG / MAX;
      B = 255 * scaledB / MAX;
    }
    else {
      //values are already between 0-255, convert to integer
      R = scaledR;
      G = scaledG;
      B = scaledB;
    }

    //compare to previously scanned value, to ensure an accurate value is sent to PIC32
    if (oldR == R && oldG == G && oldB == B) {

      StringR = conHex(R);
      StringG = conHex(G);
      StringB = conHex(B);
      
      delay(50);
      StringRGB = StringR + StringG + StringB;
      Serial.println(StringRGB);
      //Serial.print(StringRGB);
      //Serial.println(" ");
      is_ready = 0;
    }
    //else {
      //scanning
    //}
    oldR = R;
    oldG = G;
    oldB = B;

  }
}
