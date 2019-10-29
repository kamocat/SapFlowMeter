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
const uint8_t chipSelect = 10;

// Interval between data records in milliseconds.
// The interval must be greater than the maximum SD write latency plus the
// time to acquire and write data to the SD to avoid overrun errors.
// Run the bench example to check the quality of your SD card.
const uint32_t SAMPLE_INTERVAL_MS = 1000;

SdFat sd; // File system object.

char * filename = "test.csv";

// Stores samples and relative time.
// Adapt this datatype to your measurement needs.
class sample{
  private:
  uint32_t t;  // milliseconds
  uint32_t channel1;
  uint32_t channel2;

  public:
  sample( size_t oversample ){
    t = millis();
    channel1 = 0;
    channel2 = 0;
    for( auto i = 0; i < oversample; ++i ){
      channel1 += analogRead(A0);
      channel2 += analogRead(A1);
    }
  }
  sample( void ){
    sample( 1 );
  }
  // One version for printing to serial
  void print( ArduinoOutStream &stream, uint32_t offset){
    stream << setw(10) << (t - offset) << ',';
    stream << setw(6) << channel1 << ',';
    stream << setw(6) << channel2 << '\n';
  }
  // Another version for printing to a file
  void print( ofstream &stream, uint32_t offset){
    stream << setw(10) << (t - offset) << ',';
    stream << setw(6) << channel1 << ',';
    stream << setw(6) << channel2 << '\n';
  }
  // A third version for printing to serial, just in case
  void print( void ){
    ArduinoOutStream s(Serial);
    print( s, 0 );
  }
  // Print the header for a csv
  void header( ofstream &stream ){
    stream << setw(6) << "A0" << ',';
    stream << setw(6) << "A1" << '\n';
  }
  uint32_t time( void ){
    return t;
  }
};

class datastream{
  private:
  uint32_t t0;  // milliseconds
  DateTime date;
  std::vector <sample> data;
  RTC_DS3231 * clock;
  void timestamp( ofstream &cout, uint32_t sample_time ){
    // Calculate milliseconds from start of dataset
    uint32_t t = sample_time - t0;
    // Offset date according to seconds elapsed
    DateTime d = date + TimeSpan(t/1000);
    // Print standard date and time, and milliseconds
    cout<<(int)d.month()<<'/'<<(int)d.day()<<' '<<d.year()<<' ';
    cout<<(int)d.hour()<<':'<<setw(2)<<(int)d.minute()<<':'<<(int)d.second();
    char old = cout.fill('0');
    cout<<'.'<<setw(3)<<(t%1000)<<" ,";
    cout.fill(old);
  }

  public:
  datastream( RTC_DS3231 &rtc_ds ){
    clock = &rtc_ds;
  }
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

    // Print the header
    data.begin()->header(sdout);
    // Print all the data
    for( auto i = data.begin(); i != data.end(); ++i ){
      timestamp(sdout, i->time());
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
  void append( sample p ){
    if( data.empty() ){
      date = clock->now();
      t0 = p.time();
    }
    data.push_back(p);
  }
};

//------------------------------------------------------------------------------
// Instance of our RTC
RTC_DS3231 rtc_ds;

// Time used for state transitions
volatile uint32_t event_time;

// Data storage object
datastream d(rtc_ds);

enum state{
	wake,
	heating,
	cooling,
	sleep
} measuring_state;

void alarmISR() {
	// Reset the alarm.
  rtc_ds.armAlarm(1, false);
	rtc_ds.clearAlarm(1);
	event_time = millis();
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
	event_time = millis();
	
	Serial.begin(115200);
//  while(!Serial); // Wait until serial starts.
	delay(2000);
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
  d.append(sample(16)); // Oversample 16x
	if ((millis()-event_time) < 60000) {
		switch(measuring_state){
			case wake:
				digitalWrite(5, LOW); digitalWrite(6, HIGH); // Enable 5V and 3.3V rails
				Serial.print("Awoke at ");
				printTime(rtc_ds.now());
				// Sample for 3 seconds before heating
				event_time += 3000;
				measuring_state = heating;
				break;
			case heating:
				digitalWrite(HEATER, HIGH);
				Serial.print("Heater On at ");
				printTime(rtc_ds.now());
				//Set the alarm for heating time
				event_time += 6000;
				measuring_state = cooling;
				break;
			case cooling:
				digitalWrite(HEATER, LOW);
				Serial.print("Heater Off at ");
				printTime(rtc_ds.now());
				//set the alarm for cooling time
				event_time += 114*1000;
				measuring_state = sleep;
				break;
			default:
        // Log the samples to a csv file
        d.dump(filename);
				// Disable 5V and 3.3V rails
				digitalWrite(5, HIGH); digitalWrite(6, LOW); 
				Serial.println("Powering Down");
				//set the alarm for sleep time
				setTimer(180);
				//Sleep until interrupt (It works, but it's annoying for general testing)
        feather_sleep();
				measuring_state = wake;
		}
	}
}
