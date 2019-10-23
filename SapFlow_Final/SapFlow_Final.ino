///////////////////////////////////////////////////////////////////////////////

// This program samples lots of temperature data from a tree to measure the sap flow.
// TODO: Configure the pulse and sleep timing from config.h so it can be configured on the SD card.

///////////////////////////////////////////////////////////////////////////////

#define ALARM_PIN 12

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

// Detach interrupt on wake
volatile bool alarmFlag = false;
void alarmISR() { 
  detachInterrupt(digitalPinToInterrupt(ALARM_PIN)); 
  Loom.InterruptManager().get_RTC_module()->clear_alarms();
  Loom.InterruptManager().reconnect_interrupt(ALARM_PIN); 

  alarmFlag = true;
}

void setup() 
{

  pinMode(5, OUTPUT); pinMode(6, OUTPUT);
  pinMode(ALARM_PIN, INPUT_PULLUP);

  digitalWrite(5, LOW); digitalWrite(6, HIGH);
  
	Loom.begin_serial(true);
	Loom.parse_config(json_config);
	//Loom.print_config();
  measuring_state = wake;

  Loom.InterruptManager().register_ISR(ALARM_PIN, alarmISR, LOW, ISR_Type::IMMEDIATE);
  Loom.InterruptManager().RTC_alarm_duration(TimeSpan(10));

	LPrintln("\n ** Setup Complete ** ");

}

void loop() 
{
 	Loom.measure();
	Loom.package();
  delay(50); // Slow down the loop a little bit
	Loom.display_data();
	// Log using default filename as provided in configuration
	// in this case, 'datafile.csv'
	Loom.SDCARD().log();
  if (alarmFlag) {
    alarmFlag = false;
    switch(measuring_state){
    	case wake:
    		digitalWrite(5, LOW); digitalWrite(6, HIGH); // Enable 5V and 3.3V rails
    		measuring_state = heating;
    		Loom.Relay().set(true);
        LPrintln("Heater On");
    		//Set the alarm for heating time
        Loom.InterruptManager().RTC_alarm_duration(TimeSpan(2)); 
    		break;
    	case heating:
  			measuring_state = cooling;
  			//set the alarm for cooling time
        Loom.InterruptManager().RTC_alarm_duration(TimeSpan(5)); 
  			Loom.Relay().set(false);
        LPrintln("Heater Off");
    		break;
    	default:
    		//set the alarm for sleep time
        Loom.InterruptManager().RTC_alarm_duration(TimeSpan(5)); 
    		measuring_state = wake;
    		LPrintln("Powering Down");
    		digitalWrite(5, HIGH); digitalWrite(6, LOW); // Disable 5V and 3.3V rails
    		//Sleep until interrupt
        //Loom.SleepManager().sleep();
    }
  }
}
