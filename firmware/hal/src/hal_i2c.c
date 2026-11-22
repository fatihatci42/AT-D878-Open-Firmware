/**
 * @file    hal_i2c.c
 * @brief   I2C HAL Implementation - AT-D878 Open Firmware
 * @version 0.0.1
 */

#include "hal_i2c.h"

int hal_i2c_init(hal_i2c_bus_t bus, const hal_i2c_config_t *config)
{
    (void)bus; (void)config;
    return 0;
}

void hal_i2c_deinit(hal_i2c_bus_t bus)
{
    (void)bus;
}

int hal_i2c_write_reg(hal_i2c_bus_t bus, uint8_t dev_addr,
                      uint8_t reg_addr, const uint8_t *data, size_t len)
{
    (void)bus; (void)dev_addr; (void)reg_addr; (void)data; (void)len;
    return 0;
}

int hal_i2c_read_reg(hal_i2c_bus_t bus, uint8_t dev_addr,
                     uint8_t reg_addr, uint8_t *data, size_t len)
{
    (void)bus; (void)dev_addr; (void)reg_addr; (void)data;
    for (size_t i = 0; i < len; i++) data[i] = 0;
    return 0;
}

int hal_i2c_write_reg8(hal_i2c_bus_t bus, uint8_t dev_addr, uint8_t reg_addr, uint8_t value)
{
    return hal_i2c_write_reg(bus, dev_addr, reg_addr, &value, 1);
}

int hal_i2c_read_reg8(hal_i2c_bus_t bus, uint8_t dev_addr, uint8_t reg_addr)
{
    uint8_t val = 0;
    hal_i2c_read_reg(bus, dev_addr, reg_addr, &val, 1);
    return val;
}

int hal_i2c_write_reg16(hal_i2c_bus_t bus, uint8_t dev_addr, uint8_t reg_addr, uint16_t value)
{
    uint8_t buf[2];
    buf[0] = (uint8_t)(value >> 8);
    buf[1] = (uint8_t)(value & 0xFF);
    return hal_i2c_write_reg(bus, dev_addr, reg_addr, buf, 2);
}

int hal_i2c_read_reg16(hal_i2c_bus_t bus, uint8_t dev_addr, uint8_t reg_addr)
{
    uint8_t buf[2] = {0, 0};
    hal_i2c_read_reg(bus, dev_addr, reg_addr, buf, 2);
    return ((uint16_t)buf[0] << 8) | buf[1];
}

bool hal_i2c_probe(hal_i2c_bus_t bus, uint8_t dev_addr)
{
    (void)bus; (void)dev_addr;
    return true;
}

int hal_i2c_scan(hal_i2c_bus_t bus, uint8_t *addrs, size_t max_count)
{
    (void)bus; (void)addrs; (void)max_count;
    return 0;
}
