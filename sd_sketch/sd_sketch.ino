/*
 * Print a table with various formatting options
 * Format dates
 */
 #undef min
 #undef max
#include <SPI.h>
#include "SdFat.h"
#include "sdios.h"
#include <vector>

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
  size_t dump( ofstream &f ){
    for( auto i = data.begin(); i != data.end(); ++i ){
      i->print(f, t0);
    }
    f<<flush;
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

// SD chip select pin.  Be sure to disable any other SPI devices such as Enet.
const uint8_t chipSelect = 4;

// Interval between data records in milliseconds.
// The interval must be greater than the maximum SD write latency plus the
// time to acquire and write data to the SD to avoid overrun errors.
// Run the bench example to check the quality of your SD card.
const uint32_t SAMPLE_INTERVAL_MS = 1000;

// Log file base name.  Must be six characters or less.
#define FILE_BASE_NAME "Data"
//------------------------------------------------------------------------------
// File system object.
SdFat sd;

// Data storage object
datastream d;

char * filename = "mytest2.csv";

//------------------------------------------------------------------------------
void setup(void) {
  Serial.begin(115200);

  // Wait for USB Serial 
  while (!Serial) {
    SysCall::yield();
  }

  // Initialize at the highest speed supported by the board that is
  // not over 50 MHz. Try a lower speed if SPI errors occur.
  if (!sd.begin(chipSelect, SD_SCK_MHZ(50))) {
    sd.initErrorHalt();
  }


  delay(2000);
  for( int i = 0; i < 10; ++i ){
    d.append(datapoint(i*1, i*2));
    delay(10);
  }
  
  // Sava data
  Serial.print("Saving data... ");
  ofstream sdout( filename );
  d.dump( sdout );
  sdout.close();
  Serial.print("done.\n");
}
void loop(void) {
  
}
