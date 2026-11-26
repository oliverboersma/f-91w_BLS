/*
 * Buttons
 *   MODE   short: next screen (TIME -> STEPS -> HEART -> BLE). Other screens fall back to TIME after 1 min.
 *   LIGHT  short: backlight for 3 s.   long (1.5 s): start BLE advertising from any screen.
 *   ALARM  TIME: long toggles 12/24 h.  HEART: start a reading.  BLE: advertise / disconnect.
 *
 * Everything runs in the sequencer task CFG_TASK_WATCH_ID. Interrupts and timer callbacks only
 * set event bits, so the HAL and BLE calls never race each other.
 */
#include "watch_app.h"
#include "app_common.h"
#include "app_conf.h"
#include "hw_if.h"
#include "stm32_seq.h"
#include "stm32_lpm.h"
#include "board.h"
#include "display.h"
#include "lcd.h"
#include "buzzer.h"
#include "i2c_bus.h"
#include "lis2dw12.h"
#include "maxm86161.h"
#include "battery.h"
#include "stepcount.h"
#include "timeutil.h"
#include "hr_spotcheck.h"
#include "app_ble.h"
#include "watch_service.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

extern RTC_HandleTypeDef hrtc;

/* ---- events -------------------------------------------------------------------------------- */
enum {
    EV_TICK = 1 << 0, EV_BTN = 1 << 1, EV_LONG = 1 << 2, EV_LIGHT_OFF = 1 << 3, EV_ACC = 1 << 4,
    EV_PPG = 1 << 5, EV_HR_TIMEOUT = 1 << 6, EV_BLE_CMD = 1 << 7, EV_BLE_CONN = 1 << 8,
    EV_REDRAW = 1 << 9, EV_BLE_TIME = 1 << 10, EV_BLE_SET = 1 << 11,
};
static volatile uint32_t events;

static void post(uint32_t ev)
{
    __disable_irq();
    events |= ev;
    __enable_irq();
    UTIL_SEQ_SetTask(1U << CFG_TASK_WATCH_ID, CFG_SCH_PRIO_0);
}

#define MS_TO_TICKS(ms)  ((uint32_t)((uint64_t)(ms) * 1000U / CFG_TS_TICK_VAL))

static uint8_t tmr_debounce, tmr_long, tmr_light, tmr_hr;
static void cb_debounce(void) { post(EV_BTN); }
static void cb_long(void)     { post(EV_LONG); }
static void cb_light(void)    { post(EV_LIGHT_OFF); }
static void cb_hr(void)       { post(EV_HR_TIMEOUT); }

/* ---- persistent state in RTC backup registers (kept over resets, lost on battery change) ----- */
enum { BKP_MAGIC = 0, BKP_STEPS, BKP_DAY, BKP_HIST0, BKP_SETTINGS = BKP_HIST0 + WSVC_HIST_DAYS, BKP_TZ, BKP_LAST_HR };
#define MAGIC 0xF91B0001U

#define SET_24H     0x01
#define SET_CHIME   0x02
#define SET_BEEP    0x04

static struct {
    uint8_t flags, contrast, light_color, led_ma;
} cfg = { SET_24H | SET_BEEP, 4, LED_GREEN, 20 };

static int16_t  tz_min;
static uint32_t steps_today, day_stamp, hist[WSVC_HIST_DAYS];
static uint32_t last_hr;                  /* bpm | conf% << 8 | status << 16 */

static uint32_t bkp_read(uint32_t i)              { return HAL_RTCEx_BKUPRead(&hrtc, i); }
static void     bkp_write(uint32_t i, uint32_t v) { HAL_RTCEx_BKUPWrite(&hrtc, i, v); }

static void cfg_save(void)
{
    bkp_write(BKP_SETTINGS, cfg.flags | (uint32_t)cfg.contrast << 8 | (uint32_t)cfg.light_color << 16 | (uint32_t)cfg.led_ma << 24);
}

static void state_load(void)
{
    if (bkp_read(BKP_MAGIC) != MAGIC) {
        for (uint32_t i = 1; i < 20; i++) bkp_write(i, 0);
        cfg_save();
        bkp_write(BKP_MAGIC, MAGIC);
        return;
    }
    uint32_t s = bkp_read(BKP_SETTINGS);
    cfg.flags = (uint8_t)s; cfg.contrast = (uint8_t)(s >> 8) & 7;
    cfg.light_color = (uint8_t)(s >> 16) & 7; cfg.led_ma = (uint8_t)(s >> 24);
    if (cfg.led_ma == 0 || cfg.led_ma > 31) cfg.led_ma = 20;
    steps_today = bkp_read(BKP_STEPS);
    day_stamp = bkp_read(BKP_DAY);
    for (int i = 0; i < WSVC_HIST_DAYS; i++) hist[i] = bkp_read(BKP_HIST0 + i);
    tz_min = (int16_t)bkp_read(BKP_TZ);
    last_hr = bkp_read(BKP_LAST_HR);
}

