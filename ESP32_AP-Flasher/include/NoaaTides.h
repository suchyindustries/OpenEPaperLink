#ifndef _NOAA_TIDES_H
#define _NOAA_TIDES_H


typedef struct {
   int  MinsAfterMidnight;
   time_t Time; // absolute time
   int  Hrs;
   int  Mins;
   float Height; // MLLW (Mean Lower Low Water)
   bool  LowTide;
} HighLowArray_t;

// if MaxValues == 2 then return last high/low tide and next high/low tide,
// otherwise return yesterday's high/low tide, today's high/low tides and
// the first high/low tide tomorrow.
int GetNoaaTides(String StationID,std::vector<HighLowArray_t> &Results);

#endif   // _NOAA_TIDES_H

