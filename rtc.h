/*

  ESP32 RTC DS1307 implementation for the BLE Collector
  MIT License

  Copyright (c) 2018 tobozo

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.

  -----------------------------------------------------------------------------

  This driver was baked from different drivers (Adafruit and other forks) in
  order to support different time formats as inpout and output, and play well
  with other time libraries while supporting a standard syntax for easy
  substitution.

  It implements most methods from JeeLabs's library http://news.jeelabs.org/code/
  with added dependencies to PaulStoffregen's Time library  https://github.com/PaulStoffregen/Time/

*/
#include <TimeLib.h> // https://github.com/PaulStoffregen/Time
#include <Wire.h>


#define DS1307_ADDR 0x68 // I2C address



/*

  ESP32 DateTime wrapper for the BLE Collector
  MIT License

  Copyright (c) 2018 tobozo

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.

  -----------------------------------------------------------------------------

  This wrapper mainly exists to supply friend methods to the RTC Library based on JeeLabs's code http://news.jeelabs.org/code/
  The changes are added dependencies to PaulStoffregen's Time library  https://github.com/PaulStoffregen/Time/ 

*/

int8_t timeZone = 2;
int8_t minutesTimeZone = 0;
const char* NTP_SERVER = "europe.pool.ntp.org";
static bool RTCisRunning = false;
static bool ForceBleTime = false;
static bool HasBTTime = false;
// some date/time formats used in this app
const char* hhmmStringTpl = "  %02d:%02d  ";
static char hhmmString[13] = "  --:--  ";
const char* hhmmssStringTpl = "%02d:%02d:%02d";
static char hhmmssString[13] = "--:--:--"; 
const char* UpTimeStringTpl = "  %02d:%02d  ";
const char* UpTimeStringTplDays = "  %2d %s  ";
static char UpTimeString[32] = "  --:--  ";
const char* YYYYMMDD_HHMMSS_Tpl = "%04d-%02d-%02d %02d:%02d:%02d";
static char YYYYMMDD_HHMMSS_Str[32] = "YYYY-MM-DD HH:MM:SS";
static bool DayChangeTrigger = false;
static bool HourChangeTrigger = false;
int current_day = -1;
int current_hour = -1;


// helper
static uint8_t DateTimeConv2d(const char* p) {
  uint8_t v = 0;
  if ('0' <= *p && *p <= '9')
    v = *p - '0';
  return 10 * v + *++p - '0';
}

static bool TimeIsSet = false;

// Simple general-purpose date/time class (no TZ / DST / leap second handling!)
class DateTime {
  public:
    DateTime( uint32_t t=0 );
    DateTime( tmElements_t dateTimeNow );
    DateTime( uint16_t year, uint8_t month, uint8_t day,
                 uint8_t hour=0, uint8_t min=0, uint8_t sec=0 );
    DateTime( const char* date, const char* time );
    uint16_t year() const       { return 1970 + yOff; }
    uint8_t month() const       { return m; }
    uint8_t day() const         { return d; }
    uint8_t hour() const        { return hh; }
    uint8_t minute() const      { return mm; }
    uint8_t second() const      { return ss; }
    tmElements_t get_tm() const     { return tm; }
    uint32_t unixtime() const; // 32-bit times as seconds since 1/1/1970
    static uint32_t tm2unixtime(tmElements_t tm); // conversion utility
  protected:
    uint8_t yOff, m, d, hh, mm, ss;
    tmElements_t tm;
};

DateTime::DateTime(uint32_t unixtime) {
  breakTime(unixtime, tm);
  m = tm.Month;
  d = tm.Day;
  hh = tm.Hour;
  mm = tm.Minute;
  ss = tm.Second;
  yOff = tm.Year; // offset from 1970; 
};
DateTime::DateTime(tmElements_t dateTimeNow) {
  tm = dateTimeNow;
  m = tm.Month;
  d = tm.Day;
  hh = tm.Hour;
  mm = tm.Minute;
  ss = tm.Second;
  yOff = tm.Year; // offset from 1970; 
};
DateTime::DateTime(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t min, uint8_t sec) {
  if (year >= 1970)
      year -= 1970; // year to offset
  yOff = year;
  m = month;
  d = day;
  hh = hour;
  mm = min;
  ss = sec;
  tm = {ss, mm, hh, 0, d, m, yOff};
};
DateTime::DateTime (const char* date, const char* time) {
  // A convenient constructor for using "the compiler's time":
  //   DateTime now (__DATE__, __TIME__);
  // sample input: date = "Dec 26 2009", time = "12:34:56"
  yOff = DateTimeConv2d(date + 9) + 30; // 2000 offset to 1970 offset
  // Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec 
  switch (date[0]) {
      case 'J': m = date[1] == 'a' ? 1 : m = date[2] == 'n' ? 6 : 7; break;
      case 'F': m = 2; break;
      case 'A': m = date[2] == 'r' ? 4 : 8; break;
      case 'M': m = date[2] == 'r' ? 3 : 5; break;
      case 'S': m = 9; break;
      case 'O': m = 10; break;
      case 'N': m = 11; break;
      case 'D': m = 12; break;
  }
  d = DateTimeConv2d(date + 4);
  hh = DateTimeConv2d(time);
  mm = DateTimeConv2d(time + 3);
  ss = DateTimeConv2d(time + 6);
  tm = {ss, mm, hh, 0, d, m, yOff};
};
uint32_t DateTime::unixtime() const {
  return tm2unixtime( tm );
}
uint32_t DateTime::tm2unixtime(tmElements_t tm_)  {
  uint32_t unixtime = makeTime(tm_); // convert time elements into time_t
  return unixtime;
}



