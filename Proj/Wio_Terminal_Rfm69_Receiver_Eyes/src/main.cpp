//File: Wio_Terminal_Rfm69_Receiver
// Copyright Roschmi 2021, License Apache V2
// Please obey the License agreements of the libraries included like Rfm69 and others

#include <Arduino.h>
#include <time.h>
#include "DateTime.h"
#include "rpcWiFi.h"
#include "lcd_backlight.hpp"

#include "SAMCrashMonitor.h"

//#include "NTPClient_Generic.h"
//#include "Timezone_Generic.h"

#include "SysTime.h"
#include "TimeFuncs/Rs_TimeZoneHelper.h"

#include "RFM69.h"
#include "RFM69registers.h"
#include "Reform_uint16_2_float32.h"
#include "config_secret.h"
#include "config.h"
#include "PowerVM.h"

#include "TFT_eSPI.h"
#include "Free_Fonts.h"

//*********************************************************************************************
// *********** IMPORTANT SETTINGS - YOU MUST CHANGE/CONFIGURE TO FIT YOUR HARDWARE *************
//*********************************************************************************************

#define NETWORKID                   100  //the same on all nodes that talk to each other
#define NODEID                      1    // The unique identifier of this node
#define SOLARPUMP_CURRENT_SENDER_ID 2    // The node which sends current and temperature states
#define SOLAR_TEMP_SENDER_ID        3    // The node which sends state changes of the solar pump
//#define RECEIVER      3    // The recipient of packets (of the test-node)

//Match frequency to the hardware version of the radio on your Feather
#define FREQUENCY     RF69_433MHZ
//#define FREQUENCY     RF69_868MHZ
//#define FREQUENCY      RF69_915MHZ

// Changed by RoSchmi
//#define ENCRYPTKEY     "sampleEncryptKey" //exactly the same 16 characters/bytes on all nodes!
// see in config_secret.h

#define IS_RFM69HCW       true // set to 'true' if you are using an RFM69HCW module

#define RF69_SPI_CS           SS // SS is the SPI slave select pin, 
#define RF69_IRQ_PIN          4  // Rfm69 Interrupt pin
#define RFM69_RST             3  // Rfm69 reset pin

uint8_t receivedData[62] {0};

int lastPacketNum = 0;
int lastPacketNum_3 = 0;

uint32_t missedPacketNums = 0;
uint32_t missedPacketNums_3 = 0;

int missedPacketNumToShow = 0;

RFM69 rfm69(RF69_SPI_CS, RF69_IRQ_PIN, IS_RFM69HCW);

PowerVM powerVM;

TFT_eSPI tft;

#define Average_Window_Size 50

uint32_t AverWinIndex = 0;
int32_t NoiseValue    = 0;
int32_t NoiseSum      = 0;
int32_t NoiseAverage  = 0;
int32_t NoiseReadings[Average_Window_Size];


static LCDBackLight backLight;
uint8_t maxBrightness = 0;

int current_text_line = 0;

#define LCD_WIDTH 320
#define LCD_HEIGHT 240
#define LCD_FONT FreeSans9pt7b
#define LCD_LINE_HEIGHT 18

const GFXfont *textFont = FSSB9;

uint32_t screenColor = TFT_BLUE;
uint32_t backColor = TFT_WHITE;
uint32_t textColor = TFT_BLACK;

bool showPowerScreen = true;
bool lastScreenWasPower = true;

uint32_t loopCounter = 0;

typedef enum 
        {
            Temperature,
            Power
        } ValueType;


uint32_t timeNtpUpdateCounter = 0;
int32_t sysTimeNtpDelta = 0;
uint32_t ntpUpdateInterval = 60000;
uint32_t lastDateOutputMinute = 60;
uint32_t previousDisplayTimeUpdateMillis = 0;
uint32_t previousNtpUpdateMillis = 0;
uint32_t previousNtpRequestMillis = 0;

uint32_t actDay = 0;

float workAtStartOfDay = 0;

float powerDayMin = 50000;   // very high value
float powerDayMax = 0;

DateTime BootTimeUtc = DateTime();
DateTime DisplayOnTime = DateTime();
DateTime DisplayOffTime = DateTime();

char timeServer[] = "pool.ntp.org"; // external NTP server e.g. better pool.ntp.org
unsigned int localPort = 2390;      // local port to listen for UDP packets

const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message

byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets

bool ledState = false;
uint8_t lastResetCause = -1;

const int timeZoneOffset = (int)TIMEZONEOFFSET;
const int dstOffset = (int)DSTOFFSET;

unsigned long utcTime;

DateTime dateTimeUTCNow;    // Seconds since 2000-01-01 08:00:00

const char *ssid = IOT_CONFIG_WIFI_SSID;
const char *password = IOT_CONFIG_WIFI_PASSWORD;

// must be static !!
static SysTime sysTime;


Rs_TimeZoneHelper timeZoneHelper;

Timezone myTimezone; 

WiFiUDP udp;

// Array to retrieve spaces with different length
char PROGMEM spacesArray[13][13] = {  "", 
                                      " ", 
                                      "  ", 
                                      "   ", 
                                      "    ", 
                                      "     ", 
                                      "      ", 
                                      "       ", 
                                      "        ", 
                                      "         ", 
                                      "          ", 
                                      "           ",  
                                      "            "};


