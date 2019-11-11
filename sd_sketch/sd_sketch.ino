///////////////////////////////////////////////////////////////////////////////

// This program samples lots of temperature data from a tree to measure the sap flow.
// TODO: Configure the pulse and sleep timing from config.h so it can be configured on the SD card.

///////////////////////////////////////////////////////////////////////////////

#define ALARM_PIN 12

#include <OPEnS_RTC.h>
#include <LowPower.h>
#include <sdios.h> //for ArduinoOutStream

ArduinoOutStream cout(Serial);


// Instance of our RTC
RTC_DS3231 rtc_ds;


void alarmISR() {
	// Reset the alarm.
  rtc_ds.armAlarm(1, false);
	rtc_ds.clearAlarm(1);
}

void printTime( DateTime t ){
  Serial.print(t.month());
  Serial.print("/");
  Serial.print(t.day());
  Serial.print(" ");
  Serial.print(t.year());
  Serial.print(" ");
  Serial.print(t.hour());
  Serial.print(":");
  Serial.print(t.minute());
  Serial.print(":");
  Serial.println(t.second());
}
// File system object.
void setTimer( int seconds ){
	DateTime t = rtc_ds.now();
  Serial.print("The time is ");
  printTime(t);
  t = t + TimeSpan(seconds);
  Serial.print("Setting alarm...");
	rtc_ds.setAlarm(ALM1_MATCH_HOURS, t.second(), t.minute(), t.hour(), 0);
  Serial.print("Alarm set to ");
  printTime(t);
}

void feather_sleep( void ){
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

void setup() 
{
	// Enable outputs to control 5V and 3.3V rails
	pinMode(5, OUTPUT); pinMode(6, OUTPUT);
	digitalWrite(5, LOW); digitalWrite(6, HIGH);
	pinMode(ALARM_PIN, INPUT_PULLUP);
	pinMode(8, OUTPUT);
	digitalWrite(8, HIGH);
	
	Serial.begin(115200);
  while(!Serial);
  Serial.println("Starting setup");

	// Falling-edge might not wake from sleep. Need more testing.
	attachInterrupt(digitalPinToInterrupt(ALARM_PIN), alarmISR, LOW);
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

}
void loop(void) {
  while(!Serial);
  Serial.println("Welcome!");
  setTimer(60);
  delay(1000);
  feather_sleep();
}
