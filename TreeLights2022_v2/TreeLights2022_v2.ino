/*

  Holiday tree 2022 v.2
  This version of the holiday tree sends MQTT updates to a broker
  and the time can be updated via the same MQTT channel. 

  created 15 Dec 2022
  by Tom Igoe

*/

#include <Adafruit_NeoPixel.h>
#include <WiFi101.h>
// #include <WiFiNINA.h>
#include <ArduinoMqttClient.h>
#include <RTCZero.h>
#include <Arduino_JSON.h>

#include "arduino_secrets.h"

// some constants:
const int controlPin = 7;    // the I/O pin that the neopixel data signal is on
const int pixelCount = 100;  // number of pixels in the tree
const int colorCount = 3;    // number of different colors on the tree
const int timeZone = -5;     // GMT timezone

// possible statuses for the tree:
enum treeStatus {
  treeOff = 0,
  treeOn = 1,
  treeShuttingDown = 2
};

// // tree status:
int status = treeOn;
int lastStatus = treeOff;
int nextStatus = treeOff;
long lastTwinkle, lastFade, lastConnectAttempt = 0;
int lastMinute = 0;
long lastPixelOff = 0;   // last time im millis that a pixel was shut off
int shutdownDelay = 0;   // shutdown delay before next pixel
int pixelsShutdown = 0;  // how many pixels have been shut down

// initialize WiFi connection:
WiFiClient wifi;
MqttClient mqttClient(wifi);

// details for MQTT client:
char broker[] = "test.mosquitto.org";
int port = 1883;
char topic[] = "tree";
char clientID[] = "stella";

// initialize the tree:
Adafruit_NeoPixel tree = Adafruit_NeoPixel(pixelCount, controlPin,
                                           NEO_RGB + NEO_KHZ800);
// make an array of base colors for each pixel:
unsigned long baseColor[pixelCount];
// the colors on the tree:
JSONVar colors;

RTCZero rtc;  // realtime clock
// schedule of alarms for the changes:
// hour, minute, status
int alarmTimes[4][3]{
  { 6, 15, treeOn },
  { 8, 45, treeShuttingDown },
  { 16, 15, treeOn },
  { 21, 15, treeShuttingDown }
};
// which alarm is next:
int alarmIndex = 0;

