#include "hr_spotcheck.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef struct { float b0, b1, b2, a1, a2; } biquad_t;

/* RBJ cookbook 2nd-order Butterworth (Q = 1/sqrt2) high/low-pass via bilinear transform */
static biquad_t biquad_design(float fc, float fs, int highpass) {
    float w0 = 2.0f * (float)M_PI * fc / fs;
    float c = cosf(w0), s = sinf(w0);
    float alpha = s / (2.0f * 0.70710678f);
    float a0 = 1.0f + alpha;
    biquad_t q;
    if (highpass) {
        q.b0 = (1.0f + c) / 2.0f; q.b1 = -(1.0f + c); q.b2 = (1.0f + c) / 2.0f;
    } else {
        q.b0 = (1.0f - c) / 2.0f; q.b1 = 1.0f - c;    q.b2 = (1.0f - c) / 2.0f;
    }
    q.a1 = -2.0f * c; q.a2 = 1.0f - alpha;
    q.b0 /= a0; q.b1 /= a0; q.b2 /= a0; q.a1 /= a0; q.a2 /= a0;
    return q;
}

/* in-place forward pass, Direct Form II transposed, state primed to the first sample's steady state */
static void biquad_run(const biquad_t *q, float *x, int n, int reverse) {
    float z1 = 0.0f, z2 = 0.0f;
    int i0 = reverse ? n - 1 : 0, step = reverse ? -1 : 1;
    /* prime with steady-state for a constant input x0 (removes start-up transient) */
    float x0 = x[i0];
    float dc_gain = (q->b0 + q->b1 + q->b2) / (1.0f + q->a1 + q->a2);
    float y0 = dc_gain * x0;
    z1 = y0 - q->b0 * x0;
    z2 = q->b2 * x0 - q->a2 * y0;
    for (int k = 0, i = i0; k < n; k++, i += step) {
        float in = x[i];
        float out = q->b0 * in + z1;
        z1 = q->b1 * in - q->a1 * out + z2;
        z2 = q->b2 * in - q->a2 * out;
        x[i] = out;
    }
}

/* power of x at frequency f (Hz) - single-bin DFT via the Goertzel recurrence */
static float goertzel_power(const float *x, int n, float f, float fs) {
    float w = 2.0f * (float)M_PI * f / fs, c = 2.0f * cosf(w);
    float s1 = 0.0f, s2 = 0.0f;
    for (int i = 0; i < n; i++) {
        float s0 = x[i] + c * s1 - s2;
        s2 = s1; s1 = s0;
    }
    return (s1 * s1 + s2 * s2 - c * s1 * s2) / (float)n;
}

static int nearest_key_max(const float *r, int kmin, int kmax, float target, int tol) {
    int best = -1;
    int t = (int)(target + 0.5f);
    for (int k = t - tol; k <= t + tol; k++) {
        if (k < kmin || k > kmax) continue;
        if (r[k] > r[k - 1] && r[k] >= r[k + 1] && r[k] > 0.0f && (best < 0 || r[k] > r[best])) best = k;
    }
    return best;
}

static float refine_lag(const float *r, int k) {
    float y0 = r[k - 1], y1 = r[k], y2 = r[k + 1];
    float den = y0 - 2.0f * y1 + y2;
    float d = (den != 0.0f) ? 0.5f * (y0 - y2) / den : 0.0f;
    if (d > 0.5f) d = 0.5f;
    if (d < -0.5f) d = -0.5f;
    return (float)k + d;
}

void hr_default_config(hr_config_t *cfg) {
    cfg->min_confidence = 0.45f;
    cfg->min_perfusion_index = 2e-4f;
    cfg->max_motion_rms_g = 0.05f;
}

void hr_init(hr_buffer_t *b, float fs) { b->fs = fs; b->n = 0; }

int hr_push(hr_buffer_t *b, float ppg, float motion_g) {
    if (b->n < HR_MAX_SAMPLES) {
        b->ppg[b->n] = ppg;
        b->motion[b->n] = motion_g;
        b->n++;
    }
    return b->n >= HR_MAX_SAMPLES;
}

const char *hr_status_str(hr_status_t s) {
    switch (s) {
        case HR_OK: return "ok";
        case HR_TOO_SHORT: return "too short";
        case HR_LOW_SIGNAL: return "low signal";
        case HR_MOTION: return "motion";
        case HR_NO_PEAK: return "no peak";
        case HR_LOW_CONFIDENCE: return "low confidence";
    }
    return "?";
}

