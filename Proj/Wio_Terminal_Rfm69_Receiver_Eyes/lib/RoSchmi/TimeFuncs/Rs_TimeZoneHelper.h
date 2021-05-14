/****************************************************************************************************
  Based on and modified from Arduino Timezone Library (https://github.com/JChristensen/Timezone)
  
  Copyright (C) 2018 by Jack Christensen and licensed under GNU GPL v3.0, https://www.gnu.org/licenses/gpl.html
  based also on Khoi Hoang https://github.com/khoih-prog/Timezone_Generic
  Licensed under MIT license
  Version: 1.3.0
******************************************************************************************************/


#include <Arduino.h>
#include <TimeLib.h>    // https://github.com/PaulStoffregen/Time


#ifndef _RS_TIMEZONEHELPER_H_
#define _RS_TIMEZONEHELPER_H_

// structure to describe rules for when daylight/summer time begins,
// or when standard time begins.
typedef struct 
{
  char    abbrev[6];    // five chars max
  uint8_t week;         // First, Second, Third, Fourth, or Last week of the month
  uint8_t dow;          // day of week, 1=Sun, 2=Mon, ... 7=Sat
  uint8_t month;        // 1=Jan, 2=Feb, ... 12=Dec
  uint8_t hour;         // 0-23
  int     offset;       // offset from UTC in minutes
} TimeChangeRule;


#define TZ_DATA_SIZE    sizeof(TimeChangeRule)

class Timezone
{
  public:
    Timezone(TimeChangeRule dstStart, TimeChangeRule stdStart);
    Timezone(TimeChangeRule stdTime);
    Timezone(int address);
    
    // Allow a "blank" TZ object then use begin() method to set the actual TZ.
    // Feature added by 6v6gt (https://forum.arduino.cc/index.php?topic=711259)
    Timezone();
		void init(TimeChangeRule dstStart, TimeChangeRule stdStart);
		//////
    
    time_t  toLocal(time_t utc);
    time_t  toLocal(time_t utc, TimeChangeRule **tcr);
    time_t  toUTC(time_t local);
    bool    utcIsDST(time_t utc);
    bool    locIsDST(time_t local);
    void    setRules(TimeChangeRule dstStart, TimeChangeRule stdStart);
    
    //void    readRules(int address);
    void    readRules();
    void    writeRules(int address = 0);
    
    TimeChangeRule* read_DST_Rule()
    {
      return &m_dst;
    }
    
    TimeChangeRule* read_STD_Rule()
    {
      return &m_std;
    }
    
    void display_DST_Rule();  
    void display_STD_Rule();

  private:
    void    calcTimeChanges(int yr);
    void    initTimeChanges();
    time_t  toTime_t(TimeChangeRule r, int yr);
    
    TimeChangeRule m_dst;   // rule for start of dst or summer time for any year
    TimeChangeRule m_std;   // rule for start of standard time for any year
    
    time_t m_dstUTC;        // dst start for given/current year, given in UTC
    time_t m_stdUTC;        // std time start for given/current year, given in UTC
    time_t m_dstLoc;        // dst start for given/current year, given in local time
    time_t m_stdLoc;        // std time start for given/current year, given in local time
    
    void      readTZData();
    void      writeTZData(int address);
    
    uint16_t  TZ_DATA_START     = 0;
    
    bool      storageSystemInit = false;
};


class Rs_TimeZoneHelper {

public:

const char daysOfTheWeek[7][4] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
const char monthsOfTheYear[12][5] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
const char weekOfMonth[5][8] {"Last", "First", "Second", "Third", "Fourth", };
};

#include "Rs_TimeZoneHelper_Impl.h"

#endif