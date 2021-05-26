//File: Wio_Terminal_Rfm69_Receiver_Eyes
// Copyright Roschmi 2021, License Apache V2
// Please obey the License agreements of the libraries included like Rfm69 and UncannyEyes

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

// ************ for EYES ****************
// Enable ONE of these #includes for the various eyes:
#include "defaultEye.h"     // Standard human-ish hazel eye



#define DISPLAY_DC       D3 // Data/command pin for BOTH displays
#define DISPLAY_RESET    D4 // Reset pin for BOTH displays
#define SELECT_L_PIN     WIO_KEY_C // LEFT  eye chip select pin
#define SELECT_R_PIN     WIO_KEY_A // RIGHT eye chip select pin


#define TRACKING         // If enabled, eyelid tracks pupil
#define IRIS_PIN         WIO_5S_PRESS // Photocell or potentiometer (else auto iris)
#define IRIS_PIN_FLIP    WIO_5S_PRESS // If set, reverse reading from dial/photocell
#define IRIS_SMOOTH      // If enabled, filter input from IRIS_PIN
#define IRIS_MIN         140 // Clip lower analogRead() range from IRIS_PIN
#define IRIS_MAX         260 // Clip upper "
#define WINK_L_PIN       WIO_5S_LEFT  // Pin for LEFT eye wink button
#define BLINK_PIN        WIO_5S_PRESS // in for blink button (BOTH eyes)
#define WINK_R_PIN       WIO_5S_RIGHT // Pin for RIGHT eye wink button
#define AUTOBLINK        // If enabled, eyes blink autonomously

// Probably don't need to edit any config below this line, -----------------
// unless building a single-eye project (pendant, etc.), in which case one
// of the two elements in the eye[] array further down can be commented out.
// Eye blinks are a tiny 3-state machine.  Per-eye allows winks + blinks.
#define NOBLINK 0     // Not currently engaged in a blink
#define ENBLINK 1     // Eyelid is currently closing
#define DEBLINK 2     // Eyelid is currently opening

#define ANIMATION_DURATION_MS 7000
uint32_t animationStartTime = 0;

bool animationShallRun = false;



typedef struct {
  int8_t   pin;       // Optional button here for indiv. wink
  uint8_t  state;     // NOBLINK/ENBLINK/DEBLINK
  int32_t  duration;  // Duration of blink state (micros)
  uint32_t startTime; // Time (micros) of last state change
} eyeBlink;

struct {
  uint8_t     cs;     // Chip select pin
  eyeBlink    blink;  // Current blink state
} eye[] = { SELECT_L_PIN, { WINK_L_PIN, NOBLINK },
            SELECT_R_PIN, { WINK_R_PIN, NOBLINK } 
};

#define NUM_EYES 2    // pcs eye : 1 = 1 eye, 2 = 2 eye
uint32_t fstart = 0;  // start time to improve frame rate calculation at startup



//*********************************************************************************************
// ******* IMPORTANT SETTINGS FOR RFM69- YOU MUST CHANGE/CONFIGURE TO FIT YOUR HARDWARE *************
//*********************************************************************************************

#define NETWORKID                   100  //the same on all nodes that talk to each other
#define NODEID                      1    // The unique identifier of this node
#define SOLARPUMP_CURRENT_SENDER_ID 2    // The node which sends current values and state changes of the solar pump
#define SOLAR_TEMP_SENDER_ID        3    // The node which sends temperature states of the solar plant
//#define RECEIVER                    2    // The recipient of packets (here: SOLARPUMP_CURRENT_SENDER_ID)

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

const int radioPacketLength = 27;
union Radiopackets
{
    char radiopacketPlus1[radioPacketLength + 1];
    char radiopacket[radioPacketLength];
};

Radiopackets packets;

uint8_t receivedData[62] {0};

int lastPacketNum = 0;
int lastPacketNum_3 = 0;

uint32_t missedPacketNums = 0;
uint32_t missedPacketNums_3 = 0;

int missedPacketNumToShow = 0;

RFM69 rfm69(RF69_SPI_CS, RF69_IRQ_PIN, IS_RFM69HCW);

