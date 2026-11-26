/* Calendar helpers (proleptic Gregorian, valid 2000-2099 for the RTC). */
#ifndef TIMEUTIL_H
#define TIMEUTIL_H
#include <stdint.h>

typedef struct {
    uint16_t year;      /* 2000-2099 */
    uint8_t  month;     /* 1-12 */
    uint8_t  day;       /* 1-31 */
    uint8_t  hour, minute, second;
    uint8_t  weekday;   /* 1 = Monday ... 7 = Sunday (RTC convention) */
} datetime_t;

void     unix_to_datetime(int64_t t, datetime_t *dt);
int64_t  datetime_to_unix(const datetime_t *dt);
#endif