// Routine to send messages to the display
void lcd_log_line(char* line, uint32_t textCol = textColor, uint32_t backCol = backColor, uint32_t screenCol = screenColor) 
{  
  tft.setTextColor(textColor);
  tft.setFreeFont(textFont);
  tft.fillRect(0, current_text_line * LCD_LINE_HEIGHT, 320, LCD_LINE_HEIGHT, backColor);
  tft.drawString(line, 5, current_text_line * LCD_LINE_HEIGHT);
  current_text_line++;
  current_text_line %= ((LCD_HEIGHT-20)/LCD_LINE_HEIGHT);
  if (current_text_line == 0) 
  {
    tft.fillScreen(screenColor);
  }
}

// forward declarations
void runWakeUpPerformance();
unsigned long getNTPtime();
unsigned long sendNTPpacket(const char* address);
int16_2_float_function_result reform_uint16_2_float32(uint16_t u1, uint16_t u2);
int getDayNum(const char * day);
int getMonNum(const char * month);
int getWeekOfMonthNum(const char * weekOfMonth);
void showDisplayFrame(char * label_01, char * label_02, char * label_03, char * label_04, uint32_t screenCol, uint32_t backCol, uint32_t textCol);
void fillDisplayFrame(ValueType valueType, double an_1, double an_2, double an_3, double an_4, bool on_1,  bool on_2, bool on_3, bool on_4, bool pLastMssageMissed);