PowerVM powerVM;    // not used in this App

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
uint32_t ntpUpdateInterval = 60000;      // Is overwritten by value defined in config.h
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
void sendRefreshDataCmd();
void runWakeUpAnimation();
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

  //-----------------------------------// BUTTON & 5 Way Swicth ---
  pinMode(WIO_KEY_A,    INPUT_PULLUP); // RIGHT
  pinMode(WIO_KEY_B,    INPUT_PULLUP); // CENTER
  pinMode(WIO_KEY_C,    INPUT_PULLUP); // LEFT
  pinMode(WIO_5S_UP,    INPUT_PULLUP); // 
  pinMode(WIO_5S_DOWN,  INPUT_PULLUP); // 
  pinMode(WIO_5S_LEFT,  INPUT_PULLUP); // 
  pinMode(WIO_5S_RIGHT, INPUT_PULLUP); // 
  pinMode(WIO_5S_PRESS, INPUT_PULLUP); // 
  //-----------------------------------// 

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
  //rfm69.encrypt(0);
  

  Serial.printf("Working at %i MHz", FREQUENCY==RF69_433MHZ ? 433 : FREQUENCY==RF69_868MHZ ? 868 : 915);
  
  

  runWakeUpAnimation();

  /*
  showDisplayFrame(POWER_SENSOR_01_LABEL, POWER_SENSOR_02_LABEL, POWER_SENSOR_03_LABEL, POWER_SENSOR_04_LABEL, TFT_BLACK, TFT_BLACK, TFT_DARKGREY);
  
  fillDisplayFrame(ValueType::Power, 999.9, 999.9, 999.9, 999.9, false, false, false, false, false);
  */
  delay(50);



  backLight.initialize();
  maxBrightness = backLight.getMaxBrightness();

  previousNtpUpdateMillis = millis();
  previousNtpRequestMillis = millis();

  
  
}

//*****************   End EYES   *********************************


// EYE-RENDERING FUNCTION --------------------------------------------------
#define BUFFER_SIZE 256 // 64 to 512 seems optimum = 30 fps for default eye
void drawEye( // Renders one eye.  Inputs must be pre-clipped & valid.
  // Use native 32 bit variables where possible as this is 10% faster!
  uint8_t  e,      // Eye array index; 0 or 1 for left/right：0/1 = left/right
  uint32_t iScale, // Scale factor for iris
  uint32_t scleraX,// First pixel X offset into sclera image：Center of right and left
  uint32_t scleraY,// First pixel Y offset into sclera image：Upper and lower centers
  uint32_t uT,     // Upper eyelid threshold value：Up and down
  uint32_t lT)     // Lower eyelid threshold value：Up and down
  {
   tft.startWrite();
  uint32_t screenX, screenY, scleraXsave;
  uint32_t screenX_; // add macsbug
  int32_t  irisX, irisY;
  uint32_t p, a;
  uint32_t d;
  uint32_t pixels = 0;
  uint16_t pbuffer[BUFFER_SIZE]; // This one needs to be 16 bit
  // Set up raw pixel dump to entire screen.  Although such writes can wrap
  // around automatically from end of rect back to beginning, the region is
  // reset on each frame here in case of an SPI glitch.
  if (e == 0){ tft.setAddrWindow (  1,0,128,128);} // set left  window
  if (e == 1){ tft.setAddrWindow (191,0,128,128);} // set right window
  // Now just issue raw 16-bit values for every pixel...
  scleraXsave = scleraX;
  irisY = scleraY - (SCLERA_HEIGHT - IRIS_HEIGHT ) / 2;  // add : rev.3 2019.06.26
  for(screenY=0; screenY<SCREEN_HEIGHT; screenY++, scleraY++, irisY++) {
    scleraX = scleraXsave;
    irisX = scleraXsave - (SCLERA_WIDTH - IRIS_WIDTH) / 2;
    //irisY = scleraY - (SCLERA_HEIGHT - IRIS_HEIGHT ) / 2;  // add : rev.3 2019.06.26
    for(screenX=0; screenX<SCREEN_WIDTH; screenX++, scleraX++, irisX++) {
      if (e == 1){screenX_ =                    screenX;}
      if (e == 0){screenX_ = SCREEN_WIDTH - 1 - screenX;}
      if((pgm_read_byte(lower+screenY * SCREEN_WIDTH + screenX_) <= lT) ||
         (pgm_read_byte(upper+screenY * SCREEN_WIDTH + screenX_) <= uT)){
        p = 0;  //Covered by eyelid
      } else if((irisY < 0) || (irisY >= IRIS_HEIGHT) ||
                (irisX < 0) || (irisX >= IRIS_WIDTH)) { // In sclera
        p = pgm_read_word(sclera + scleraY * SCLERA_WIDTH + scleraX);
      } else {                                          // Maybe iris...
        p = pgm_read_word(polar+irisY*IRIS_WIDTH+irisX);// Polar angle/dist
        d = (iScale * (p & 0x7F)) / 128;                // Distance (Y)
        if(d < IRIS_MAP_HEIGHT) {                       // Within iris area
          a = (IRIS_MAP_WIDTH * (p >> 7)) / 512;        // Angle (X)
          p = pgm_read_word(iris+d*IRIS_MAP_WIDTH+a);   // Pixel = iris
        } else {                                        // Not in iris
          p = pgm_read_word(sclera+scleraY*SCLERA_WIDTH+scleraX);//Pixel=clear
        }
      }
      *(pbuffer + pixels++) = p>>8 | p<<8;
      if (pixels >= BUFFER_SIZE){//yield();
          tft.pushColors((uint8_t*)pbuffer,pixels*2); pixels=0; //drawEye    
      }
    }
  }
   if (pixels) { tft.pushColors(pbuffer, pixels); pixels = 0;}
   tft.endWrite();
}

