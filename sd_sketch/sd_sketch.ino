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
Adafruit_MAX31865 rtd2 = Adafruit_MAX31865(A5);

#define RREF 430.0
#define RNOMINAL 100.0
// Adapt this datatype to your measurement needs.
// SD chip select pin.  Be sure to disable any other SPI devices such as Enet.
const uint8_t chipSelect = 4;

// Interval between data records in milliseconds.
// The interval must be greater than the maximum SD write latency plus the
// time to acquire and write data to the SD to avoid overrun errors.
// Run the bench example to check the quality of your SD card.
const uint32_t SAMPLE_INTERVAL_MS = 1000;

SdFat sd; // File system object.

char * filename = "test4.csv";
ArduinoOutStream cout(Serial);

// Stores samples and relative time.
// Adapt this datatype to your measurement needs.
class sample{
  private:
  uint32_t t;  // milliseconds
  float channel1;
  float channel2;

  public:
  sample( Adafruit_MAX31865 r1, Adafruit_MAX31865 r2 ){
    t = millis();
    r1.readRTD();
    r2.readRTD();
    channel1 = r1.temperature(RNOMINAL, RREF);
    channel2 = r2.temperature(RNOMINAL, RREF);
  }
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
  void print( ArduinoOutStream &stream){
    stream << setw(6) << channel1 << ',';
    stream << setw(6) << channel2 << '\n';
  }
  // Another version for printing to a file
  void print( ofstream &stream){
    stream << setw(6) << channel1 << ',';
    stream << setw(6) << channel2 << '\n';
  }
  // A third version for printing to serial, just in case
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


// Instance of our RTC
RTC_DS3231 rtc_ds;

volatile uint32_t event_time;

datastream d(rtc_ds, filename);

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
// File system object.
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
  if (!sd.begin(chipSelect, SD_SCK_MHZ(50))) {
    sd.initErrorHalt();
  }

}
void loop(void) {
  while(!Serial);
  Serial.println("Welcome!");
  d.append(sample(1));
  d.flush();
  setTimer(2);
  delay(1000);
  feather_sleep();
}