void setup() {
  tft.begin();
  tft.setRotation(3);
  tft.fillScreen(screenColor);
  tft.setFreeFont(&LCD_FONT);
  tft.setTextColor(TFT_BLACK);

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(WIO_5S_PRESS, INPUT_PULLUP);

  // Hard Reset the RFM module
  pinMode(RFM69_RST, OUTPUT);   // Wio Terminal: 3
  digitalWrite(RFM69_RST, HIGH);  
  delay(100);
  digitalWrite(RFM69_RST, LOW);
  delay(100);

  Serial.begin(115200);
  //while(!Serial);
  Serial.println("Starting...");

  lcd_log_line((char *)"Start - Disable watchdog.");
  SAMCrashMonitor::begin();

  // Get last ResetCause
  // Ext. Reset: 16
  // WatchDog:   32
  // BySystem:   64
  lastResetCause = SAMCrashMonitor::getResetCause();
  lcd_log_line((char *)SAMCrashMonitor::getResetDescription().c_str());
  SAMCrashMonitor::dump();
  SAMCrashMonitor::disableWatchdog();

  // Logging can be activated here:
  // Seeed_Arduino_rpcUnified/src/rpc_unified_log.h:
  // ( https://forum.seeedstudio.com/t/rpcwifi-library-only-working-intermittently/255660/5 )

// buffer to hold messages for display
  char buf[100];
  
  sprintf(buf, "RTL8720 Firmware: %s", rpc_system_version());
  lcd_log_line(buf);
  sprintf(buf, "Initial WiFi-Status: %i", (int)WiFi.status());
  lcd_log_line(buf);

  // Setting Daylightsavingtime. Enter values for your zone in file include/config.h
  // Program aborts in some cases of invalid values
  
  int dstWeekday = getDayNum(DST_START_WEEKDAY);
  int dstMonth = getMonNum(DST_START_MONTH);
  int dstWeekOfMonth = getWeekOfMonthNum(DST_START_WEEK_OF_MONTH);

  TimeChangeRule dstStart {DST_ON_NAME, (uint8_t)dstWeekOfMonth, (uint8_t)dstWeekday, (uint8_t)dstMonth, DST_START_HOUR, TIMEZONEOFFSET + DSTOFFSET};
  
  bool firstTimeZoneDef_is_Valid = (dstWeekday == -1 || dstMonth == - 1 || dstWeekOfMonth == -1 || DST_START_HOUR > 23 ? true : DST_START_HOUR < 0 ? true : false) ? false : true;
  
  dstWeekday = getDayNum(DST_STOP_WEEKDAY);
  dstMonth = getMonNum(DST_STOP_MONTH) + 1;
  dstWeekOfMonth = getWeekOfMonthNum(DST_STOP_WEEK_OF_MONTH);

  TimeChangeRule stdStart {DST_OFF_NAME, (uint8_t)dstWeekOfMonth, (uint8_t)dstWeekday, (uint8_t)dstMonth, (uint8_t)DST_START_HOUR, (int)TIMEZONEOFFSET};

  bool secondTimeZoneDef_is_Valid = (dstWeekday == -1 || dstMonth == - 1 || dstWeekOfMonth == -1 || DST_STOP_HOUR > 23 ? true : DST_STOP_HOUR < 0 ? true : false) ? false : true;
  
  if (firstTimeZoneDef_is_Valid && secondTimeZoneDef_is_Valid)
  {
    myTimezone.setRules(dstStart, stdStart);
  }
  else
  {
    // If timezonesettings are not valid: -> take UTC and wait for ever  
    TimeChangeRule stdStart {DST_OFF_NAME, (uint8_t)dstWeekOfMonth, (uint8_t)dstWeekday, (uint8_t)dstMonth, (uint8_t)DST_START_HOUR, (int)0};
    myTimezone.setRules(stdStart, stdStart);
    while (true)
    {
      Serial.println("Invalid DST Timezonesettings");
      delay(5000);
    }
  }

  //******************************************************
  
   //Set WiFi to station mode and disconnect from an AP if it was previously connected
  WiFi.mode(WIFI_STA);
  lcd_log_line((char *)"First disconnecting.");
  while (WiFi.status() != WL_DISCONNECTED)
  {
    WiFi.disconnect();
    delay(200); 
  }
  
  sprintf(buf, "Status: %i", (int)WiFi.status());
  lcd_log_line(buf);
  
  delay(500);

  sprintf(buf, "Connecting to SSID: %s", ssid);
  lcd_log_line(buf);

  if (!ssid || *ssid == 0x00 || strlen(ssid) > 31)
  {
    lcd_log_line((char *)"Invalid: SSID or PWD, Stop");
    // Stay in endless loop
      while (true)
      {         
        delay(1000);
      }
  }

#if USE_WIFI_STATIC_IP == 1
  IPAddress presetIp(192, 168, 1, 83);
  IPAddress presetGateWay(192, 168, 1, 1);
  IPAddress presetSubnet(255, 255, 255, 0);
  IPAddress presetDnsServer1(8,8,8,8);
  IPAddress presetDnsServer2(8,8,4,4);
#endif

WiFi.begin(ssid, password);
 
if (!WiFi.enableSTA(true))
{
  #if WORK_WITH_WATCHDOG == 1   
    __unused int timeout = SAMCrashMonitor::enableWatchdog(4000);
  #endif

  while (true)
  {
    // Stay in endless loop to reboot through Watchdog
    lcd_log_line((char *)"Connect failed.");
    delay(1000);
    }
}

#if USE_WIFI_STATIC_IP == 1
  if (!WiFi.config(presetIp, presetGateWay, presetSubnet, presetDnsServer1, presetDnsServer2))
  {
    while (true)
    {
      // Stay in endless loop
    lcd_log_line((char *)"WiFi-Config failed");
      delay(3000);
    }
  }
  else
  {
    lcd_log_line((char *)"WiFi-Config successful");
    delay(1000);
  }
  #endif

  while (WiFi.status() != WL_CONNECTED)
  {  
    delay(1000);
    lcd_log_line(itoa((int)WiFi.status(), buf, 10));
    WiFi.begin(ssid, password);
  }

  lcd_log_line((char *)"Connected, new Status:");
  lcd_log_line(itoa((int)WiFi.status(), buf, 10));

  #if WORK_WITH_WATCHDOG == 1
    
    Serial.println(F("Enabling watchdog."));
    int timeout = SAMCrashMonitor::enableWatchdog(4000);
    sprintf(buf, "Watchdog enabled: %i %s", timeout, "ms");
    lcd_log_line(buf);
  #endif
  
  IPAddress localIpAddress = WiFi.localIP();
  IPAddress gatewayIp =  WiFi.gatewayIP();
  IPAddress subNetMask =  WiFi.subnetMask();
  IPAddress dnsServerIp = WiFi.dnsIP();
   
// Wait for 1500 ms
  for (int i = 0; i < 4; i++)
  {
    delay(500);
    #if WORK_WITH_WATCHDOG == 1
      SAMCrashMonitor::iAmAlive();
    #endif
  }
  
  current_text_line = 0;
  tft.fillScreen(screenColor);
    
  lcd_log_line((char *)"> SUCCESS.");
  sprintf(buf, "Ip: %s", (char*)localIpAddress.toString().c_str());
  lcd_log_line(buf);
  sprintf(buf, "Gateway: %s", (char*)gatewayIp.toString().c_str());
  lcd_log_line(buf);
  sprintf(buf, "Subnet: %s", (char*)subNetMask.toString().c_str());
  lcd_log_line(buf);
  sprintf(buf, "DNS-Server: %s", (char*)dnsServerIp.toString().c_str());
  lcd_log_line(buf);
  
  ntpUpdateInterval =  (NTP_UPDATE_INTERVAL_MINUTES < 1 ? 1 : NTP_UPDATE_INTERVAL_MINUTES) * 60 * 1000;

#if WORK_WITH_WATCHDOG == 1
    SAMCrashMonitor::iAmAlive();
  #endif

  int getTimeCtr = 0; 
  utcTime = getNTPtime();
  while ((getTimeCtr < 5) && utcTime == 0)
  {   
      getTimeCtr++;
      #if WORK_WITH_WATCHDOG == 1
        SAMCrashMonitor::iAmAlive();
      #endif

      utcTime = getNTPtime();
  }

  #if WORK_WITH_WATCHDOG == 1
    SAMCrashMonitor::iAmAlive();
  #endif

  if (utcTime != 0 )
  {
    sysTime.begin(utcTime);
    dateTimeUTCNow = utcTime;
  }
  else
  {
    lcd_log_line((char *)"Failed get network time");
    delay(10000);
    // do something, evtl. reboot
    while (true)
    {
      delay(100);
    }   
  }
  
  dateTimeUTCNow = sysTime.getTime();

  BootTimeUtc = dateTimeUTCNow;
  DisplayOnTime = dateTimeUTCNow;
  DisplayOffTime = dateTimeUTCNow;
  
  // RoSchmi for DST tests
  // dateTimeUTCNow = DateTime(2021, 10, 31, 1, 1, 0);

  sprintf(buf, "%s-%i-%i-%i %i:%i", (char *)"UTC-Time is  :", dateTimeUTCNow.year(), 
                                        dateTimeUTCNow.month() , dateTimeUTCNow.day(),
                                        dateTimeUTCNow.hour() , dateTimeUTCNow.minute());
  Serial.println(buf);                            
  lcd_log_line(buf);
  
  
  DateTime localTime = myTimezone.toLocal(dateTimeUTCNow.unixtime());
  
  sprintf(buf, "%s-%i-%i-%i %i:%i", (char *)"Local-Time is:", localTime.year(), 
                                        localTime.month() , localTime.day(),
                                        localTime.hour() , localTime.minute());
  Serial.println(buf);
  lcd_log_line(buf);

  Serial.println("Initializing Rfm69...");

  bool initResult = rfm69.initialize(RF69_433MHZ, NODEID, NETWORKID);

  Serial.printf("Rfm69 initialization %s\r\n", initResult == true ? "was successful" : "failed");

  rfm69.setPowerLevel(31); // power output ranges from 0 (5dBm) to 31 (20dBm), default = 31

  rfm69.encrypt(ENCRYPTKEY);
  //encrypt(0);

  Serial.printf("Working at %i MHz", FREQUENCY==RF69_433MHZ ? 433 : FREQUENCY==RF69_868MHZ ? 868 : 915);

  
  showDisplayFrame(POWER_SENSOR_01_LABEL, POWER_SENSOR_02_LABEL, POWER_SENSOR_03_LABEL, POWER_SENSOR_04_LABEL, TFT_BLACK, TFT_BLACK, TFT_DARKGREY);
  
  fillDisplayFrame(ValueType::Power, 999.9, 999.9, 999.9, 999.9, false, false, false, false, false);
  delay(50);

  backLight.initialize();
  maxBrightness = backLight.getMaxBrightness();

  previousNtpUpdateMillis = millis();
  previousNtpRequestMillis = millis();
}
  