// EYE ANIMATION -----------------------------------------------------------
const uint8_t ease[] = { // Ease in/out curve for eye movements 3*t^2-2*t^3
    0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,  1,  2,  2,  2,  3,   // T
    3,  3,  4,  4,  4,  5,  5,  6,  6,  7,  7,  8,  9,  9, 10, 10,   // h
   11, 12, 12, 13, 14, 15, 15, 16, 17, 18, 18, 19, 20, 21, 22, 23,   // x
   24, 25, 26, 27, 27, 28, 29, 30, 31, 33, 34, 35, 36, 37, 38, 39,   // 2
   40, 41, 42, 44, 45, 46, 47, 48, 50, 51, 52, 53, 54, 56, 57, 58,   // A
   60, 61, 62, 63, 65, 66, 67, 69, 70, 72, 73, 74, 76, 77, 78, 80,   // l
   81, 83, 84, 85, 87, 88, 90, 91, 93, 94, 96, 97, 98,100,101,103,   // e
  104,106,107,109,110,112,113,115,116,118,119,121,122,124,125,127,   // c
  128,130,131,133,134,136,137,139,140,142,143,145,146,148,149,151,   // J
  152,154,155,157,158,159,161,162,164,165,167,168,170,171,172,174,   // a
  175,177,178,179,181,182,183,185,186,188,189,190,192,193,194,195,   // c
  197,198,199,201,202,203,204,205,207,208,209,210,211,213,214,215,   // o
  216,217,218,219,220,221,222,224,225,226,227,228,228,229,230,231,   // b
  232,233,234,235,236,237,237,238,239,240,240,241,242,243,243,244,   // s
  245,245,246,246,247,248,248,249,249,250,250,251,251,251,252,252,   // o
  252,253,253,253,254,254,254,254,254,255,255,255,255,255,255,255 }; // n

#ifdef AUTOBLINK
uint32_t timeOfLastBlink = 0L, timeToNextBlink = 0L;
#endif

