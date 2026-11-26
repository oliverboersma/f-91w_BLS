/*
 * stepcount: portable step counter for a wrist accelerometer (LIS2DW12 at 12.5-25 Hz).
 *
 * |a| -> remove gravity (1 s mean) -> light low-pass -> peak picking with an adaptive threshold.
 * A peak only counts as a step if it is 0.25-2.0 s after the previous one, and steps are only
 * credited once STEPCOUNT_CONFIRM regular steps have happened in a row, so a hand wave or a
 * door knock does not add steps.
 */
#ifndef STEPCOUNT_H
#define STEPCOUNT_H

#include <stdint.h>

#define STEPCOUNT_CONFIRM   6       /* regular steps needed before a walk is credited */
#define STEPCOUNT_MIN_G     0.06f   /* smallest peak (g above the mean) that can be a step */

typedef struct {
    float    fs;
    float    a_hp, a_lp;            /* filter coefficients */
    float    grav;                  /* running mean of |a| */
    float    y1, y2;                /* last two filtered samples */
    float    env;                   /* peak envelope for the adaptive threshold */
    uint32_t n;                     /* samples seen */
    uint32_t last_peak;             /* sample index of the last accepted peak */
    uint32_t min_gap, max_gap;      /* allowed step period, in samples */
    uint16_t run;                   /* regular steps in the current run */
    uint32_t steps;                 /* total credited steps */
} stepcount_t;

void     stepcount_init(stepcount_t *s, float fs);
/* Feed one sample in g. Returns the number of steps credited by this sample (usually 0 or 1). */
uint32_t stepcount_push(stepcount_t *s, float ax, float ay, float az);

#endif