void loop() {
  //check if something was received (could be an interrupt from the radio)
  if (rfm69.receiveDone())
  {
    // Save received data
    memcpy(receivedData, rfm69.DATA, rfm69.PAYLOADLEN);
    uint16_t senderID = rfm69.SENDERID;
    char cmdChar = (char)receivedData[4];

    uint32_t sensor_1 = 0;
    uint32_t sensor_2 = 0;
    uint32_t sensor_3 = 0;
    uint16_t sensor_3_Lower = 0;
    uint16_t sensor_3_Higher = 0;

    bool lastMessageMissed = false;

    //print message received to serial
    Serial.printf("[%i]\r\n", senderID);
    uint8_t payLoadLen = rfm69.PAYLOADLEN;
    Serial.println(payLoadLen);
    
    /*
    for (int i = 0; i < payLoadLen; i++)
    {
      Serial.printf("0x%02x ", receivedData[i]);
    }
    Serial.println("");
    */
    
    char workBuffer[10] {0};
    memcpy(workBuffer, receivedData, 3);
    workBuffer[3] = '\0';
    int actPacketNum = atoi(workBuffer);

    memcpy(workBuffer, receivedData + 8, 4);
    workBuffer[4] = '\0';
    int sendInfo = atoi(workBuffer);

    int selectedLastPacketNum = (cmdChar == '3') ? lastPacketNum_3 : lastPacketNum;
                
    if (actPacketNum != selectedLastPacketNum)   // neglect double sended meesages
    {
      char oldChar = receivedData[6];

      sensor_1 = (uint32_t)((uint32_t)receivedData[16] | (uint32_t)receivedData[15] << 8 | (uint32_t)receivedData[14] << 16 | (uint32_t)receivedData[13] << 24);
      sensor_2 = (uint32_t)((uint32_t)receivedData[21] | (uint32_t)receivedData[20] << 8 | (uint32_t)receivedData[19] << 16 | (uint32_t)receivedData[18] << 24);                                        
      sensor_3 = (uint32_t)((uint32_t)receivedData[26] | (uint32_t)receivedData[25] << 8 | (uint32_t)receivedData[24] << 16 | (uint32_t)receivedData[23] << 24);
      sensor_3_Lower = (uint16_t)((uint32_t)receivedData[26] | (uint32_t)receivedData[25] << 8);
      sensor_3_Higher = (uint16_t)((uint32_t)receivedData[24] | (uint32_t)receivedData[23] << 8);
                   
      switch (senderID)
      {
          case SOLARPUMP_CURRENT_SENDER_ID :    
          {
            lastPacketNum = lastPacketNum == 0 ? actPacketNum -1 : lastPacketNum;
            if ((lastPacketNum + 1) < actPacketNum)
            {
              lastMessageMissed = true;
              missedPacketNums += (actPacketNum - lastPacketNum - 1);
            }
            else
            {
              lastMessageMissed = false;
            }
            lastPacketNum = actPacketNum;
            missedPacketNumToShow = missedPacketNums;

            switch (cmdChar)
              {
                case '0':             // comes from solar pump
                case '1':
                {
                  Serial.println("SolarPump event Sendr Id 2");
                  break;
                }
                case '2':             // comes from current sensor
                {

                float currentInFloat = ((float)sensor_1 / 100);
                String currentInString = String(currentInFloat, 2);
                Serial.print("Current: ");
                Serial.println(currentInString);

                float powerInFloat = ((float)sensor_2 / 100);
                String powerInString = String(powerInFloat, 2);
                Serial.print("Power: ");
                Serial.println(powerInString);
                            
                // convert to float
                int16_2_float_function_result workInFloat = reform_uint16_2_float32(sensor_3_Higher, sensor_3_Lower);
                Serial.print("Work: ");       
                Serial.println(workInFloat.value, 2);
                dateTimeUTCNow = sysTime.getTime();
                DateTime localTime = myTimezone.toLocal(dateTimeUTCNow.unixtime());

                if (localTime.day() != actDay)  // evera day calculate new minimum and maximum power values
                {
                  actDay = localTime.day();
                  powerDayMin = 50000;   // very high value
                  powerDayMax = 0;
                  workAtStartOfDay = workAtStartOfDay < 0.0001 ? workInFloat.value : workAtStartOfDay;
                  }
                    powerDayMin = powerInFloat < powerDayMin ? powerInFloat : powerDayMin;   // actualize day minimum power value
                    powerDayMax = powerInFloat > powerDayMax ? powerInFloat : powerDayMax;   // actualize day maximum power value
                    if (showPowerScreen)
                    {
                      if (!lastScreenWasPower)                             
                      {
                        lastScreenWasPower = true;
                        showDisplayFrame(POWER_SENSOR_01_LABEL, POWER_SENSOR_02_LABEL, POWER_SENSOR_03_LABEL, POWER_SENSOR_04_LABEL, TFT_BLACK, TFT_BLACK, TFT_DARKGREY);                                 
                      }
                      fillDisplayFrame(ValueType::Power, powerInFloat, workInFloat.value - workAtStartOfDay, powerDayMin, powerDayMax, false, false, false, false, lastMessageMissed);
                    }
                    Serial.printf("Missed Power-Messages: %d \r\n", missedPacketNums);
                    break;
                  }                                                      
                }
                break;
              }
          case SOLAR_TEMP_SENDER_ID :    // came from temp sensors
          {
            lastPacketNum_3 = lastPacketNum_3 == 0 ? actPacketNum -1 : lastPacketNum_3;
            if ((lastPacketNum_3 + 1) < actPacketNum)
            {
              lastMessageMissed = true;
              missedPacketNums_3 += (actPacketNum - lastPacketNum_3 - 1);
            }
            else
            {
              lastMessageMissed = false;
            }
            lastPacketNum_3 = actPacketNum;
            missedPacketNumToShow = missedPacketNums_3;

            switch (cmdChar)
            {                          
              case '3':           // came from temp sensors, cmdType 3
              {
                float collector_float = (float)(((float)sensor_1 / 10) - 70);
                String collectorInString = String(collector_float, 1);
                Serial.print("Collector: ");
                Serial.println(collectorInString);
                              
                float storage_float = (float)(((float)sensor_2 / 10) - 70);
                String storageInString = String(storage_float, 1);
                Serial.print("Storage: ");
                Serial.println(collectorInString);

                float water_float = (float)(((float)sensor_3 / 10) - 70);
                String waterInString = String(water_float, 1);
                Serial.print("Water: ");
                Serial.println(waterInString);
                if (!showPowerScreen)
                {
                  if (lastScreenWasPower)                             
                  {
                    lastScreenWasPower = false;
                    showDisplayFrame(TEMP_SENSOR_01_LABEL, TEMP_SENSOR_02_LABEL, TEMP_SENSOR_03_LABEL, TEMP_SENSOR_04_LABEL, TFT_BLACK, TFT_BLACK, TFT_DARKGREY);                                 
                  }
                  fillDisplayFrame(ValueType::Temperature, collector_float, storage_float, water_float, 999.9, false, false, false, false, lastMessageMissed);
                }
                Serial.printf("Missed Temp-Messages: %d \r\n", missedPacketNums_3);
                break;
              }                           
            }
          }
          break;
      }                                                                           
    }                  
                
    Serial.print("   [RX_RSSI:");Serial.print(rfm69.RSSI);Serial.print("]\r\n");

    //check condition if Ack should be sent (example: contains Hello World) (is here never the case, left only as an example)
    if (strstr((char *)rfm69.DATA, "Hello World"))
    {
      //check if sender wanted an ACK
      if (rfm69.ACKRequested())
      {       
          unsigned long Start = millis();
          while(millis() - Start < 50);    // A delay is needed for NETMF (50 ms ?)
                                           // if ACK comes to early it is missed by NETMF                          
          rfm69.sendACK();
          Serial.println(" - ACK sent");
      }
      //Blink(LED, 40, 3); //blink LED 3 times, 40ms between blinks
    }  
  }
  rfm69.receiveDone(); //put radio in RX mode
  Serial.flush(); //make sure all serial data is clocked out before sleeping the MCU

  //********   End of Rfm69 stuff   **************

  if (loopCounter++ % 10000 == 0)   // Do other things only every 10000 th round and toggle Led to signal that App is running
  {
    ledState = !ledState;
    digitalWrite(LED_BUILTIN, ledState);

    #if WORK_WITH_WATCHDOG == 1
      SAMCrashMonitor::iAmAlive();
    #endif

    
    // Update RTC from Ntp when ntpUpdateInterval has expired, retry after 20 sec if update was not successful
    if (((millis() - previousNtpUpdateMillis) >= ntpUpdateInterval) && ((millis() - previousNtpRequestMillis) >= 20000))  
    {      
        dateTimeUTCNow = sysTime.getTime();
        uint32_t actRtcTime = dateTimeUTCNow.secondstime();

        int loopCtr = 0;
        unsigned long  utcNtpTime = getNTPtime();   // Get NTP time, try up to 4 times        
        while ((loopCtr < 4) && utcTime == 0)
        {
          loopCtr++;
          utcTime = getNTPtime();
        }

        previousNtpRequestMillis = millis();
        
        if (utcNtpTime != 0 )       // if NTP request was successful --> synchronize RTC 
        {  
            previousNtpUpdateMillis = millis();     
            dateTimeUTCNow = utcNtpTime;
            sysTimeNtpDelta = actRtcTime - dateTimeUTCNow.secondstime();
            
            sysTime.setTime(dateTimeUTCNow);
            timeNtpUpdateCounter++;   
        }  
      }
      else            // it was not NTP Update, proceed with send to analog table or On/Off-table
      {
        dateTimeUTCNow = sysTime.getTime();
      }
      
      // Time to reduce backlight has expired ?
      if ((dateTimeUTCNow.operator-(DisplayOnTime)).minutes() > SCREEN_OFF_TIME_MINUTES)
      {
        
        DisplayOnTime = dateTimeUTCNow;
        if ((dateTimeUTCNow.operator-(DisplayOffTime)).minutes() > SCREEN_DARK_TIME_MINUTES)
        {
          backLight.setBrightness(maxBrightness / 40);          
        }
        else
        {
          backLight.setBrightness(maxBrightness / 20);         
        }


      }
      
      // every second check if a minute is elapsed, then actualize the time on the display
      if (millis() - previousDisplayTimeUpdateMillis > 1000)
      {
        DateTime localTime = myTimezone.toLocal(dateTimeUTCNow.unixtime());
    
        char buf[35] = "DDD, DD MMM hh:mm:ss";
        char lineBuf[40] {0};
    
        localTime.toString(buf);
  
        uint8_t actMinute = localTime.minute();
        if (lastDateOutputMinute != actMinute)
        {
          lastDateOutputMinute = actMinute;
          TimeSpan onTime = dateTimeUTCNow.operator-(BootTimeUtc);
          buf[strlen(buf) - 3] =  (char)'\0';       
          current_text_line = 10;
          sprintf(lineBuf, "%s  On: %d:%02d:%02d  f:%2d", buf, onTime.days(), onTime.hours(), onTime.minutes(), missedPacketNumToShow);
         
          lcd_log_line(lineBuf);
        }
      }
      
      // if 5-way button is presses longer than 2 sec, toggle display (power/temperature)
      if (digitalRead(WIO_5S_PRESS) == LOW)    // Toggle between power screen and temp screen
      {
        uint32_t startTime = millis(); 
        // Wait until button released
        while(digitalRead(WIO_5S_PRESS) == LOW);

        backLight.setBrightness (maxBrightness);
        DisplayOnTime = dateTimeUTCNow;

        if ((millis() - startTime) > 2000)
        {         
          if (showPowerScreen)
          {
            showPowerScreen = false;
            showDisplayFrame(TEMP_SENSOR_01_LABEL, TEMP_SENSOR_02_LABEL, TEMP_SENSOR_03_LABEL, TEMP_SENSOR_04_LABEL, TFT_BLACK, TFT_BLACK, TFT_DARKGREY);        
          }
          else
          {
            showPowerScreen = true;
            showDisplayFrame(POWER_SENSOR_01_LABEL, POWER_SENSOR_02_LABEL, POWER_SENSOR_03_LABEL, POWER_SENSOR_04_LABEL, TFT_BLACK, TFT_BLACK, TFT_DARKGREY);        
          }
          fillDisplayFrame(ValueType::Power, 999.9, 999.9, 999.9, 999.9, false, false, false, false, false);
        }
        if ((dateTimeUTCNow.operator-(DisplayOffTime)).minutes() > SCREEN_DARK_TIME_MINUTES)
        {
          runWakeUpPerformance();       
        }
        DisplayOffTime = dateTimeUTCNow;
      }
  }
  else     // listen for noise in background to set backlight to max brightness
  {
    if (loopCounter % 200 == 0)    // read microphone every 200 th round
    {
      NoiseValue = analogRead(WIO_MIC);
      NoiseSum = NoiseSum - NoiseReadings[AverWinIndex];
      NoiseReadings[AverWinIndex] = NoiseValue;
      NoiseSum += NoiseValue;
      AverWinIndex = (AverWinIndex + 1) % Average_Window_Size;
      NoiseAverage = NoiseSum / Average_Window_Size;

      if (abs(NoiseValue - NoiseAverage) >  (NoiseAverage / 4))  // Set to max brightness if threshold exceeded
      {
        backLight.setBrightness (maxBrightness);
        DisplayOnTime = dateTimeUTCNow;
        if ((dateTimeUTCNow.operator-(DisplayOffTime)).minutes() > SCREEN_DARK_TIME_MINUTES)
        {
          runWakeUpPerformance();
        }
        DisplayOffTime = dateTimeUTCNow;

        Serial.print(NoiseAverage);
        Serial.print("  ");
        //Serial.print(absValue);
        Serial.print("  ");
        Serial.println(NoiseValue);
        volatile int dummy34 = 1;
      }  
    }
  }
}