void frame( // Process motion for a single frame of left or right eye
  uint32_t        iScale) {     // Iris scale (0-1023) passed in
  static uint32_t frames   = 0; // Used in frame rate calculation
  static uint8_t  eyeIndex = 0; // eye[] array counter
  int32_t         eyeX, eyeY;
  uint32_t        t = micros(); // Time at start of function
  //Serial.print((++frames * 1000) / (millis() - fstart));
  //Serial.println("fps");        // Show frame rate
  if(++eyeIndex >= NUM_EYES) eyeIndex = 0;// Cycle through eyes, 1 per call
  // Autonomous X/Y eye motion
  // Periodically initiates motion to a new random point, random speed,
  // holds there for random period until next motion.
  static boolean  eyeInMotion      = false;
  static int32_t  eyeOldX=512, eyeOldY=512, eyeNewX=512, eyeNewY=512;
  static uint32_t eyeMoveStartTime = 0L;
  static int32_t  eyeMoveDuration  = 0L;
  int32_t dt = t - eyeMoveStartTime;      // uS elapsed since last eye event
  if(eyeInMotion) {                       // Currently moving?
    if(dt >= eyeMoveDuration) {           // Time up?  Destination reached.
      eyeInMotion      = false;           // Stop moving
      eyeMoveDuration  = random(3000000L);// 0-3 sec stop
      eyeMoveStartTime = t;               // Save initial time of stop
      eyeX = eyeOldX = eyeNewX;           // Save position
      eyeY = eyeOldY = eyeNewY;
    } else { // Move time's not yet fully elapsed -- interpolate position
      int16_t e = ease[255 * dt / eyeMoveDuration] + 1;   // Ease curve
      eyeX = eyeOldX + (((eyeNewX - eyeOldX) * e) / 256); // Interp X
      eyeY = eyeOldY + (((eyeNewY - eyeOldY) * e) / 256); // and Y
    }
  } else {                                // Eye stopped
    eyeX = eyeOldX;
    eyeY = eyeOldY;
    if(dt > eyeMoveDuration) {            // Time up?  Begin new move.
      int16_t  dx, dy;
      uint32_t d;
      do {                                // Pick new dest in circle
        eyeNewX = random(1024);
        eyeNewY = random(1024);
        dx      = (eyeNewX * 2) - 1023;
        dy      = (eyeNewY * 2) - 1023;
      } while((d = (dx * dx + dy * dy)) > (1023 * 1023)); // Keep trying
      eyeMoveDuration  = random(50000,150000); // ~1/14sec
                       //random(72000,144000); // ~1/ 7sec
      eyeMoveStartTime = t;               // Save initial time of move
      eyeInMotion      = true;            // Start move on next frame
    }
  }

// Blinking
#ifdef AUTOBLINK
  // Similar to the autonomous eye movement above -- blink start times
  // and durations are random (within ranges).
  if((t - timeOfLastBlink) >= timeToNextBlink) { // Start new blink?
    timeOfLastBlink = t;
    uint32_t blinkDuration = random(36000, 72000); // ~1/28 - ~1/14 sec
    // Set up durations for both eyes (if not already winking)
    for(uint8_t e=0; e<NUM_EYES; e++) {
      if(eye[e].blink.state == NOBLINK) {
         eye[e].blink.state     = ENBLINK;
         eye[e].blink.startTime = t;
         eye[e].blink.duration  = blinkDuration;
      }
    }
    timeToNextBlink = blinkDuration * 3 + random(4000000);
  }
#endif

  if(eye[eyeIndex].blink.state) { // Eye currently blinking?
    // Check if current blink state time has elapsed
    if((t - eye[eyeIndex].blink.startTime) >= eye[eyeIndex].blink.duration){
      // Yes -- increment blink state, unless...
      if((eye[eyeIndex].blink.state == ENBLINK) && // Enblinking and...
        ((digitalRead(BLINK_PIN) == LOW) ||        // blink or wink held...
          digitalRead(eye[eyeIndex].blink.pin) == LOW)) {
        // Don't advance state yet -- eye is held closed instead
      } else { // No buttons, or other state...
        if(++eye[eyeIndex].blink.state > DEBLINK) {// Deblinking finished?
          eye[eyeIndex].blink.state = NOBLINK;     // No longer blinking
        } else { // Advancing from ENBLINK to DEBLINK mode
          eye[eyeIndex].blink.duration *= 2;//DEBLINK is 1/2 ENBLINK speed
          eye[eyeIndex].blink.startTime = t;
        }
      }
    }
  } else { // Not currently blinking...check buttons!
    if(digitalRead(BLINK_PIN) == LOW) {
      // Manually-initiated blinks have random durations like auto-blink
      uint32_t blinkDuration = random(36000, 72000);
      for(uint8_t e=0; e<NUM_EYES; e++) {
        if(eye[e].blink.state    == NOBLINK) {
           eye[e].blink.state     = ENBLINK;
           eye[e].blink.startTime = t;
           eye[e].blink.duration  = blinkDuration;
        }
      }
    } else if(digitalRead(eye[eyeIndex].blink.pin) == LOW) { // Wink!
      eye[eyeIndex].blink.state     = ENBLINK;
      eye[eyeIndex].blink.startTime = t;
      eye[eyeIndex].blink.duration  = random(45000, 90000);
    }
  }

  // Process motion, blinking and iris scale into renderable values
  // Iris scaling: remap from 0-1023 input to iris map height pixel units
  iScale = ((IRIS_MAP_HEIGHT + 1) * 1024) /
           (1024 - (iScale * (IRIS_MAP_HEIGHT - 1) / IRIS_MAP_HEIGHT));

  // Scale eye X/Y positions (0-1023) to pixel units used by drawEye()
  if (eyeIndex == 0){eyeX = map(eyeX,1023,0,0,SCLERA_WIDTH - 128);}// left
  if (eyeIndex == 1){eyeX = map(eyeX,0,1023,0,SCLERA_WIDTH - 128);}// right
  eyeY = map(eyeY, 0, 1023, 0, SCLERA_HEIGHT - 128);
  if(eyeIndex == 1) eyeX = (SCLERA_WIDTH - 128) - eyeX; // Mirrored display

  // Horizontal position is offset so that eyes are very slightly crossed
  // to appear fixated (converged) at a conversational distance.  Number
  // here was extracted from my posterior and not mathematically based.
  // I suppose one could get all clever with a range sensor, but for now...
  eyeX += 4;
  if(eyeX > (SCLERA_WIDTH - 128)) eyeX = (SCLERA_WIDTH - 128);

  // Eyelids are rendered using a brightness threshold image.  This same
  // map can be used to simplify another problem: making the upper eyelid
  // track the pupil (eyes tend to open only as much as needed -- e.g. look
  // down and the upper eyelid drops).  Just sample a point in the upper
  // lid map slightly above the pupil to determine the rendering threshold.
  static uint8_t uThreshold = 128;
  uint8_t        lThreshold, n;

#ifdef TRACKING
  int16_t sampleX = SCLERA_WIDTH  / 2 - (eyeX / 2), // Reduce X influence
          sampleY = SCLERA_HEIGHT / 2 - (eyeY + IRIS_HEIGHT / 4);
  // Eyelid is slightly asymmetrical, so two readings are taken, averaged
  if(sampleY < 0) n = 0;
  else n = (pgm_read_byte(upper + sampleY * SCREEN_WIDTH + sampleX) +
   pgm_read_byte(upper+sampleY*SCREEN_WIDTH+(SCREEN_WIDTH-1-sampleX)))/2;
  uThreshold = (uThreshold * 3 + n) / 4; // Filter/soften motion
  // Lower eyelid doesn't track the same way, but seems to be pulled upward
  // by tension from the upper lid.
  lThreshold = 254 - uThreshold;
#else // No tracking -- eyelids full open unless blink modifies them
  uThreshold = lThreshold = 0;
#endif
  // The upper/lower thresholds are then scaled relative to the current
  // blink position so that blinks work together with pupil tracking.
  if(eye[eyeIndex].blink.state) { // Eye currently blinking?
    uint32_t s = (t - eye[eyeIndex].blink.startTime);
    if(s >= eye[eyeIndex].blink.duration) s = 255;// At or past blink end
    else s = 255 * s / eye[eyeIndex].blink.duration; // Mid-blink
    s          = (eye[eyeIndex].blink.state == DEBLINK) ? 1 + s : 256 - s;
    n          = (uThreshold * s + 254 * (257 - s)) / 256;
    lThreshold = (lThreshold * s + 254 * (257 - s)) / 256;
  } else {
    n          = uThreshold;
  }
  // Pass all the derived values to the eye-rendering function:
  drawEye(eyeIndex, iScale, eyeX, eyeY, n, lThreshold);
}

