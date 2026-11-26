/*
 * MAXM86161 PPG module on the caseback flex (I2C 0x62, INTB -> A1, power from the TPS610995
 * boost on A0). Single green channel (LED1), 25 sps, FIFO almost-full interrupt every 1 s.
 * Register values follow the MAXM86161 datasheet (register map cross-checked against
 * MikroE's Heart Rate 2 Click driver, which uses the same module).
 */
#ifndef MAXM86161_H
#define MAXM86161_H
#include <stdbool.h>
#include <stdint.h>

#define MAXM86161_SPS       25
#define MAXM86161_FIFO_IRQ  25          /* samples per interrupt */

/* Power the flex, check the part ID and start sampling. led_ma: LED1 (green) current, 0-31 mA. */
bool maxm86161_start(uint8_t led_ma);
/* Read green samples (19-bit counts) from the FIFO. Returns the number read. */
int  maxm86161_read_fifo(uint32_t *out, int max);
/* Shut the module down and cut the boost. */
void maxm86161_stop(void);
#endif
