///////////////////////////////////////////////////////////////////////////////

// This program samples lots of temperature data from a tree to measure the sap flow.
// TODO: Configure the pulse and sleep timing from config.h so it can be configured on the SD card.

///////////////////////////////////////////////////////////////////////////////

#define ALARM_PIN 12
#define HEATER 11

#include <OPEnS_RTC.h>
#include <LowPower.h>
 #undef min
 #undef max
#include <SPI.h>
#include "SdFat.h"
#include "sdios.h"
#include <vector>

// SD chip select pin.  Be sure to disable any other SPI devices such as Enet.
const uint8_t chipSelect = 4;

// Interval between data records in milliseconds.
// The interval must be greater than the maximum SD write latency plus the
// time to acquire and write data to the SD to avoid overrun errors.
// Run the bench example to check the quality of your SD card.
const uint32_t SAMPLE_INTERVAL_MS = 1000;

SdFat sd; // File system object.

char * filename = "test.csv";

// Stores samples and relative time.
// Adapt this datatype to your measurement needs.
class datapoint{
  private:
  uint32_t t;  // milliseconds
  int16_t sample1;
  int16_t sample2;

  public:
  datapoint( int16_t s1, int16_t s2 ){
    t = millis();
    sample1 = s1;
    sample2 = s2;
  }
  // One version for printing to serial
  void print( ArduinoOutStream &stream, uint32_t offset){
    stream << setw(10) << (t - offset) << ',';
    stream << setw(6) << sample1 << ',';
    stream << setw(6) << sample2 << '\n';
  }
  // Another version for printing to a file
  void print( ofstream &stream, uint32_t offset){
    stream << setw(10) << (t - offset) << ',';
    stream << setw(6) << sample1 << ',';
    stream << setw(6) << sample2 << '\n';
  }
  // A third version for printing to serial, just in case
  void print( void ){
    ArduinoOutStream s(Serial);
    print( s, 0 );
  }
  uint32_t time( void ){
    return t;
  }
};

class datastream{
  private:
  uint32_t t0;  // milliseconds
  //DateTime date;
  std::vector <datapoint> data;

  public:
  size_t dump( ArduinoOutStream &cout ){
    for( auto i = data.begin(); i != data.end(); ++i ){
      i->print(cout, t0);
    }
    size_t k = data.size();
    data.clear();
    return k;
  }
  size_t dump( char * fname ){
    // Append data to existing file.
    ofstream sdout( fname, ios::out | ios::app );
    for( auto i = data.begin(); i != data.end(); ++i ){
      i->print(sdout, t0);
    }
    sdout<<flush;
    sdout.close();
    size_t k = data.size();
    data.clear();
    return k;
  }
  size_t dump( void ){
    ArduinoOutStream cout(Serial);
    dump( cout );
  }
  void append( datapoint p ){
    if( data.empty() ){
      t0 = p.time();
    }
    data.push_back(p);
  }
};

//------------------------------------------------------------------------------


// Data storage object
datastream d;

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
	// Reset the alarm.
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

void feather_sleep( void ){
  // Prep for sleep
  Serial.end();
  USBDevice.detach();
  digitalWrite(LED_BUILTIN, LOW);

  // Sleep
  LowPower.standby();

  // Prep to resume
  USBDevice.attach();
  Serial.begin(115200);
  digitalWrite(LED_BUILTIN, HIGH);
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
  if (!sd.begin(chipSelect, SD_SCK_MHZ(50))) {
      sd.initErrorHalt();
    }
	Serial.println("\n ** Setup Complete ** ");
}

void loop() 
{
	delay(100);	// Slow down the loop a little
  d.append(datapoint(analogRead(A0), analogRead(A1)));
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
				setTimer(5);
				break;
			default:
        // Log the samples to a csv file
        d.dump(filename);
				// Disable 5V and 3.3V rails
				digitalWrite(5, HIGH); digitalWrite(6, LOW); 
				Serial.println("Powering Down");
				measuring_state = wake;
				//set the alarm for sleep time
				setTimer(5);
				//Sleep until interrupt (It works, but it's annoying for general testing)
        feather_sleep();
		}
	}
}
