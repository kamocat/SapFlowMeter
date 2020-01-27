/*
  Use the Qwiic Scale to read load cells and scales
  By: Nathan Seidle @ SparkFun Electronics
  Date: March 3rd, 2019
  License: This code is public domain but you buy me a beer if you use this 
  and we meet someday (Beerware license).

  The Qwiic Scale is an I2C device that converts analog signals to a 24-bit
  digital signal. This makes it possible to create your own digital scale
  either by hacking an off-the-shelf bathroom scale or by creating your
  own scale using a load cell.

  By default the NAU7802 is set to a gain of 16x with 10 samples per second.
  This example shows how to change the gain and sample rate. After these 
  settings are changed it is recommended to do an internal calibration.
  This calibration is for internal configuration of the NAU7802 and is
  different from tearing and calibrating the scale.
  
  SparkFun labored with love to create this code. Feel like supporting open
  source? Buy a board from SparkFun!
  https://www.sparkfun.com/products/15242

  Hardware Connections:
  Plug a Qwiic cable into the Qwiic Scale and a RedBoard Qwiic
  If you don't have a platform with a Qwiic connection use the SparkFun Qwiic Breadboard Jumper (https://www.sparkfun.com/products/14425)
  Open the serial monitor at 9600 baud to see the output
*/

#include <Wire.h>
#include <sdios.h> //for ArduinoOutStream
#include "SparkFun_Qwiic_Scale_NAU7802_Arduino_Library.h" // Click here to get the library: http://librarymanager/All#SparkFun_NAU8702

NAU7802 myScale; //Create instance of the NAU7802 class

ArduinoOutStream cout(Serial);

void setup()
{
  Serial.begin(9600);
  Serial.println("Qwiic Scale Example");

  Wire.begin();

  if (myScale.begin() == false)
  {
    Serial.println("Scale not detected. Please check wiring. Freezing...");
    while (1);
  }
  Serial.println("Scale detected!");

  myScale.setSampleRate(NAU7802_SPS_10); //Sample rate can be set to 10, 20, 40, 80, or 320Hz
  myScale.setGain(NAU7802_GAIN_32); //Gain can be set to 1, 2, 4, 8, 16, 32, 64, or 128.
  myScale.calibrateAFE(); //Does an internal calibration.
  myScale.setChannel(1); // Channel can be 0 or 1
  myScale.calibrateAFE(); //Does an internal calibration.

}

void loop()
{
  if(myScale.available() == true)
  {
    myScale.setGain(NAU7802_GAIN_1); //Gain can be set to 1, 2, 4, 8, 16, 32, 64, or 128.
    myScale.setBit(1, NAU7802_I2C_CONTROL); // read temperature
    myScale.calibrateAFE(); //Does an internal calibration.
    long temp = myScale.getReading();
    myScale.setGain(NAU7802_GAIN_32); //Gain can be set to 1, 2, 4, 8, 16, 32, 64, or 128.
    myScale.setBit(0, NAU7802_I2C_CONTROL); // read channel 0
    myScale.setChannel(0);
    myScale.calibrateAFE(); //Does an internal calibration.
    long A0 = myScale.getReading();
    myScale.setChannel(1);
    myScale.calibrateAFE(); //Does an internal calibration.
    long A1 = myScale.getReading();
    cout<<temp<<", "<<A0<<", "<<A1<<endl;
    
  }
}
