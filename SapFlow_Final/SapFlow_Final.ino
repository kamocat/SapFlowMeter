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
  digitalWrite(5, LOW); digitalWrite(6, HIGH); 
  
  Loom.Relay().set(false);

  LPrintln("Reading Primary Sensors");
  ((LoomSensor*)Loom.modules[0])->measure();
  ((LoomSensor*)Loom.modules[1])->measure();
  ((LoomSensor*)Loom.modules[2])->measure();

  LPrintln("Heating");
  Loom.Relay().set(true);

  delay(3000);

  Loom.Relay().set(false);

  delay(57000);

  LPrintln("Reading Secondary Sensors");
  ((LoomSensor*)Loom.modules[3])->measure();
  ((LoomSensor*)Loom.modules[4])->measure();
  
	Loom.package();
	Loom.display_data();
	
	// Log using default filename as provided in configuration
	// in this case, 'datafile.csv'
	Loom.SDCARD().log();

	// Or log to a specific file (does not change what default file is set to)	
	// Loom.SDCARD().log("specific.csv");

  LPrintln("Powering Down");
  digitalWrite(5, HIGH); digitalWrite(6, LOW);
	Loom.nap(169000);
}
