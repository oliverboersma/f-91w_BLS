#include "lis2dw12.h"
#include "i2c_bus.h"
#include "board_f91w_ble.h"
#include "stm32wbxx_hal.h"

#define ADDR            I2C_ADDR_LIS2DW12
#define REG_WHO_AM_I    0x0F
#define REG_CTRL1       0x20
#define REG_CTRL2       0x21
#define REG_CTRL3       0x22
#define REG_CTRL4_INT1  0x23
#define REG_CTRL6       0x25
#define REG_OUT_X_L     0x28
#define REG_FIFO_CTRL   0x2E
#define REG_FIFO_SAMPLES 0x2F
#define REG_CTRL7       0x3F

bool lis2dw12_init(void)
{
    uint8_t id = 0;
    if (!i2c_read_regs(ADDR, REG_WHO_AM_I, &id, 1) || id != 0x44) return false;
    i2c_write_reg(ADDR, REG_CTRL2, 0x40);                   /* SOFT_RESET */
    HAL_Delay(2);
    i2c_write_reg(ADDR, REG_CTRL2, 0x0C);                   /* BDU, IF_ADD_INC */
    i2c_write_reg(ADDR, REG_CTRL6, 0x00);                   /* BW ODR/2, +-2 g, low noise off */
    i2c_write_reg(ADDR, REG_FIFO_CTRL, (0x6 << 5) | LIS2DW12_FIFO_WTM); /* continuous mode, threshold */
    i2c_write_reg(ADDR, REG_CTRL4_INT1, 0x02);              /* INT1_FTH */
    i2c_write_reg(ADDR, REG_CTRL7, 0x20);                   /* INTERRUPTS_ENABLE */
    i2c_write_reg(ADDR, REG_CTRL1, 0x20);                   /* ODR 12.5 Hz, low-power mode 1 (12 bit) */
    return true;
}

int lis2dw12_read_fifo(float (*xyz)[3], int max)
{
    uint8_t st = 0;
    if (!i2c_read_regs(ADDR, REG_FIFO_SAMPLES, &st, 1)) return 0;
    int n = st & 0x3F;
    if (n > max) n = max;
    for (int i = 0; i < n; i++) {
        uint8_t b[6];
        if (!i2c_read_regs(ADDR, REG_OUT_X_L, b, 6)) return i;
        for (int k = 0; k < 3; k++) {
            int16_t raw = (int16_t)(b[2 * k] | (b[2 * k + 1] << 8));   /* left-justified */
            xyz[i][k] = raw * (1.0f / 16384.0f);                        /* +-2 g full scale */
        }
    }
    return n;
}
