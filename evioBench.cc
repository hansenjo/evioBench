// EVIO file IO benchmark
// using EVIO C-library
// Compile with -std=c++14 or higher
// Ole Hansen, 2-Apr-2021

#include "evio.h"
#include <iostream>
#include <iomanip>
#include <stdexcept>
#include <memory>
#include <limits>
#include <algorithm>
#include <cstdint>
#include <vector>
#include <string>
#include <chrono>

using namespace std;

static const char* prog = nullptr;

// Hardcoded event buffer size
static const unsigned int MAXEVLEN = 102400;  // 100 ki longwords (400 kiB)

// Hall A event types (from hana_decode/Decoder.h)
static const unsigned int MAX_PHYS_EVTYPE    = 14;  // Types up to this are physics
static const unsigned int SYNC_EVTYPE        = 16;
static const unsigned int PRESTART_EVTYPE    = 17;
static const unsigned int GO_EVTYPE          = 18;
static const unsigned int PAUSE_EVTYPE       = 19;
static const unsigned int END_EVTYPE         = 20;
static const unsigned int TS_PRESCALE_EVTYPE = 120;
static const unsigned int EPICS_EVTYPE       = 131;
static const unsigned int PRESCALE_EVTYPE    = 133;
static const unsigned int DETMAP_FILE        = 135;
static const unsigned int TRIGGER_FILE       = 136;
static const unsigned int SCALER_EVTYPE      = 140;

void usage()
{
  cout << "Usage: " << prog << " <coda-file1> [<coda-file2> ...]" << endl;
  cout << "  Benchmark EVIO file read speed" << endl;
  cout << "  <coda-file> = EVIO file(s) (version 2 or 3)" << endl;
  exit(1);
}
  
int main(int argc, const char* argv[])
{
  prog = argv[0];
  
  if( argc < 2 )
    usage();
  
  int retcode = 0;
  int handle = 0;
  unsigned int gEvNum = 0;
  vector<string> fnames;
  for( int i=1; i<argc; ++i )
    fnames.emplace_back(argv[i]);

  try {
    unsigned int nev = 0;
    unsigned int nphys = 0;
    unsigned int max_evlen = 0;
    unsigned int min_evlen = numeric_limits<unsigned int>::max();
    unsigned int max_evlen_any = 0;
    uint64_t totlen = 0;

    // Allocate event buffer (400 kiB)
    auto pbuf = make_unique<unsigned int[]>(MAXEVLEN);
    auto* evbuffer = pbuf.get();

    // Start timer
    auto start = chrono::steady_clock::now();
    
    // Process all files
    for( const auto& fname : fnames ) {
      // Open file
      int ret = evOpen((char*)fname.c_str(), (char*)"r", &handle);
      if( ret != S_SUCCESS )
        throw runtime_error(evPerror(ret));
      if( !handle )
        throw runtime_error("Bad file handle");
      cout << "Opened " << fname << endl;
    
      // Get file version
      int version = 0;
      ret = evIoctl(handle, (char*)"v", &version);
      if( ret != S_SUCCESS )
        throw runtime_error(evPerror(ret));
      cout << "EVIO version " << version << endl;
      if( version != 2 && version != 3 )
        throw runtime_error("This EVIO version is not supported");

      // Read through the file, keeping basic statistics
      while (true) {
        ret = evRead(handle, evbuffer, MAXEVLEN);
        if( ret != S_SUCCESS )
          break;
        ++nev;

        unsigned int evlen = evbuffer[0]+1;  // in longwords (4 bytes)
        if( evlen > MAXEVLEN )
          throw runtime_error("Buffer overflow (evlen > MAXEVLEN)?");
        max_evlen_any = std::max(max_evlen_any,evlen);
        totlen += evlen;

        unsigned int evtype = 0;
        if( version == 2 ) {
          evtype = evbuffer[1]>>16;
        } else {
          // EVIO v3 decoding from hana_decode/CodaDecoder.cxx
          // CodaDecoder::interpretCoda3
          unsigned int bank_tag = evbuffer[1]>>16;
          switch (bank_tag) {
          case 0xffd1:
            evtype = PRESTART_EVTYPE;
            break;
          case 0xffd2:
            evtype = GO_EVTYPE;
            break;
          case 0xffd4:
            evtype = END_EVTYPE;
            break;
          case 0xff50:
          case 0xff58: // Physics event with sync bit
          case 0xff70:
            evtype=1;  // Physics event type
            break;
          default:
            cerr << "bank_tag = " << hex << bank_tag << dec << endl;
            throw runtime_error("Undefined CODA 3 event type");
          }
        }
        if( evtype <= MAX_PHYS_EVTYPE ) {
          unsigned int evnum = 0;
          ++nphys;
          max_evlen = std::max(max_evlen,evlen);
          min_evlen = std::min(min_evlen,evlen);
          if( version == 2 ) {
            evnum = evbuffer[4];
            gEvNum = evnum;
          } else {
            evnum = ++gEvNum;
          }
          if( (evnum % 25000) == 0 )
            cout << evnum << endl;
        }
      }
      if( ret != EOF )
        throw runtime_error(evPerror(ret));
      cout << "End of file" << endl;
      evClose(handle);
    }

    // Stop timer
    auto stop = chrono::steady_clock::now();
    chrono::duration<double> wall_time = stop-start;
    
    // Print statistics
    cout << fnames.size() << " file" << ((fnames.size() > 1) ? "s" : "")
         << " analyzed" << endl;
    cout << nev   << " events" << endl;
    cout << nphys << " physics events" << endl;
    cout << 4*totlen << " bytes read ("
         << 4.0*static_cast<double>(totlen)/1024./1024.
         << " MiB)" << endl;
    cout << 4*min_evlen << "/" << 4*max_evlen << "/" << 4*max_evlen_any
         << " bytes min_physics/max_physics/max event lengths" << endl;
    cout << 4.0 * static_cast<double>(totlen) / static_cast<double>(nev)
         << " bytes average event length" << endl;
    cout << wall_time.count() << " seconds wall time" << endl;
    cout << setprecision(4) << 1e6 * wall_time.count() / static_cast<double>(nev)
         << " µs/event" << endl;
    cout << setprecision(4) << 4.0*static_cast<double>(totlen)/1024./1024./wall_time.count()
         << " MiB/s throughput" << endl;
  }
  catch( const exception& e ) {
    cerr << "ERROR at event=" << gEvNum << ": " << e.what() << endl;
    retcode = 2;
    evClose(handle);
  }
  
  return retcode;
}