void setup() {
  // initialize the randomizer:
  randomSeed(analogRead(A0));
  // Start serial, 3 sec. delay for serial monitor to open:
  Serial.begin(9600);
  if (!Serial) delay(3000);

  // JSONVar arrays have to be set in a function:
  colors[0] = tree.ColorHSV(1900, 255, 210);  // orangeish
  colors[1] = tree.ColorHSV(31250, 255, 80);  // blue
  colors[2] = tree.ColorHSV(22941, 15, 50);   // grey

  tree.begin();
  // fill with first color before WiFi is connected:
  tree.fill(colors[0]);
  tree.show();

  // while you're not connected to a WiFi AP:
  WiFi.begin(SECRET_SSID, SECRET_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    WiFi.begin(SECRET_SSID, SECRET_PASS);
    // blink the tree to show an unconnected status
    // fill with first color before WiFi is connected:
    if (tree.getPixelColor(0) != (unsigned int)colors[0]) {
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

  if (WiFi.status() == WL_CONNECTED) {
    // print the SSID of the network you're attached to:
    if (Serial) {
      // print your WiFi network name and deviceIP address:
      Serial.print("Connected to: ");
      Serial.println(WiFi.SSID());
      Serial.print("IP Address: ");
      Serial.println(WiFi.localIP());
    }
  }

  mqttClient.setId(clientID);
  // if needed, login to the broker with a username and password:
  //mqttClient.setUsernamePassword(SECRET_MQTT_USER, SECRET_MQTT_PASS);

  // start the clock:
  rtc.begin();
  // set the time from the network:
  unsigned long epoch;
  do {
    if (Serial) Serial.println("Attempting to get network time");
    epoch = WiFi.getTime();
    delay(1000);
  } while (epoch == 0);
  // set the time zone offset:
  setTimeOffset(epoch);
  // third color on to indicate MQTT Ok:
  tree.fill(colors[2]);
  tree.show();
  // turn tree on:
  resetLights();
  // update next alarm:
  crontab();
  // connect to MQTT broker:
  if (connectToBroker()) {
    // sent an MQTT update:
    mqttUpdate();
  }
}

void loop() {
  // depending on the status, change the pixels on the tree:
  switch (status) {
    case treeOff:
      tree.clear();
      tree.show();
      break;
    case treeOn:
      // fade pixels back to base color one point per 30ms:
      if ((millis() - lastFade > 30)) {
        fadePixels();
      }
      // random twinkle. Every 5-10 seconds:
      if (millis() - lastTwinkle > (random(5) + 5) * 1000) {
        twinkle();
      }
      // update the tree:
      tree.show();
      break;
    case treeShuttingDown:
      // if there are still pixels on:
      if (pixelsShutdown < pixelCount) {
        // if the shutdownDelay has passed:
        if (millis() - lastPixelOff > shutdownDelay) {
          // turn off the latest pixel:
          tree.setPixelColor(pixelsShutdown, 0);
          // update the tree:
          tree.show();
          // generate a new delay for the next pixel to shut down:
          shutdownDelay = random(300) + 200;
          // timestamp this pixel's shutdown time:
          lastPixelOff = millis();
          // increment which pixel you are on:
          pixelsShutdown++;
        }
      }
      // if the last pixel has been turned off:
      else {
        status = treeOff;
        // clear the tree:
        tree.clear();
        mqttUpdate();
      }
      break;
  }

  // update the crontab every time the status has changed:
  if (status != lastStatus) {
    if (Serial) {
      Serial.print("New tree status: ");
      Serial.println(status);
    }
    // when tree goes off, update time:
    if (status == treeOff) {
      // set the time from the network:
      unsigned long epoch;
      int attempts = 0;
      do {
        if (Serial) Serial.println("Attempting to get network time");
        epoch = WiFi.getTime();
        delay(1000);
      } while (attempts > 6);
    }
    if (mqttClient.connected()) {
      mqttUpdate();
    }
    // update next alarm time:
    crontab();
    lastStatus = status;
  }

  if (mqttClient.connected()) {
    // if you're connected, poll the broker for updates:
    mqttClient.poll();
  }


  //  send an update to the broker once a minute:
  int currentMinute = rtc.getMinutes();
  // update broker once a minute:
  if ((currentMinute != lastMinute)) {
    // if not connected to the broker, try to connect:
    if (!mqttClient.connected()) {
      if (Serial) Serial.println("reconnecting");
      connectToBroker();
    } else {
      mqttUpdate();
    }
    lastMinute = currentMinute;
  }
}

void treeUpdate() {
  // change the status of the tree:
  status = nextStatus;
  // Except on Christmas day and Christmas eve between 6 AM and 10 PM
}

void treeShutdown() {
  Serial.println("tree shutting down");
  status = treeShuttingDown;
  shutdownDelay = random(300) + 200;
  for (int pixel = 0; pixel < pixelCount; pixel++) {
    baseColor[pixel] = 0;
  }
}
/*
  this function creates the twinkle effect:
*/
void twinkle() {
  int thisPixel = random(pixelCount);
  unsigned long currentColor = tree.getPixelColor(thisPixel);
  tree.setPixelColor(thisPixel, 0xFFEEEE);
  lastTwinkle = millis();
}

// fade the pixels:
//  Can possibly redo this using HSV
void fadePixels() {
  // iterate over all pixels:
  for (int thisPixel = 0; thisPixel < pixelCount; thisPixel++) {
    // fade the pixel one step from the current color to the target color:
    compare(thisPixel, tree.getPixelColor(thisPixel), baseColor[thisPixel]);
  }
  lastFade = millis();
}

/*
  takes two colors and returns a color that's a point on each axis (r, g, b)
  closer to the target color

  This version uses HSV
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

// reset the lights to a random array of the three colors:
void resetLights() {
  status = treeOn;
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

// format the time as hh:mm:ss
String getTimeStamp() {
  String timestamp = "";
  if (rtc.getMonth() <= 9) timestamp += "0";
  timestamp += rtc.getMonth();
  timestamp += "-";
  if (rtc.getDay() <= 9) timestamp += "0";
  timestamp += rtc.getDay();
  timestamp += "-";
  if (rtc.getYear() <= 9) timestamp += "0";
  timestamp += rtc.getYear();
  timestamp += "_";
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

// set the time zone offset:
void setTimeOffset(long thisEpoch) {
  rtc.setEpoch(thisEpoch);
  if (rtc.getHours() >= abs(timeZone)) {
    rtc.setHours(rtc.getHours() + timeZone);
  } else {
    rtc.setHours(24 + rtc.getHours() + timeZone);
  }
  if (Serial) Serial.println(getTimeStamp());
}

// sets next time the tree should change:
void setNextAlarm(int nextAlarm[]) {
  // set next status:
  nextStatus = nextAlarm[2];
  // turn off alarm:
  rtc.disableAlarm();
  rtc.detachInterrupt();
  // set the next alarm time:
  rtc.setAlarmTime(nextAlarm[0], nextAlarm[1], 0);

  // enable alarm and interrupt
  rtc.enableAlarm(rtc.MATCH_HHMMSS);
  rtc.attachInterrupt(treeUpdate);
}

void crontab() {
  int now, next = 0;
  int thisIndex = 0;
  // loop over the alarm times to determine
  // the next one after the current time:
  while (now >= next) {
    // get current time in minutes:
    now = rtc.getHours() * 60 + rtc.getMinutes();
    // compare to next  time in alarmTimes, in minutes:
    next = (int)alarmTimes[thisIndex][0] * 60 + (int)alarmTimes[thisIndex][1];

    if (now >= next) {
      // increment thisIndex to make it the next available time:
      thisIndex++;
      thisIndex %= 4;
      // wrap around for next day:
      if (thisIndex == 0) {
        break;
      }
    }
  }

  alarmIndex = thisIndex;
  // set the next alarm time:
  setNextAlarm(alarmTimes[alarmIndex]);
}

// connect to the MQTT broker:
boolean connectToBroker() {
  lastConnectAttempt = millis();
  // if the MQTT client is not connected:
  if (!mqttClient.connect(broker, port)) {
    // print out the error message:
    Serial.print("MOTT connection failed. Error no: ");
    Serial.println(mqttClient.connectError());
    // return that you're not connected:
    return false;
  }

  // set the message receive callback
  mqttClient.onMessage(onMqttMessage);
  // listen to changes on /epoch:
  String subscription = String(topic) + "/epoch";
  mqttClient.subscribe(subscription);
  // subscribe to any topics that you want to be writable:
  subscription = String(topic) + "/status";
  mqttClient.subscribe(subscription);
  // subscribe to any topics that you want to be writable:
  subscription = String(topic) + "/update";
  mqttClient.subscribe(subscription);
  // return that you're connected:
  return true;
}

// handler for when MQTT messages arrive:
void onMqttMessage(int messageSize) {
  String message = "";

  // get the position of the beginning of the subtopic (after the /):
  int topicLength = String(topic).length() + 1;
  // get the subtopic:
  String subTopic = String(mqttClient.messageTopic()).substring(topicLength);
  subTopic.trim();

  // you received a message, print out the topic and contents
  while (mqttClient.available()) {
    message = mqttClient.readStringUntil('\n');
  }
  if (Serial) {
    Serial.print("subTopic: ");
    Serial.print(subTopic);
    Serial.print(",  message: ");
    Serial.println(message);
  }
  // if it's on the setTime topic, set the time with the value:
  if (subTopic == "epoch") {
    long now = message.toInt();
    // if it's not the same as current:
    if ((now != rtc.getEpoch())) {
      setTimeOffset(now);
      crontab();
    }
  }
  // if subtopic "status" is updated, change tree status:
  if (subTopic == "status") {
    // change the tree status to the current value
    int value = message.toInt();
    // if value is in range of treeStatus values:
    if (value >= treeOff && value <= treeShuttingDown) {
      status = value;
    }
  }
  // if subtopic "update" is updated, send a general update:
  if (subTopic == "update") {
    if (Serial) Serial.print(message);
    mqttUpdate();
  }
}

// send an MQTT update:
void mqttUpdate() {
  // JSON object for all the characteristics of the tree:
  JSONVar treeStatus;
  treeStatus["time"] = getTimeStamp();
  treeStatus["status"] = status;
  treeStatus["name"] = clientID;
  String nextAlarm = String(rtc.getAlarmHours()) + ":";
  nextAlarm += String(rtc.getAlarmMinutes());
  treeStatus["nextAlarm"] = nextAlarm;

  // iterate over the JSON object to send MQTT messages:
  JSONVar keys = treeStatus.keys();
  for (int i = 0; i < keys.length(); i++) {
    JSONVar value = treeStatus[keys[i]];
    String thisTopic = keys[i];
    thisTopic = "/" + thisTopic;
    thisTopic = String(topic) + thisTopic;
    // update the topic:
    mqttClient.beginMessage(thisTopic);
    // print the body of the message:
    mqttClient.println(value);
    // send the message:
    mqttClient.endMessage();
  }
}
