/* Host test for app/timeutil.c */
#include <stdio.h>
#include "timeutil.h"

int main(void)
{
    int ok = 1;
    datetime_t d;
    unix_to_datetime(1791438482, &d);          /* 2026-10-08 05:48:02 UTC, a Thursday */
    ok &= d.year == 2026 && d.month == 10 && d.day == 8 && d.hour == 5 && d.minute == 48 && d.second == 2 && d.weekday == 4;
    unix_to_datetime(951782400, &d);           /* 2000-02-29 00:00:00, Tuesday */
    ok &= d.year == 2000 && d.month == 2 && d.day == 29 && d.weekday == 2;
    for (long long t = 946684800; t < 4102444800LL; t += 86400 * 37 + 3601) {
        unix_to_datetime(t, &d);
        if (datetime_to_unix(&d) != t) { ok = 0; printf("round trip failed at %lld\n", t); break; }
    }
    printf(ok ? "PASS\n" : "FAIL\n");
    return !ok;
}
