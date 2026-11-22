/**
 * @file    hal_i2c.c
 * @brief   Blocking open-drain I2C HAL for GD32F303.
 */

#include "hal_i2c.h"
#include "hal_gpio.h"
#include "hal_systick.h"

#define I2C_STRETCH_LIMIT 1000U

typedef struct {
    hal_gpio_id_t scl;
    hal_gpio_id_t sda;
    uint8_t half_period_us;
    bool initialized;
} i2c_bus_state_t;

static i2c_bus_state_t s_buses[HAL_I2C_COUNT] = {
    { { GPIO_PORT_B, 6 }, { GPIO_PORT_B, 7 }, 5, false },
    { { GPIO_PORT_B, 10 }, { GPIO_PORT_B, 11 }, 5, false },
};

static void i2c_delay(hal_i2c_bus_t bus)
{
    hal_delay_us(s_buses[bus].half_period_us);
}

static int i2c_wait_scl_high(hal_i2c_bus_t bus)
{
    i2c_bus_state_t *state = &s_buses[bus];
    hal_gpio_set(state->scl);
    for (uint32_t i = 0; i < I2C_STRETCH_LIMIT; i++) {
        if (hal_gpio_read(state->scl)) return 0;
        hal_delay_us(1);
    }
    return -1;
}

static int i2c_start(hal_i2c_bus_t bus, bool repeated)
{
    i2c_bus_state_t *state = &s_buses[bus];
    hal_gpio_set(state->sda);
    if (i2c_wait_scl_high(bus) != 0) return -1;
    i2c_delay(bus);
    if (!repeated && !hal_gpio_read(state->sda)) return -1;
    hal_gpio_clear(state->sda);
    i2c_delay(bus);
    hal_gpio_clear(state->scl);
    i2c_delay(bus);
    return 0;
}

static int i2c_stop(hal_i2c_bus_t bus)
{
    i2c_bus_state_t *state = &s_buses[bus];
    hal_gpio_clear(state->sda);
    i2c_delay(bus);
    if (i2c_wait_scl_high(bus) != 0) return -1;
    i2c_delay(bus);
    hal_gpio_set(state->sda);
    i2c_delay(bus);
    return 0;
}

static int i2c_write_byte(hal_i2c_bus_t bus, uint8_t byte)
{
    i2c_bus_state_t *state = &s_buses[bus];
    for (uint8_t mask = 0x80; mask != 0; mask >>= 1) {
        hal_gpio_clear(state->scl);
        if (byte & mask) hal_gpio_set(state->sda);
        else hal_gpio_clear(state->sda);
        i2c_delay(bus);
        if (i2c_wait_scl_high(bus) != 0) return -1;
        i2c_delay(bus);
    }

    hal_gpio_clear(state->scl);
    hal_gpio_set(state->sda); /* Release SDA for the ACK bit. */
    i2c_delay(bus);
    if (i2c_wait_scl_high(bus) != 0) return -1;
    i2c_delay(bus);
    bool acknowledged = !hal_gpio_read(state->sda);
    hal_gpio_clear(state->scl);
    i2c_delay(bus);
    return acknowledged ? 0 : -1;
}

static int i2c_read_byte(hal_i2c_bus_t bus, uint8_t *byte, bool acknowledge)
{
    i2c_bus_state_t *state = &s_buses[bus];
    uint8_t value = 0;
    hal_gpio_set(state->sda); /* Release SDA so the slave can drive it. */

    for (uint8_t bit = 0; bit < 8; bit++) {
        hal_gpio_clear(state->scl);
        i2c_delay(bus);
        if (i2c_wait_scl_high(bus) != 0) return -1;
        i2c_delay(bus);
        value = (uint8_t)((value << 1) | (hal_gpio_read(state->sda) ? 1U : 0U));
    }

    hal_gpio_clear(state->scl);
    if (acknowledge) hal_gpio_clear(state->sda);
    else hal_gpio_set(state->sda);
    i2c_delay(bus);
    if (i2c_wait_scl_high(bus) != 0) return -1;
    i2c_delay(bus);
    hal_gpio_clear(state->scl);
    hal_gpio_set(state->sda);
    *byte = value;
    return 0;
}

static int i2c_write_address(hal_i2c_bus_t bus, uint8_t dev_addr, bool read)
{
    if (dev_addr > 0x7FU) return -1;
    return i2c_write_byte(bus, (uint8_t)((dev_addr << 1) | (read ? 1U : 0U)));
}

