///////////////////////////////////////////////////////////////////////////////

// This program samples lots of temperature data from a tree to measure the sap flow.
// TODO: Configure the pulse and sleep timing from config.h so it can be configured on the SD card.

///////////////////////////////////////////////////////////////////////////////

#define ALARM_PIN 12
#define HEATER 11

#include <OPEnS_RTC.h>
#include <LowPower.h>
#include <sdios.h> //for ArduinoOutStream

#include <SPI.h>
#include "SdFat.h"
#include "sdios.h"
#include <Adafruit_MAX31865.h>

Adafruit_MAX31865 rtd1 = Adafruit_MAX31865(A4);
Adafruit_MAX31865 rtd2 = Adafruit_MAX31865(A5);

#define RREF 430.0
#define RNOMINAL 100.0

ArduinoOutStream cout(Serial);

// SD chip select pin.  Be sure to disable any other SPI devices such as Enet.
const uint8_t chipSelect = 10;

// Interval between data records in milliseconds.
// The interval must be greater than the maximum SD write latency plus the
// time to acquire and write data to the SD to avoid overrun errors.
// Run the bench example to check the quality of your SD card.
const uint32_t SAMPLE_INTERVAL_MS = 1000;

SdFat sd; // File system object.

ofstream sdout;
char * filename = "sapflow.csv";
ArduinoOutStream cout(Serial);

// Stores samples and relative time.
// Adapt this datatype to your measurement needs.
class sample{
  private:
  float channel1;
  float channel2;

  public:
  sample( Adafruit_MAX31865 r1, Adafruit_MAX31865 r2 ){
    r1.readRTD();
    r2.readRTD();
    channel1 = r1.temperature(RNOMINAL, RREF);
    channel2 = r2.temperature(RNOMINAL, RREF);
  }
  sample( size_t oversample ){
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
  // Print the header for a csv
  void header( ofstream &stream ){
    stream << setw(6) << "A0" << ',';
    stream << setw(6) << "A1" << '\n';
  }
};

class datastream{
  private:
  DateTime date;
  RTC_DS3231 * clock;
  char * fname;
  ofstream sdout;
  bool file_open;

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

  // Disable this interrupt
  detachInterrupt(digitalPinToInterrupt(ALARM_PIN));
  event_time = millis();
}

void setTimer( int seconds ){
  DateTime t = rtc_ds.now();
  Serial.print("The time is ");
  Serial.println(t.text());
  t = t + TimeSpan(seconds);
  rtc_ds.setAlarm(t);
  Serial.print("Alarm set to ");
  Serial.println(t.text());
}

void feather_sleep( void ){
  while(!digitalRead(ALARM_PIN)){
    Serial.print("Waiting on alarm pin...");
    delay(10);
  }
  // Low-level so we can wake from sleep
  // I think calling this twice clears the interrupt.
  attachInterrupt(digitalPinToInterrupt(ALARM_PIN), alarmISR, LOW);
  attachInterrupt(digitalPinToInterrupt(ALARM_PIN), alarmISR, LOW);
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
  sd.begin(chipSelect, SD_SCK_MHZ(1));
}

void setup() 
{
  // Enable outputs to control 5V and 3.3V rails
  pinMode(5, OUTPUT); pinMode(6, OUTPUT);
  digitalWrite(5, LOW); digitalWrite(6, HIGH);
  pinMode(HEATER, OUTPUT);
  pinMode(ALARM_PIN, INPUT_PULLUP);
  digitalWrite(HEATER, LOW);
  measuring_state = wake;
  event_time = millis();
  
  Serial.begin(115200);
  Serial.println("Starting setup");

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
  
  Serial.println("Initializing SD Card");
  if (!sd.begin(chipSelect, SD_SCK_MHZ(50))) {
    sd.initErrorHalt();
  }

  rtd1.begin(MAX31865_4WIRE);  // set to 2WIRE or 4WIRE as necessary
  rtd2.begin(MAX31865_4WIRE);  // set to 2WIRE or 4WIRE as necessary
  Serial.println("\n ** Setup Complete ** ");
}

void loop() 
{
  sample s = sample(rtd1, rtd2); // Sample RTDs
  s.print(cout);
  d.append(s);  // Log to SD card
  if ((millis()-event_time) < 60000) {
    switch(measuring_state){
      case wake:
        digitalWrite(5, LOW); digitalWrite(6, HIGH); // Enable 5V and 3.3V
        digitalWrite(chipSelect, HIGH);  // Disable SD card for now.
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
        event_time += 114000;
        measuring_state = sleep;
        break;
      default:
        // Make sure we're done logging
        d.flush();
        // Disable 5V and 3.3V rails
        digitalWrite(5, HIGH); digitalWrite(6, LOW);
        Serial.println("Powering Down");
        //set the alarm for sleep time
        setTimer(177);
        event_time += 300000;
        //Sleep until interrupt (It works, but it's annoying for general testing)
        feather_sleep();
        measuring_state = wake;
    }
  }
}
