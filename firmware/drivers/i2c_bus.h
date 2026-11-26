/* I2C1 on PB8/PB9 (10k pull-ups), 400 kHz, blocking register access. */
#ifndef I2C_BUS_H
#define I2C_BUS_H
#include <stdbool.h>
#include <stdint.h>
void i2c_bus_init(void);
bool i2c_write_reg(uint8_t addr7, uint8_t reg, uint8_t val);
bool i2c_read_regs(uint8_t addr7, uint8_t reg, uint8_t *buf, uint16_t len);
#endif