void runWakeUpPerformance(){

  volatile int dummy8976 = 1;
  dummy8976++;

}

// To manage daylightsavingstime stuff convert input ("Last", "First", "Second", "Third", "Fourth") to int equivalent
int getWeekOfMonthNum(const char * weekOfMonth)
{
  for (int i = 0; i < 5; i++)
  {  
    if (strcmp((char *)timeZoneHelper.weekOfMonth[i], weekOfMonth) == 0)
    {
      return i;
    }   
  }
  return -1;
}

int getMonNum(const char * month)
{
  for (int i = 0; i < 12; i++)
  {  
    if (strcmp((char *)timeZoneHelper.monthsOfTheYear[i], month) == 0)
    {
      return i + 1;
    }   
  }
  return -1;
}

int getDayNum(const char * day)
{
  for (int i = 0; i < 7; i++)
  {  
    if (strcmp((char *)timeZoneHelper.daysOfTheWeek[i], day) == 0)
    {
      return i + 1;
    }   
  }
  return -1;
}

unsigned long getNTPtime() 
{
    // module returns a unsigned long time valus as secs since Jan 1, 1970 
    // unix time or 0 if a problem encounted
    //only send data when connected
    if (WiFi.status() == WL_CONNECTED) 
    {
        //initializes the UDP state
        //This initializes the transfer buffer
        udp.begin(WiFi.localIP(), localPort);
 
        sendNTPpacket(timeServer); // send an NTP packet to a time server
        // wait to see if a reply is available
     
        delay(1000);
        
        if (udp.parsePacket()) 
        {
            // We've received a packet, read the data from it
            udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer
 
            //the timestamp starts at byte 40 of the received packet and is four bytes,
            // or two words, long. First, extract the two words:
 
            unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
            unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
            // combine the four bytes (two words) into a long integer
            // this is NTP time (seconds since Jan 1 1900):
            unsigned long secsSince1900 = highWord << 16 | lowWord;
            // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
            const unsigned long seventyYears = 2208988800UL;
            // subtract seventy years:
            unsigned long epoch = secsSince1900 - seventyYears;
 
            // adjust time for timezone offset in secs +/- from UTC
            // WA time offset from UTC is +8 hours (28,800 secs)
            // + East of GMT
            // - West of GMT

            // RoSchmi: inactivated timezone offset
            // long tzOffset = 28800UL;
            long tzOffset = 0UL;
         
            unsigned long adjustedTime;
            return adjustedTime = epoch + tzOffset;
        }
        else {
            // were not able to parse the udp packet successfully
            // clear down the udp connection
            udp.stop();
            return 0; // zero indicates a failure
        }
        // not calling ntp time frequently, stop releases resources
        udp.stop();
    }
    else 
    {
        // network not connected
        return 0;
    }
 
}
 
