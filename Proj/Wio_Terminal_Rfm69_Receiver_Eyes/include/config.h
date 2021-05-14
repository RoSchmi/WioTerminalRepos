#include <Arduino.h>

#ifndef _CONFIG_H
#define _CONFIG_H

// This file is for not secret user specific configurations
//
// Please set your timezone offset (time difference from your zone 
// to UTC time in units of minutes) and set the time difference
// used for DaylightSaving Time in minutes
// Define begin and end of Daylightsaving 
//


#define POWER_SENSOR_01_LABEL "Watt"    // Labels for sensors to be displayed on Wio Terminal 1. screen (length max 13)
#define POWER_SENSOR_02_LABEL "Today kWh"       
#define POWER_SENSOR_03_LABEL "Watt min"
#define POWER_SENSOR_04_LABEL "Watt max"

#define TEMP_SENSOR_01_LABEL "Collector"    // Labels for sensors to be displayed on Wio Terminal 2. screen (length max 13)
#define TEMP_SENSOR_02_LABEL "Storage"       
#define TEMP_SENSOR_03_LABEL "Water"
#define TEMP_SENSOR_04_LABEL ""


#define INVALIDATEINTERVAL_MINUTES 10   // Invalidateinterval in minutes 
                                        // (limited to values between 1 - 60)
                                        // (Sensor readings are considered to be invalid if not successsfully
                                        // read within this timespan)

#define NTP_UPDATE_INTERVAL_MINUTES  25  //  With this interval sytem time is updated via NTP
                                        //  with internet time (is limited to be not below 1 min)

                                   

#define WORK_WITH_WATCHDOG 0           // 1 = yes, 0 = no, Watchdog is used (1) or not used (0)

// Set timezoneoffset and daylightsavingtime settings according to your zone
// https://en.wikipedia.org/wiki/Daylight_saving_time_by_country
// https://en.wikipedia.org/wiki/List_of_time_zone_abbreviations

#define TIMEZONEOFFSET 60           // TimeZone time difference to UTC in minutes
#define DSTOFFSET 60                // Additional DaylightSaving Time offset in minutes

#define  DST_ON_NAME                "CEST"
#define  DST_START_MONTH            "Mar"    // Jan, Feb, Mar, Apr, May, Jun, Jul, Aug, Sep, Oct, Nov or Dec
#define  DST_START_WEEKDAY          "Sun"    // Sun, Mon, Tue, Wed, Thu, Fri, Sat
#define  DST_START_WEEK_OF_MONTH    "Fourth"   // Last, First, Second, Third, Fourth
#define  DST_START_HOUR              2        // 0 - 23

#define  DST_OFF_NAME               "CET"
#define  DST_STOP_MONTH             "Oct"    // Jan, Feb, Mar, Apr, May, Jun, Jul, Aug, Sep, Oct, Nov or Dec
#define  DST_STOP_WEEKDAY           "Sun"    // Sun, Tue, Wed, Thu, Fri, Sat
#define  DST_STOP_WEEK_OF_MONTH     "Last"   // Last, First, Second, Third, Fourth
#define  DST_STOP_HOUR               3       // 0 - 23
       

                                 

#define USE_WIFI_STATIC_IP 0     // 1 = use static IpAddress, 0 = use DHCP
                                 // for static IP: Ip-addresses have to be set in the code

#define MIN_DATAVALUE -40.0             // Values below are treated as invalid
#define MAX_DATAVALUE 140.0             // Values above are treated as invalid
#define MAGIC_NUMBER_INVALID 999.9      // Invalid values are replaced with this value (should be 999.9)
                                        // Not sure if it works with other values than 999.9

#define SHOW_GRAPHIC_SCREEN 1          // 1 = A graphic screen with actual values is shown
                                        // 0 = a log with actions is shown on the screen

#define USE_SIMULATED_SENSORVALUES     // Activates simulated sensor values (sinus curve) or (test values)
//#define USE_TEST_VALUES              // Activates sending of test values (see Code in main.cpp)
                                       // if activated we select test values, not sinus curves

#define SCREEN_OFF_TIME_MINUTES  1      // Switch screen a liitle bit dark after this time
#define SCREEN_DARK_TIME_MINUTES 2      // Switch screen very dark after this time

#define SENSOR_1_OFFSET     0.0        // Calibration Offset to sensor No 1
#define SENSOR_2_OFFSET     0.0        // Calibration Offset to sensor No 2
#define SENSOR_3_OFFSET     0.0        // Calibration Offset to sensor No 3
#define SENSOR_4_OFFSET     0.0        // Calibration Offset to sensor No 4

# define SENSOR_1_FAHRENHEIT 0         // 1 = yes, 0 = no - Display in Fahrenheit scale
# define SENSOR_2_FAHRENHEIT 0        // 1 = yes, 0 = no - Display in Fahrenheit scale
# define SENSOR_3_FAHRENHEIT 0         // 1 = yes, 0 = no - Display in Fahrenheit scale
# define SENSOR_4_FAHRENHEIT 0         // 1 = yes, 0 = no - Display in Fahrenheit scale





#endif // _CONFIG_H