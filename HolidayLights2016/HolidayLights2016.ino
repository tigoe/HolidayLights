#include <Interval.h>

/*
  This sketch makes 100 NeoPixels fade in a candle-like behavior.

  created 10 Dec 2016
  updated 23 Dec 2016
  by Tom Igoe

*/
#include <Adafruit_NeoPixel.h>
#include <Bridge.h>
#include <Console.h>
#include <Mailbox.h>

const int neoPixelPin = 3;    // pin that's controlling the neoPixels
const int numPixels = 100;    // count of neoPixels

// initialize neoPixel strand:
Adafruit_NeoPixel strand = Adafruit_NeoPixel(numPixels, neoPixelPin, NEO_RGB + NEO_KHZ800);


// colors for flicker effect and twinkle effect
unsigned long keyColors[] = {0x043864, 0x065496, 0x0367B9, 0x09BCFF};
unsigned long newYearsColors[] = {0xA262E3, 0xB4075C, 0xB03E18, 0xBA1530};
unsigned long twinkleColor = 0xCCEEFF;

// count of keyframe colors:
byte numColors = sizeof(keyColors) / 4;

// Extra array to store colors for new years:
unsigned long referenceColors[] = {0, 0, 0, 0};

unsigned long targetColor[numPixels];    // next target for each pixel
unsigned long pixelColor[numPixels];     // current color for each pixel
int lastPixel = 0;                       // last pixel twinkled

// timers for flicker, twinkle, and new year's effect:
Interval flickerTimer;
Interval twinkleTimer;
Interval newYearTimer;

unsigned long twinkles = 0;   // counters for twinkles and flickers
unsigned long flickers = 0;

// state variables:
boolean running = true;
boolean newYears = false;

void setup()  {
  Bridge.begin();   // initialize Bridge
  Mailbox.begin();  // initialize Mailbox
  Console.begin();  // initialize Console library (for debugging)
  strand.begin();    // initialize neoPixel strand

  Console.println("Starting");
  randomSeed(millis() + analogRead(A0));
  flickerTimer.setInterval(flickerPixels, 20);
  twinkleTimer.setInterval(twinkle, 3000);
  beginString();
}

void loop() {
  String message;
  unsigned long twinkleTime = random(3000) + 3000;
  // read the next message present in the queue
  while (Mailbox.messageAvailable()) {
    Mailbox.readMessage(message);
    if (message == "off" ) {
      running = turnOff();
    }
    if (message == "on") {
      running = beginString();
    }
    if (message == "twinkle") {
      twinkle();
    }
    if (message == "hny") {
      happyNewYear();
    }

    if (!newYears) {
      twinkleTimer.setDelay(twinkleTime);
      twinkleTimer.reset();
    }
    if (!running) turnOff();
  }

  strand.show();
  flickerTimer.check();
  twinkleTimer.check();

  if (newYears) newYearTimer.check();
}


boolean turnOff() {
  strand.clear();       // clear the strand
  twinkleTimer.stop();  // stop the timers
  flickerTimer.stop();
  return false;
}

/*
  this function creates the twinkle effect:
*/
void twinkle() {
  twinkles++;
  // pick a random pixel:
  int thisPixel = random(numPixels);
  // if it's the same as the last one flickered, don't do anything more:
  if (thisPixel == lastPixel) return;
  // set the pixel's color:
  pixelColor[thisPixel] = twinkleColor;
  // save pixel number for comparison next time:
  lastPixel = thisPixel;
}

boolean beginString() {
  // reset the intervals:
  Console.println("Starting up");
  twinkleTimer.reset();
  flickerTimer.reset();

  //  set the pixels with colors from the keyColors array:
  for (int pixel = 0; pixel < numPixels; pixel++) {
    pixelColor[pixel] = keyColors[random(numColors)];        // set the pixel color
    strand.setPixelColor(pixel, pixelColor[pixel]);  // set pixel using keyColor
  }
  return true;
}

/*
  this function creates the flicker effect:
*/
void flickerPixels() {
  flickers++;
  // iterate over all pixels:
  for (int thisPixel = 0; thisPixel < numPixels; thisPixel++) {
    // if the target color matches the current color for this pixel,
    // then pick a new target color randomly:
    if (targetColor[thisPixel] == pixelColor[thisPixel]) {
      int nextColor = random(numColors);
      targetColor[thisPixel] = keyColors[nextColor];
    }
    // fade the pixel one step from the current color to the target color:
    pixelColor[thisPixel] = compare(pixelColor[thisPixel], targetColor[thisPixel]);
    // set the pixel color in the strand:
    strand.setPixelColor(thisPixel, pixelColor[thisPixel]);// set the color for this pixel
  }
}

/*
  takes two colors and returns a color that's a point on each axis (r, g, b)
  closer to the target color
*/
unsigned long compare(unsigned long thisColor, unsigned long thatColor) {
  // separate the first color:
  byte r = thisColor >> 16;
  byte g = thisColor >>  8;
  byte b = thisColor;

  // separate the second color:
  byte targetR = thatColor >> 16;
  byte targetG = thatColor >>  8;
  byte targetB = thatColor;

  // fade the first color toward the second color:
  if (r > targetR) r--;
  if (g > targetG) g--;
  if (b > targetB) b--;

  if (r < targetR) r++;
  if (g < targetG) g++;
  if (b < targetB) b++;

  // combine the values to get the new color:
  unsigned long result = ((unsigned long)r << 16) | ((unsigned long)g << 8) | b;
  return result;
}


/*
  Surprise effect and triggered with a cron job
  for New Year's Eve celebration.

*/
void happyNewYear() {
  Console.println("Happy New Year!");

  // save the old keyColors:
  for (int i = 0; i < numColors; i++) {
    referenceColors[i] = keyColors[i];
    keyColors[i] = newYearsColors[i];
  }

  // set all the pixels to the new reference colors,
  // one at a time:
  for (int pixel = 0; pixel < numPixels; pixel++) {
    // set the pixel color:
    int d = random(numColors);
    unsigned long thisColor = keyColors[d];
    strand.setPixelColor(pixel, thisColor);// set the color for this pixel
    strand.show();
    delay(100);
  }
  // run this display for thirty seconds:
  newYearTimer.setTimeout(newYearsOff, 30000);
  twinkleTimer.setDelay(750);
  flickerTimer.setDelay(4);
  newYears = true;
  running = true;
}

void newYearsOff() {
  Console.println("New Years Over");
  // reset to old colors:
  for (int i = 0; i < numColors; i++) {
    keyColors[i] = referenceColors[i];
  }
  // reset string:
  beginString();
  // reset twinkle delay:
  twinkleTimer.setDelay(random(3000) + 3000);
  flickerTimer.setDelay(20);
  twinkleTimer.reset();
  flickerTimer.reset();
  newYears = false;
}

