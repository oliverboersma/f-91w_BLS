/* LIS2DW12 accelerometer: 12.5 Hz low-power mode 1, +-2 g, FIFO watermark on INT1. ~1 uA. */
#ifndef LIS2DW12_H
#define LIS2DW12_H
#include <stdbool.h>
#include <stdint.h>

#define LIS2DW12_ODR_HZ     12.5f
#define LIS2DW12_FIFO_WTM   25          /* samples per INT1 (2 s) */

bool lis2dw12_init(void);               /* false if the part does not answer */
/* Read up to max samples from the FIFO, in g. Returns the number read. */
int  lis2dw12_read_fifo(float (*xyz)[3], int max);
#endif
