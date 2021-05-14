/****************************************************************************************************
  Based on and modified from Arduino Timezone Library (https://github.com/JChristensen/Timezone)
  
  Copyright (C) 2018 by Jack Christensen and licensed under GNU GPL v3.0, https://www.gnu.org/licenses/gpl.html
  based also on Khoi Hoang https://github.com/khoih-prog/Timezone_Generic
  Licensed under MIT license
  Version: 1.3.0
******************************************************************************************************/

#include "Rs_TimeZoneHelper.h"
#include <time.h>

#pragma once

#ifndef TIMEZONE_GENERIC_IMPL_H
#define TIMEZONE_GENERIC_IMPL_H


/*----------------------------------------------------------------------*
   Create a Timezone object from the given time change rules.
  ----------------------------------------------------------------------*/
Timezone::Timezone(TimeChangeRule dstStart, TimeChangeRule stdStart)
  : m_dst(dstStart), m_std(stdStart), TZ_DATA_START(0)
{
  this->TZ_DATA_START = 0;
  
  initTimeChanges();
}

/*----------------------------------------------------------------------*
   Create a Timezone object for a zone that does not observe
   daylight time.
  ----------------------------------------------------------------------*/
Timezone::Timezone(TimeChangeRule stdTime)
  : m_dst(stdTime), m_std(stdTime), TZ_DATA_START(0)
{ 
  initTimeChanges();
}

//////

// Allow a "blank" TZ object then use begin() method to set the actual TZ.
// Feature added by 6v6gt (https://forum.arduino.cc/index.php?topic=711259)
/*----------------------------------------------------------------------*
   Create a Timezone object for later initialisation 6v6gt 31.10.2020
  ----------------------------------------------------------------------*/
Timezone::Timezone()
: TZ_DATA_START(0)
{
		initTimeChanges();
}

/*----------------------------------------------------------------------*
   initialise (used where void constructor is called)  6v6gt
  ----------------------------------------------------------------------*/
void Timezone::init(TimeChangeRule dstStart, TimeChangeRule stdStart)
{
  m_dst = dstStart;
  m_std = stdStart;
}

//////




/*----------------------------------------------------------------------*
   Initialize the DST and standard time change points.
  ----------------------------------------------------------------------*/
void Timezone::initTimeChanges()
{
  m_dstLoc = 0;
  m_stdLoc = 0;
  m_dstUTC = 0;
  m_stdUTC = 0;
}

/*----------------------------------------------------------------------*
   Convert the given UTC time to local time, standard or
   daylight time, as appropriate.
  ----------------------------------------------------------------------*/
time_t Timezone::toLocal(time_t utc)
{
  // recalculate the time change points if needed
  if (year(utc) != year(m_dstUTC)) 
    calcTimeChanges(year(utc));

  if (utcIsDST(utc))
    return utc + m_dst.offset * SECS_PER_MIN;
  else
    return utc + m_std.offset * SECS_PER_MIN;
}

/*----------------------------------------------------------------------*
   Convert the given UTC time to local time, standard or
   daylight time, as appropriate, and return a pointer to the time
   change rule used to do the conversion. The caller must take care
   not to alter this rule.
  ----------------------------------------------------------------------*/
time_t Timezone::toLocal(time_t utc, TimeChangeRule **tcr)
{
  // recalculate the time change points if needed
  if (year(utc) != year(m_dstUTC)) 
    calcTimeChanges(year(utc));

  if (utcIsDST(utc)) 
  {
    *tcr = &m_dst;
    return utc + m_dst.offset * SECS_PER_MIN;
  }
  else 
  {
    *tcr = &m_std;
    return utc + m_std.offset * SECS_PER_MIN;
  }
}

/*----------------------------------------------------------------------*
   Convert the given local time to UTC time.
 *                                                                      *
   WARNING:
   This function is provided for completeness, but should seldom be
   needed and should be used sparingly and carefully.
 *                                                                      *
   Ambiguous situations occur after the Standard-to-DST and the
   DST-to-Standard time transitions. When changing to DST, there is
   one hour of local time that does not exist, since the clock moves
   forward one hour. Similarly, when changing to standard time, there
   is one hour of local times that occur twice since the clock moves
   back one hour.
 *                                                                      *
   This function does not test whether it is passed an erroneous time
   value during the Local -> DST transition that does not exist.
   If passed such a time, an incorrect UTC time value will be returned.
 *                                                                      *
   If passed a local time value during the DST -> Local transition
   that occurs twice, it will be treated as the earlier time, i.e.
   the time that occurs before the transistion.
 *                                                                      *
   Calling this function with local times during a transition interval
   should be avoided!
  ----------------------------------------------------------------------*/
