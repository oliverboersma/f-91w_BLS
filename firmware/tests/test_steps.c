/* Host test for app/stepcount.c on synthetic wrist accelerometer data. */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "stepcount.h"

static float frand(void) { return (float)rand() / RAND_MAX * 2.0f - 1.0f; }

/* walk at cadence spm for secs; amp is the vertical bounce in g; noise in g */
static uint32_t run(float fs, float spm, float secs, float amp, float noise, int gestures)
{
    stepcount_t s;
    stepcount_init(&s, fs);
    float f = spm / 60.0f;
    int n = (int)(secs * fs);
    for (int i = 0; i < n; i++) {
        float t = i / fs;
        float v = 0.0f;
        if (amp > 0) v = amp * sinf(2 * 3.14159265f * f * t) + 0.3f * amp * sinf(4 * 3.14159265f * f * t + 1.0f);
        /* a few isolated jolts (door knock, hand wave) */
        if (gestures && (i % (int)(7 * fs)) < (int)(0.6f * fs)) v += 0.5f * sinf(2 * 3.14159265f * 3.0f * t);
        float ax = 0.1f * sinf(2 * 3.14159265f * f / 2 * t) + noise * frand();
        float ay = noise * frand();
        float az = 1.0f + v + noise * frand();
        stepcount_push(&s, ax, ay, az);
    }
    return s.steps;
}

static int check(const char *name, uint32_t got, float want, float tol)
{
    int ok = fabsf((float)got - want) <= tol;
    printf("%-34s got %4u want %6.1f +- %4.1f  %s\n", name, (unsigned)got, want, tol, ok ? "ok" : "FAIL");
    return ok;
}

int main(void)
{
    int ok = 1;
    srand(1);
    ok &= check("walk 110 spm, 60 s, 12.5 Hz",   run(12.5f, 110, 60, 0.25f, 0.02f, 0), 110, 110 * 0.05f);
    ok &= check("walk 100 spm, 60 s, 25 Hz",     run(25.0f, 100, 60, 0.20f, 0.03f, 0), 100, 100 * 0.05f);
    ok &= check("slow walk 70 spm, 60 s",        run(12.5f,  70, 60, 0.15f, 0.02f, 0),  70,  70 * 0.07f);
    ok &= check("run 170 spm, 60 s",             run(12.5f, 170, 60, 0.60f, 0.05f, 0), 170, 170 * 0.05f);
    ok &= check("desk: noise only, 120 s",       run(12.5f, 100, 120, 0.0f, 0.02f, 0),   0, 0);
    ok &= check("desk: isolated gestures, 120 s",run(12.5f, 100, 120, 0.0f, 0.02f, 1),   0, 0);
    printf(ok ? "PASS\n" : "FAIL\n");
    return ok ? 0 : 1;
}
