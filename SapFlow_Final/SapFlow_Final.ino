///////////////////////////////////////////////////////////////////////////////

// This is a basic example that demonstrates how to log data to an SD card
// using Loom.

///////////////////////////////////////////////////////////////////////////////

#include <Loom.h>

// Include configuration
const char* json_config = 
#include "config.h"
;

// Set enabled modules
LoomFactory<
	Enable::Internet::Disabled,
	Enable::Sensors::Enabled,
	Enable::Radios::Enabled,
	Enable::Actuators::Enabled,
	Enable::Max::Enabled
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
}

void loop() 
{
  digitalWrite(5, LOW); digitalWrite(6, HIGH); // What does this do?

 	Loom.measure();
	Loom.package();
	Loom.display_data();
	
	// Log using default filename as provided in configuration
	// in this case, 'datafile.csv'
	Loom.SDCARD().log();

	/* Need a way of measuring time. We want the relay to be on
	 * for a period of time, and then off for a period of time.
	 * Configurable through the JSON file would be really nice.
	 * Then we want the system to sleep until the RTC wakes it up again. */
  Loom.Relay().set(true);

  LPrintln("Powering Down");
  digitalWrite(5, HIGH); digitalWrite(6, LOW);
}
