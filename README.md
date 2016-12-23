# Holiday Lights

This is a repository of holiday lights routines.  

The 2016 lights were written for an Arduino Yún and a strand of 100 NeoPixels.  

The lights were controlled by the 32U4 processor on the board, communicating with the linux processor on the board to provide automation and a network interface using the Mailbox class of the Arduino [Bridge](https://www.arduino.cc/en/Reference/YunBridgeLibrary) library.

The www folder contains an HTML page written using [p5.js](http://www.p5js.org). This page should be stored in a subdirectory of the Yún's www directory. I called that directory /tree.

The setup-cron.sh file enables cron on the Linux side of the Yún, and the cron.txt file contains cron settings to turn the lights on and off automatically. These settings will turn the tree on fro 7:00 AM to 8:20 AM, and again from 5:30 PM to 11:15 PM. On New Year's Day at midnight, the cron settings will trigger the New Year's function.