/* ---- RTC ----------------------------------------------------------------------------------- */
static void rtc_now(datetime_t *dt)
{
    RTC_TimeTypeDef t;
    RTC_DateTypeDef d;
    /* Shadow registers are stale after Stop2: clear RSF and wait for the next sync (<= 2 RTCCLK).
     * Interrupts are off while the RTC is unlocked because the timer server also toggles WPR. */
    __disable_irq();
    __HAL_RTC_WRITEPROTECTION_DISABLE(&hrtc);
    hrtc.Instance->ISR &= ~RTC_ISR_RSF;
    __HAL_RTC_WRITEPROTECTION_ENABLE(&hrtc);
    __enable_irq();
    for (int i = 0; i < 2000 && !(hrtc.Instance->ISR & RTC_ISR_RSF); i++) { }
    HAL_RTC_GetTime(&hrtc, &t, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &d, RTC_FORMAT_BIN);  /* must follow GetTime to unlock the shadows */
    dt->year = (uint16_t)(2000 + d.Year); dt->month = d.Month; dt->day = d.Date; dt->weekday = d.WeekDay;
    dt->hour = t.Hours; dt->minute = t.Minutes; dt->second = t.Seconds;
}

static void rtc_set(const datetime_t *dt)
{
    RTC_TimeTypeDef t = {0};
    RTC_DateTypeDef d = {0};
    t.Hours = dt->hour; t.Minutes = dt->minute; t.Seconds = dt->second;
    d.Year = (uint8_t)(dt->year - 2000); d.Month = dt->month; d.Date = dt->day; d.WeekDay = dt->weekday;
    HAL_RTC_SetTime(&hrtc, &t, RTC_FORMAT_BIN);
    HAL_RTC_SetDate(&hrtc, &d, RTC_FORMAT_BIN);
}

static uint32_t now_unix_utc(void)
{
    datetime_t dt;
    rtc_now(&dt);
    return (uint32_t)(datetime_to_unix(&dt) - tz_min * 60);
}

