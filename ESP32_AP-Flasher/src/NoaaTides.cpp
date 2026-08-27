#ifndef WITHOUT_NOAA_TIDES
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
int NoaaTides::GetNoaaTides(String StationID,time_t Start)
{
   time_t Time;
   time_t EndTime;
   time_t TideTime;
   struct tm Timeinfo;
   struct tm TimeinfoToday;
   char BeginDate[10];
   char EndDate[10];  // for example "20240605"
   JsonDocument doc;
   JsonDocument loc;
   int Yesterday = 0;
   int Today = 0;
   int Entries = 0;
   int Day;
   float Height;
   int Hrs;
   int Mins;
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

      Values.Height = Height;
      Values.Hrs = Hrs;
      Values.Mins = Mins;
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
      LOG("%d: %f foot %s tide @ %s",
          i,
          Tides[i].Height,
          Tides[i].LowTide ? "low" : "high",
          ctime(&Tides[i].Time));
   }

   return Entries;
}

int NoaaTides::PlotNoaaTides(class DrawOWM * &owm,time_t Start,time_t Duration)
{
   float Height;
   float MinHeight = 1000.0;
   float MaxHeight = -1000.0;
   int x0,x1,y0,y1;
   int i;
   int Entries = Tides.size();

   LOG("Start %d Duration %d Entries %d\n",Start,Duration,Entries);

   owm->GetBB(BB_GRAPH_DATA,x0,x1,y0,y1);
#if 0
   owm->display.drawLine(x0,y0,x1,y1,TFT_YELLOW);
#else

// Update Min/Max height for midnight
   std::vector<bezier::Vec2> BezierPoints;
	bezier::Bezier<3> cubicBezier;
	bezier::Vec2 ThisPoint;
   double MidwayTime;
   double SegmentTime;

   for(i = 0; i < Entries - 1; i++) {
      InitSegment(Start,i,cubicBezier,SegmentTime);
      ThisPoint = cubicBezier.valueAt(SegmentTime);
      double Height = ThisPoint.y;

      if(MinHeight > Height) {
         MinHeight = Height;
      }
      if(MaxHeight < Height) {
         MaxHeight = Height;
      }
		LOG("%d: SegmentTime %f height %f\n",i,SegmentTime,Height);
      BezierPoints.clear();
   }

   LOG("MinHeight %f\n",MinHeight);
   LOG("MaxHeight %f\n",MaxHeight);

#if 1
   uint16_t GraphLeft = x0;
   uint16_t GraphTop = y0;
   uint16_t GraphBottom = y1;
	uint16_t GraphWidth = x1 - x0;
   uint16_t DrawX = x0; // right
   uint16_t DrawY = y1; // bottom

	bool bFirst = true;
	uint16_t LastY = 0;
	i = 0;

   for(int j = 0; j < GraphWidth; j++) {
   // Start at the last high/low tide
		double t = (double) j / GraphWidth;
		int Time = Start + (t * Duration);

		if(bFirst || Time >= Tides[i+1].Time) {
			if(bFirst) {
				LOG("First Segment\n");
				bFirst = false;
			}
			else {
			// move to next segment
				i++;
				LOG("%d > %d i %d\n",Time,Tides[i+1].Time,i);
				BezierPoints.clear();
			}
         InitSegment(Start,i,cubicBezier,SegmentTime);
		}

		SegmentTime = Time - Tides[i].Time;
      SegmentTime /= Tides[i+1].Time - Tides[i].Time; 
		LOG("t %f Time %d SegmentTime %f i %d %d %d\n",
          t,Time,SegmentTime,i,Tides[i+i].Time,Tides[i].Time);

		if(SegmentTime < 0.0 || SegmentTime > 1.0) {
			LOG("Invalid SegmentTime\n");
			return 1;
		}
      ThisPoint = cubicBezier.valueAt(SegmentTime);
		LOG("%d t %f (%f) = %f\n",DrawX,t,SegmentTime,ThisPoint.y);
      Height = ThisPoint.y;
      DrawY = ((Height - MinHeight) / (MaxHeight - MinHeight)) *
               (GraphBottom - GraphTop);

		LOG("DrawY %d\n",DrawY);
      DrawY = GraphBottom - DrawY;
		LOG("DrawY %d\n",DrawY);
#if 0
      DrawY += CharHeight / 2;
		LOG("DrawY %d\n",DrawY);
#endif
		if(LastY != 0) {
			owm->display.drawLine(DrawX,LastY,DrawX + 1,DrawY,TFT_YELLOW);
			DrawX++;
		}
		LastY = DrawY;
   }
#endif 
#if 0
// Round MinHeight and MaxHeight to nearest .5 foot

// 1.5 initial line spacing from title
   DrawY += CharHeight + (CharHeight / 2);
   CharHeight = GRAPH_LABEL_SIZE;

   drawString(spr,LowTidesS,DrawX,DrawY,
              GRAPH_LABEL_FONT,TL_DATUM,TFT_BLACK,GRAPH_LABEL_SIZE);
   drawString(spr,HighTidesS,spr.width()-10,DrawY,
              GRAPH_LABEL_FONT,TR_DATUM,TFT_BLACK);


   DrawY += CharHeight * 2;

   uint16_t GraphTop = DrawY;
// Leave room for X axis labels below the graph
   uint16_t GraphBottom = imageParams.height - 1 - (CharHeight *2) - BOTTOM_MARGIN;
   uint16_t GraphRight = imageParams.width - 1 - RIGHT_MARGIN;

   LOG("Min, max Height %f, %f -> ",MinHeight,MaxHeight);
	if(MinHeight < 0.0) {
		MinHeight = round(MinHeight * 2.0) / 2.0;
	}
	else {
		MinHeight = floor(MinHeight * 2.0) / 2.0;
	}
   MaxHeight = round(MaxHeight * 2.0) / 2.0;
   LOG("%f, %f\n",MinHeight,MaxHeight);

// Draw Min height and max height lines plus NUM_HEIGHT_LINES - 2 

   float HeightIncrement = (MaxHeight - MinHeight) / NUM_HEIGHT_LINES;
// force labels to be on 1/2 foot boundaries
   LOG("HeightIncrement %f -> ",HeightIncrement);
   HeightIncrement = round((HeightIncrement + 0.499) * 2.0) / 2.0;
   LOG("%f\n",HeightIncrement);
	float Margin = (MinHeight + (HeightIncrement * NUM_HEIGHT_LINES)) - 
					   (MaxHeight - MinHeight);

// Adjust MinHeight / MaxHeight
   LOG("MinHeight/MaxHeight %f %f -> ",MinHeight,MaxHeight);
   MaxHeight = MaxHeight + round(Margin / 2.0);
	MinHeight =  MaxHeight - (HeightIncrement * NUM_HEIGHT_LINES);
   LOG("%f %f\n",MinHeight,MaxHeight);

// Draw tide height at left first

   int MaxWidth = 0;
   int IncrementY = (GraphBottom - GraphTop) / NUM_HEIGHT_LINES;

   for(int i = 0; i < NUM_HEIGHT_LINES + 1; i++) {
      Height = MinHeight + (i * HeightIncrement);
      DrawY = ((Height - MinHeight) / (MaxHeight - MinHeight)) *
               (GraphBottom - GraphTop);
      DrawY = GraphBottom - DrawY;
      String HeightS(Height);
      HeightS += " ft ";
      LOG("Drawing %2.1f @ %d %d %d\n",Height,DrawX,DrawY,i);
      StringWidth = drawString(spr,HeightS,DrawX,DrawY,
                               GRAPH_LABEL_FONT,TL_DATUM,TFT_BLACK,
                               GRAPH_LABEL_SIZE);
      if(MaxWidth < StringWidth) {
         MaxWidth = StringWidth;
      }
   }
   DrawX += MaxWidth;
   uint16_t GraphLeft = DrawX;
	uint16_t GraphWidth = GraphRight - GraphLeft;

   LOG("MaxWidth %d DrawX %d DrawY %d\n",MaxWidth,DrawX,DrawY);

// adjust GraphRight and GraphWidth to make room for "12am" 
// centered under right side
	StringWidth = drawString(spr,"12am",0,
									 GraphBottom + CharHeight + (CharHeight / 2),
									 GRAPH_LABEL_FONT,TL_DATUM,TFT_BLACK,
									 GRAPH_LABEL_SIZE);
	GraphRight -= StringWidth / 2;
	GraphWidth -= StringWidth / 2;

// Adjust GraphTop and GraphBottom to plot area
   GraphTop += (CharHeight / 2);
   GraphBottom += (CharHeight / 2);

// Draw tide height values
   LOG("Grid lines @ ");
   Height = MinHeight;
   for(int i = 0; i < NUM_HEIGHT_LINES + 1; i++) {
      DrawY = ((Height - MinHeight) / (MaxHeight - MinHeight)) *
               (GraphBottom - GraphTop);
      DrawY = GraphBottom - DrawY;
      LOG("%2.1f DrawY %d",Height,DrawY);
		if(i == 0 || i == NUM_HEIGHT_LINES) {
		// Solid top and bottom lines
			spr.drawLine(DrawX,DrawY,GraphRight,DrawY,TFT_BLACK);
		}
		else {
		// otherwise dotted line
			for(uint16_t x = DrawX; x < GraphRight; x += 2) {
				spr.drawPixel(x,DrawY,TFT_BLACK);
			}
		}
      Height += HeightIncrement;
   }
   LOG("\n");

   spr.drawLine(GraphLeft,GraphTop,GraphLeft,GraphBottom,TFT_BLACK);
   spr.drawLine(GraphRight,GraphTop,GraphRight,GraphBottom,TFT_BLACK);

// Draw time at bottom 4 hour increments from midnight to midnight
// |12am 4am   8am  noon   4pm   8pm  12am|
	DrawX = GraphLeft;
	DrawY = GraphBottom + CharHeight;

	MaxWidth = 0;
   for(int i = 0; i <= 24;  i += TIME_LINE_INCREMENT) {
		String TimeS(i);
		
		if(i == 0 || i == 24) {
			TimeS = "12am";
		}
		else if(i < 12) {
			TimeS += "am";
		}
		else if(i > 12) {
			TimeS = i - 12;
			TimeS += "pm";
		}
		else {
			TimeS = "noon";
		}
	// draw it at x=0 to get width
		StringWidth = drawString(spr,TimeS,0,DrawY,GRAPH_LABEL_FONT,
										 TL_DATUM,TFT_BLACK,GRAPH_LABEL_SIZE);
      if(MaxWidth < StringWidth) {
         MaxWidth = StringWidth;
      }

	// draw centered under time line
		DrawX = GraphLeft + ((i * GraphWidth) / 24);
		for(uint16_t y = GraphTop; y < GraphBottom; y+= 2) {
			spr.drawPixel(DrawX,y,TFT_BLACK);

		}
		DrawX -= StringWidth / 2;
		drawString(spr,TimeS,DrawX,DrawY,GRAPH_LABEL_FONT,
					  TL_DATUM,TFT_BLACK,GRAPH_LABEL_SIZE);
   }
// stuff we wrote to measure the label widths
	spr.fillRect(0,DrawY,MaxWidth,CharHeight,TFT_WHITE);

   DrawX = GraphLeft;
	i = 0;
	bool bFirst = true;
	uint16_t LastY = 0;
   for(int j = 0; j < GraphWidth; j++) {
   // Start at the last high/low tide
		double t = (double) j / GraphWidth;
		int Time = (24*60) * t;

		if(bFirst || Time >= Tides[i+1].Time) {
			LOG("bFirst %d Time %d Tides[%d].Time %d\n",
				 bFirst,Time,i+1,Tides[i+1].Time);
			if(bFirst) {
				bFirst = false;
			}
			else {
			// move to next segment
				LOG("%f > %d i %d\n",t,Tides[i+1].Time,i);
				i++;
				BezierPoints.clear();
			}
			MidwayTime = (double) (Tides[i+1].Time - Tides[i].Time) / 2;
			BezierPoints.push_back({(double)Tides[i].Time,
									 (double) Tides[i].Height});
			BezierPoints.push_back({MidwayTime,(double) Tides[i].Height});
			BezierPoints.push_back({(double) Tides[i+1].Time,
									 (double) Tides[i+1].Height});
			BezierPoints.push_back({MidwayTime,(double) Tides[i+1].Height});
			if(BezierPoints.size() != 4) {
				LOG("BezierPoints %d i %d\n",BezierPoints.size(),i);
				return;
			}
			cubicBezier = bezier::Bezier<3>(BezierPoints);
		}

		double SegmentTime;
		LOG("i %d\n",i);
		SegmentTime = Time - Tides[i].Time;
		LOG("SegmentTime %f i %d %d %d\n",SegmentTime,i,
			 Tides[i+i].Time,Tides[i].Time);

		int SegmentDelta = Tides[i+1].Time - Tides[i].Time;
		LOG("SegmentDelta %d\n",SegmentDelta);
		SegmentTime /= SegmentDelta;
		LOG("SegmentTime %f\n",SegmentTime);

		if(SegmentTime < 0.0 || SegmentTime > 1.0) {
			LOG("Invalid SegmentTime %f t %f Tides[%d].Time %d\n",
				 SegmentTime,t,i,Tides[i].Time);
			LOG("%d %f\n",Tides[i+1],
				 (double)(Tides[i+i].Time - Tides[i].Time));
			for(int k = 0; k < Entries; k++) {
				LOG("Tides[%d].Time %d\n",k,Tides[k].Time);
			}
			return;
		}
      ThisPoint = cubicBezier.valueAt(SegmentTime);
		LOG("%d t %f (%f) = %f\n",DrawX,t,SegmentTime,ThisPoint.y);
      Height = ThisPoint.y;
      DrawY = ((Height - MinHeight) / (MaxHeight - MinHeight)) *
               (GraphBottom - GraphTop);
		LOG("DrawY %d\n",DrawY);
      DrawY = GraphBottom - DrawY;
		LOG("DrawY %d\n",DrawY);
      DrawY += CharHeight / 2;
		LOG("DrawY %d\n",DrawY);
		if(LastY != 0) {
			spr.drawLine(DrawX,LastY,DrawX + 1,DrawY,TFT_BLACK);
			DrawX++;
		}
		LastY = DrawY;
   }
#endif
#endif
   return 0;
}

