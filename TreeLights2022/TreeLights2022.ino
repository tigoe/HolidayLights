#include <Adafruit_NeoPixel.h>
#include <WiFi101.h>
//#include <WiFiNINA.h>
#include <WiFiMDNSResponder.h>
#include <RTCZero.h>

#include "arduino_secrets.h"
#include "index.h"

// some constants:
const char mdnsName[] = "tree";  // mdns name
const int controlPin = 7;        // the I/O pin that the neopixel data signal is on
const int pixelCount = 100;      // number of pixels in the tree
const int colorCount = 3;        // number of different colors on the tree
const int timeZone = -5;         // GMT timezone


WiFiServer server(80);            // make an instance of the server class
WiFiMDNSResponder mdnsResponder;  // MDNS listener

long lastPixelOff = 0;   // last time im millis that a pixel was shut off
int shutdownDelay = 0;   // shutdown delay before next pixel
int pixelsShutdown = 0;  // how many pixels have been shut down

// initialize the tree:
Adafruit_NeoPixel tree = Adafruit_NeoPixel(pixelCount, controlPin,
                                           NEO_RGB + NEO_KHZ800);
// make an array of base colors for each pixel:
unsigned long baseColor[pixelCount];

// here are the main colors on the tree:
unsigned int colors[] = {
// orangeish
  tree.ColorHSV(1900, 255, 210),
  // blue
  tree.ColorHSV(31250, 255, 80),
  // grey
  tree.ColorHSV(22941, 15, 50)
};


// tree status:
String status = "on";

RTCZero rtc;  // realtime clock
int alarmTimes[4][2] = {
  { 6, 15 }, { 8, 45 }, { 16, 15 }, { 22, 45 }
};
int nextAlarmTime[] = { 0, 0 };  // next automated change
int alarmIndex = 0;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  if (!Serial) delay(3000);
  tree.begin();
  // fill with first color before WiFi is connected:
  tree.fill(colors[0]);
  tree.show();

  // while you're not connected to a WiFi AP:
  //  WiFi.beginProvision();
  WiFi.begin(SECRET_SSID, SECRET_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    WiFi.begin(SECRET_SSID, SECRET_PASS);
    // blink the tree to show an unconnected status
    // fill with first color before WiFi is connected:
    if (tree.getPixelColor(0) != colors[0]) {
      tree.fill(colors[0]);
    } else {
      tree.clear();
    }
    tree.show();
    delay(500);
  }
  // second color on to indicate WiFi connected:
  tree.fill(colors[1]);
  tree.show();
  delay(2000);

  if (WiFi.status() == WL_CONNECTED) {
    // print the SSID of the network you're attached to:
    if (Serial) Serial.print("Connected to: ");
    if (Serial) Serial.println(WiFi.SSID());
    // print your WiFi shield's IP address:
    IPAddress ip = WiFi.localIP();
    if (Serial) Serial.print("IP Address: ");
    if (Serial) Serial.println(ip);

    // Setup the MDNS responder to listen to the configured name.
    // NOTE: You _must_ call this _after_ connecting to the WiFi network and
    // being assigned an IP address.
    if (!mdnsResponder.begin(mdnsName)) {
      if (Serial) Serial.println("Failed to start MDNS responder!");
      while (true)
        ;
    }
    server.begin();
    // third color on to indicate MDNS and server:
    tree.fill(colors[2]);
    tree.show();
    if (Serial) Serial.println("started MDNS responder");
  }

  // start the clock:
  rtc.begin();
  // set the time from the network:
  unsigned long epoch;
  do {
    if (Serial) Serial.println("Attempting to get network time");
    epoch = WiFi.getTime();
    delay(1000);
  } while (epoch == 0);

  rtc.setEpoch(epoch);
  if (rtc.getHours() >= abs(timeZone)) {
    rtc.setHours(rtc.getHours() + timeZone);
  } else {
    rtc.setHours(24 + rtc.getHours() + timeZone);
  }
  if (Serial) Serial.println(getTimeStamp());

  // get current hour:
  int now = rtc.getHours();
  // compare to next hour in alarmTimes:
  int next = alarmTimes[alarmIndex][0];
  while (now >= next) {
    // increment alarmIndex to make it the next available time:
    alarmIndex++;
    alarmIndex %= 4;
    next = alarmTimes[alarmIndex][0];
  }
  crontab();
  // turn tree on:
  resetLights();
}

