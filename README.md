# Holiday Lights

This is a repository of holiday lights routines.  

## 2022

In 2022 I switched to the ColorHSV function in the Adafruit library and dropped my custom one. 

## 2020 

In 2020, I added an index page so that the user could get the status, set schedules, and turn the tree of and on from a browser.

## 2019

In 2019, the lights were rewritten for the MKR1000. The lights are controlled using the Adafruit_Neopixel library. It had a very basic RESTful interface:

Address:
```http://tree.local/```

API:
* GET /h/`hue`&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;// hue, 0-360
* GET /s/`saturation`&nbsp;&nbsp;&nbsp;// saturation, 0-100
* GET /i/`intensity`&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;// intensity, 0-100
* GET /on&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;// turn on
* GET /off&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;// turn off
* GET /twinkle&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;// make a twinkle
       

## 2016
The 2016 lights were written for an Arduino Yún and a strand of 100 NeoPixels.  

The lights were controlled by the 32U4 processor on the board, communicating with the linux processor on the board to provide automation and a network interface using the Mailbox class of the Arduino [Bridge](https://www.arduino.cc/en/Reference/YunBridgeLibrary) library.

The www folder contains an HTML page written using [p5.js](http://www.p5js.org). This page should be stored in a subdirectory of the Yún's www directory. I called that directory /tree.

The setup-cron.sh file enables cron on the Linux side of the Yún, and the cron.txt file contains cron settings to turn the lights on and off automatically. These settings will turn the tree on fro 7:00 AM to 8:20 AM, and again from 5:30 PM to 11:15 PM. On New Year's Day at midnight, the cron settings will trigger the New Year's function.
