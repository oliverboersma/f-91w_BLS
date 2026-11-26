#include "maxm86161.h"
#include "i2c_bus.h"
#include "board.h"

#define ADDR            I2C_ADDR_MAXM86161
#define REG_INT_STATUS1 0x00
#define REG_INT_EN1     0x02
#define REG_FIFO_CNT    0x07
#define REG_FIFO_DATA   0x08
#define REG_FIFO_CFG1   0x09
#define REG_FIFO_CFG2   0x0A
#define REG_SYS_CTRL    0x0D
#define REG_PPG_CFG1    0x11
#define REG_PPG_CFG2    0x12
#define REG_PPG_CFG3    0x13
#define REG_PD_BIAS     0x15
#define REG_LED_SEQ1    0x20
#define REG_LED_SEQ2    0x21
#define REG_LED_SEQ3    0x22
#define REG_LED1_PA     0x23
#define REG_LED_RANGE1  0x2A
#define REG_PART_ID     0xFF
#define PART_ID         0x36

#define SYS_RESET       0x01
#define SYS_SHDN        0x02
#define SYS_SINGLE_PPG  0x08
#define TAG_PPG1_LEDC1  0x01

static bool running;

static void irq(bool on)
{
    if (on) { __HAL_GPIO_EXTI_CLEAR_IT(EXTI_PIN_PPG_INT); HAL_NVIC_EnableIRQ(EXTI2_IRQn); }
    else    HAL_NVIC_DisableIRQ(EXTI2_IRQn);
}

bool maxm86161_start(uint8_t led_ma)
{
    uint8_t id = 0, st;
    board_ppg_power(true);
    HAL_Delay(5);                                   /* boost soft start + module POR */
    if (!i2c_read_regs(ADDR, REG_PART_ID, &id, 1) || id != PART_ID) {
        board_ppg_power(false);
        return false;
    }
    i2c_write_reg(ADDR, REG_SYS_CTRL, SYS_RESET);
    HAL_Delay(2);
    i2c_write_reg(ADDR, REG_SYS_CTRL, SYS_SHDN);     /* configure while shut down */
    i2c_read_regs(ADDR, REG_INT_STATUS1, &st, 1);   /* clear PWR_RDY */

    if (led_ma > 31) led_ma = 31;
    i2c_write_reg(ADDR, REG_PPG_CFG1, 0x08 | 0x03);  /* ALC on, ADC range 16 uA, t_int 123.8 us */
    i2c_write_reg(ADDR, REG_PPG_CFG2, 0x00);         /* 25 sps, no averaging */
    i2c_write_reg(ADDR, REG_PPG_CFG3, 0x40);         /* LED settling 6 us */
    i2c_write_reg(ADDR, REG_PD_BIAS, 0x01);          /* PD capacitance 0-65 pF */
    i2c_write_reg(ADDR, REG_LED_RANGE1, 0x00);       /* 31 mA full scale, 0.12 mA/LSB */
    i2c_write_reg(ADDR, REG_LED1_PA, (uint8_t)((led_ma * 100U + 6U) / 12U > 255 ? 255 : (led_ma * 100U + 6U) / 12U));
    i2c_write_reg(ADDR, REG_LED_SEQ1, 0x01);         /* LEDC1 = LED1 (green), LEDC2 off */
    i2c_write_reg(ADDR, REG_LED_SEQ2, 0x00);
    i2c_write_reg(ADDR, REG_LED_SEQ3, 0x00);
    i2c_write_reg(ADDR, REG_FIFO_CFG1, 128 - MAXM86161_FIFO_IRQ); /* A_FULL with 25 samples unread */
    i2c_write_reg(ADDR, REG_FIFO_CFG2, 0x10 | 0x08 | 0x02);       /* flush, clear on data read, roll over */
    i2c_write_reg(ADDR, REG_INT_EN1, 0x80);          /* A_FULL */
    irq(true);
    i2c_write_reg(ADDR, REG_SYS_CTRL, SYS_SINGLE_PPG);
    running = true;
    return true;
}

int maxm86161_read_fifo(uint32_t *out, int max)
{
    uint8_t cnt = 0, st, b[3 * 32];
    int got = 0;
    if (!running) return 0;
    i2c_read_regs(ADDR, REG_INT_STATUS1, &st, 1);
    if (!i2c_read_regs(ADDR, REG_FIFO_CNT, &cnt, 1)) return 0;
    while (cnt > 0 && got < max) {
        int n = cnt > 32 ? 32 : cnt;
        if (!i2c_read_regs(ADDR, REG_FIFO_DATA, b, (uint16_t)(3 * n))) break;
        for (int i = 0; i < n; i++) {
            uint8_t tag = (uint8_t)((b[3 * i] >> 3) & 0x1F);
            uint32_t v = ((uint32_t)(b[3 * i] & 0x07) << 16) | ((uint32_t)b[3 * i + 1] << 8) | b[3 * i + 2];
            if (tag == TAG_PPG1_LEDC1 && got < max) out[got++] = v;
        }
        cnt = (uint8_t)(cnt - n);
    }
    return got;
}

void maxm86161_stop(void)
{
    irq(false);
    if (running) i2c_write_reg(ADDR, REG_SYS_CTRL, SYS_SHDN);
    running = false;
    board_ppg_power(false);
}
