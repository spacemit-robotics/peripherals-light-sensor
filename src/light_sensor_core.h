/*
 * Copyright (C) 2026 SpacemiT (Hangzhou) Technology Co. Ltd.
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef LIGHT_SENSOR_CORE_H
#define LIGHT_SENSOR_CORE_H

/*
 * Private header for light_sensor component (motor/nfc-like minimal style).
 */

#include "../include/light_sensor.h"
#include <stddef.h>

/* 1. 参数适配包：用于将 alloc 参数打包成 void*（同时携带 instance 名称） */
struct light_sensor_args_i2c {
    const char *instance;
    const char *dev_path;
    uint8_t addr;
};

/* 2. 驱动类型枚举 */
enum light_sensor_driver_type {
    LIGHT_SENSOR_DRV_I2C = 0,
};

/* 3. 虚函数表（驱动实现） */
struct light_sensor_ops {
    int (*init)(struct light_sensor_dev *dev);
    int (*poll)(struct light_sensor_dev *dev, uint32_t *light_value);
    void (*free)(struct light_sensor_dev *dev);
};

/* 4. 设备对象（私有实现） */
struct light_sensor_dev {
    const char *name; /* instance name */
    const struct light_sensor_ops *ops;
    void *priv_data;
};

/* 5. 通用工厂函数类型 */
typedef struct light_sensor_dev *(*light_sensor_factory_t)(void *args);

/* 6. 注册节点结构 */
struct light_sensor_driver_info {
    const char *name;                     /* driver name */
    enum light_sensor_driver_type type;   /* bus type */
    light_sensor_factory_t factory;
    struct light_sensor_driver_info *next;
};

void light_sensor_driver_register(struct light_sensor_driver_info *info);

#define REGISTER_LIGHT_SENSOR_DRIVER(_name, _type, _factory) \
    static struct light_sensor_driver_info __drv_info_##_factory = { \
        .name = _name, \
        .type = _type, \
        .factory = _factory, \
        .next = 0 \
    }; \
    void __auto_reg_##_factory(void) { \
        light_sensor_driver_register(&__drv_info_##_factory); \
    }

struct light_sensor_dev *light_sensor_dev_alloc(const char *instance, size_t priv_size);

#endif /* LIGHT_SENSOR_CORE_H */
