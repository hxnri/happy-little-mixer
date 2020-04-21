# Happy Little Mixer

Created by Henri Clarke, Michael Rivera, and Priya Kattappurath
ECE 4760: Digital Design Using Microcontrollers
Cornell University

Final PIC32 Code is in [here]("https://github.com/hxnri/happy-little-mixer/tree/master/code")

Final Arduino Code is in [here]("https://github.com/hxnri/happy-little-mixer/tree/master/code/Adafruit_TCS34725-master/examples/COLORSENSOR/COLORSENSOR")

[Read our lab report.]("hxnri.github.io/happy-little-mixer/")

We wanted to create something fun that would spark joy in its user -- a project that would be simultaneously technically interesting while also having a creative aspect. Through an inspiration-filled night of brainstorming, we decided to make an automatic ink color mixer. Why? Because it could be used to teach young artists the concepts of hue and saturation, to help artists mix colors, and because we wanted to learn more about creating accurate colors. We christened it “Happy Little Mixer,” because we were inspired by the well known phrase, “happy little trees,” used by Bob Ross on his show “Joy of Painting.”

A user can use Google’s color picker, select a color, and input its hex value to the Happy Little Mixer. The Happy Little Mixer then creates that color by measuring out cyan, magenta, yellow, and black (CMYK) ink. If the user is not satisfied with the color created, they can scan it using the color sensor and the system will attempt to apply corrections to the color. We found that our system limits how saturated the final colors can be based on the concentration of the CMYK inks. 

