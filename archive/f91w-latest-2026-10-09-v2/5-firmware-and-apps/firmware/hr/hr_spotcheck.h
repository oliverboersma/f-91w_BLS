/*
 * hr_spotcheck: on-demand heart-rate estimate from a wrist PPG window (e.g. MAXM86161 green channel).
 * Portable C99 + float (STM32WB55 Cortex-M4 has an FPU). No dynamic allocation.
 *
 * Usage: hr_init() -> hr_push() each sample (PPG counts + motion in g) -> hr_estimate() once the
 * buffer holds >= HR_MIN_SECONDS of data. The user is expected to hold still (spot check).
 */
#ifndef HR_SPOTCHECK_H
#define HR_SPOTCHECK_H

#define HR_MAX_SAMPLES   400     /* 16 s at 25 sps */
#define HR_MIN_SECONDS   6.0f
#define HR_BPM_MIN       35.0f
#define HR_BPM_MAX       220.0f

typedef enum {
    HR_OK = 0,
    HR_TOO_SHORT,      /* not enough samples yet */
    HR_LOW_SIGNAL,     /* perfusion index too small: loose watch / no skin contact */
    HR_MOTION,         /* accelerometer says the wrist moved */
    HR_NO_PEAK,        /* no periodicity in the heart-rate band */
    HR_LOW_CONFIDENCE  /* periodic, but not convincingly */
} hr_status_t;

typedef struct {
    float fs;                         /* sample rate, Hz */
    int   n;                          /* samples held */
    float ppg[HR_MAX_SAMPLES];        /* raw optical counts (any unit, DC included) */
    float motion[HR_MAX_SAMPLES];     /* |a| - 1 g, in g (0 if no accelerometer) */
} hr_buffer_t;

typedef struct {
    float       bpm;
    float       confidence;           /* normalised autocorrelation at the chosen lag, 0..1 */
    float       perfusion_index;      /* AC rms / DC */
    hr_status_t status;
} hr_result_t;

typedef struct {
    float min_confidence;             /* default 0.45 */
    float min_perfusion_index;        /* default 2e-4 (0.02 %) */
    float max_motion_rms_g;           /* default 0.05 g */
} hr_config_t;

void        hr_default_config(hr_config_t *cfg);
void        hr_init(hr_buffer_t *b, float fs);
int         hr_push(hr_buffer_t *b, float ppg, float motion_g);   /* returns 1 when the buffer is full */
hr_result_t hr_estimate(const hr_buffer_t *b, const hr_config_t *cfg);
const char *hr_status_str(hr_status_t s);

#endif
