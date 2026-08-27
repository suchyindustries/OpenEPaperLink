#ifndef _NOAA_TIDES_H
#define _NOAA_TIDES_H

#include "bezier.h"

typedef struct {
   time_t Time; // absolute time
   int  Hrs;
   int  Mins;
   float Height; // MLLW (Mean Lower Low Water)
   bool  LowTide;
} HighLowArray_t;

class NoaaTides {
public:
   int GetNoaaTides(String StationID,time_t Start);
   int PlotNoaaTides(class DrawOWM * &owm,time_t Start,time_t Duration = 24*60*60);
   std::vector<HighLowArray_t> Tides;
private:
   void InitSegment(time_t,int,bezier::Bezier<3> &,double &);
};


#endif   // _NOAA_TIDES_H

