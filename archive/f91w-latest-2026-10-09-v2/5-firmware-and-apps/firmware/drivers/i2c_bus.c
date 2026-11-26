#include "i2c_bus.h"
#include "board.h"

static I2C_HandleTypeDef hi2c1;

void HAL_I2C_MspInit(I2C_HandleTypeDef *h)
{
    (void)h;
    RCC_PeriphCLKInitTypeDef clk = {0};
    clk.PeriphClockSelection = RCC_PERIPHCLK_I2C1;
    clk.I2c1ClockSelection = RCC_I2C1CLKSOURCE_PCLK1;
    HAL_RCCEx_PeriphCLKConfig(&clk);
    __HAL_RCC_I2C1_CLK_ENABLE();

    GPIO_InitTypeDef g = {0};
    g.Pin = BOARD_MASK(PIN_I2C_SCL) | BOARD_MASK(PIN_I2C_SDA);
    g.Mode = GPIO_MODE_AF_OD;
    g.Pull = GPIO_NOPULL;                     /* external 10k */
    g.Speed = GPIO_SPEED_FREQ_LOW;
    g.Alternate = GPIO_AF4_I2C1;
    HAL_GPIO_Init(GPIOB, &g);
}

void i2c_bus_init(void)
{
    hi2c1.Instance = I2C1;
    /* 400 kHz from a 32 MHz PCLK1: PRESC=3 (125 ns), SCLDEL=3, SDADEL=1, SCLH=3, SCLL=9
     * (RM0434 fast-mode example timings, scaled from 8 MHz). */
    hi2c1.Init.Timing = 0x30310309;
    hi2c1.Init.OwnAddress1 = 0;
    hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    HAL_I2C_Init(&hi2c1);
    HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE);
}

bool i2c_write_reg(uint8_t addr7, uint8_t reg, uint8_t val)
{
    return HAL_I2C_Mem_Write(&hi2c1, (uint16_t)(addr7 << 1), reg, I2C_MEMADD_SIZE_8BIT, &val, 1, 10) == HAL_OK;
}

bool i2c_read_regs(uint8_t addr7, uint8_t reg, uint8_t *buf, uint16_t len)
{
    return HAL_I2C_Mem_Read(&hi2c1, (uint16_t)(addr7 << 1), reg, I2C_MEMADD_SIZE_8BIT, buf, len, 20) == HAL_OK;
}
