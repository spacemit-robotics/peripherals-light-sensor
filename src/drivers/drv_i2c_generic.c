/*
 * Copyright (C) 2026 SpacemiT (Hangzhou) Technology Co. Ltd.
 * SPDX-License-Identifier: Apache-2.0
 */
/*
 * Generic I2C driver for light sensor
 * Provides a simple framework for I2C-based ambient light sensors.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "light_sensor_core.h"

#ifndef I2C_SLAVE
#define I2C_SLAVE 0x0703
#endif

/* #define LIGHT_SENSOR_I2C_DEBUG */

#ifdef LIGHT_SENSOR_I2C_DEBUG
#define ls_log(...) do { \
    printf("[LIGHT_SENSOR_I2C] "); \
    printf(__VA_ARGS__); \
} while (0)
#else
#define ls_log(...) do {} while (0)
#endif

/* ---------- private driver state ---------- */
struct i2c_generic_priv {
    char dev_path[64];
    uint8_t addr;
    int fd;
};

/* ---------- low-level I2C access ---------- */

static int i2c_write_reg(int fd, uint8_t reg, uint8_t value)
{
    uint8_t buffer[2];
    buffer[0] = reg;
    buffer[1] = value;
    ssize_t wr = write(fd, buffer, 2);
    return (wr == 2) ? 0 : -1;
}

static int i2c_read_reg(int fd, uint8_t reg, uint8_t *data, size_t len)
{
    ssize_t wr = write(fd, &reg, 1);
    if (wr != 1)
        return -1;
    ssize_t rd = read(fd, data, len);
    return (rd == (ssize_t)len) ? 0 : -1;
}

/* ---------- ops implementation ---------- */

static int i2c_generic_init(struct light_sensor_dev *dev)
{
    struct i2c_generic_priv *priv;
    int fd;

    if (!dev || !dev->priv_data)
        return -1;

    priv = (struct i2c_generic_priv *)dev->priv_data;

    fd = open(priv->dev_path, O_RDWR);
    if (fd < 0) {
        ls_log("open %s failed: %s\n", priv->dev_path, strerror(errno));
        return -1;
    }

    if (ioctl(fd, I2C_SLAVE, priv->addr) < 0) {
        ls_log("ioctl I2C_SLAVE 0x%02X failed: %s\n", priv->addr, strerror(errno));
        close(fd);
        return -1;
    }

    priv->fd = fd;

    ls_log("init OK: dev=%s addr=0x%02X fd=%d\n", priv->dev_path, priv->addr, fd);
    return 0;
}

static int i2c_generic_poll(struct light_sensor_dev *dev, uint32_t *light_value)
{
    struct i2c_generic_priv *priv;
    uint8_t data[2] = {0};
    int ret;

    if (!dev || !dev->priv_data || !light_value)
        return -1;

    priv = (struct i2c_generic_priv *)dev->priv_data;

    if (priv->fd < 0)
        return -1;


    i2c_write_reg(priv->fd, 0x00, 0x00);  // Dummy write to set register pointer
    /*
     * Generic read: read 2 bytes from register 0x00.
     * The actual register depends on the specific sensor chip.
     * Subclass or configure for specific sensors.
     */
    ret = i2c_read_reg(priv->fd, 0x00, data, 2);
    if (ret < 0) {
        ls_log("read light value failed\n");
        return -1;
    }

    /* Combine as 16-bit value (big-endian or little-endian depends on chip) */
    *light_value = (uint32_t)((data[0] << 8) | data[1]);

    ls_log("poll: raw=%02X %02X => %u\n", data[0], data[1], *light_value);
    return 0;
}

static void i2c_generic_free(struct light_sensor_dev *dev)
{
    struct i2c_generic_priv *priv;

    if (!dev)
        return;

    priv = (struct i2c_generic_priv *)dev->priv_data;
    if (priv) {
        if (priv->fd >= 0) {
            close(priv->fd);
            priv->fd = -1;
        }
        free(priv);
    }

    if (dev->name)
        free((void *)dev->name);

    free(dev);
}

static const struct light_sensor_ops i2c_generic_ops = {
    .init = i2c_generic_init,
    .poll = i2c_generic_poll,
    .free = i2c_generic_free,
};

/* ---------- factory function ---------- */

static struct light_sensor_dev *i2c_generic_factory(void *args)
{
    struct light_sensor_args_i2c *a = (struct light_sensor_args_i2c *)args;
    struct light_sensor_dev *dev;
    struct i2c_generic_priv *priv;

    if (!a || !a->dev_path)
        return NULL;

    dev = light_sensor_dev_alloc(a->instance, sizeof(struct i2c_generic_priv));
    if (!dev)
        return NULL;

    priv = (struct i2c_generic_priv *)dev->priv_data;
    strncpy(priv->dev_path, a->dev_path, sizeof(priv->dev_path) - 1);
    priv->dev_path[sizeof(priv->dev_path) - 1] = '\0';
    priv->addr = a->addr;
    priv->fd = -1;

    dev->ops = &i2c_generic_ops;

    ls_log("factory: instance=%s dev=%s addr=0x%02X\n",
        a->instance ? a->instance : "(null)",
        a->dev_path, a->addr);

    return dev;
}

REGISTER_LIGHT_SENSOR_DRIVER("I2C", LIGHT_SENSOR_DRV_I2C, i2c_generic_factory);
