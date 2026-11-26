/* F-91W BLE rev A board support: GPIO, buttons, backlight LED, PPG boost. */
#ifndef BOARD_H
#define BOARD_H

#include <stdbool.h>
#include <stdint.h>
#include "stm32wbxx_hal.h"
#include "board_f91w_ble.h"

/* PIN_x macros in board_f91w_ble.h expand to "GPIOx, n" */
#define BOARD_PORT_(port, n)  port
#define BOARD_MASK_(port, n)  ((uint16_t)(1U << (n)))
#define BOARD_PORT(...)       BOARD_PORT_(__VA_ARGS__)
#define BOARD_MASK(...)       BOARD_MASK_(__VA_ARGS__)

/* Build options (Makefile: NO_LED=1, NO_BUZZER=1) for boards built without D1 or L3. */
#ifndef BOARD_HAS_LED
#define BOARD_HAS_LED     1
#endif
#ifndef BOARD_HAS_BUZZER
#define BOARD_HAS_BUZZER  1
#endif

typedef enum { BTN_MODE = 0, BTN_LIGHT, BTN_ALARM, BTN_COUNT } button_t;

typedef enum { LED_OFF = 0, LED_RED = 1, LED_GREEN = 2, LED_BLUE = 4, LED_WHITE = 7 } led_color_t;

void board_init(void);
bool board_button_pressed(button_t b);
void board_led(led_color_t c);
void board_ppg_power(bool on);       /* TPS610995 boost for the PPG flex (A0) */

/* EXTI lines owned by the drivers; HAL_GPIO_EXTI_Callback in watch_app.c dispatches them. */
#define EXTI_PIN_BTN_ALARM  BOARD_MASK(PIN_BTN_ALARM)
#define EXTI_PIN_BTN_LIGHT  BOARD_MASK(PIN_BTN_LIGHT)
#define EXTI_PIN_BTN_MODE   BOARD_MASK(PIN_BTN_MODE)
#define EXTI_PIN_ACC_INT1   BOARD_MASK(PIN_ACC_INT1)
#define EXTI_PIN_PPG_INT    BOARD_MASK(PPG_INT)

#endif
