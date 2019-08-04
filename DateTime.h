// Code by JeeLabs http://news.jeelabs.org/code/
// Released to the public domain! Enjoy!
#include "Arduino.h"

#ifndef DATETIME_H
#define DATETIME_H

class DateTime;

byte GetMoonPhase(DateTime aDate);
DateTime GetSunRise(DateTime aDate);
DateTime GetSunSet(DateTime aDate);
DateTime HoursMinutes(float aTime);
int DiffinDays(DateTime aDate1, DateTime aDate2);
uint16_t date2days(uint16_t y, uint8_t m, uint8_t d);
bool IsDst(DateTime aDate);
bool IsDst(uint16_t aY, uint8_t aM, uint8_t aD);

// Simple general-purpose date/time class (no TZ / DST / leap second handling!)
class DateTime {
public:
    DateTime ();
    DateTime (uint16_t year, uint8_t month, uint8_t day, uint8_t hour =0, uint8_t min =0);
    DateTime (const char* date, const char* time);
    void Set (uint16_t year, uint8_t month, uint8_t day, uint8_t hour =0, uint8_t min =0);
    void Clear();
    bool IsValid();

    bool operator==(const DateTime theOther);
    bool operator!=(const DateTime theOther);

    const char* GetTimeStr();
    const char* GetDateStr();
    
    uint16_t year() const       { return 2000 + yOff; }
    uint8_t dayOfWeek() const;

    // 32-bit times as seconds since 1/1/2000
    long secondstime() const;   
    // 32-bit times as seconds since 1/1/1970
    uint32_t unixtime(void) const;

    uint8_t yOff, m, d, hh, mm;
};



#endif
