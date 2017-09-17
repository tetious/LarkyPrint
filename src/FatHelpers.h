#pragma once


#include "Arduino.h"

#define JD_1_JAN_1970 2440588 // 1 Jan 1970 in Julian day number

static time_t gdate_to_jdays(int day, int month, int year) {
    return static_cast<time_t>(day - 32075 + 1461 * (year + 4800 + (month - 14) / 12) / 4 +
               367 * (month - 2 - (month - 14) / 12 * 12) / 12 -
               3 * ((year + 4900 + (month - 14) / 12) / 100) / 4);
}
static time_t date_dos2unix(uint16_t dos_time, uint16_t dos_date) {

    int hour, min, sec;
    int day, month, year;

    sec = (dos_time & ((1 << 5) - 1)) * 2;
    dos_time >>= 5;
    min = (dos_time & ((1 << 6) - 1));
    dos_time >>= 6;
    hour = dos_time;

    day = (dos_date & ((1 << 5) - 1));
    dos_date >>= 5;
    month = (dos_date & ((1 << 4) - 1));
    dos_date >>= 4;
    year = dos_date + 1980;

    time_t ts = gdate_to_jdays(day, month, year);

    ts -= JD_1_JAN_1970;
    ts = (ts * 24 * 3600) + (sec + min * 60 + hour * 3600);

    return ts;
}