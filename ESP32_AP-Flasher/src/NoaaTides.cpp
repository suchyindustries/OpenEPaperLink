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

// Return tide predications for the last tide yesterday, the currnet tdies
// for today plus the first tide tomorrow
int GetNoaaTides(String StationID,std::vector<HighLowArray_t> &Tides)
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
   HighLowArray_t YesterdayValues;

   time(&Now);
   localtime_r(&Now,&TimeinfoToday);

   LOG("Current time %s",ctime(&Now));

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
      if(Yesterday == 0 || Day == Yesterday) {
         if(Yesterday == 0) {
         // Must be the first high/low from yesterday
            LOG("Set Yesterday to %d\n",Day);
            Yesterday = Day;
         }
         Values.MinsAfterMidnight -= (24 * 60);
         YesterdayValues = Values;
      }
      else if(Day != Yesterday && (Today == 0 || Day == Today)) {
      // must be today
         if(Today == 0) {
            Today = Day;
            LOG("Set Today to %d\n",Day);
         // Save yesterday's last value
            Entries++;
            Tides.push_back(YesterdayValues);
         }
         Entries++;
         Tides.push_back(Values);
      }
      else {
      // Not yesterday or today, must be tomorrow
         Values.MinsAfterMidnight += (24 * 60);
         Entries++;
         Tides.push_back(Values);
         break;   // we're done
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