void loop() {
  int h = 10;   // hue
  int s = 255;  // saturation
  int i = 255;  // intensity

  mdnsResponder.poll();
  WiFiClient client = server.available();  // listen for incoming clients

  if (client) {                                // if you get a client,
    if (Serial) Serial.print("new client: ");  // print a message out the serial port
    if (Serial) Serial.println(client.remoteIP());
    String currentLine = "";                         // make a String to hold incoming data from the client
    if (client.connected()) {                        // loop while the client's connected
      while (client.available()) {                   // if there's bytes to read from the client,
        currentLine = client.readStringUntil('\n');  // read a byte, then
        currentLine.trim();                          // trim any return characters
        if (Serial) Serial.println(currentLine);     // print it out the serial monitor

        if (currentLine.startsWith("GET /h/")) {
          String hue = currentLine.substring(6);  // get the substring after the /
          h = hue.toFloat();                      // convert to a number
          changeString(h, s, i);
        }

        if (currentLine.startsWith("GET /s/")) {
          String sat = currentLine.substring(6);  // get the substring after the /
          s = sat.toFloat();                      // convert to a number
          changeString(h, s, i);
        }

        if (currentLine.startsWith("GET /i/")) {
          String intensity = currentLine.substring(6);  // get the substring after the /
          i = intensity.toFloat();                      // convert to a number
          changeString(h, s, i);
        }
        // A message of /on starts the tree:
        if (currentLine.startsWith("GET /on")) {
          resetLights();
        }

        // A message of /off stops the tree:
        if (currentLine.startsWith("GET /off")) {
          treeShutdown();
        }
        // the twinkle routine fades to white:
        if (currentLine.startsWith("GET /twinkle")) {
          twinkle();
        }

        // if there's a time= string in the body, convert
        // to off time:
        if (currentLine.startsWith("nextTime=")) {
          String nextHour = currentLine.substring(9, 11);
          String nextMinute = currentLine.substring(14, 16);
          setNextAlarm(nextHour.toInt(), nextMinute.toInt());
        }
      }  // end while client.available()
      // nothing left from the client, send a response:
      sendResponse(client);
      client.stop();
      if (Serial) Serial.println("client disconnected");
    }
  }
  // fade pixels back to base color one point per 30ms:
  if ((status == "on") && (millis() % 30 < 2)) {
    fadePixels();
  }
  // random twinkle. It's very rare:
  if ((status == "on") && (random(50) == millis() % 1000)) {
    twinkle();
  }
  // update the tree:
  if (status == "on") tree.show();

  // shut down the next pixel of the tree:
  if (status == "shutting down") {
    if ((millis() - lastPixelOff > shutdownDelay) && (pixelsShutdown < pixelCount)) {
      tree.setPixelColor(pixelsShutdown, 0);
      tree.show();
      shutdownDelay = random(300) + 200;
      lastPixelOff = millis();
      pixelsShutdown++;
      if (Serial) Serial.println(pixelsShutdown);
    }
    if (pixelsShutdown >= pixelCount) {
      status = "off";
      tree.clear();
    }
  }
}

void treeShutdown() {
  status = "shutting down";
  shutdownDelay = random(300) + 200;
}
/*
  this function creates the twinkle effect:
*/
void twinkle() {
  int thisPixel = random(pixelCount);
  unsigned long currentColor = tree.getPixelColor(thisPixel);
  tree.setPixelColor(thisPixel, 0xFFEEEE);
}

// fade the pixels:
void fadePixels() {
  // iterate over all pixels:
  for (int thisPixel = 0; thisPixel < pixelCount; thisPixel++) {
    // fade the pixel one step from the current color to the target color:
    compare(thisPixel, tree.getPixelColor(thisPixel), baseColor[thisPixel]);
  }
}

