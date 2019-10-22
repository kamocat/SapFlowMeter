///////////////////////////////////////////////////////////////////////////////

// This program samples lots of temperature data from a tree to measure the sap flow.
// TODO: Configure the pulse and sleep timing from config.h so it can be configured on the SD card.

///////////////////////////////////////////////////////////////////////////////

#include <Loom.h>

// Include configuration
const char* json_config = 
#include "config.h"
;

enum state{
	wake,
	heating,
	cooling,
	sleep
} measuring_state;


// Set enabled modules
LoomFactory<
	Enable::Internet::Disabled,
	Enable::Sensors::Enabled,
	Enable::Radios::Enabled,
	Enable::Actuators::Enabled,
	Enable::Max::Disabled
> ModuleFactory{};

LoomManager Loom{ &ModuleFactory };

DateTime t;

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
  		//Set the alarm for heating time
      t = Loom.DS3231().now() + TimeSpan(3);
  		Loom.DS3231().set_alarm( t );
  		break;
  	case heating:
  		if( !digitalRead(12) ){ // alarm went off
  			measuring_state = cooling;
  			//set the alarm for cooling time
        t = t + TimeSpan(5);
  		  Loom.DS3231().set_alarm( t );
  			Loom.Relay().set(false);
  		}
  		break;
  	case cooling:
  		if( !digitalRead(12) ){ // alarm went off
  			measuring_state = sleep;
  		}
  	case sleep:
  	default:
  		//set the alarm for sleep time
      t = t + TimeSpan(2);
  		Loom.DS3231().set_alarm( t );
  		measuring_state = wake;
  		LPrintln("Powering Down");
  		digitalWrite(5, HIGH); digitalWrite(6, LOW); // Disable 5V and 3.3V rails
  		attachInterrupt(6, wakeISR, FALLING);
  		//Sleep until interrupt
      Loom.SleepManager().sleep();
  }
}
