/* Host test: synthetic wrist-PPG (fundamental + 2nd harmonic, HRV, respiration baseline, drift,
 * white noise) at 25 sps; checks accuracy, noise rejection, motion and low-signal gating. */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "hr_spotcheck.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static unsigned long long rng = 88172645463325252ULL;
static double urand(void) { rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17; return (rng >> 11) * (1.0 / 9007199254740992.0); }
static double gauss(void) { double u = urand() + 1e-12, v = urand(); return sqrt(-2.0 * log(u)) * cos(2 * M_PI * v); }

static double FS = 25.0;
static void synth(hr_buffer_t *b, double bpm, double seconds, double pi, double noise_rel, double motion_g, double harm2) {
    const double fs = FS, dc = 100000.0;
    hr_init(b, (float)fs);
    double A = pi * dc;                     /* AC amplitude from perfusion index */
    double ph = urand() * 2 * M_PI, ph2 = urand() * 2 * M_PI, phase = 0.0;
    double resp = 0.2 + 0.15 * urand(), hrv = 0.03;
    int n = (int)(seconds * fs);
    for (int i = 0; i < n; i++) {
        double t = i / fs;
        double f = bpm / 60.0 * (1.0 + hrv * sin(2 * M_PI * 0.25 * t + ph));   /* HRV / RSA */
        phase += 2 * M_PI * f / fs;
        double ac = A * (sin(phase) + harm2 * sin(2 * phase + ph2));
        double base = dc * (1.0 + 0.004 * sin(2 * M_PI * resp * t) + 0.002 * t / seconds);
        double x = base - ac + noise_rel * A * gauss();
        double mo = motion_g * gauss();
        hr_push(b, (float)x, (float)mo);
    }
}

static hr_buffer_t buf;

int main(void) {
    hr_config_t cfg; hr_default_config(&cfg);
    int fails = 0;

    /* 1) sweep heart rates at moderate noise */
    printf("1) sweep (PI 0.3%%, noise 0.3xAC, 10 s):\n");
    double rates[] = {40, 45, 55, 60, 72, 85, 100, 120, 140, 160, 180, 200};
    for (unsigned i = 0; i < sizeof rates / sizeof rates[0]; i++) {
        synth(&buf, rates[i], 10.0, 0.003, 0.3, 0.0, 0.4);
        hr_result_t r = hr_estimate(&buf, &cfg);
        double err = r.bpm - rates[i];
        int ok = r.status == HR_OK && fabs(err) <= 3.0;
        printf("   true %5.1f  est %6.2f  conf %.2f  %s%s\n", rates[i], r.bpm, r.confidence, hr_status_str(r.status), ok ? "" : "   <-- FAIL");
        fails += !ok;
    }

    /* 2) Monte Carlo accuracy at several noise levels */
    double noises[] = {0.2, 0.5, 1.0};
    for (unsigned j = 0; j < 3; j++) {
        int trials = 300, okc = 0, within3 = 0, valid = 0; double sae = 0, maxe = 0;
        for (int t = 0; t < trials; t++) {
            double bpm = 45 + urand() * 135;
            synth(&buf, bpm, 10.0, 0.002 + 0.004 * urand(), noises[j], 0.0, 0.2 + 0.4 * urand());
            hr_result_t r = hr_estimate(&buf, &cfg);
            if (r.status == HR_OK) {
                valid++;
                double e = fabs(r.bpm - bpm); sae += e; if (e > maxe) maxe = e; if (e <= 3.0) within3++;
            }
            okc++;
        }
        printf("2) noise %.1fxAC: valid %3d/%d, of valid: MAE %.2f BPM, max %.1f, within 3 BPM %.1f%%\n",
               noises[j], valid, trials, valid ? sae / valid : 0, maxe, valid ? 100.0 * within3 / valid : 0);
        if (j == 0 && (valid < trials * 0.98 || within3 < valid * 0.98)) { printf("   <-- FAIL\n"); fails++; }
        if (j == 1 && (valid < trials * 0.90 || within3 < valid * 0.95)) { printf("   <-- FAIL\n"); fails++; }
        if (j == 2 && within3 < valid * 0.85) { printf("   <-- FAIL (valid readings must stay accurate)\n"); fails++; }
    }

    /* 2b) same at 50 sps (window must fit HR_MAX_SAMPLES: 8 s) */
    FS = 50.0;
    {
        int trials = 300, valid = 0, within3 = 0; double sae = 0, maxe = 0;
        for (int t = 0; t < trials; t++) {
            double bpm = 45 + urand() * 135;
            synth(&buf, bpm, 8.0, 0.002 + 0.004 * urand(), 1.0, 0.0, 0.2 + 0.4 * urand());
            hr_result_t r = hr_estimate(&buf, &cfg);
            if (r.status == HR_OK) { valid++; double e = fabs(r.bpm - bpm); sae += e; if (e > maxe) maxe = e; if (e <= 3.0) within3++; }
        }
        printf("2b) 50 sps, 8 s, noise 1.0xAC: valid %3d/%d, MAE %.2f BPM, max %.1f, within 3 BPM %.1f%%\n",
               valid, trials, valid ? sae / valid : 0, maxe, valid ? 100.0 * within3 / valid : 0);
        if (within3 < valid * 0.9) { printf("   <-- FAIL\n"); fails++; }
    }
    FS = 25.0;

    /* 3) pure noise must not produce confident readings */
    int fp = 0, trials = 300;
    for (int t = 0; t < trials; t++) {
        synth(&buf, 70, 10.0, 0.003, 0.0, 0.0, 0.0);              /* build timing/DC */
        for (int i = 0; i < buf.n; i++) buf.ppg[i] = 100000.0f + 300.0f * (float)gauss();
        hr_result_t r = hr_estimate(&buf, &cfg);
        fp += r.status == HR_OK;
    }
    printf("3) white noise only: %d/%d false 'ok' readings\n", fp, trials);
    if (fp > trials * 0.05) { printf("   <-- FAIL\n"); fails++; }

    /* 4) gating */
    synth(&buf, 75, 10.0, 0.003, 0.3, 0.2, 0.4);
    hr_result_t r = hr_estimate(&buf, &cfg);
    printf("4) motion 0.2 g rms -> %s\n", hr_status_str(r.status)); fails += r.status != HR_MOTION;
    synth(&buf, 75, 10.0, 0.00005, 0.3, 0.0, 0.4);
    r = hr_estimate(&buf, &cfg);
    printf("   PI 0.005%% -> %s (PI %.5f)\n", hr_status_str(r.status), r.perfusion_index); fails += r.status != HR_LOW_SIGNAL;
    synth(&buf, 75, 4.0, 0.003, 0.3, 0.0, 0.4);
    r = hr_estimate(&buf, &cfg);
    printf("   4 s window -> %s\n", hr_status_str(r.status)); fails += r.status != HR_TOO_SHORT;

    printf("\n%s (%d failing checks)\n", fails ? "FAIL" : "PASS", fails);
    return fails ? 1 : 0;
}
