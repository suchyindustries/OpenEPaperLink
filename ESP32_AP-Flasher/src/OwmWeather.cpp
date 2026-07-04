#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <FS.h>
#include <DrawOWM.h>

#include "tag_db.h"
#include "makeimage.h"
#include "TFT_eSPI.h"
#include "contentmanager.h"
#include "web.h"
#include "storage.h"

#define ENABLE_LOGGING  1
#if ENABLE_LOGGING && __has_include("logging.h") 
#include "logging.h"
#else
#define LOG(format, ...)
#define LOG_RAW(format, ...)
#endif

LocaleStrings_t Strings;
typedef struct {
   const char *Name;
   const char **Value;
} LookupTbl_t;

const LookupTbl_t LookupTbl[] = {
#if 0
   {"days",LC_DAY},
   {"daysShort",LC_ABDAY},
   {"months",LC_MON},
   {"monthsShort",LC_ABMON},
#endif
   {"LC_D_T_FMT",&Strings.LC_D_T_FMT},
   {"LC_D_FMT",&Strings.LC_D_FMT},
   {"LC_T_FMT",&Strings.LC_T_FMT},
   {"LC_T_FMT_AMPM",&Strings.LC_T_FMT_AMPM},
   {"LC_AM_STR",&Strings.LC_AM_STR},
   {"LC_PM_STR",&Strings.LC_PM_STR},
   {"LC_ERA",&Strings.LC_ERA},
   {"LC_ERA_D_FMT",&Strings.LC_ERA_D_FMT},
   {"LC_ERA_D_T_FMT",&Strings.LC_ERA_D_T_FMT},
   {"LC_ERA_T_FMT",&Strings.LC_ERA_T_FMT},
   {"TXT_UNKNOWN",&Strings.TXT_UNKNOWN},
   {"TXT_FEELS_LIKE",&Strings.TXT_FEELS_LIKE},
   {"TXT_SUNRISE",&Strings.TXT_SUNRISE},
   {"TXT_SUNSET",&Strings.TXT_SUNSET},
   {"TXT_MOONRISE",&Strings.TXT_MOONRISE},
   {"TXT_MOONSET",&Strings.TXT_MOONSET},
   {"TXT_WIND",&Strings.TXT_WIND},
   {"TXT_HUMIDITY",&Strings.TXT_HUMIDITY},
   {"TXT_UV_INDEX",&Strings.TXT_UV_INDEX},
   {"TXT_PRESSURE",&Strings.TXT_PRESSURE},
   {"TXT_AIR_QUALITY",&Strings.TXT_AIR_QUALITY},
   {"TXT_AIR_POLLUTION",&Strings.TXT_AIR_POLLUTION},
   {"TXT_VISIBILITY",&Strings.TXT_VISIBILITY},
   {"TXT_INDOOR_TEMPERATURE",&Strings.TXT_INDOOR_TEMPERATURE},
   {"TXT_INDOOR_HUMIDITY",&Strings.TXT_INDOOR_HUMIDITY},
   {"TXT_DEWPOINT",&Strings.TXT_DEWPOINT},
   {"TXT_MOONPHASE",&Strings.TXT_MOONPHASE},
   {"TXT_NEW_MOON",&Strings.TXT_NEW_MOON},
   {"TXT_WAXING_CRESCENT",&Strings.TXT_WAXING_CRESCENT},
   {"TXT_FIRST_QUARTER",&Strings.TXT_FIRST_QUARTER},
   {"TXT_WAXING_GIBBOUS",&Strings.TXT_WAXING_GIBBOUS},
   {"TXT_FULL_MOON",&Strings.TXT_FULL_MOON},
   {"TXT_WANING_GIBBOUS",&Strings.TXT_WANING_GIBBOUS},
   {"TXT_THIRD_QUARTER",&Strings.TXT_THIRD_QUARTER},
   {"TXT_WANING_CRESCENT",&Strings.TXT_WANING_CRESCENT},
   {"TXT_UV_LOW",&Strings.TXT_UV_LOW},
   {"TXT_UV_MODERATE",&Strings.TXT_UV_MODERATE},
   {"TXT_UV_HIGH",&Strings.TXT_UV_HIGH},
   {"TXT_UV_VERY_HIGH",&Strings.TXT_UV_VERY_HIGH},
   {"TXT_UV_EXTREME",&Strings.TXT_UV_EXTREME},
   {"TXT_WIFI_EXCELLENT",&Strings.TXT_WIFI_EXCELLENT},
   {"TXT_WIFI_GOOD",&Strings.TXT_WIFI_GOOD},
   {"TXT_WIFI_FAIR",&Strings.TXT_WIFI_FAIR},
   {"TXT_WIFI_WEAK",&Strings.TXT_WIFI_WEAK},
   {"TXT_WIFI_NO_CONNECTION",&Strings.TXT_WIFI_NO_CONNECTION},
   {"TXT_UNITS_TEMP_KELVIN",&Strings.TXT_UNITS_TEMP_KELVIN},
   {"TXT_UNITS_TEMP_CELSIUS",&Strings.TXT_UNITS_TEMP_CELSIUS},
   {"TXT_UNITS_TEMP_FAHRENHEIT",&Strings.TXT_UNITS_TEMP_FAHRENHEIT},
   {"TXT_UNITS_SPEED_METERSPERSECOND",&Strings.TXT_UNITS_SPEED_METERSPERSECOND},
   {"TXT_UNITS_SPEED_FEETPERSECOND",&Strings.TXT_UNITS_SPEED_FEETPERSECOND},
   {"TXT_UNITS_SPEED_KILOMETERSPERHOUR",&Strings.TXT_UNITS_SPEED_KILOMETERSPERHOUR},
   {"TXT_UNITS_SPEED_MILESPERHOUR",&Strings.TXT_UNITS_SPEED_MILESPERHOUR},
   {"TXT_UNITS_SPEED_KNOTS",&Strings.TXT_UNITS_SPEED_KNOTS},
   {"TXT_UNITS_SPEED_BEAUFORT",&Strings.TXT_UNITS_SPEED_BEAUFORT},
   {"TXT_UNITS_PRES_HECTOPASCALS",&Strings.TXT_UNITS_PRES_HECTOPASCALS},
   {"TXT_UNITS_PRES_PASCALS",&Strings.TXT_UNITS_PRES_PASCALS},
   {"TXT_UNITS_PRES_MILLIMETERSOFMERCURY",&Strings.TXT_UNITS_PRES_MILLIMETERSOFMERCURY},
   {"TXT_UNITS_PRES_INCHESOFMERCURY",&Strings.TXT_UNITS_PRES_INCHESOFMERCURY},
   {"TXT_UNITS_PRES_MILLIBARS",&Strings.TXT_UNITS_PRES_MILLIBARS},
   {"TXT_UNITS_PRES_ATMOSPHERES",&Strings.TXT_UNITS_PRES_ATMOSPHERES},
   {"TXT_UNITS_PRES_GRAMSPERSQUARECENTIMETER",&Strings.TXT_UNITS_PRES_GRAMSPERSQUARECENTIMETER},
   {"TXT_UNITS_PRES_POUNDSPERSQUAREINCH",&Strings.TXT_UNITS_PRES_POUNDSPERSQUAREINCH},
   {"TXT_UNITS_DIST_KILOMETERS",&Strings.TXT_UNITS_DIST_KILOMETERS},
   {"TXT_UNITS_DIST_MILES",&Strings.TXT_UNITS_DIST_MILES},
   {"TXT_UNITS_PRECIP_MILLIMETERS",&Strings.TXT_UNITS_PRECIP_MILLIMETERS},
   {"TXT_UNITS_PRECIP_CENTIMETERS",&Strings.TXT_UNITS_PRECIP_CENTIMETERS},
   {"TXT_UNITS_PRECIP_INCHES",&Strings.TXT_UNITS_PRECIP_INCHES},
   {NULL}
};

