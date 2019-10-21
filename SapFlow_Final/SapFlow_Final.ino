///////////////////////////////////////////////////////////////////////////////

// This is a basic example that demonstrates how to log data to an SD card
// using Loom.

///////////////////////////////////////////////////////////////////////////////

#include <Loom.h>
#include <OPEnS_RTC.h>

// Include configuration
const char* json_config = 
#include "config.h"
;

enum state{
	wake;
	heating;
	cooling;
	sleep;
} measuring_state;

Datetime t;
RTC_DS3231 rtc;
const TimeSpan heating_time = TimeSpan(2);
const TimeSpan cooling_time = TimeSpan(5);
const TimeSpan sleep_time = TimeSpan(3);

// Set enabled modules
LoomFactory<
	Enable::Internet::Disabled,
	Enable::Sensors::Enabled,
	Enable::Radios::Enabled,
	Enable::Actuators::Enabled,
	Enable::Max::Disabled
> ModuleFactory{};

LoomManager Loom{ &ModuleFactory };

// Detach interrupt on wake
void wakeISR() { 
  detachInterrupt(6); 
  LPrintln("Alarm went off");
}

void setup() 
{

  pinMode(5, OUTPUT); pinMode(6, OUTPUT);

  digitalWrite(5, LOW); digitalWrite(6, HIGH);
  
	Loom.begin_serial(true);
	Loom.parse_config(json_config);
	Loom.print_config();

	LPrintln("\n ** Setup Complete ** ");

 for(auto module : Loom.modules) {
  LPrintln(module->module_name);
 }
 measuring_state = wake;
}

void loop() 
{
 	Loom.measure();
	Loom.package();
	Loom.display_data();
	// Log using default filename as provided in configuration
	// in this case, 'datafile.csv'
	Loom.SDCARD().log();
  switch(measuring_state){
  	case wake:
  		digitalWrite(5, LOW); digitalWrite(6, HIGH); // Enable 5V and 3.3V rails
  		measuring_state = heating;
  		Loom.Relay().set(true);
  		//FIXME: Set the alarm for heating time
  		t = rtc.now();
  		t = t + heating_time;
  		rtc.setAlarm(1, t.seconds(), t.minutes(), t.hours(), t.)
  		break;
  	case heating:
  		if( !digitalRead(12) ){ // alarm went off
  			measuring_state = cooling;
  			//FIXME: set the alarm for cooling time
  			Loom.Relay().set(false);
  		}
  		break;
  	case cooling:
  		if( !digitalRead(12) ){ // alarm went off
  			measuring_state = sleep;
  		}
  	case sleep:
  	default:
  		//FIXME: set the alarm for sleep time
  		measuring_state = wake;
  		LPrintln("Powering Down");
  		digitalWrite(5, HIGH); digitalWrite(6, LOW); // Disable 5V and 3.3V rails
  		attachInterrupt(6);
  		//FIXME: Sleep until interrupt
  }
}
