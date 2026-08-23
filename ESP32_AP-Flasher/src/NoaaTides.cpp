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

// Return tide predications for 24 hours starting at the specified time including
// one tide event from before the start time and once after the 24 hour 
// period.
int GetNoaaTides(String StationID,std::vector<HighLowArray_t> &Tides,time_t Start)
{
   time_t Time;
   time_t EndTime;
   time_t TideTime;
   struct tm Timeinfo;
   struct tm TimeinfoToday;
   char BeginDate[10];
   char EndDate[10];  // for xample "20240605"
   JsonDocument doc;
   JsonDocument loc;
   int Yesterday = 0;
   int Today = 0;
   int Entries = 0;
   int Day;
   float Height;
   int Hrs;
   int Mins;
   int MinsAfterMidnight;
   String DateTime;
   String TideType;
   int i;
   HighLowArray_t Values;
   HighLowArray_t FirstTideValues;

   Time = Start;
   EndTime = Time + 24*60*60;
   LOG("Start time %s",ctime(&Time));
   localtime_r(&Time,&Timeinfo);
   Day = Timeinfo.tm_mday;
   LOG("Set Today to %d\n",Day);

// Begin predictions 24 hours ago, ending after 24 hours
   Time -= 24*60*60;
   localtime_r(&Time,&Timeinfo);
   snprintf(BeginDate,sizeof(BeginDate),"%d%02d%02d",
            Timeinfo.tm_year + 1900,Timeinfo.tm_mon+1,Timeinfo.tm_mday);

   localtime_r(&EndTime,&Timeinfo);
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

      Values.Height = Height;
      Values.Hrs = Hrs;
      Values.Mins = Mins;
      Values.MinsAfterMidnight = MinsAfterMidnight;
      Values.Time = TideTime;

      if(TideType == "H") {
         Values.LowTide = false;
      }
      else if(TideType == "L") {
         Values.LowTide = true;
      }
      else {
         ELOG("Unknown tide type %s\n",TideType.c_str());
         break;
      }

   // Adjust MinsAfterMidnight if needed
      if(Yesterday == 0 || Day == Yesterday) {
         if(Yesterday == 0) {
         // Must be the first high/low from yesterday
            LOG("Set Yesterday to %d\n",Day);
            Yesterday = Day;
         }
         Values.MinsAfterMidnight -= (24 * 60);
      }
      else if(Day != Yesterday && Day != Today) {
      // Not yesterday or today, must be tomorrow
         Values.MinsAfterMidnight += (24 * 60);
      }

      if(TideTime < Start) {
      // Save for now, we only return one tide before the start time
         FirstTideValues = Values;
      }
      else if(TideTime > EndTime) {
      // We're doen
         Tides.push_back(Values);
         Entries++;
         break;
      }
      else {
      // Tide is in range
         if(Entries == 0) {
         // Save the last tide before the start
            Tides.push_back(FirstTideValues);
            Entries++;
         }
         Tides.push_back(Values);
         Entries++;
      }
   }

   LOG("Returning %d (%d) entries\n",Entries,Tides.size());

   for(i = 0; i < Tides.size(); i++) {
      LOG("%d: %f foot %s tide @ %d minutes after midnight t: %s\n",
          i,
          Tides[i].Height,
          Tides[i].LowTide ? "low" : "high",
          Tides[i].MinsAfterMidnight,
          ctime(&Tides[i].Time));
   }

   return Entries;
}

#endif // WITHOUT_NOAA_TIDES

