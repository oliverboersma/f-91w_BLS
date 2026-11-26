/* Piezo through L3 (14 mH) + Q1, driven by LPTIM1 PWM on PC1 from the LSE. Non-blocking. */
#ifndef BUZZER_H
#define BUZZER_H
#include <stdint.h>

typedef struct { uint16_t hz; uint16_t ms; } buzzer_note_t;   /* hz = 0: rest. ms = 0 ends a tune */

void buzzer_init(void);
void buzzer_play(const buzzer_note_t *tune);   /* tune must stay valid until it ends */
void buzzer_beep(void);                        /* short button click */
void buzzer_stop(void);
#endif
