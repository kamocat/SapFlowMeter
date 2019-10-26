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
  void print( ArduinoOutStream &stream, uint32_t offset){
    stream << setw(10) << (t - offset) << ',';
    stream << setw(6) << sample1 << ',';
    stream << setw(6) << sample2 << '\n';
  }
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
  size_t dump( ArduinoOutStream cout ){
    for( auto i = data.begin(); i != data.end(); ++i ){
      i->print(cout, t0);
    }
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
void setup(void) {
  Serial.begin(115200);

  // Wait for USB Serial 
  while (!Serial) {
    SysCall::yield();
  }
  delay(2000);

  datastream d;

  for( int i = 0; i < 20; ++i ){
    d.append(datapoint(i*1, i*2));
    delay(10);
  }
  d.dump();

  
}
void loop(void) {}
