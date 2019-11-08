///////////////////////////////////////////////////////////////////////////////

// This program samples lots of temperature data from a tree to measure the sap flow.
// TODO: Configure the pulse and sleep timing from config.h so it can be configured on the SD card.

///////////////////////////////////////////////////////////////////////////////

#define ALARM_PIN 12
#define HEATER 11

#include <OPEnS_RTC.h>
#include <LowPower.h>
#include <SPI.h>
#include "SdFat.h"
#include "sdios.h"
#include <Adafruit_MAX31865.h>

Adafruit_MAX31865 rtd1 = Adafruit_MAX31865(A4);

#define RREF 430.0
#define RNOMINAL 100.0

// SD chip select pin.  Be sure to disable any other SPI devices such as Enet.
const uint8_t chipSelect = 10;

// Interval between data records in milliseconds.
// The interval must be greater than the maximum SD write latency plus the
// time to acquire and write data to the SD to avoid overrun errors.
// Run the bench example to check the quality of your SD card.
const uint32_t SAMPLE_INTERVAL_MS = 1000;

SdFat sd; // File system object.

char * filename = "temperature_test.csv";
ArduinoOutStream cout(Serial);

// Stores samples and relative time.
// Adapt this datatype to your measurement needs.
class sample{
  private:
  uint32_t t;  // milliseconds
  float channel1;
  uint32_t channel2;

  public:
  sample( size_t oversample, Adafruit_MAX31865 &r1 ){
    t = millis();
    channel1 = 0;
    for( auto i = 0; i < oversample; ++i ){
      channel1 += analogRead(A0);
    }
    r1.readRTD();
    channel2 = r1.temperature(RNOMINAL, RREF);
  }
  // One version for printing to serial
  void print( ArduinoOutStream &stream){
    stream << setw(10) << channel1 << ',';
    stream << setw(10) << channel2 << '\n';
  }
  // Another version for printing to a file
  void print( ofstream &stream){
    stream << setw(10) << channel1 << ',';
    stream << setw(10) << channel2 << '\n';
  }
  // Print the header for a csv
  void header( ofstream &stream ){
    stream << setw(10) << "A0" << ',';
    stream << setw(10) << "MAX31865" << '\n';
  }
  uint32_t time( void ){
    return t;
  }
};

class datastream{
  private:
  uint32_t t0;  // milliseconds
  DateTime date;
  RTC_DS3231 * clock;
  char * fname;
  ofstream sdout;
  bool file_open;
  void timestamp( ofstream &cout, uint32_t sample_time ){
    // Calculate milliseconds from start of dataset
    uint32_t t = sample_time - t0;
    // Offset date according to seconds elapsed
    DateTime d = date + TimeSpan(t/1000);
    t = t % 1000; // Calculate remainder milliseconds
    // Print date and time with milliseconds
    char old = cout.fill('0');
    cout<<setw(4)<<d.year()<<'-';
    cout<<setw(2)<<(int)d.month()<<'-'<<setw(2)<<(int)d.day()<<' ';
    cout<<setw(2)<<(int)d.hour()<<':'<<setw(2)<<(int)d.minute()<<':';
    cout<<setw(2)<<(int)d.second()<<'.'<<setw(3)<< t <<"  ,";
    cout.fill(old);
  }

  public:
  datastream( RTC_DS3231 &rtc_ds, char * filename ){
    clock = &rtc_ds;
    fname = filename;
  }
  void flush( void ){
    sdout.flush();
    sdout.close();
    file_open = false;
  }
  void append( sample p ){
    if( !file_open ){
      date = clock->now();
      t0 = p.time();
      sdout = ofstream( fname, ios::out | ios::app );
      file_open = true;
    }
    timestamp(sdout, p.time());
    p.print(sdout);
  }
};

//------------------------------------------------------------------------------
// Instance of our RTC
RTC_DS3231 rtc_ds;

// Time used for state transitions
volatile uint32_t event_time;

// Data storage object
datastream d(rtc_ds, filename);

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
//  digitalWrite(LED_BUILTIN, LOW);

  // Sleep
  LowPower.standby();

  // Prep to resume
  USBDevice.attach();
  Serial.begin(115200);
//  digitalWrite(LED_BUILTIN, HIGH);
}

void setup() 
{
	// Enable outputs to control 5V and 3.3V rails
	pinMode(5, OUTPUT); pinMode(6, OUTPUT);
	pinMode(HEATER, OUTPUT);
	pinMode(ALARM_PIN, INPUT_PULLUP);
	digitalWrite(HEATER, LOW);
	measuring_state = wake;
	event_time = millis();
	
	Serial.begin(115200);
  while(!Serial);
  Serial.println("Starting setup");

	// Falling-edge might not wake from sleep. Need more testing.
	attachInterrupt(digitalPinToInterrupt(ALARM_PIN), alarmISR, LOW);
  // RTC Timer settings here
  if (! rtc_ds.begin()) {
    Serial.println("Couldn't find RTC");
  }
  // This may end up causing a problem in practice - what if RTC loses power in field? Shouldn't happen with coin cell batt backup
  if (rtc_ds.lostPower()) {
    Serial.println("RTC lost power, lets set the time!");
    // Set the RTC to the date & time this sketch was compiled
    rtc_ds.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
  if (!sd.begin(chipSelect, SD_SCK_MHZ(50))) {
    sd.initErrorHalt();
  }

  rtd1.begin(MAX31865_4WIRE);  // set to 2WIRE or 4WIRE as necessary
	Serial.println("\n ** Setup Complete ** ");
}

void loop() 
{
	//delay(100);	// Slow down the loop a little
  sample s = sample(64, rtd1); // Sample A0 and RTD
  s.print(cout);
  d.append(s);  // Log to SD card
}