hr_result_t hr_estimate(const hr_buffer_t *b, const hr_config_t *cfg_in) {
    static float y[HR_MAX_SAMPLES];            /* work buffer (static: keeps the stack small) */
    static float r[HR_MAX_SAMPLES];
    hr_config_t cfg;
    if (cfg_in) cfg = *cfg_in; else hr_default_config(&cfg);
    hr_result_t res = {0.0f, 0.0f, 0.0f, HR_TOO_SHORT};
    const int n = b->n;
    const float fs = b->fs;
    if (n < (int)(HR_MIN_SECONDS * fs) || fs <= 0.0f) return res;

    /* motion gate: RMS of (|a| - 1 g) over the window */
    double msum = 0.0;
    for (int i = 0; i < n; i++) msum += (double)b->motion[i] * b->motion[i];
    float motion_rms = (float)sqrt(msum / n);

    /* DC level and mean removal */
    double dc = 0.0;
    for (int i = 0; i < n; i++) dc += b->ppg[i];
    dc /= n;
    for (int i = 0; i < n; i++) y[i] = (float)(b->ppg[i] - dc);

    /* zero-phase band-pass 0.6-3.7 Hz (36-222 BPM): HP + LP, forward and backward */
    biquad_t hp = biquad_design(0.6f, fs, 1);
    biquad_t lp = biquad_design(3.7f, fs, 0);
    biquad_run(&hp, y, n, 0); biquad_run(&lp, y, n, 0);
    biquad_run(&hp, y, n, 1); biquad_run(&lp, y, n, 1);

    /* ignore 1 s at each edge (filter edge effects) */
    int e = (int)(1.0f * fs);
    const float *s = y + e;
    int m = n - 2 * e;
    double p = 0.0;
    for (int i = 0; i < m; i++) p += (double)s[i] * s[i];
    float ac_rms = (float)sqrt(p / m);
    res.perfusion_index = (dc != 0.0) ? ac_rms / (float)fabs(dc) : 0.0f;
    if (res.perfusion_index < cfg.min_perfusion_index || p <= 0.0) { res.status = HR_LOW_SIGNAL; return res; }
    if (motion_rms > cfg.max_motion_rms_g) { res.status = HR_MOTION; return res; }

    /* normalised (unbiased) autocorrelation over the heart-rate lag range */
    int kmin = (int)floorf(fs * 60.0f / HR_BPM_MAX);
    int kmax = (int)ceilf(fs * 60.0f / HR_BPM_MIN);
    if (kmin < 2) kmin = 2;
    if (kmax > m / 2) kmax = m / 2;
    double r0 = p / m;
    for (int k = kmin - 1; k <= kmax + 1; k++) {
        double acc = 0.0;
        for (int i = 0; i + k < m; i++) acc += (double)s[i] * s[i + k];
        r[k] = (float)((acc / (m - k)) / r0);
    }

    /* McLeod-style pick: among the local maxima ("key maxima") in range, take the SHORTEST lag whose
     * height is within 85 % of the tallest. Autocorrelation peaks repeat at 2T, 3T... with nearly the
     * same height (slowly varying heart rate), so "tallest" alone often lands on a multiple of T. */
    float rmax = 0.0f;
    int nkeys = 0;
    for (int k = kmin; k <= kmax; k++) {
        if (r[k] > r[k - 1] && r[k] >= r[k + 1] && r[k] > 0.0f) {
            nkeys++;
            if (r[k] > rmax) rmax = r[k];
        }
    }
    if (nkeys == 0) { res.status = HR_NO_PEAK; return res; }
    int best = -1;
    for (int k = kmin; k <= kmax; k++) {
        if (r[k] > r[k - 1] && r[k] >= r[k + 1] && r[k] >= 0.85f * rmax) { best = k; break; }
    }

    /* octave check in the spectrum: the PPG fundamental carries the most power, so compare the
     * candidate period with its octave/third neighbours and keep the one with the strongest bin. */
    int cands[4]; int nc = 0;
    cands[nc++] = best;
    int c2 = nearest_key_max(r, kmin, kmax, best / 2.0f, 1);
    int c3 = nearest_key_max(r, kmin, kmax, best / 3.0f, 1);
    int cx = nearest_key_max(r, kmin, kmax, best * 2.0f, 2);
    if (c2 > 0) cands[nc++] = c2;
    if (c3 > 0) cands[nc++] = c3;
    if (cx > 0) cands[nc++] = cx;
    float best_pow = -1.0f, lag = (float)best;
    for (int i = 0; i < nc; i++) {
        float L = refine_lag(r, cands[i]);
        float pw = goertzel_power(s, m, fs / L, fs);
        if (pw > best_pow) { best_pow = pw; best = cands[i]; lag = L; }
    }
    float y1 = r[best];

    /* frequency refinement: scan the spectrum +-6 % around the chosen period (Goertzel, 0.2 % steps)
     * - the lag grid is coarse at high heart rates (~8 samples per beat at 180 BPM, 25 sps) */
    float f0 = fs / lag, fbest = f0, pbest = -1.0f;
    for (int i = -30; i <= 30; i++) {
        float f = f0 * (1.0f + 0.002f * (float)i);
        float pw = goertzel_power(s, m, f, fs);
        if (pw > pbest) { pbest = pw; fbest = f; }
    }
    res.bpm = 60.0f * fbest;
    res.confidence = y1;
    res.status = (y1 >= cfg.min_confidence) ? HR_OK : HR_LOW_CONFIDENCE;
    return res;
}
