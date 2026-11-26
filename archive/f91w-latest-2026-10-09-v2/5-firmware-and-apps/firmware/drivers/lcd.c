#include "lcd.h"
#include "board.h"
#include "display.h"

static LCD_HandleTypeDef hlcd;

/* Every pin that drives the glass, from the LCD_GLASS_*_TO_MCU_* tables in board_f91w_ble.h. */
static const struct { GPIO_TypeDef *port; uint16_t mask; } lcd_pins[] = {
    {GPIOA, (1U<<1)|(1U<<2)|(1U<<3)|(1U<<4)|(1U<<6)|(1U<<7)|(1U<<8)|(1U<<9)|(1U<<10)|(1U<<15)},
    {GPIOB, (1U<<3)|(1U<<4)|(1U<<5)|(1U<<6)|(1U<<7)|(1U<<10)|(1U<<11)|(1U<<12)|(1U<<13)|(1U<<14)|(1U<<15)},
    {GPIOC, (1U<<4)|(1U<<5)|(1U<<6)|(1U<<10)|(1U<<11)|(1U<<12)},
};

void HAL_LCD_MspInit(LCD_HandleTypeDef *h)
{
    (void)h;
    __HAL_RCC_LCD_CLK_ENABLE();             /* LCD kernel clock is the RTC clock (LSE) */
    for (unsigned i = 0; i < sizeof lcd_pins / sizeof lcd_pins[0]; i++) {
        GPIO_InitTypeDef g = {0};
        g.Pin = lcd_pins[i].mask;
        g.Mode = GPIO_MODE_AF_PP;
        g.Pull = GPIO_NOPULL;
        g.Speed = GPIO_SPEED_FREQ_LOW;
        g.Alternate = GPIO_AF11_LCD;
        HAL_GPIO_Init(lcd_pins[i].port, &g);
    }
    /* PC3 = VLCD: external 1 uF for the internal step-up converter */
    GPIO_InitTypeDef v = {0};
    v.Pin = GPIO_PIN_3;
    v.Mode = GPIO_MODE_AF_PP;
    v.Alternate = GPIO_AF11_LCD;
    HAL_GPIO_Init(GPIOC, &v);
}

static uint32_t contrast_level(uint8_t c)
{
    static const uint32_t lv[8] = {
        LCD_CONTRASTLEVEL_0, LCD_CONTRASTLEVEL_1, LCD_CONTRASTLEVEL_2, LCD_CONTRASTLEVEL_3,
        LCD_CONTRASTLEVEL_4, LCD_CONTRASTLEVEL_5, LCD_CONTRASTLEVEL_6, LCD_CONTRASTLEVEL_7,
    };
    return lv[c & 7];
}

void lcd_init(uint8_t contrast)
{
    hlcd.Instance = LCD;
    hlcd.Init.Prescaler = LCD_PRESCALER_16;        /* 32768 / 16 / 17 / 3 = 40 Hz frame */
    hlcd.Init.Divider = LCD_DIVIDER_17;
    hlcd.Init.Duty = LCD_DUTY_1_3;
    hlcd.Init.Bias = LCD_BIAS_1_3;
    hlcd.Init.VoltageSource = LCD_VOLTAGESOURCE_INTERNAL;
    hlcd.Init.Contrast = contrast_level(contrast);
    hlcd.Init.DeadTime = LCD_DEADTIME_0;
    hlcd.Init.PulseOnDuration = LCD_PULSEONDURATION_2;
    hlcd.Init.MuxSegment = LCD_MUXSEGMENT_DISABLE;
    hlcd.Init.BlinkMode = LCD_BLINKMODE_OFF;
    hlcd.Init.BlinkFrequency = LCD_BLINKFREQUENCY_DIV32;
    hlcd.Init.HighDrive = LCD_HIGHDRIVE_DISABLE;
    if (HAL_LCD_Init(&hlcd) != HAL_OK) return;     /* watch keeps running without a display */
    disp_clear();
    disp_flush();
}

void lcd_set_contrast(uint8_t contrast)
{
    __HAL_LCD_CONTRAST_CONFIG(&hlcd, contrast_level(contrast));
}

/* Translate the glass frame buffer to MCU LCD RAM and request a display update.
 * Registers are written directly: HAL_LCD_UpdateDisplayRequest() busy-waits a whole frame (25 ms)
 * for the update to finish, which would cost tens of uA at one update per second. */
void disp_flush(void)
{
    if (hlcd.State != HAL_LCD_STATE_READY) return;
    for (int i = 0; i < 1000 && (LCD->SR & LCD_SR_UDR); i++) { }   /* previous update still pending */
    for (int com = 0; com < DISP_COMS; com++) {
        uint32_t ram = 0, g = disp_fb[com];
        for (int seg = 0; seg < LCD_NUM_GLASS_SEGS; seg++)
            if (g & (1UL << seg)) ram |= 1UL << LCD_GLASS_SEG_TO_MCU_SEG[seg];
        LCD->RAM[LCD_GLASS_COM_TO_MCU_COM[com] * 2U] = ram;           /* RAM0/2/4 = COM0/1/2, SEG0-31 */
    }
    LCD->SR = LCD_SR_UDR;
}