int hal_i2c_init(hal_i2c_bus_t bus, const hal_i2c_config_t *config)
{
    if (bus >= HAL_I2C_COUNT || !config ||
        (config->speed != HAL_I2C_SPEED_STANDARD && config->speed != HAL_I2C_SPEED_FAST)) {
        return -1;
    }

    i2c_bus_state_t *state = &s_buses[bus];
    if (hal_gpio_configure(state->scl, GPIO_MODE_OUTPUT_OD, GPIO_SPEED_2MHZ) != 0 ||
        hal_gpio_configure(state->sda, GPIO_MODE_OUTPUT_OD, GPIO_SPEED_2MHZ) != 0) {
        state->initialized = false;
        return -1;
    }
    state->half_period_us = (config->speed == HAL_I2C_SPEED_FAST) ? 1U : 5U;
    hal_gpio_set(state->scl);
    hal_gpio_set(state->sda);
    i2c_delay(bus);
    state->initialized = true;
    return (hal_gpio_read(state->scl) && hal_gpio_read(state->sda)) ? 0 : -2;
}

void hal_i2c_deinit(hal_i2c_bus_t bus)
{
    if (bus < HAL_I2C_COUNT) s_buses[bus].initialized = false;
}

int hal_i2c_write_reg(hal_i2c_bus_t bus, uint8_t dev_addr,
                      uint8_t reg_addr, const uint8_t *data, size_t len)
{
    if (bus >= HAL_I2C_COUNT || !s_buses[bus].initialized || !data || len == 0 ||
        dev_addr > 0x7FU) return -1;

    int rc = i2c_start(bus, false);
    if (rc == 0) rc = i2c_write_address(bus, dev_addr, false);
    if (rc == 0) rc = i2c_write_byte(bus, reg_addr);
    for (size_t i = 0; rc == 0 && i < len; i++) {
        rc = i2c_write_byte(bus, data[i]);
    }
    if (i2c_stop(bus) != 0 && rc == 0) rc = -2;
    return rc;
}

int hal_i2c_read_reg(hal_i2c_bus_t bus, uint8_t dev_addr,
                     uint8_t reg_addr, uint8_t *data, size_t len)
{
    if (bus >= HAL_I2C_COUNT || !s_buses[bus].initialized || !data || len == 0 ||
        dev_addr > 0x7FU) return -1;

    int rc = i2c_start(bus, false);
    if (rc == 0) rc = i2c_write_address(bus, dev_addr, false);
    if (rc == 0) rc = i2c_write_byte(bus, reg_addr);
    if (rc == 0) rc = i2c_start(bus, true);
    if (rc == 0) rc = i2c_write_address(bus, dev_addr, true);
    for (size_t i = 0; rc == 0 && i < len; i++) {
        rc = i2c_read_byte(bus, &data[i], i + 1U < len);
    }
    if (i2c_stop(bus) != 0 && rc == 0) rc = -2;
    return rc;
}

int hal_i2c_write_reg8(hal_i2c_bus_t bus, uint8_t dev_addr,
                       uint8_t reg_addr, uint8_t value)
{
    return hal_i2c_write_reg(bus, dev_addr, reg_addr, &value, 1);
}

int hal_i2c_read_reg8(hal_i2c_bus_t bus, uint8_t dev_addr, uint8_t reg_addr)
{
    uint8_t value = 0;
    int rc = hal_i2c_read_reg(bus, dev_addr, reg_addr, &value, 1);
    return rc == 0 ? value : rc;
}

int hal_i2c_write_reg16(hal_i2c_bus_t bus, uint8_t dev_addr,
                        uint8_t reg_addr, uint16_t value)
{
    uint8_t bytes[2] = { (uint8_t)(value >> 8), (uint8_t)value };
    return hal_i2c_write_reg(bus, dev_addr, reg_addr, bytes, sizeof(bytes));
}

int hal_i2c_read_reg16(hal_i2c_bus_t bus, uint8_t dev_addr, uint8_t reg_addr)
{
    uint8_t bytes[2] = { 0, 0 };
    int rc = hal_i2c_read_reg(bus, dev_addr, reg_addr, bytes, sizeof(bytes));
    return rc == 0 ? (int)(((uint16_t)bytes[0] << 8) | bytes[1]) : rc;
}

bool hal_i2c_probe(hal_i2c_bus_t bus, uint8_t dev_addr)
{
    if (bus >= HAL_I2C_COUNT || !s_buses[bus].initialized || dev_addr > 0x7FU) return false;
    int rc = i2c_start(bus, false);
    if (rc == 0) rc = i2c_write_address(bus, dev_addr, false);
    if (i2c_stop(bus) != 0 && rc == 0) rc = -2;
    return rc == 0;
}

int hal_i2c_scan(hal_i2c_bus_t bus, uint8_t *addrs, size_t max_count)
{
    if (bus >= HAL_I2C_COUNT || !s_buses[bus].initialized || (max_count > 0 && !addrs)) {
        return -1;
    }

    int found = 0;
    for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
        if (hal_i2c_probe(bus, addr)) {
            if ((size_t)found < max_count) addrs[found] = addr;
            found++;
        }
    }
    return found;
}