// send an NTP request to the time server at the given address
unsigned long sendNTPpacket(const char* address) {
    // set all bytes in the buffer to 0
    for (int i = 0; i < NTP_PACKET_SIZE; ++i) {
        packetBuffer[i] = 0;
    }
    // Initialize values needed to form NTP request
    // (see URL above for details on the packets)
    packetBuffer[0] = 0b11100011;   // LI, Version, Mode
    packetBuffer[1] = 0;     // Stratum, or type of clock
    packetBuffer[2] = 6;     // Polling Interval
    packetBuffer[3] = 0xEC;  // Peer Clock Precision
    // 8 bytes of zero for Root Delay & Root Dispersion
    packetBuffer[12] = 49;
    packetBuffer[13] = 0x4E;
    packetBuffer[14] = 49;
    packetBuffer[15] = 52;
 
    // all NTP fields have been given values, now
    // you can send a packet requesting a timestamp:
    udp.beginPacket(address, 123); //NTP requests are to port 123
    udp.write(packetBuffer, NTP_PACKET_SIZE);
    udp.endPacket();
    return 0;
}

void showDisplayFrame(char * label_01, char * label_02, char * label_03, char * label_04, uint32_t screenCol, uint32_t backCol, uint32_t textCol)
{ 
    tft.fillScreen(screenCol);
    backColor = backCol; 
    textColor = textCol;
    textFont = FSSB9;  
    
    char line[35]{0};
    char label_left[15] {0};
    
    strncpy(label_left, label_01, 13);
    char label_right[15] {0};
    strncpy(label_right, label_02, 13);
    int32_t gapLength_1 = (13 - strlen(label_left)) / 2;
    int32_t gapLength_2 = (13 - strlen(label_right)) / 2; 
    sprintf(line, "%s%s%s%s%s%s%s ", spacesArray[3], spacesArray[(int)(gapLength_1 * 1.7)], label_left, spacesArray[(int)(gapLength_1 * 1.7)], spacesArray[5], spacesArray[(int)(gapLength_2 * 1.7)], label_right);
    current_text_line = 1;
    lcd_log_line((char *)line, textColor, backColor, screenColor);

    strncpy(label_left, label_03, 13); 
    strncpy(label_right, label_04, 13);
    gapLength_1 = (13 - strlen(label_left)) / 2; 
    gapLength_2 = (13 - strlen(label_right)) / 2;
    sprintf(line, "%s%s%s%s%s%s%s ", spacesArray[3], spacesArray[(int)(gapLength_1 * 1.7)], label_left, spacesArray[(int)(gapLength_1 * 1.7)], spacesArray[5], spacesArray[(int)(gapLength_2 * 1.7)], label_right);
    current_text_line = 6;
    lcd_log_line((char *)line, textColor, backColor, screenColor);
    current_text_line = 10;   
    line[0] = '\0';   
    lcd_log_line((char *)line, textColor, backColor, screenColor);
}

