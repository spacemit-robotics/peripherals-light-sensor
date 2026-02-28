/*
 * Copyright (C) 2026 SpacemiT (Hangzhou) Technology Co. Ltd.
 * SPDX-License-Identifier: Apache-2.0
 */
#include "light_sensor_core.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

/*
 * Force link drivers when building as static library.
 * The constructor functions won't be called automatically in static libs,
 * so we need to explicitly reference them.
 */
extern void __auto_reg_w1160_i2c_factory(void);
extern void __auto_reg_i2c_generic_factory(void);

__attribute__((constructor))
static void light_sensor_init_drivers(void)
{
    /* Force link W1160 driver */
    __auto_reg_w1160_i2c_factory();
    /* Force link generic I2C driver */
    __auto_reg_i2c_generic_factory();
}

struct light_sensor_dev *light_sensor_dev_alloc(const char *name, size_t priv_size)
{
    struct light_sensor_dev *dev;
    void *priv = NULL;
    char *name_copy = NULL;

    dev = calloc(1, sizeof(*dev));
    if (!dev)
        return NULL;

    if (priv_size) {
        priv = calloc(1, priv_size);
        if (!priv) {
            free(dev);
            return NULL;
        }
        dev->priv_data = priv;
    }

    if (name) {
        size_t n = strlen(name);
        name_copy = calloc(1, n + 1);
        if (!name_copy) {
            free(priv);
            free(dev);
            return NULL;
        }
        memcpy(name_copy, name, n);
        name_copy[n] = '\0';
        dev->name = name_copy;
    }

    return dev;
}

int light_sensor_init(struct light_sensor_dev *dev)
{
    if (!dev || !dev->ops || !dev->ops->init)
        return -1;

    return dev->ops->init(dev);
}

int light_sensor_poll(struct light_sensor_dev *dev, uint32_t *light_value)
{
    if (!dev || !dev->ops || !dev->ops->poll || !light_value)
        return -1;

    return dev->ops->poll(dev, light_value);
}

void light_sensor_free(struct light_sensor_dev *dev)
{
    if (!dev)
        return;

    if (dev->ops && dev->ops->free) {
        dev->ops->free(dev);
        return;
    }

    if (dev->priv_data)
        free(dev->priv_data);
    if (dev->name)
        free((void *)dev->name);
    free(dev);
}

/* --- driver registry (minimal, motor/nfc-like) --- */

static struct light_sensor_driver_info *g_driver_list = NULL;

void light_sensor_driver_register(struct light_sensor_driver_info *info)
{
    if (!info)
        return;
    info->next = g_driver_list;
    g_driver_list = info;
}

static struct light_sensor_driver_info *find_driver(const char *name, enum light_sensor_driver_type type)
{
    struct light_sensor_driver_info *curr = g_driver_list;
    while (curr) {
        if (curr->name && name && strcmp(curr->name, name) == 0) {
            if (curr->type == type)
                return curr;
            printf("[LIGHT_SENSOR] driver '%s' type mismatch (expected %d got %d)\n",
                name, (int)type, (int)curr->type);
            return NULL;
        }
        curr = curr->next;
    }
    printf("[LIGHT_SENSOR] driver '%s' not found\n", name ? name : "(null)");
    return NULL;
}

static int split_driver_instance(const char *name,
    char *driver, size_t driver_sz,
    const char **instance)
{
    const char *sep;
    size_t len;

    if (!name || !driver || !driver_sz || !instance)
        return -1;

    sep = strchr(name, ':');
    if (!sep)
        return 0;

    len = (size_t)(sep - name);
    if (len == 0 || len + 1 > driver_sz || !*(sep + 1))
        return -1;

    memcpy(driver, name, len);
    driver[len] = '\0';
    *instance = sep + 1;
    return 1;
}

/* --- factory functions (public API) --- */

struct light_sensor_dev *light_sensor_alloc_i2c(const char *name, const char *i2c_dev, uint8_t addr)
{
    struct light_sensor_driver_info *drv;
    struct light_sensor_args_i2c args;
    char driver[32];
    const char *instance = NULL;
    int r;

    if (!name || !i2c_dev)
        return NULL;

    /* default: name is instance, driver is "I2C" */
    strncpy(driver, "I2C", sizeof(driver) - 1);
    driver[sizeof(driver) - 1] = '\0';
    instance = name;

    r = split_driver_instance(name, driver, sizeof(driver), &instance);
    if (r < 0)
        return NULL;

    drv = find_driver(driver, LIGHT_SENSOR_DRV_I2C);
    if (!drv || !drv->factory)
        return NULL;

    args.instance = instance;
    args.dev_path = i2c_dev;
    args.addr = addr;
    return drv->factory(&args);
}