// AUTONOMOUS IRIS SCALING (if no photocell or dial) -----------------------
#if !defined(IRIS_PIN) || (IRIS_PIN < 0)
// Autonomous iris motion uses a fractal behavior to similate both the major
// reaction of the eye plus the continuous smaller adjustments that occur.
uint16_t oldIris = (IRIS_MIN + IRIS_MAX) / 2, newIris;

void split( // Subdivides motion path into two sub-paths w/randimization
  int16_t  startValue, // Iris scale value (IRIS_MIN to IRIS_MAX) at start
  int16_t  endValue,   // Iris scale value at end
  uint32_t startTime,  // micros() at start
  int32_t  duration,   // Start-to-end time, in microseconds
  int16_t  range) {    // Allowable scale value variance when subdividing

  if(range >= 8) {     // Limit subdvision count, because recursion
    range    /= 2;     // Split range & time in half for subdivision,
    duration /= 2;     // then pick random center point within range:
    int16_t  midValue = (startValue + endValue - range)/2+random(range);
    uint32_t midTime  = startTime + duration;
    split(startValue, midValue, startTime, duration, range);//First half
    split(midValue  , endValue, midTime  , duration, range);//Second half
  } else {             // No more subdivisons, do iris motion...
    int32_t dt;        // Time (micros) since start of motion
    int16_t v;         // Interim value
    while((dt = (micros() - startTime)) < duration) {
      v = startValue + (((endValue - startValue) * dt) / duration);
      if(v < IRIS_MIN)      v = IRIS_MIN; // Clip just in case
      else if(v > IRIS_MAX) v = IRIS_MAX;
      frame(v);        // Draw frame w/interim iris scale value
    }
    volatile int dummy56 = 1;
    dummy56++;
  }
}
#endif // !IRIS_PIN