void fillDisplayFrame(ValueType valueType, double an_1, double an_2, double an_3, double an_4, bool on_1,  bool on_2, bool on_3, bool on_4, bool pLastMessageMissed)
{
    static TFT_eSprite spr = TFT_eSprite(&tft);

    static uint8_t lastDateOutputMinute = 60;

    an_1 = constrain(an_1, -999.9, 50000.0);
    an_2 = constrain(an_2, -999.9, 50000.0);
    an_3 = constrain(an_3, -999.9, 50000.0);
    an_4 = constrain(an_4, -999.9, 50000.0);

    char lineBuf[40] {0};

    char valueStringArray[4][8] = {{0}, {0}, {0}, {0}};

    
    if (valueType == ValueType::Temperature)
    {
      sprintf(valueStringArray[0], "%.1f", SENSOR_1_FAHRENHEIT == 1 ?  an_1 * 9 / 5 + 32 :  an_1 );
      sprintf(valueStringArray[1], "%.1f", SENSOR_2_FAHRENHEIT == 1 ?  an_1 * 9 / 5 + 32 :  an_2 );
      sprintf(valueStringArray[2], "%.1f", SENSOR_3_FAHRENHEIT == 1 ?  an_1 * 9 / 5 + 32 :  an_3 );
      sprintf(valueStringArray[3], "%.1f", SENSOR_4_FAHRENHEIT == 1 ?  an_1 * 9 / 5 + 32 :  an_4 );
    }
    else
    {
      sprintf(valueStringArray[0], "%.1f", an_1);
      sprintf(valueStringArray[1], "%.2f", an_2);
      sprintf(valueStringArray[2], "%.1f", an_3);
      sprintf(valueStringArray[3], "%.1f", an_4);
    }
    
    for (int i = 0; i < 4; i++)
    {
        // 999.9 is invalid, 1831.8 is invalid when tempatures are expressed in Fahrenheit
        
        if ( strcmp(valueStringArray[i], "999.9") == 0 || strcmp(valueStringArray[i], "999.90") == 0 || strcmp(valueStringArray[i], "1831.8") == 0)
        {
            strcpy(valueStringArray[i], "--.-");
        }
        
    }

    int charCounts[4];
    charCounts[0] = 6 - strlen(valueStringArray[0]);
    charCounts[1] = 6 - strlen(valueStringArray[1]);
    charCounts[2] = 6 - strlen(valueStringArray[2]);
    charCounts[3] = 6 - strlen(valueStringArray[3]);
    
    spr.createSprite(120, 30);

    spr.setTextColor(TFT_ORANGE);
    spr.setFreeFont(FSSBO18);
    
    for (int i = 0; i <4; i++)
    {
      spr.fillSprite(TFT_DARKGREEN);
      sprintf(lineBuf, "%s%s", spacesArray[charCounts[i]], valueStringArray[i]);
      spr.drawString(lineBuf, 0, 0);
      switch (i)
      {
        case 0: {spr.pushSprite(25, 54); break;}
        case 1: {spr.pushSprite(160, 54); break;}
        case 2: {spr.pushSprite(25, 138); break;}
        case 3: {spr.pushSprite(160, 138); break;}
      }     
    }
    
    dateTimeUTCNow = sysTime.getTime();

    myTimezone.utcIsDST(dateTimeUTCNow.unixtime());

    int timeZoneOffsetUTC = myTimezone.utcIsDST(dateTimeUTCNow.unixtime()) ? TIMEZONEOFFSET + DSTOFFSET : TIMEZONEOFFSET;
    
    DateTime localTime = myTimezone.toLocal(dateTimeUTCNow.unixtime());
    
    char buf[35] = "DDD, DD MMM YYYY hh:mm:ss GMT";
    
    localTime.toString(buf);
  
    
    volatile uint8_t actMinute = localTime.minute();
    if (lastDateOutputMinute != actMinute)
    {
      lastDateOutputMinute = actMinute;     
       current_text_line = 10;
       sprintf(lineBuf, "%s", (char *)buf);

       lineBuf[strlen(lineBuf) -3] = (char)'\0';
       lcd_log_line(lineBuf);
    }

    // Show signal circle on the screen, showing if no message was missed (green) or not (red)
    if (pLastMessageMissed)
    {
      tft.fillCircle(300, 9, 8, TFT_RED);              
    }
    else
    {
      tft.fillCircle(300, 9, 8, TFT_DARKGREEN);           
    }       

    tft.fillRect(16, 12 * LCD_LINE_HEIGHT, 60, LCD_LINE_HEIGHT, on_1 ? TFT_RED : TFT_DARKGREY);
    tft.fillRect(92, 12 * LCD_LINE_HEIGHT, 60, LCD_LINE_HEIGHT, on_2 ? TFT_RED : TFT_DARKGREY);
    tft.fillRect(168, 12 * LCD_LINE_HEIGHT, 60, LCD_LINE_HEIGHT, on_3 ? TFT_RED : TFT_DARKGREY);
    tft.fillRect(244, 12 * LCD_LINE_HEIGHT, 60, LCD_LINE_HEIGHT, on_4 ? TFT_RED : TFT_DARKGREY);
}

