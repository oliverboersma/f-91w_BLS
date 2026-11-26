#include "stepcount.h"
#include <math.h>

void stepcount_init(stepcount_t *s, float fs)
{
    *s = (stepcount_t){0};
    s->fs = fs;
    s->a_hp = 1.0f - expf(-1.0f / fs);                 /* ~1 s gravity tracker */
    s->a_lp = 1.0f - expf(-2.0f * 3.14159265f * 3.5f / fs); /* ~3.5 Hz low-pass */
    s->grav = 1.0f;
    s->min_gap = (uint32_t)(0.25f * fs + 0.5f);
    s->max_gap = (uint32_t)(2.0f * fs + 0.5f);
    s->env = 2.0f * STEPCOUNT_MIN_G;
}

uint32_t stepcount_push(stepcount_t *s, float ax, float ay, float az)
{
    float m = sqrtf(ax * ax + ay * ay + az * az);
    uint32_t credited = 0;

    s->grav += (m - s->grav) * s->a_hp;
    float y = s->y1 + ((m - s->grav) - s->y1) * s->a_lp;

    /* y1 is a local maximum? */
    float thr = 0.5f * s->env;
    if (thr < STEPCOUNT_MIN_G) thr = STEPCOUNT_MIN_G;
    if (s->n >= 2 && s->y1 > s->y2 && s->y1 >= y && s->y1 > thr) {
        uint32_t idx = s->n - 1, gap = idx - s->last_peak;
        if (s->run == 0 || gap > s->max_gap) {
            s->run = 1;                     /* first peak of a new run */
            s->last_peak = idx;
        } else if (gap >= s->min_gap) {
            s->last_peak = idx;
            if (++s->run == STEPCOUNT_CONFIRM) credited = STEPCOUNT_CONFIRM;
            else if (s->run > STEPCOUNT_CONFIRM) credited = 1;
            if (s->run > 60000) s->run = STEPCOUNT_CONFIRM + 1;
        }
        /* peaks closer than min_gap are ringing on the same step: ignored */
        s->env += (s->y1 - s->env) * 0.25f;
    }
    s->env *= 1.0f - 0.2f / s->fs;          /* envelope decays over ~5 s */
    if (s->run && s->n - s->last_peak > s->max_gap) s->run = 0;

    s->y2 = s->y1;
    s->y1 = y;
    s->n++;
    s->steps += credited;
    return credited;
}
