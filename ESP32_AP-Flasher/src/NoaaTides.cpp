#ifndef WITHOUT_NOAA_TIDES
#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <FS.h>

#include "tag_db.h"
#include "makeimage.h"
#include "TFT_eSPI.h"
#include "contentmanager.h"
#include "web.h"
#include "storage.h"
#include "util.h"
#include "NoaaTides.h"

#define ENABLE_LOGGING  1
#if ENABLE_LOGGING && __has_include("logging.h") 
#include "logging.h"
#else
#define LOG(format, ...)
#define LOG_RAW(format, ...)
#endif


//   String StationID = "8446613"; // Wellfleet
//   String StationID = "9415009"; // San Pedro

// if MaxValues == 2 then return last high/low tide and next high/low tide,
// otherwise return yesterday's high/low tide, today's high/low tides and
// the first high/low tide tomorrow.
int GetNoaaTides(String StationID,HighLowArray_t *Results,int MaxValues)
{
   time_t Now;
   time_t TideTime;
   struct tm Timeinfo;
   struct tm TimeinfoToday;
   char BeginDate[10];
   char EndDate[10];  // for xample "20240605"
   JsonDocument doc;
   JsonDocument loc;
   int Yesterday = 0;
   int Today = 0;
   int Tomorrow = 0;
   int Entries = 0;
   int Day;
   float Height;
   float MinHeight = 1000.0;
   float MaxHeight = -1000.0;
   int Year;
   int Month;
   int Hrs;
   int Mins;
   int MinsAfterMidnight;
   String DateTime;
   String TideType;
   uint16_t StringWidth;
   int LowTides = 0;
   int HighTides = 0;
   int i;
   int Ret = 0;

   #define MAX_HIGH_LOW_ENTRIES    10
   HighLowArray_t HighLowArray[MAX_HIGH_LOW_ENTRIES];

   time(&Now);
   localtime_r(&Now,&TimeinfoToday);

   LOG("Current time %d:%02d\n\n",TimeinfoToday.tm_hour,TimeinfoToday.tm_min);

// Begin predictions yesterday, ending tomorrow
   Now -= 24*60*60;

   localtime_r(&Now,&Timeinfo);
   snprintf(BeginDate,sizeof(BeginDate),"%d%02d%02d",
            Timeinfo.tm_year + 1900,Timeinfo.tm_mon+1,Timeinfo.tm_mday);

   Now += 24*60*60*2;
   localtime_r(&Now,&Timeinfo);
   snprintf(EndDate,sizeof(EndDate),"%d%02d%02d",
            Timeinfo.tm_year + 1900,Timeinfo.tm_mon+1,Timeinfo.tm_mday);

   String TideUrl = "https://api.tidesandcurrents.noaa.gov/"
                    "api/prod/datagetter?begin_date=";
   TideUrl += BeginDate;
   TideUrl += "&end_date=";
   TideUrl += EndDate;
   TideUrl += "&station=";
   TideUrl += StationID;
   TideUrl += "&product=predictions&datum=MLLW"
              "&time_zone=lst_ldt&units=english"
              "&format=json&interval=hilo";

   LOG("TideUrl: %s\n",TideUrl.c_str());

   const bool success = util::httpGetJson(TideUrl,doc, 5000);
   if(!success) {
      LOG("httpGetJson() failed\n");
      return true;
   }
// Need last high/low tide from yesterday and the first high/low tide
// tomorrow plus todays values.

   JsonArray Predictions = doc["predictions"];
   for(JsonObject Prediction : Predictions) {
      DateTime = Prediction["t"].as<String>();
      Height = Prediction["v"].as<float>();
      TideType = Prediction["type"].as<String>();

      Serial.println("{ t: " + DateTime
                     + ", v: " + Height
                     + ", type: " + (Prediction["type"].as<String>())
                     + "}");
      if(sscanf(DateTime.c_str(),"%*d-%*d-%d %d:%d",&Day,&Hrs,&Mins) != 3) {
         Serial.println("Couldn't convert " + DateTime);
         break;
      }

      strptime(DateTime.c_str(),"%F %R",&Timeinfo);
      TideTime = mktime(&Timeinfo);
      LOG("Tide time: %d-%d-%d %d:%02d\n",
          Timeinfo.tm_year + 1900,Timeinfo.tm_mon+1,Timeinfo.tm_mday,
          Timeinfo.tm_hour,Timeinfo.tm_min);

      LOG("Day %d Hrs %d Mins %d\n",Day,Hrs,Mins);
      MinsAfterMidnight = Mins + (Hrs * 60);

      if(Yesterday == 0) {
         // Must be the first high/low from yesterday
         LOG("Set Yesterday to %d\n",Day);
         Yesterday = Day;
      }
      else if(Day != Yesterday && Today == 0) {
      // must be today
         Today = Day;
         LOG("Set Today to %d\n",Day);
         Entries++;
      }
      else if(Day != Yesterday && Day != Today) {
         // Not yesterday or today, must be tomorrow
         Tomorrow = Day;
         LOG("Set Tomorrow to %d\n",Day);
      }

      if(Day == Yesterday) {
         // Adjust time
         MinsAfterMidnight -= (24 * 60);
      }
      else if(Day == Tomorrow) {
         // Adjust time
         MinsAfterMidnight += (24 * 60);
      }
      else if(Day != Today) {
         LOG("Internal error Day %d\n",Day);
      }

      LOG("Set entry %d %f %d %d\n",Entries,Height,Mins,MinsAfterMidnight);

      HighLowArray[Entries].Height = Height;
      HighLowArray[Entries].Hrs = Hrs;
      HighLowArray[Entries].Mins = Mins;
      HighLowArray[Entries].MinsAfterMidnight = MinsAfterMidnight;
      HighLowArray[Entries].Time = TideTime;
      if(TideType == "H") {
         HighLowArray[Entries].LowTide = false;
         if(Day == Today) {
            HighTides++; // count it
            if(MaxHeight < Height) {
               MaxHeight = Height;
               LOG("New MaxHeight %f\n",MaxHeight);
            }
         }
      }
      else if(TideType == "L") {
         HighLowArray[Entries].LowTide = true;
         if(Day == Today) {
            LowTides++; // count it
            if(MinHeight > Height) {
               MinHeight = Height;
               LOG("New MinHeight %f\n",MinHeight);
            }
         }
      }
      else {
         LOG("Unknown tide type %s\n",TideType.c_str());
      }

      if(Day != Yesterday) {
         // Just save the last value from yesterday
         Entries++;
      }

      if(Day == Tomorrow) {
         // We're done
         break;
      }
   }

   time(&Now);
   if(MaxValues == 2) {
   // just want last high/low tide and next high/low tide,
      for(i = 1; i < Entries + 1; i++) {
         if(HighLowArray[i].Time >= Now) {
            Results[0] = HighLowArray[i - 1];
            LOG("Next tide event is entry %d\n",i);
            Results[1] = HighLowArray[i];

            localtime_r(&Results[0].Time,&TimeinfoToday);
            LOG("Last tide %d:%02d\n\n",TimeinfoToday.tm_hour,TimeinfoToday.tm_min);
            localtime_r(&Results[1].Time,&TimeinfoToday);
            LOG("Next tide %d:%02d\n\n",TimeinfoToday.tm_hour,TimeinfoToday.tm_min);

            break;
         }
         Ret = 2;
      }
   }
   else {
      for(i = 0; i < Entries + 1; i++) {
         Ret++;
         if(Ret > MaxValues) {
            ELOG("Results array is too small, %d < %d\n",MaxValues,Entries);
            break;
         }
         Results[i] = HighLowArray[i];
      }
   }

   return Ret;
}

#endif // WITHOUT_NOAA_TIDES