// for debug

void dumpTime(const char* message, DateTime dateTime) { 
   Serial.printf("[%s]: %04d-%02d-%02d %02d:%02d:%02d\n", 
    message,
    dateTime.year(),
    dateTime.month(),
    dateTime.day(),
    dateTime.hour(),
    dateTime.minute(),
    dateTime.second()
  );
}

void dumpTime(const char* message, tmElements_t tm) {
  Serial.printf("[%s]: %04d-%02d-%02d %02d:%02d:%02d\n", 
    message,
    tm.Year + 1970,
    tm.Month,
    tm.Day,
    tm.Hour,
    tm.Minute,
    tm.Second
  );
}

void dumpTime(const char* message, time_t epoch) {
  tmElements_t nowUnixDateTime;
  breakTime( epoch, nowUnixDateTime );
  dumpTime( message, nowUnixDateTime );
}







class BLE_RTC_DS1307 {
  public:
    static bool begin(uint8_t sdaPin=SDA, uint8_t sclPin=SCL);
    static void adjust(const tmElements_t& dt);
    static void adjust(const time_t& dt);
    static void adjust(const DateTime& dt);
    static time_t get(); // for use with setSyncProvider
    uint8_t isrunning(void);
    static tmElements_t now();
    static uint32_t unixtime();
};

int ZEROINT = 0;
static uint8_t BLE_RTC_bcd2bin (uint8_t val) { return val - 6 * (val >> 4); }
static uint8_t BLE_RTC_bin2bcd (uint8_t val) { return val + 6 * (val / 10); }


bool BLE_RTC_DS1307::begin(uint8_t sdaPin, uint8_t sclPin) {
  Wire.begin(sdaPin, sclPin);
  return true;
}

time_t BLE_RTC_DS1307::get()   // Aquire data from buffer and convert to time_t
{
  tmElements_t tm = now();
  if (tm.Second & 0x80) return 0; // clock is halted
  //if (read(tm) == false) return 0;
  return(makeTime(tm));
}


void BLE_RTC_DS1307::adjust(const DateTime& dt) {
  //dumpTime("RTC Will adjust from DateTime ", dt.get_tm());
  adjust( dt.get_tm() );
}
void BLE_RTC_DS1307::adjust(const time_t& dt) {
  tmElements_t dateTimeNow;
  breakTime(dt, dateTimeNow);
  //dumpTime("RTC Will adjust from time_t ", dateTimeNow );
  adjust( dateTimeNow );
}

uint32_t BLE_RTC_DS1307::unixtime() {
  return DateTime::tm2unixtime( now() );
}
uint8_t BLE_RTC_DS1307::isrunning(void) {
  Wire.beginTransmission(DS1307_ADDR);
  Wire.write(ZEROINT);  
  Wire.endTransmission();
  Wire.requestFrom(DS1307_ADDR, 1);
  uint8_t ss = Wire.read();
  return !(ss>>7);
}
void BLE_RTC_DS1307::adjust(const tmElements_t& dt) {
  //dumpTime("RTC Will adjust from tmElements_t ", dt );
  Wire.beginTransmission(DS1307_ADDR);
  Wire.write(ZEROINT);
  Wire.write(BLE_RTC_bin2bcd(dt.Second));
  Wire.write(BLE_RTC_bin2bcd(dt.Minute));
  Wire.write(BLE_RTC_bin2bcd(dt.Hour));
  Wire.write(BLE_RTC_bin2bcd(0));
  Wire.write(BLE_RTC_bin2bcd(dt.Day));
  Wire.write(BLE_RTC_bin2bcd(dt.Month));
  Wire.write(BLE_RTC_bin2bcd(dt.Year)); // 2000 to 1970 offset
  Wire.write(ZEROINT);
  Wire.endTransmission();
}
tmElements_t BLE_RTC_DS1307::now() {
  Wire.beginTransmission(DS1307_ADDR);
  Wire.write(ZEROINT);  
  Wire.endTransmission();
  Wire.requestFrom(DS1307_ADDR, 7);
  uint8_t ss = BLE_RTC_bcd2bin(Wire.read() & 0x7F);
  uint8_t mm = BLE_RTC_bcd2bin(Wire.read());
  uint8_t hh = BLE_RTC_bcd2bin(Wire.read());
  Wire.read();
  uint8_t d = BLE_RTC_bcd2bin(Wire.read());
  uint8_t m = BLE_RTC_bcd2bin(Wire.read());
  uint8_t y = BLE_RTC_bcd2bin(Wire.read()); // 2000 to 1970 offset
  return tmElements_t {ss, mm, hh, 0, d, m, y};
}
