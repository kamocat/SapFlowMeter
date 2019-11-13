///////////////////////////////////////////////////////////////////////////////

// This program samples lots of temperature data from a tree to measure the sap flow.
// TODO: Configure the pulse and sleep timing from config.h so it can be configured on the SD card.

///////////////////////////////////////////////////////////////////////////////

#define ALARM_PIN 12

#include <OPEnS_RTC.h>
#include <LowPower.h>
#include <sdios.h> //for ArduinoOutStream

#include <SPI.h>
#include "SdFat.h"

ArduinoOutStream cout(Serial);

// SD chip select pin.  Be sure to disable any other SPI devices such as Enet.
const uint8_t chipSelect = 10;

// Interval between data records in milliseconds.
// The interval must be greater than the maximum SD write latency plus the
// time to acquire and write data to the SD to avoid overrun errors.
// Run the bench example to check the quality of your SD card.
const uint32_t SAMPLE_INTERVAL_MS = 1000;

SdFat sd; // File system object.

ofstream sdout;

// Instance of our RTC
RTC_DS3231 rtc_ds;


void alarmISR() {
	// Reset the alarm.
  rtc_ds.armAlarm(1, false);
  // Disable this interrupt
  detachInterrupt(digitalPinToInterrupt(ALARM_PIN));
}

// File system object.
void setTimer( int seconds ){
	DateTime t = rtc_ds.now();
  Serial.print("The time is ");
  Serial.println(t.text());
  t = t + TimeSpan(seconds);
	rtc_ds.setAlarm(t);
  Serial.print("Alarm set to ");
  Serial.println(t.text());
}

void feather_sleep( void ){
  while(!digitalRead(ALARM_PIN)){
    Serial.print("Waiting on alarm pin...");
    delay(10);
  }
  // Low-level so we can wake from sleep
  // I think calling this twice clears the interrupt.
  attachInterrupt(digitalPinToInterrupt(ALARM_PIN), alarmISR, LOW);
  attachInterrupt(digitalPinToInterrupt(ALARM_PIN), alarmISR, LOW);
  // Prep for sleep
  Serial.end();
  USBDevice.detach();
	digitalWrite(5, HIGH); digitalWrite(6, LOW);
  digitalWrite(LED_BUILTIN, LOW);
  // Sleep
  LowPower.standby();

  // Prep to resume
  digitalWrite(LED_BUILTIN, HIGH);
	digitalWrite(5, LOW); digitalWrite(6, HIGH);
  USBDevice.attach();
  Serial.begin(115200);

}

void log_time( void ){
  Serial.println("Initializing SD Card");
  if (!sd.begin(chipSelect, SD_SCK_MHZ(50))) {
    sd.initErrorHalt();
  }
  sdout = ofstream( "test", ios::out | ios::app );
  DateTime t = rtc_ds.now();
  char * msg = "Awoke at ";
  sdout<<msg<<t.text()<<endl;
  sdout.flush();
}

void setup() 
{
	// Enable outputs to control 5V and 3.3V rails
	pinMode(5, OUTPUT); pinMode(6, OUTPUT);
	digitalWrite(5, LOW); digitalWrite(6, HIGH);
	pinMode(ALARM_PIN, INPUT_PULLUP);
	pinMode(8, OUTPUT);
	digitalWrite(8, HIGH);
	
	Serial.begin(115200);
  //while(!Serial);
  Serial.println("Starting setup");

  // Wait for USB Serial 
  if (! rtc_ds.begin()) {
    Serial.println("Couldn't find RTC");
  }

  // Initialize at the highest speed supported by the board that is
  if (rtc_ds.lostPower()) {
    Serial.println("RTC lost power, lets set the time!");
    // Set the RTC to the date & time this sketch was compiled
    rtc_ds.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
  Serial.println("Setup complete");
  delay(1000); // Give Arduino IDE time to recognize it's running
}
void loop(void) {
  log_time();
  setTimer(10);
  feather_sleep();
}
