/*
 * GATT services of the F-91W BLE watch:
 *   - Battery Service (0x180F) and Heart Rate Service (0x180D): standard, readable without pairing.
 *   - F-91W service 6f910001-c2a6-4b3e-8f1d-0a5e3b7c9d10: time, steps, history, heart rate,
 *     settings, control and raw PPG. Reading or writing it requires an authenticated (passkey)
 *     bond; the passkey is shown on the watch. Byte layouts are documented in docs/BLE.md.
 */
#ifndef WATCH_SERVICE_H
#define WATCH_SERVICE_H
#include <stdbool.h>
#include <stdint.h>

#define WSVC_TIME_LEN      6    /* u32 unix UTC, i16 tz offset in minutes */
#define WSVC_HIST_DAYS     7
#define WSVC_HR_LEN        8    /* u8 status, u8 bpm, u8 confidence %, u8 rsvd, u32 unix UTC */
#define WSVC_SETTINGS_LEN  4    /* u8 flags, u8 contrast, u8 backlight colour, u8 PPG LED mA */
#define WSVC_PPG_MAX       6    /* samples per raw-PPG notification (u16 seq + 6 x u24) */

/* control opcodes (first byte written to the control characteristic) */
#define WSVC_CMD_HR_START     0x01
#define WSVC_CMD_FIND         0x02
#define WSVC_CMD_OTA_REBOOT   0x03
#define WSVC_CMD_RAW_PPG      0x04  /* arg u8: 1 = stream raw PPG during readings */
#define WSVC_CMD_FORGET_BONDS 0x06

void WSVC_Init(void);
void WSVC_SetConnected(uint16_t conn_handle, bool connected);
void WSVC_SetEncrypted(bool encrypted);

/* value updates (notify subscribed clients when connected) */
void WSVC_UpdateBattery(uint8_t percent);
void WSVC_UpdateTime(uint32_t unix_utc, int16_t tz_min);
void WSVC_UpdateSteps(uint32_t steps);
void WSVC_UpdateHistory(const uint32_t *days);           /* WSVC_HIST_DAYS entries, [0] = yesterday */
void WSVC_UpdateHeartRate(const uint8_t *hr8, uint8_t bpm_for_hrs); /* bpm_for_hrs 0: no HRS notify */
void WSVC_UpdateSettings(const uint8_t *s4);
void WSVC_StreamPpg(uint16_t seq, const uint32_t *samples, int n);

/* implemented by the application (watch_app.c); called from BLE event context */
void watch_app_ble_time_written(uint32_t unix_utc, int16_t tz_min);
void watch_app_ble_settings_written(const uint8_t *s4);
void watch_app_ble_command(const uint8_t *data, uint8_t len);
void watch_app_ble_connection(bool connected);
void watch_app_ble_passkey(uint32_t passkey);            /* show it on the LCD; 0xFFFFFFFF = done */
void watch_app_ble_advertising(bool on);
#endif