//*****************   End EYES   *********************************


  
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
                  //workAtStartOfDay = workAtStartOfDay < 0.0001 ? workInFloat.value : workAtStartOfDay;
                  workAtStartOfDay = workInFloat.value;

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
      else            // it was not NTP Update, proceed 
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
        //RoSchmi
        sendRefreshDataCmd();
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
          runWakeUpAnimation();       
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

      if (abs(NoiseValue - NoiseAverage) >  (NoiseAverage + NoiseAverage / 4))  // Set to max brightness if threshold exceeded
      {
        backLight.setBrightness (maxBrightness);
        DisplayOnTime = dateTimeUTCNow;
        if ((dateTimeUTCNow.operator-(DisplayOffTime)).minutes() > SCREEN_DARK_TIME_MINUTES)
        {
          runWakeUpAnimation();
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
    else    //show eyes 
    {
       if (animationShallRun)
       {
          #if defined(IRIS_PIN) && (IRIS_PIN >= 0)   // Interactive iris
          uint16_t v = 512; //analogRead(IRIS_PIN);// Raw dial/photocell reading
          #ifdef IRIS_PIN_FLIP
          v = 1023 - v;
          #endif
          v = map(v, 0, 1023, IRIS_MIN, IRIS_MAX); // Scale to iris range
          #ifdef IRIS_SMOOTH // Filter input (gradual motion)
          static uint16_t irisValue = (IRIS_MIN + IRIS_MAX) / 2;
          irisValue = ((irisValue * 15) + v) / 16;
          frame(irisValue);
          #else  // Unfiltered (immediate motion)
          frame(v);
          #endif // IRIS_SMOOTH
          #else  // Autonomous iris scaling -- invoke recursive function
          newIris = random(IRIS_MIN, IRIS_MAX);
          split(oldIris, newIris, micros(), 10000000L, IRIS_MAX - IRIS_MIN);
          oldIris = newIris;
          #endif // IRIS_PIN
          //if ((dateTimeUTCNow.operator-(animationStartTime)).totalseconds() > ANIMATION_DURATION_SECONDS)

          if ((millis() - animationStartTime) > ANIMATION_DURATION_MS)
          {
            animationShallRun = false;
            if (showPowerScreen)
            {
              showDisplayFrame(POWER_SENSOR_01_LABEL, POWER_SENSOR_02_LABEL, POWER_SENSOR_03_LABEL, POWER_SENSOR_04_LABEL, TFT_BLACK, TFT_BLACK, TFT_DARKGREY);
            }
            else
            {
               showDisplayFrame(TEMP_SENSOR_01_LABEL, TEMP_SENSOR_02_LABEL, TEMP_SENSOR_03_LABEL, TEMP_SENSOR_04_LABEL, TFT_BLACK, TFT_BLACK, TFT_DARKGREY);   
            }         
            fillDisplayFrame(ValueType::Power, 999.9, 999.9, 999.9, 999.9, false, false, false, false, false);
          }

       }
  
    
    }
  }
}

void sendRefreshDataCmd()
{
  sprintf(packets.radiopacket, "RefreshCurrentData");
  Serial.println(packets.radiopacket);
  rfm69.send(SOLARPUMP_CURRENT_SENDER_ID, packets.radiopacket, radioPacketLength, false);
}


void runWakeUpAnimation()
{
  tft.fillScreen(TFT_BLACK);
  uint8_t e = 0;
  fstart = millis()-1; // Subtract 1 to avoid divide by zero later
  animationStartTime = millis();
  animationShallRun = true;
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

