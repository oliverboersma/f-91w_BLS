/* STM32WB55 LCD controller driving the F-91W glass: 1/3 duty, 1/3 bias, LSE clock, ~40 Hz frame. */
#ifndef LCD_H
#define LCD_H
#include <stdint.h>
void lcd_init(uint8_t contrast);        /* contrast 0..7 (VLCD step-up level) */
void lcd_set_contrast(uint8_t contrast);
#endif
