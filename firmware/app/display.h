/*
 * Frame buffer and character rendering for the stock F-91W glass (3 COM x 24 SEG).
 * Coordinates are glass (com, seg) as Sensor Watch / Movement name them; drivers/lcd.c
 * translates them to MCU LCD lines with board_f91w_ble.h when the frame is flushed.
 *
 * Positions: 0-1 weekday/mode letters, 2-3 day of month, 4-5 hours, 6-7 minutes, 8-9 seconds.
 */
#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>
#include <stdbool.h>

#define DISP_COMS 3

typedef enum {
    DISP_IND_SIGNAL = 0, /* the "wifi" bars: used for BLE */
    DISP_IND_BELL,
    DISP_IND_PM,
    DISP_IND_24H,
    DISP_IND_LAP,
    DISP_IND_COUNT
} disp_indicator_t;

extern uint32_t disp_fb[DISP_COMS];  /* bit n of disp_fb[com] = glass (com, SEGn) */

void disp_clear(void);
void disp_pixel(uint8_t com, uint8_t seg, bool on);
void disp_char(uint8_t pos, char c);
void disp_text(uint8_t pos, const char *s);   /* stops at position 9 or end of string */
void disp_colon(bool on);
void disp_indicator(disp_indicator_t ind, bool on);
void disp_flush(void);                        /* implemented by the LCD driver (or the host test) */

#endif
