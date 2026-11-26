/* Battery (VDD = CR2016, no regulator) measured through VREFINT on ADC1. */
#ifndef BATTERY_H
#define BATTERY_H
#include <stdint.h>
uint16_t battery_mv(void);              /* takes ~100 us, ADC off afterwards */
uint8_t  battery_percent(uint16_t mv);  /* rough CR2016 curve, 3.0 V = 100 %, 2.5 V = 0 % */
#endif