void NoaaTides::InitSegment(
   time_t Start,
   int Segment,bezier::Bezier<3> &cubicBezier,
   double &SegmentTime)
{
   std::vector<bezier::Vec2> BezierPoints;
   double MidwayTime;

   MidwayTime = (double) (Tides[Segment+1].Time - Tides[Segment].Time) / 2;
   BezierPoints.push_back({(double)Tides[Segment].Time,
                      (double) Tides[Segment].Height});
   BezierPoints.push_back({MidwayTime,(double) Tides[Segment].Height});
   BezierPoints.push_back({(double) Tides[Segment+1].Time,
                      (double) Tides[Segment+1].Height});
   BezierPoints.push_back({MidwayTime,(double) Tides[Segment+1].Height});
   cubicBezier = bezier::Bezier<3>(BezierPoints);

   SegmentTime = 1.0;
   if(Segment == 0) {
   // previous tide, the entire segment is not plotted
      time_t SegmentDelta = Tides[1].Time - Tides[0].Time;
      SegmentTime = ((double) (Start - Tides[0].Time) / SegmentDelta);
   }

   if(SegmentTime < 0.0 || SegmentTime > 1.0) {
      ELOG("Invalid SegmentTime %f\n",SegmentTime);
   }
}
#endif // WITHOUT_NOAA_TIDES

