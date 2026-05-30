/*
 * Copyright (C) 2026 SpacemiT (Hangzhou) Technology Co. Ltd.
 * SPDX-License-Identifier: Apache-2.0
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "light_sensor_core.h"

void __auto_reg_w1160_i2c_factory(void) {}
void __auto_reg_i2c_generic_factory(void) {}

struct mock_light_priv {
    int initialized;
    uint32_t value;
};

static int g_failures;
static int g_free_count;
static int g_registered;

#define CHECK_TRUE(expr) do { \
    if (!(expr)) { \
        printf("FAIL:%s:%d: expected true: %s\n", __FILE__, __LINE__, #expr); \
        g_failures++; \
    } \
} while (0)

#define CHECK_INT_EQ(actual, expected) do { \
    int _actual = (int)(actual); \
    int _expected = (int)(expected); \
    if (_actual != _expected) { \
        printf("FAIL:%s:%d: expected %s == %d, got %d\n", \
            __FILE__, __LINE__, #actual, _expected, _actual); \
        g_failures++; \
    } \
} while (0)

static void reset_test_state(void)
{
    g_failures = 0;
    g_free_count = 0;
}

static int mock_init(struct light_sensor_dev *dev)
{
    struct mock_light_priv *priv;

    if (!dev || !dev->priv_data)
        return -EINVAL;

    priv = dev->priv_data;
    priv->initialized = 1;
    return 0;
}

static int mock_poll(struct light_sensor_dev *dev, uint32_t *light_value)
{
    struct mock_light_priv *priv;

    if (!dev || !dev->priv_data || !light_value)
        return -EINVAL;

    priv = dev->priv_data;
    if (!priv->initialized)
        return -EAGAIN;

    *light_value = priv->value;
    priv->value += 11;
    return 0;
}

static void mock_free(struct light_sensor_dev *dev)
{
    if (!dev)
        return;

    g_free_count++;
    free(dev->priv_data);
    free((void *)dev->name);
    free(dev);
}

static const struct light_sensor_ops mock_ops = {
    .init = mock_init,
    .poll = mock_poll,
    .free = mock_free,
};

static struct light_sensor_dev *mock_factory(void *args)
{
    struct light_sensor_args_i2c *i2c_args = args;
    struct light_sensor_dev *dev;
    struct mock_light_priv *priv;

    if (!i2c_args || !i2c_args->instance || !i2c_args->dev_path)
        return NULL;

    dev = light_sensor_dev_alloc(i2c_args->instance, sizeof(*priv));
    if (!dev)
        return NULL;

    dev->ops = &mock_ops;
    priv = dev->priv_data;
    priv->value = 1234;
    return dev;
}

static void register_mock_driver(void)
{
    static struct light_sensor_driver_info info = {
        .name = "MOCK",
        .type = LIGHT_SENSOR_DRV_I2C,
        .factory = mock_factory,
        .next = NULL,
    };

    if (!g_registered) {
        light_sensor_driver_register(&info);
        g_registered = 1;
    }
}

static void test_error_paths(void)
{
    struct light_sensor_dev *dev;
    uint32_t value = 0;

    register_mock_driver();

    CHECK_INT_EQ(light_sensor_init(NULL), -1);
    CHECK_INT_EQ(light_sensor_poll(NULL, &value), -1);
    CHECK_INT_EQ(light_sensor_poll(NULL, NULL), -1);
    light_sensor_free(NULL);

    CHECK_TRUE(light_sensor_alloc_i2c(NULL, "mock://als", 0x48) == NULL);
    CHECK_TRUE(light_sensor_alloc_i2c("MOCK:als0", NULL, 0x48) == NULL);
    CHECK_TRUE(light_sensor_alloc_i2c("MOCK:", "mock://als", 0x48) == NULL);
    CHECK_TRUE(light_sensor_alloc_i2c(":als0", "mock://als", 0x48) == NULL);
    CHECK_TRUE(light_sensor_alloc_i2c("MISSING:als0", "mock://als", 0x48) == NULL);

    dev = light_sensor_alloc_i2c("MOCK:not-ready", "mock://als", 0x48);
    CHECK_TRUE(dev != NULL);
    if (!dev)
        return;

    CHECK_INT_EQ(light_sensor_poll(dev, &value), -EAGAIN);
    CHECK_INT_EQ(light_sensor_poll(dev, NULL), -1);
    light_sensor_free(dev);
    CHECK_INT_EQ(g_free_count, 1);
}

static void test_functional(void)
{
    struct light_sensor_dev *dev;
    uint32_t value = 0;

    register_mock_driver();

    dev = light_sensor_alloc_i2c("MOCK:als0", "mock://als", 0x48);
    CHECK_TRUE(dev != NULL);
    if (!dev)
        return;

    CHECK_INT_EQ(light_sensor_init(dev), 0);
    CHECK_INT_EQ(light_sensor_poll(dev, &value), 0);
    CHECK_INT_EQ(value, 1234);
    CHECK_INT_EQ(light_sensor_poll(dev, &value), 0);
    CHECK_INT_EQ(value, 1245);

    light_sensor_free(dev);
    CHECK_INT_EQ(g_free_count, 1);
}

static int finish_test(const char *name)
{
    if (g_failures != 0) {
        printf("%s FAILED: %d failure(s)\n", name, g_failures);
        return 1;
    }
    printf("%s PASSED\n", name);
    return 0;
}

int main(int argc, char **argv)
{
    const char *mode = (argc > 1) ? argv[1] : "all";

    if (strcmp(mode, "functional") == 0) {
        reset_test_state();
        test_functional();
        return finish_test("light_sensor api functional test");
    }
    if (strcmp(mode, "error-paths") == 0) {
        reset_test_state();
        test_error_paths();
        return finish_test("light_sensor api error paths test");
    }
    if (strcmp(mode, "all") == 0) {
        reset_test_state();
        test_functional();
        if (finish_test("light_sensor api functional test") != 0)
            return 1;
        reset_test_state();
        test_error_paths();
        if (finish_test("light_sensor api error paths test") != 0)
            return 1;
        printf("light_sensor api contract test PASSED\n");
        return 0;
    }

    fprintf(stderr, "usage: %s [all|functional|error-paths]\n", argv[0]);
    return 2;
}
