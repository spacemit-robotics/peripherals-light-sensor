/*
 * Copyright (C) 2026 SpacemiT (Hangzhou) Technology Co. Ltd.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Test program for W1160 light sensor I2C driver
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "../include/light_sensor.h"

int main(int argc, char **argv)
{
    const char *dev_path = "/dev/i2c-5";
    uint8_t addr = 0x48;  /* W1160 default I2C address */

    if (argc > 1)
        dev_path = argv[1];
    if (argc > 2)
        addr = (uint8_t)strtoul(argv[2], NULL, 0);

    printf("W1160 Light Sensor I2C Test\n");
    printf("Using: dev=%s addr=0x%02X\n", dev_path, addr);

    /* Allocate device using W1160 driver */
    struct light_sensor_dev *dev = light_sensor_alloc_i2c("W1160:als0", dev_path, addr);
    if (!dev) {
        fprintf(stderr, "alloc W1160 i2c failed\n");
        return -1;
    }

    /* Initialize */
    if (light_sensor_init(dev) < 0) {
        fprintf(stderr, "W1160 init failed\n");
        light_sensor_free(dev);
        return -1;
    }

    printf("W1160 initialization OK, polling light values...\n");

    /* Poll 20 times */
    for (int i = 0; i < 200; i++) {
        uint32_t light_value = 0;
        int ret = light_sensor_poll(dev, &light_value);
        if (ret == 0) {
            printf("[%d] Light value: %u lux\n", i, light_value);
        } else {
            printf("[%d] Poll failed (ret=%d)\n", i, ret);
        }
        usleep(1000000); /* 1000ms - W1160 needs time for FIFO data */
    }

    /* Cleanup */
    light_sensor_free(dev);
    printf("Test complete.\n");

    return 0;
}
