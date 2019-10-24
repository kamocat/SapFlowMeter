///////////////////////////////////////////////////////////////////////////////

// This program samples lots of temperature data from a tree to measure the sap flow.
// TODO: Configure the pulse and sleep timing from config.h so it can be configured on the SD card.

///////////////////////////////////////////////////////////////////////////////

#define ALARM_PIN 12
#define HEATER 11

#include <OPEnS_RTC.h>

enum state{
	wake,
	heating,
	cooling,
	sleep
} measuring_state;


// Instance of our RTC
RTC_DS3231 rtc_ds;

volatile bool alarmFlag = false;
void alarmISR() {
	// Do I need to detach the interrupt every time, or can I just clear the alarm in the RTC?
  rtc_ds.armAlarm(1, false);
	rtc_ds.clearAlarm(1);
	alarmFlag = true;
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

void setTimer( int seconds ){
	DateTime t = rtc_ds.now();
  Serial.print("The time is ");
  printTime(t);
  t = t + TimeSpan(seconds);
	rtc_ds.setAlarm(ALM1_MATCH_HOURS, t.second(), t.minute(), t.hour(), 0);
  Serial.print("Alarm set to ");
  printTime(t);
}

void setup() 
{
	// Enable outputs to control 5V and 3.3V rails
	pinMode(5, OUTPUT); pinMode(6, OUTPUT);
	pinMode(HEATER, OUTPUT);
	pinMode(ALARM_PIN, INPUT_PULLUP);
	measuring_state = wake;
  alarmFlag = true;
	
	Serial.begin(115200);
  while(!Serial); // Wait until serial starts.
  Serial.println("Starting setup");

	// Falling-edge might not wake from sleep. Need more testing.
	attachInterrupt(digitalPinToInterrupt(ALARM_PIN), alarmISR, LOW);
  // RTC Timer settings here
  if (! rtc_ds.begin()) {
    Serial.println("Couldn't find RTC");
  }
  // This may end up causing a problem in practice - what if RTC looses power in field? Shouldn't happen with coin cell batt backup
  if (rtc_ds.lostPower()) {
    Serial.println("RTC lost power, lets set the time!");
    // Set the RTC to the date & time this sketch was compiled
    rtc_ds.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
	Serial.println("\n ** Setup Complete ** ");
}

void loop() 
{
	delay(500);	// Slow down the loop a little
  Serial.print("State: ");
  Serial.print(measuring_state); // To show what state we're in.
  Serial.print("Time: ");
  printTime(rtc_ds.now());
	if (alarmFlag) {
		alarmFlag = false;
    Serial.println("Alarm went off!");
		switch(measuring_state){
			case wake:
				digitalWrite(5, LOW); digitalWrite(6, HIGH); // Enable 5V and 3.3V rails
				measuring_state = heating;
				digitalWrite(HEATER, HIGH);
				Serial.println("Heater On");
				//Set the alarm for heating time
				setTimer(2);
				break;
			case heating:
				measuring_state = cooling;
				digitalWrite(HEATER, LOW);
				Serial.println("Heater Off");
				//set the alarm for cooling time
				setTimer(2);
				break;
			default:
				// Disable 5V and 3.3V rails
				digitalWrite(5, HIGH); digitalWrite(6, LOW); 
				Serial.println("Powering Down");
				measuring_state = wake;
				//set the alarm for sleep time
				setTimer(2);
				//FIXME: Sleep until interrupt
		}
	}
}
