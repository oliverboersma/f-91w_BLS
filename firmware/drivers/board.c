#include "board.h"

static void gpio(GPIO_TypeDef *port, uint16_t mask, uint32_t mode, uint32_t pull)
{
    GPIO_InitTypeDef g = {0};
    g.Pin = mask;
    g.Mode = mode;
    g.Pull = pull;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(port, &g);
}

#define GPIO_PIN(pin, mode, pull)  gpio(BOARD_PORT(pin), BOARD_MASK(pin), mode, pull)
#define WRITE(pin, v)  HAL_GPIO_WritePin(BOARD_PORT(pin), BOARD_MASK(pin), (v) ? GPIO_PIN_SET : GPIO_PIN_RESET)

void board_init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();

    /* Outputs: set the level before switching the pin to output so nothing glitches. */
    WRITE(PIN_LED_R_N, 1); WRITE(PIN_LED_G_N, 1); WRITE(PIN_LED_B_N, 1);   /* active low: off */
    WRITE(PPG_BOOST_EN, 0);
    WRITE(PIN_BUZZER, 0);                                                    /* Q1 off */
    GPIO_PIN(PIN_LED_R_N, GPIO_MODE_OUTPUT_PP, GPIO_NOPULL);
    GPIO_PIN(PIN_LED_G_N, GPIO_MODE_OUTPUT_PP, GPIO_NOPULL);
    GPIO_PIN(PIN_LED_B_N, GPIO_MODE_OUTPUT_PP, GPIO_NOPULL);
    GPIO_PIN(PPG_BOOST_EN, GPIO_MODE_OUTPUT_PP, GPIO_NOPULL);                /* 1M pull-down on the flex */
    GPIO_PIN(PIN_BUZZER, GPIO_MODE_OUTPUT_PP, GPIO_NOPULL);

    /* Buttons are active high (pressed = VDD): pull-down, interrupt on both edges for press/release. */
    GPIO_PIN(PIN_BTN_MODE,  GPIO_MODE_IT_RISING_FALLING, GPIO_PULLDOWN);
    GPIO_PIN(PIN_BTN_LIGHT, GPIO_MODE_IT_RISING_FALLING, GPIO_PULLDOWN);
    GPIO_PIN(PIN_BTN_ALARM, GPIO_MODE_IT_RISING_FALLING, GPIO_PULLDOWN);

    /* LIS2DW12 INT1 (push-pull, active high). INT2 is not used yet. */
    GPIO_PIN(PIN_ACC_INT1, GPIO_MODE_IT_RISING, GPIO_PULLDOWN);
    GPIO_PIN(PIN_ACC_INT2, GPIO_MODE_ANALOG, GPIO_NOPULL);

    /* MAXM86161 INTB: analog (off) until a reading powers the flex, see board_ppg_power(). */
    GPIO_PIN(PPG_INT, GPIO_MODE_ANALOG, GPIO_NOPULL);

    /* Spare J1 lines: analog (lowest leakage) */
    GPIO_PIN(PIN_A2, GPIO_MODE_ANALOG, GPIO_NOPULL);

    HAL_NVIC_SetPriority(EXTI0_IRQn, 6, 0);     HAL_NVIC_EnableIRQ(EXTI0_IRQn);
    HAL_NVIC_SetPriority(EXTI15_10_IRQn, 6, 0); HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
    HAL_NVIC_SetPriority(EXTI9_5_IRQn, 6, 0);   HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);
    HAL_NVIC_SetPriority(EXTI2_IRQn, 6, 0);     /* enabled by the HR driver */
}

bool board_button_pressed(button_t b)
{
    switch (b) {
    case BTN_MODE:  return HAL_GPIO_ReadPin(BOARD_PORT(PIN_BTN_MODE),  BOARD_MASK(PIN_BTN_MODE))  == GPIO_PIN_SET;
    case BTN_LIGHT: return HAL_GPIO_ReadPin(BOARD_PORT(PIN_BTN_LIGHT), BOARD_MASK(PIN_BTN_LIGHT)) == GPIO_PIN_SET;
    case BTN_ALARM: return HAL_GPIO_ReadPin(BOARD_PORT(PIN_BTN_ALARM), BOARD_MASK(PIN_BTN_ALARM)) == GPIO_PIN_SET;
    default:        return false;
    }
}

void board_led(led_color_t c)
{
#if BOARD_HAS_LED
    WRITE(PIN_LED_R_N, !(c & LED_RED));
    WRITE(PIN_LED_G_N, !(c & LED_GREEN));
    WRITE(PIN_LED_B_N, !(c & LED_BLUE));
#else
    (void)c;
#endif
}

void board_ppg_power(bool on)
{
    /* INTB is open drain with a 100k pull-up on the flex; only listen to it while the flex is powered,
     * so an unplugged or unpowered flex cannot cause spurious wake-ups. */
    if (!on) {
        HAL_NVIC_DisableIRQ(EXTI2_IRQn);
        GPIO_PIN(PPG_INT, GPIO_MODE_ANALOG, GPIO_NOPULL);
    }
    WRITE(PPG_BOOST_EN, on);
    if (on) GPIO_PIN(PPG_INT, GPIO_MODE_IT_FALLING, GPIO_NOPULL);
}
