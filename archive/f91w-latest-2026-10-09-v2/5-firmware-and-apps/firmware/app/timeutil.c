#include "timeutil.h"

/* Howard Hinnant's days_from_civil / civil_from_days */
static int64_t days_from_civil(int64_t y, unsigned m, unsigned d)
{
    y -= m <= 2;
    int64_t era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = (unsigned)(y - era * 400);
    unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (int64_t)doe - 719468;
}

void unix_to_datetime(int64_t t, datetime_t *dt)
{
    int64_t days = t >= 0 ? t / 86400 : -((-t + 86399) / 86400);
    int64_t secs = t - days * 86400;
    int64_t z = days + 719468;
    int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    unsigned doe = (unsigned)(z - era * 146097);
    unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    unsigned mp = (5 * doy + 2) / 153;
    unsigned d = doy - (153 * mp + 2) / 5 + 1;
    unsigned m = mp < 10 ? mp + 3 : mp - 9;
    dt->year = (uint16_t)(yoe + era * 400 + (m <= 2));
    dt->month = (uint8_t)m;
    dt->day = (uint8_t)d;
    dt->hour = (uint8_t)(secs / 3600);
    dt->minute = (uint8_t)(secs / 60 % 60);
    dt->second = (uint8_t)(secs % 60);
    dt->weekday = (uint8_t)(((days % 7) + 7 + 3) % 7 + 1);   /* 1970-01-01 was a Thursday */
}

int64_t datetime_to_unix(const datetime_t *dt)
{
    return days_from_civil(dt->year, dt->month, dt->day) * 86400
         + dt->hour * 3600 + dt->minute * 60 + dt->second;
}
