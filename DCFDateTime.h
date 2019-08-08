// Code by JeeLabs http://news.jeelabs.org/code/
// Released to the public domain! Enjoy!
#include "Arduino.h"

#ifndef DCFDateTime_H
#define DCFDateTime_H

class DCFDateTime;

byte GetMoonPhase(DCFDateTime aDate);
DCFDateTime GetSunRise(DCFDateTime aDate);
//DCFDateTime GetSunSet(DCFDateTime aDate);
//DCFDateTime HoursMinutes(float aTime);
int DiffinDays(DCFDateTime aDate1, DCFDateTime aDate2);
uint16_t date2days(uint16_t y, uint8_t m, uint8_t d);
bool IsDst(DCFDateTime aDate);
bool IsDst(uint16_t aY, uint8_t aM, uint8_t aD);

// Simple general-purpose date/time class (no TZ / DST / leap second handling!)
class DCFDateTime {
public:
    DCFDateTime ();
    DCFDateTime (uint16_t year, uint8_t month, uint8_t day, uint8_t hour =0, uint8_t min =0);
    DCFDateTime (const char* date, const char* time);
    void Set (uint16_t year, uint8_t month, uint8_t day, uint8_t hour =0, uint8_t min =0);
    void Clear();
    bool IsValid();

    bool operator==(const DCFDateTime theOther);
    bool operator!=(const DCFDateTime theOther);

    const char* GetTimeStr( const char* separator=":");
    const char* GetDateStr();
    
    uint16_t year() const       { return 2000 + yOff; }
    uint8_t dayOfWeek() const;

    // 32-bit times as seconds since 1/1/2000
    long secondstime() const;   
    // 32-bit times as seconds since 1/1/1970
    //uint32_t unixtime(void) const;

    uint8_t yOff, m, d, hh, mm;
};



#endif
