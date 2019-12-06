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
uint16_t oldR, oldG, oldB;

void setup(void) {
  Serial.begin(38400);

  if (tcs.begin()) {
    Serial.println("Found sensor");
  } else {
    Serial.println("No TCS34725 found ... check your connections");
    while (1);
  }
  //initialize globals
  oldR = 0;
  oldG = 0;
  oldB = 0;
  // Now we're ready to get readings!
}

void loop(void) {
  //r,g,b are the raw ADC values, range from 0-65535
  //max is used to scale those values if they are too big
  //R,G,B are the final values that will be sent back to the PIC32
  uint16_t r, g, b, c, MAX, R, G, B, len_StringRGB;
  //raw values scaled from 0-2550
  float scaledR, scaledG, scaledB;

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
    //Serial.print("R: "); Serial.print(R, DEC); Serial.print(" ");
    //Serial.print("G: "); Serial.print(G, DEC); Serial.print(" ");
    //Serial.print("B: "); Serial.print(B, DEC); Serial.print(" ");
    //Serial.println(" ");

    StringR = String(R);
    StringG = String(G);
    StringB = String(B);
    
    StringRGB = StringR + "," + StringG + "," + StringB;
    len_StringRGB = StringRGB.length();
    
    Serial.println(StringRGB);
    
        
    while (1);
  }
  else {
    Serial.println("Scanning");
  }
  oldR = R;
  oldG = G;
  oldB = B;

}