time_t Timezone::toUTC(time_t local)
{
  // recalculate the time change points if needed
  if (year(local) != year(m_dstLoc)) 
    calcTimeChanges(year(local));

  if (locIsDST(local))
    return local - m_dst.offset * SECS_PER_MIN;
  else
    return local - m_std.offset * SECS_PER_MIN;
}

/*----------------------------------------------------------------------*
   Determine whether the given UTC time_t is within the DST interval
   or the Standard time interval.
  ----------------------------------------------------------------------*/
bool Timezone::utcIsDST(time_t utc)
{
  // recalculate the time change points if needed
  if (year(utc) != year(m_dstUTC)) 
    calcTimeChanges(year(utc));

  if (m_stdUTC == m_dstUTC)       // daylight time not observed in this tz
    return false;
  else if (m_stdUTC > m_dstUTC)   // northern hemisphere
    return (utc >= m_dstUTC && utc < m_stdUTC);
  else                            // southern hemisphere
    return !(utc >= m_stdUTC && utc < m_dstUTC);
}

/*----------------------------------------------------------------------*
   Determine whether the given Local time_t is within the DST interval
   or the Standard time interval.
  ----------------------------------------------------------------------*/
bool Timezone::locIsDST(time_t local)
{
  // recalculate the time change points if needed
  if (year(local) != year(m_dstLoc)) 
    calcTimeChanges(year(local));

  if (m_stdUTC == m_dstUTC)       // daylight time not observed in this tz
    return false;
  else if (m_stdLoc > m_dstLoc)   // northern hemisphere
    return (local >= m_dstLoc && local < m_stdLoc);
  else                            // southern hemisphere
    return !(local >= m_stdLoc && local < m_dstLoc);
}

/*----------------------------------------------------------------------*
   Calculate the DST and standard time change points for the given
   given year as local and UTC time_t values.
  ----------------------------------------------------------------------*/
void Timezone::calcTimeChanges(int yr)
{
  m_dstLoc = toTime_t(m_dst, yr);
  m_stdLoc = toTime_t(m_std, yr);
  m_dstUTC = m_dstLoc - m_std.offset * SECS_PER_MIN;
  m_stdUTC = m_stdLoc - m_dst.offset * SECS_PER_MIN;
}


/*----------------------------------------------------------------------*
   Convert the given time change rule to a time_t value
   for the given year.
  ----------------------------------------------------------------------*/
time_t Timezone::toTime_t(TimeChangeRule r, int yr)
{
  uint8_t m = r.month;     // temp copies of r.month and r.week
  uint8_t w = r.week;
  
  if (w == 0)              // is this a "Last week" rule?
  {
    if (++m > 12)        // yes, for "Last", go to the next month
    {
      m = 1;
      ++yr;
    }
    
    w = 1;               // and treat as first week of next month, subtract 7 days later
  }

  // calculate first day of the month, or for "Last" rules, first day of the next month
  tmElements_t tm;
  
  tm.Hour   = r.hour;
  tm.Minute = 0;
  tm.Second = 0;
  tm.Day    = 1;
  tm.Month  = m;
  tm.Year   = yr - 1970;
  time_t t  = makeTime(tm);

  // add offset from the first of the month to r.dow, and offset for the given week
  t += ( (r.dow - weekday(t) + 7) % 7 + (w - 1) * 7 ) * SECS_PER_DAY;
  
  // back up a week if this is a "Last" rule
  if (r.week == 0) 
    t -= 7 * SECS_PER_DAY;
    
  return t;
}

/*----------------------------------------------------------------------*
   Read or update the daylight and standard time rules from RAM.
  ----------------------------------------------------------------------*/
void Timezone::setRules(TimeChangeRule dstStart, TimeChangeRule stdStart)
{
  m_dst = dstStart;
  m_std = stdStart;
  initTimeChanges();  // force calcTimeChanges() at next conversion call
}

void Timezone::display_DST_Rule()
{
  Serial.println("DST rule");
  
  Serial.printf("abbrev : %s,  week : %s",  m_dst.abbrev, m_dst.week);
  Serial.printf("dow : %s,  month : %s",  m_dst.dow, m_dst.month);
  Serial.printf("hour : %s,  offset : %s",  m_dst.hour, m_dst.hour);

}

void Timezone::display_STD_Rule()
{
  Serial.println("DST rule");
  
  Serial.printf("abbrev : %s,  week : %s",  m_std.abbrev, m_std.week);
  Serial.printf("dow : %s,  month : %s",  m_std.dow, m_std.month);
  Serial.printf("hour : %s,  offset : %s",  m_std.hour, m_std.hour);

}





#endif    // TIMEZONE_GENERIC_IMPL_H