/*
  takes two colors and returns a color that's a point on each axis (r, g, b)
  closer to the target color
*/
void compare(int thisPixel, unsigned long thisColor, unsigned long thatColor) {
  // separate the first color:
  byte r = thisColor >> 16;
  byte g = thisColor >> 8;
  byte b = thisColor;

  // separate the second color:
  byte targetR = thatColor >> 16;
  byte targetG = thatColor >> 8;
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
  tree.setPixelColor(thisPixel, result);
}


// change the string to a single color:
void changeString(int hue, int sat, int intensity) {
  // create a single color from hue, sat, intensity:
  //  unsigned long color = tree.ColorHSV(hue, sat, intensity);

  unsigned long color = tree.ColorHSV(hue, sat, intensity);

  // loop over all the pixels:
  for (int pixel = 0; pixel < pixelCount; pixel++) {
    tree.setPixelColor(pixel, color);  // set the color for this pixel
  }
}

// reset the lights to a random array of the three colors:
void resetLights() {
  if (Serial) Serial.println("reset");
  status = "on";
  // reset pixelsShutdown for next shutdown:
  pixelsShutdown = 0;
  // loop over all the pixels:
  for (int pixel = 0; pixel < pixelCount; pixel++) {
    int colorNumber = random(colorCount);
    int color = colors[colorNumber];
    tree.setPixelColor(pixel, color);  // set the color for this pixel
    baseColor[pixel] = color;
  }
  tree.show();
}

void sendResponse(WiFiClient client) {
  String response = "HTTP/1.1 200 OK\n";
  response += "Content-type:text/html\n\n";
  response += html;
  // add the tree status to the HTML:
  response.replace("TREESTATUS", status);
  // add the current  time:
  response.replace("TIME", getTimeStamp());

  String alarm = "";
  if (nextAlarmTime[0] <= 9) alarm += "0";
  alarm += nextAlarmTime[0];
  alarm += ":";
  if (nextAlarmTime[1] <= 9) alarm += "0";
  alarm += nextAlarmTime[1];

  response.replace("ALARM", alarm);
  // the content of the HTTP response follows the header:
  client.print(response);

  // The HTTP response ends with another blank line:
  client.println();
}

// format the time as hh:mm:ss
String getTimeStamp() {
  String timestamp = "";
  if (rtc.getHours() <= 9) timestamp += "0";
  timestamp += rtc.getHours();
  timestamp += ":";
  if (rtc.getMinutes() <= 9) timestamp += "0";
  timestamp += rtc.getMinutes();
  timestamp += ":";
  if (rtc.getSeconds() <= 9) timestamp += "0";
  timestamp += rtc.getSeconds();
  return timestamp;
}

// sets next time the tree should change:

void setNextAlarm(int hours, int minutes) {
  nextAlarmTime[0] = hours;
  nextAlarmTime[1] = minutes;
  if (Serial) Serial.print("next alarm: ");
  if (Serial) Serial.print(hours);
  if (Serial) Serial.print(":");
  if (Serial) Serial.println(minutes);

  // turn off alarm:
  rtc.disableAlarm();
  rtc.detachInterrupt();
  // set the next alarm time:
  rtc.setAlarmTime(nextAlarmTime[0], nextAlarmTime[1], 0);
  // enable alarm and interrupt
  rtc.enableAlarm(rtc.MATCH_HHMMSS);
  rtc.attachInterrupt(crontab);

  // update alarm index:
  alarmIndex++;
  alarmIndex %= 4;
}

void crontab() {
  if (status == "on") {
    status = "shutting down";
  } else {
    resetLights();
  }

  setNextAlarm(alarmTimes[alarmIndex][0], alarmTimes[alarmIndex][1]);
  alarmIndex++;
  alarmIndex %= 4;

  // Except on Christmas day and Christmas eve
  if ((rtc.getDay() == 25 || rtc.getDay() == 24) && rtc.getMonth() == 12) {
    status == "on";
  }
}