/* RTC alarm A with every field masked: one interrupt per second, on the second boundary. */
static void rtc_second_alarm(void)
{
    RTC_AlarmTypeDef a = {0};
    a.Alarm = RTC_ALARM_A;
    a.AlarmMask = RTC_ALARMMASK_ALL;
    a.AlarmSubSecondMask = RTC_ALARMSUBSECONDMASK_ALL;
    a.AlarmDateWeekDaySel = RTC_ALARMDATEWEEKDAYSEL_DATE;
    a.AlarmDateWeekDay = 1;
    HAL_RTC_SetAlarm_IT(&hrtc, &a, RTC_FORMAT_BIN);
    HAL_NVIC_SetPriority(RTC_Alarm_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(RTC_Alarm_IRQn);
}

void HAL_RTC_AlarmAEventCallback(RTC_HandleTypeDef *h) { (void)h; post(EV_TICK); }

static uint32_t stamp(const datetime_t *dt) { return (uint32_t)dt->year << 16 | (uint32_t)dt->month << 8 | dt->day; }

/* ---- UI state ------------------------------------------------------------------------------ */
typedef enum { M_TIME, M_STEPS, M_HEART, M_BLE, M_COUNT } mode_t;
static mode_t mode;
static uint16_t idle_s;
static bool btn_state[BTN_COUNT], long_fired;
static int  long_btn = -1;
static bool ble_conn, ble_adv, acc_ok;
static uint32_t passkey = 0xFFFFFFFF;

/* heart-rate reading */
static hr_buffer_t hrbuf;
static bool hr_running, raw_stream;
static int hr_skip;
static uint16_t ppg_seq;
static float motion_g;
#define HR_STATUS_NO_SENSOR  0xFF
#define HR_WINDOW_S          10

static stepcount_t sc;

/* BLE requests handed over to the task */
static uint8_t ble_cmd[20], ble_cmd_len, ble_set[WSVC_SETTINGS_LEN];
static uint32_t ble_time; static int16_t ble_tz;

static void beep(void) { if (cfg.flags & SET_BEEP) buzzer_beep(); }

static void draw(void)
{
    char buf[24];
    datetime_t dt;
    disp_clear();
    disp_indicator(DISP_IND_SIGNAL, ble_conn || ble_adv);
    disp_indicator(DISP_IND_BELL, cfg.flags & SET_CHIME);

    if (passkey != 0xFFFFFFFF) {                      /* pairing: show the passkey */
        snprintf(buf, sizeof buf, "PA  %06lu", (unsigned long)passkey);
        disp_text(0, buf);
        disp_flush();
        return;
    }
    switch (mode) {
    case M_TIME: {
        static const char *wd[8] = {"  ", "MO", "TU", "WE", "TH", "FR", "SA", "SU"};
        rtc_now(&dt);
        uint8_t h = dt.hour;
        if (!(cfg.flags & SET_24H)) { disp_indicator(DISP_IND_PM, h >= 12); h = h % 12 ? h % 12 : 12; }
        else disp_indicator(DISP_IND_24H, true);
        snprintf(buf, sizeof buf, "%s%2u%2u%02u%02u", wd[dt.weekday & 7], dt.day, h, dt.minute, dt.second);
        disp_text(0, buf);
        disp_colon(true);
        break;
    }
    case M_STEPS:
        if (!acc_ok) { disp_text(0, "St  no Acc"); break; }
        snprintf(buf, sizeof buf, "St  %6lu", (unsigned long)(steps_today % 1000000U));
        disp_text(0, buf);
        break;
    case M_HEART:
        if (hr_running) {
            snprintf(buf, sizeof buf, "HR%2d  ----", HR_WINDOW_S - hrbuf.n / MAXM86161_SPS);
        } else if ((last_hr >> 16 & 0xFF) == HR_OK && (last_hr & 0xFF)) {
            snprintf(buf, sizeof buf, "HR    %3lu ", (unsigned long)(last_hr & 0xFF));
        } else {
            static const char *err[] = {"    ", "Short ", "no Con", "MovEd ", "retry ", "retry "};
            uint8_t st = (uint8_t)(last_hr >> 16);
            snprintf(buf, sizeof buf, "HR  %s", st == HR_STATUS_NO_SENSOR ? "no PPG" : st < 6 ? err[st] : "      ");
        }
        disp_text(0, buf);
        break;
    case M_BLE:
        disp_text(0, ble_conn ? "bL    Conn" : ble_adv ? "bL    AdV " : "bL     oFF");
        break;
    default:
        break;
    }
    disp_flush();
}

/* ---- sync of values to the GATT table --------------------------------------------------------- */
static void ble_push_all(void)
{
    uint8_t s4[WSVC_SETTINGS_LEN] = {cfg.flags, cfg.contrast, cfg.light_color, cfg.led_ma};
    uint8_t hr8[WSVC_HR_LEN] = {(uint8_t)(last_hr >> 16), (uint8_t)last_hr, (uint8_t)(last_hr >> 8), 0, 0, 0, 0, 0};
    uint32_t ts = bkp_read(BKP_LAST_HR + 1);
    memcpy(hr8 + 4, &ts, 4);
    WSVC_UpdateTime(now_unix_utc(), tz_min);
    WSVC_UpdateSteps(steps_today);
    WSVC_UpdateHistory(hist);
    WSVC_UpdateHeartRate(hr8, 0);
    WSVC_UpdateSettings(s4);
    WSVC_UpdateBattery(battery_percent(battery_mv()));
}

/* ---- steps ----------------------------------------------------------------------------------- */
static void day_rollover(const datetime_t *dt)
{
    uint32_t st = stamp(dt);
    if (st == day_stamp) return;
    if (day_stamp != 0) {                       /* shift history, yesterday goes to [0] */
        memmove(&hist[1], &hist[0], sizeof hist[0] * (WSVC_HIST_DAYS - 1));
        hist[0] = steps_today;
        for (int i = 0; i < WSVC_HIST_DAYS; i++) bkp_write(BKP_HIST0 + i, hist[i]);
    }
    steps_today = 0;
    day_stamp = st;
    bkp_write(BKP_STEPS, 0);
    bkp_write(BKP_DAY, st);
    if (ble_conn) WSVC_UpdateHistory(hist);
}

static void acc_service(void)
{
    float xyz[32][3];
    int n = lis2dw12_read_fifo(xyz, 32);
    uint32_t added = 0;
    float ms = 0;
    for (int i = 0; i < n; i++) {
        added += stepcount_push(&sc, xyz[i][0], xyz[i][1], xyz[i][2]);
        float m = sqrtf(xyz[i][0] * xyz[i][0] + xyz[i][1] * xyz[i][1] + xyz[i][2] * xyz[i][2]) - 1.0f;
        ms += m * m;
    }
    if (n) motion_g = sqrtf(ms / n);           /* used by the HR quality check */
    if (added) {
        steps_today += added;
        bkp_write(BKP_STEPS, steps_today);
        if (ble_conn) WSVC_UpdateSteps(steps_today);
        if (mode == M_STEPS) draw();
    }
}

/* ---- heart rate ------------------------------------------------------------------------------ */
static void hr_finish(uint8_t status, uint8_t bpm, uint8_t conf)
{
    maxm86161_stop();
    HW_TS_Stop(tmr_hr);
    hr_running = false;
    last_hr = (uint32_t)status << 16 | (uint32_t)conf << 8 | bpm;
    uint32_t ts = now_unix_utc();
    bkp_write(BKP_LAST_HR, last_hr);
    bkp_write(BKP_LAST_HR + 1, ts);
    uint8_t hr8[WSVC_HR_LEN] = {status, bpm, conf, 0};
    memcpy(hr8 + 4, &ts, 4);
    WSVC_UpdateHeartRate(hr8, status == HR_OK ? bpm : 0);
    static const buzzer_note_t done[] = {{2731, 60}, {0, 60}, {3277, 80}, {0, 0}};
    if (cfg.flags & SET_BEEP) buzzer_play(done);
    draw();
}

static void hr_start(void)
{
    if (hr_running) return;
    mode = M_HEART;
    if (!maxm86161_start(cfg.led_ma)) { hr_finish(HR_STATUS_NO_SENSOR, 0, 0); return; }
    hr_init(&hrbuf, (float)MAXM86161_SPS);
    hr_running = true;
    hr_skip = MAXM86161_SPS;                   /* drop the first second while ALC settles */
    ppg_seq = 0;
    HW_TS_Start(tmr_hr, MS_TO_TICKS((HR_WINDOW_S + 6) * 1000));
    draw();
}

static void ppg_service(void)
{
    uint32_t s[64];
    int n = maxm86161_read_fifo(s, 64);
    if (!hr_running) return;
    if (raw_stream) { WSVC_StreamPpg(ppg_seq, s, n); ppg_seq = (uint16_t)(ppg_seq + n); }
    for (int i = 0; i < n; i++) {
        if (hr_skip > 0) { hr_skip--; continue; }
        hr_push(&hrbuf, (float)s[i], acc_ok ? motion_g : 0.0f);
    }
    if (hrbuf.n >= HR_WINDOW_S * MAXM86161_SPS) {
        hr_config_t c;
        hr_default_config(&c);
        hr_result_t r = hr_estimate(&hrbuf, &c);
        hr_finish((uint8_t)r.status, r.status == HR_OK ? (uint8_t)(r.bpm + 0.5f) : 0, (uint8_t)(r.confidence * 100));
    } else if (mode == M_HEART) {
        draw();
    }
}

/* ---- buttons --------------------------------------------------------------------------------- */
static void on_press(button_t b)
{
    idle_s = 0;
    if (passkey != 0xFFFFFFFF) return;         /* keep the passkey on screen */
    switch (b) {
    case BTN_MODE:
        beep();
        mode = (mode_t)((mode + 1) % M_COUNT);
        break;
    case BTN_LIGHT:
        board_led((led_color_t)cfg.light_color);
        HW_TS_Start(tmr_light, MS_TO_TICKS(3000));
        break;
    case BTN_ALARM:
        if (mode == M_HEART) { beep(); hr_start(); }
        else if (mode == M_BLE) {
            beep();
            if (ble_conn) APP_BLE_Disconnect();
            else if (ble_adv) APP_BLE_StopAdvertising();
            else APP_BLE_StartAdvertising();
        }
        break;
    default:
        break;
    }
    draw();
}

static void on_long(button_t b)
{
    if (b == BTN_LIGHT) { beep(); mode = M_BLE; APP_BLE_StartAdvertising(); }
    else if (b == BTN_ALARM && mode == M_TIME) { beep(); cfg.flags ^= SET_24H; cfg_save(); }
    draw();
}

static void buttons_service(void)
{
    for (int b = 0; b < BTN_COUNT; b++) {
        bool p = board_button_pressed((button_t)b);
        if (p == btn_state[b]) continue;
        btn_state[b] = p;
        if (p) {
            long_btn = b; long_fired = false;
            HW_TS_Start(tmr_long, MS_TO_TICKS(1500));
            if (b != BTN_ALARM) on_press((button_t)b);     /* MODE and LIGHT act on press */
        } else {
            if (b == long_btn) { HW_TS_Stop(tmr_long); long_btn = -1; }
            if (b == BTN_ALARM && !long_fired) on_press(BTN_ALARM);   /* short ALARM acts on release */
        }
    }
}

/* ---- once a second --------------------------------------------------------------------------- */
static void tick(void)
{
    datetime_t dt;
    rtc_now(&dt);
    day_rollover(&dt);
    if (mode != M_TIME && !hr_running && passkey == 0xFFFFFFFF && ++idle_s >= 60) { mode = M_TIME; idle_s = 0; }
    if ((cfg.flags & SET_CHIME) && dt.minute == 0 && dt.second == 0) {
        static const buzzer_note_t chime[] = {{4096, 40}, {0, 80}, {4096, 40}, {0, 0}};
        buzzer_play(chime);
    }
    if (ble_conn && dt.second == 0) WSVC_UpdateTime(now_unix_utc(), tz_min);
    /* INT1 is a level signal: if a FIFO read ever failed it stays high and no new edge comes. */
    if (acc_ok && dt.second % 10 == 0 &&
        HAL_GPIO_ReadPin(BOARD_PORT(PIN_ACC_INT1), BOARD_MASK(PIN_ACC_INT1)) == GPIO_PIN_SET) acc_service();
    if (dt.minute == 30 && dt.second == 0) WSVC_UpdateBattery(battery_percent(battery_mv()));
    draw();
}

/* ---- BLE requests ---------------------------------------------------------------------------- */
static void ota_reboot(void)
{
    /* ST BLE_Ota loader protocol: SRAM1[0] = reboot reason, [1] = first sector, [2] = sector count */
    *(volatile uint8_t *)(SRAM1_BASE + 0) = 0x01;                     /* CFG_REBOOT_ON_BLE_OTA_APP */
    *(volatile uint8_t *)(SRAM1_BASE + 1) = 7;                        /* app at 0x08007000 */
    *(volatile uint8_t *)(SRAM1_BASE + 2) = 0;                        /* loader erases what it needs */
    disp_clear(); disp_text(4, " OtA  "); disp_flush();
    HAL_Delay(100);
    NVIC_SystemReset();
}

static void ble_command(void)
{
    static const buzzer_note_t find[] = {
        {2731, 120}, {0, 60}, {3277, 120}, {0, 60}, {4096, 200}, {0, 300},
        {2731, 120}, {0, 60}, {3277, 120}, {0, 60}, {4096, 200}, {0, 0}};
    switch (ble_cmd[0]) {
    case WSVC_CMD_HR_START:     hr_start(); break;
    case WSVC_CMD_FIND:         buzzer_play(find); board_led(LED_WHITE); HW_TS_Start(tmr_light, MS_TO_TICKS(2000)); break;
    case WSVC_CMD_OTA_REBOOT:   ota_reboot(); break;
    case WSVC_CMD_RAW_PPG:      raw_stream = ble_cmd_len > 1 && ble_cmd[1]; break;
    case WSVC_CMD_FORGET_BONDS: APP_BLE_ForgetBonds(); break;
    default: break;
    }
}

static void ble_time_set(void)
{
    datetime_t dt;
    tz_min = ble_tz;
    bkp_write(BKP_TZ, (uint32_t)(int32_t)tz_min);
    unix_to_datetime((int64_t)ble_time + tz_min * 60, &dt);
    if (dt.year < 2000 || dt.year > 2099) return;
    rtc_set(&dt);
    day_rollover(&dt);
    WSVC_UpdateTime(ble_time, tz_min);
}

static void ble_settings_set(void)
{
    cfg.flags = ble_set[0] & (SET_24H | SET_CHIME | SET_BEEP);
    cfg.contrast = ble_set[1] & 7;
    cfg.light_color = ble_set[2] & 7;
    cfg.led_ma = (ble_set[3] >= 1 && ble_set[3] <= 31) ? ble_set[3] : cfg.led_ma;
    cfg_save();
    lcd_set_contrast(cfg.contrast);
    uint8_t s4[WSVC_SETTINGS_LEN] = {cfg.flags, cfg.contrast, cfg.light_color, cfg.led_ma};
    WSVC_UpdateSettings(s4);
}

/* ---- task ------------------------------------------------------------------------------------ */
static void watch_task(void)
{
    __disable_irq();
    uint32_t ev = events;
    events = 0;
    __enable_irq();

    if (ev & EV_BTN)        buttons_service();
    if (ev & EV_LONG && long_btn >= 0 && board_button_pressed((button_t)long_btn)) {
        long_fired = true;
        on_long((button_t)long_btn);
    }
    if (ev & EV_LIGHT_OFF)  board_led(LED_OFF);
    if (ev & EV_ACC)        acc_service();
    if (ev & EV_PPG)        ppg_service();
    if (ev & EV_HR_TIMEOUT && hr_running) hr_finish(HR_TOO_SHORT, 0, 0);
    if (ev & EV_BLE_TIME)   ble_time_set();
    if (ev & EV_BLE_SET)    ble_settings_set();
    if (ev & EV_BLE_CMD)    ble_command();
    if (ev & EV_BLE_CONN && ble_conn) ble_push_all();
    if (ev & EV_TICK)       tick();
    else if (ev & (EV_BLE_CONN | EV_REDRAW | EV_BLE_TIME | EV_BLE_SET)) draw();
}

/* ---- interrupts ------------------------------------------------------------------------------ */
void HAL_GPIO_EXTI_Callback(uint16_t pin)
{
    if (pin == EXTI_PIN_BTN_MODE || pin == EXTI_PIN_BTN_LIGHT || pin == EXTI_PIN_BTN_ALARM)
        HW_TS_Start(tmr_debounce, MS_TO_TICKS(25));   /* restart on every bounce */
    else if (pin == EXTI_PIN_ACC_INT1)
        post(EV_ACC);
    else if (pin == EXTI_PIN_PPG_INT)
        post(EV_PPG);
}

/* ---- BLE callbacks (BLE event context) ------------------------------------------------------- */
void watch_app_ble_time_written(uint32_t unix_utc, int16_t tz) { ble_time = unix_utc; ble_tz = tz; post(EV_BLE_TIME); }
void watch_app_ble_settings_written(const uint8_t *s4) { memcpy(ble_set, s4, sizeof ble_set); post(EV_BLE_SET); }
void watch_app_ble_command(const uint8_t *d, uint8_t len)
{
    ble_cmd_len = len > sizeof ble_cmd ? sizeof ble_cmd : len;
    memcpy(ble_cmd, d, ble_cmd_len);
    post(EV_BLE_CMD);
}
void watch_app_ble_connection(bool c) { ble_conn = c; if (c) ble_adv = false; post(EV_BLE_CONN); }
void watch_app_ble_passkey(uint32_t k) { passkey = k; post(EV_REDRAW); }
void watch_app_ble_advertising(bool on) { ble_adv = on; post(EV_REDRAW); }

/* ---- init ------------------------------------------------------------------------------------ */
void watch_app_init(void)
{
    datetime_t dt;
    state_load();
    lcd_init(cfg.contrast);
    buzzer_init();
    i2c_bus_init();
    acc_ok = lis2dw12_init();
    stepcount_init(&sc, LIS2DW12_ODR_HZ);

    UTIL_SEQ_RegTask(1U << CFG_TASK_WATCH_ID, UTIL_SEQ_RFU, watch_task);
    HW_TS_Create(CFG_TIM_PROC_ID_ISR, &tmr_debounce, hw_ts_SingleShot, cb_debounce);
    HW_TS_Create(CFG_TIM_PROC_ID_ISR, &tmr_long, hw_ts_SingleShot, cb_long);
    HW_TS_Create(CFG_TIM_PROC_ID_ISR, &tmr_light, hw_ts_SingleShot, cb_light);
    HW_TS_Create(CFG_TIM_PROC_ID_ISR, &tmr_hr, hw_ts_SingleShot, cb_hr);

    rtc_now(&dt);
    if (dt.year < 2026) {                        /* fresh battery: start at the firmware build date */
        unix_to_datetime(1791417600, &dt);       /* 2026-10-08 00:00 */
        rtc_set(&dt);
    }
    day_rollover(&dt);
    rtc_second_alarm();
    if (acc_ok) post(EV_ACC);                    /* drain a FIFO left over from before a reset */
    draw();
}