bool HttpQuery(String &url,String &Response);

bool OwmWeather(TFT_eSprite &spr, JsonObject &cfgobj, const tagRecord *taginfo, imgParam &imageParams)
{
   bool Ret = false; // Assume the worse
   JsonDocument doc;
   JsonObject languageObject;
   OwmConfig Config;

// Don't add timestamp to our display, we draw our own
   imageParams.ts_option = 0; 
   do {
      if(imageParams.width >=  800 && imageParams.height >= 480) {
         Config.DisplayFormat = FORMAT_800X480;
      }
      else if(imageParams.width >= 640 && imageParams.height >= 384) {
         Config.DisplayFormat = FORMAT_640X384;
      }
      else if(imageParams.width >= 400 | imageParams.height >= 300) {
         Config.DisplayFormat = FORMAT_400X300;
      }
      else {
         LOG("%d x %d display not supported\n",imageParams.width,imageParams.height);
         break;
      }

      getLocation(cfgobj);
      String City = cfgobj["location"];
      Config.City = City.c_str();
      Config.bMetric = cfgobj["units"] == "0";

      Config.WindSpeed = Config.bMetric ? UNITS_SPEED_KILOMETERSPERHOUR : 
                     UNITS_SPEED_MILESPERHOUR;
      Config.DistanceType = Config.bMetric ? UNITS_DIST_KILOMETERS : UNITS_DIST_MILES;
      Config.PrecipType = Config.bMetric ? UNITS_DAILY_PRECIP_MILLIMETERS : 
                                       UNITS_DAILY_PRECIP_INCHES;

      Config.PrecipHrType = Config.bMetric ? UNITS_HOURLY_PRECIP_MILLIMETERS :
                                         UNITS_HOURLY_PRECIP_INCHES;
      Config.PressureType = Config.bMetric ? UNITS_PRES_MILLIBARS :
                                         UNITS_PRES_INCHESOFMERCURY;

      Config.bDisplayAlerts = Config.bMetric ? false : true;

      Config.inTemp         = taginfo->temperature;
      Config.Rssi           = taginfo->RSSI;
      Config.batteryVoltage = taginfo->batteryMv;

      Config.bLiPo = false;
      Config.TimeFormat = "%l:%M %P";
      Config.DateFormat = "%a, %B %e";
      Config.inHumidity = NAN;

      Config.PosSunrise      = 0;
      Config.PosSunset       = 1;
      Config.PosWind         = 2;
      Config.PosHumidity     = 3;
      Config.PosUvi          = 4;
      Config.PosPressure     = 5;
      Config.PosAirQuality   = 6;
      Config.PosVisibility   = 7;
      Config.PosIntemp       = 8;
      Config.PosDewpoint     = 9;
      Config.PosInhumidity   = -1;
      Config.PosMoonrise     = -1;
      Config.PosMoonset      = -1;
      Config.PosMoonphase    = -1;

      switch(Config.DisplayFormat) {
         case FORMAT_800X480:
            Config.DisplayWidth    = 800;
            Config.DisplayHeight   = 480;
            break;

         case FORMAT_640X384:
     // positions 6,7,8,9 are not available on the 640 x 384 display
            Config.DisplayWidth    = 640;
            Config.DisplayHeight   = 384;
            Config.PosVisibility   = 4;
            Config.PosIntemp       = 5;
            Config.PosUvi          = -1;
            Config.PosPressure     = -1;
            Config.PosAirQuality   = -1;
            Config.PosInhumidity   = -1;
            break;

         case FORMAT_400X300:
            Config.DisplayWidth    = 400;
            Config.DisplayHeight   = 300;
            Config.bDisplayAlerts = false;   // no room
            break;
      }

      Config.xOffset = (imageParams.width - Config.DisplayWidth) / 2;
      Config.yOffset = (imageParams.height - Config.DisplayHeight) / 2;

      String url = "http://api.openweathermap.org/data/3.0/onecall?lat=";
      url += cfgobj["#lat"].as<String>() + "&lon=" + cfgobj["#lon"].as<String>() 
             + "&lang=" + "en" /* fix me */
             + "&units=standard&exclude=minutelyalerts&appid="
             + config.owmApiKey;
      String ForecastResponse;
      if(!HttpQuery(url,ForecastResponse)) {
         ELOG("\n");
         break;
      }
      Config.ForecastApiResponse = ForecastResponse.c_str();
      String OwmPollutionResponse;

      if(Config.PosAirQuality != -1) {
      // set start and end to appropriate values so that the last 24 hours 
      // of air pollution history is returned. Unix, UTC.
         time_t now;
         int64_t end = time(&now);
      // minus 1 is important here, otherwise we could get an extra hour of history
         int64_t start = end - ((3600 * 24) - 1);
         char endStr[22];
         char startStr[22];
         sprintf(endStr, "%lld", end);
         sprintf(startStr, "%lld", start);

         url = "http://api.openweathermap.org/data/2.5/air_pollution/history?lat=";
         url += cfgobj["#lat"].as<String>() += "&lon=" 
                + cfgobj["#lon"].as<String>() + "&start=" + startStr 
                + "&end=" + endStr + "&appid=" + config.owmApiKey;

         if(!HttpQuery(url,OwmPollutionResponse)) {
            ELOG("\n");
            break;
         }
         Config.AirPollutionApiResponse = OwmPollutionResponse.c_str();
         // LOG("response = %s\n",Config.AirPollutionApiResponse);
      }
      else {
         Config.AirPollutionApiResponse = NULL;
      }

      class DrawOWM *owm = new DrawOWM(spr,Config);
      if(config.language) {
      // Set locale
         JsonDocument filter;
         const char *Name;
         const LookupTbl_t *p = LookupTbl;

         filter[String(config.language)] = true;
         memset(&Strings,0,sizeof(Strings));

         do {
            DeserializationError error;
            File file = contentFS->open("/languages.json", "r");
            if (!file) {
                Serial.println("Failed to open languages.json file");
                break;
            }
            error = deserializeJson(doc,file,DeserializationOption::Filter(filter));
            if(error) {
                LOG("Failed to parse JSON:\n%s\n",error.c_str());
                break;
            }
            languageObject = doc[String(config.language)];
            for(int i = 0; i < 7; i++) {
               Strings.LC_ABDAY[i] = languageObject["daysShort"][i].as<const char *>();
               Strings.LC_DAY[i] = languageObject["days"][i].as<const char *>();
            }
            for(int i = 0; i < 12; i++) {
               Strings.LC_MON[i] = languageObject["months"][i].as<const char *>();
            }
            while((Name = p->Name) != NULL) {
               if(languageObject[Name]) {
                  *p->Value = languageObject[Name].as<const char *>();
               }
               p++;
            }
            owm->SetLocale(&Strings);
         } while(false);
      }

      owm->DrawIt();
      delete owm;
      Ret = true;
   } while(false);

   return Ret;
}

bool HttpQuery(String &url,String &Response)
{
   HTTPClient http;
   bool Ret = false;

   // LOG("Fetching %s\n",url.c_str());

   http.begin(url);
   http.setTimeout(5000);
   http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
   int httpCode = http.GET();
   if (httpCode != 200) {
       http.end();
       String Err = "[HttpQuery] ";
       Err += url + " code " + httpCode;
       wsErr(Err);
       LOG("http.GET() failed: %s\n",Err.c_str());
       String Err1 = http.getString();
       wsErr(Err1);
       LOG("%s\n",Err1.c_str());
   }
   else {
      Response = http.getString().c_str();
      Ret = true;
   }
   http.end();

   return Ret;
}
