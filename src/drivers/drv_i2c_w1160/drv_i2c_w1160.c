/*
 * Copyright (C) 2026 SpacemiT (Hangzhou) Technology Co. Ltd.
 * SPDX-License-Identifier: Apache-2.0
 */
/*
 * W1160 I2C driver for light sensor
 * I2C address (7 bit) = 0x48
 *
 * Adapted to light_sensor driver framework.
 */

#include "drv_i2c_w1160.h"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "light_sensor_core.h"

#ifndef I2C_SLAVE
#define I2C_SLAVE 0x0703
#endif


// #define USE_HW_AGC

#define ALS_FIFO_DATA_MAX     512
#define ALS_READ_FIFO_BYTE    64
#define ALS_TARGET_FIFO_DATA  64

#define W1160_ALS0_MAX_GAIN 1024*4*6
#define W1160_ALS1_MAX_GAIN 1024*4*2

#define STK_ALGO_INSTANCE_ID  0

// #define SCL_DEBUG
// #define SCL_INFO
#define SCL_ERR

#ifdef SCL_DEBUG
#define scl_log_debug(...) printf("%s,%d,debug:", __func__, __LINE__); printf(__VA_ARGS__)
#else
#define scl_log_debug(...)
#endif

#ifdef SCL_INFO
#define scl_log_info(...) printf("%s,%d,info:", __func__, __LINE__); printf(__VA_ARGS__)
#else
#define scl_log_info(...)
#endif

#ifdef SCL_ERR
#define scl_log_err(...) printf("%s,%d,err:", __func__, __LINE__); printf(__VA_ARGS__)
#else
#define scl_log_err(...)
#endif

/* ---------- private driver state ---------- */
struct w1160_i2c_priv {
    char dev_path[64];
    uint8_t addr;
    int fd;
    bool initialized;
    float last_lux_data;
};

/* ---------- W1160 register/function declarations ---------- */
static void w1160_Init(int _file);
static uint32_t w1160_enable_ALS(int _file);
static uint32_t w1160_DisEnable_ALS(int _file);
static float getAls(int _file, float *last_lux);
static void w1160_Delay(uint16_t ms);
static void w1160_sw_reset(int _file);
static void w1160_PowerOn(int _file);
#ifdef SCL_DEBUG
static void w1160_Show_Allreg(int _file);
#endif

/* ---------- Lock file helpers ---------- */
/**
 * Create lock file in /var/lock for I2C device
 * Returns lock file descriptor, or -1 on error
 */
static int w1160_lock_acquire(const char *dev_path)
{
    char lock_path[128];
    int lock_fd;

    if (!dev_path)
        return -1;

    /* Convert /dev/i2c-X to /var/lock/w1160_i2c_X.lock */
    snprintf(lock_path, sizeof(lock_path), "/var/lock/w1160_%s.lock",
        dev_path + strlen(dev_path) - 1);  /* Get last char (device number) */

    lock_fd = open(lock_path, O_CREAT | O_RDWR, 0644);
    if (lock_fd < 0) {
        scl_log_err("Failed to open lock file %s: %s\n", lock_path, strerror(errno));
        return -1;
    }

    /* Try to acquire exclusive lock (blocking) */
    if (flock(lock_fd, LOCK_EX) < 0) {
        scl_log_err("Failed to acquire lock on %s: %s\n", lock_path, strerror(errno));
        close(lock_fd);
        return -1;
    }

    scl_log_debug("Lock acquired for %s\n", dev_path);
    return lock_fd;
}

/**
 * Release lock file
 */
static void w1160_lock_release(int lock_fd)
{
    if (lock_fd < 0)
        return;

    flock(lock_fd, LOCK_UN);
    close(lock_fd);
    scl_log_debug("Lock released\n");
}

static int w1160_write_to_i2c(int file_, uint8_t reg, uint8_t value) {
    uint8_t buffer[2];
    buffer[0] = reg;     // 寄存器地址
    buffer[1] = value;   // 写入的值

    if (write(file_, buffer, 2) != 2) {
        // printf("I2C write failed\n");
        return -1;
    }
    return 0;
}

// 从I2C设备读取数据
static int w1160_read_from_i2c(int file_, uint8_t reg, uint8_t *data_, size_t len)
{
    if (write(file_, &reg, 1) != 1) {
        // printf("I2C write register failed\n");
        return -1;
    }

    size_t read_bytes = read(file_, data_, len);
    if (read_bytes != len) {
        // printf("I2C read failed\n");
        return -1;
    }
    return 0;
}

#ifdef SCL_DEBUG
static uint8_t w1160_ReadReg(int _file, uint8_t adr)
{
    uint8_t data[2] = {};  // 用于存储读取到的数据

    if (w1160_read_from_i2c(_file, adr, data, 1)) {
        scl_log_err("read fial, \n");
    }

    return data[0];
}
#endif

static uint8_t w1160_MultiReadReg(int _file, uint8_t adr, uint8_t *data, size_t len)
{
    if (w1160_read_from_i2c(_file, adr, data, len)) {
        scl_log_err("read fial, \n");
    }

    return 0;
}

static uint8_t w1160_WriteReg(int _file, uint8_t adr, uint8_t value)
{
    if (w1160_write_to_i2c(_file, adr, value)) {
        scl_log_err("write fail, \n");
        return -1;
    } else {
        return 0;
    }
}

#ifdef SCL_DEBUG
void w1160_Show_Allreg(int _file)
{
    char reg[29] = {0x00, 0x02, 0x03, 0x05, 0x06,
                    0x07, 0x08, 0x10, 0x13, 0x14,
                    0x3D, 0x3E, 0x60, 0x61, 0x62,
                    0x63, 0x64, 0x65, 0x67, 0x68,
                    0x69, 0x6A, 0x6B, 0x6C, 0x6D,
                    0x17, 0x6E, 0x6F, 0xA4};

    int i = 0;

    uint8_t val = 0;
    for (i = 0; i < 29; i++) {
        val = w1160_ReadReg(_file, reg[i]);
        scl_log_debug("w1160_Show_Allreg Reg(%x) = %x\n", reg[i], val);
    }
}
#endif

static AlgoParam CaliData;
static void w1160_PowerOn(int _file)
{
    w1160_sw_reset(_file);
    w1160_Delay(20);

#ifdef SCL_DEBUG
    uint8_t ID = w1160_ReadReg(_file, 0x3E);
    scl_log_info("w1160_PowerOn ID = 0x%x, 0x%x\r\n", ID,
        w1160_ReadReg(_file, 0x3F));
#endif

    w1160_Init(_file);

#ifdef SCL_DEBUG
    w1160_Show_Allreg(_file);
#endif

    // Calibration Parameter Configure
    // DO NOT MODIFIED !!
    CaliData.DisplayNode[0].Brightness = 255;
    CaliData.DisplayNode[0].Alpha.ParameterG = 1.33288;
    CaliData.DisplayNode[0].Beta.ParameterG = 1.0f;
    CaliData.DisplayNode[0].Bias.ParameterG = -24.70069;
    CaliData.DisplayNode[0].LowTHD.ChannelG = 15;
    CaliData.DisplayNode[0].HighTHD.ChannelG = 4597;


    CaliData.DebouncePct = 0.3;
    CaliData.DebounceCount = 2;

    // Query Algorithm Version
    scl_log_info("Algorithm Version: %08X\n", STK_getVersion());

    // Query Algorithm Build Date
    scl_log_info("Algorithm Build Date: %d\n", STK_getBuildDate());

    // Run once for Initialize or Update Parameter, e.g. boot process
    STK_initAlgo(STK_ALGO_INSTANCE_ID, &CaliData);
}

static void w1160_sw_reset(int _file)
{
    w1160_WriteReg(_file, 0x80, 0x00);  // W any data to reset the chip
}

static void w1160_Init(int _file)
{
    w1160_WriteReg(_file, 0x02, 0x00);
    w1160_WriteReg(_file, 0x03, 0x20);

    w1160_WriteReg(_file, 0x05, 0x00);
    w1160_WriteReg(_file, 0x06, 0x2C);

    w1160_WriteReg(_file, 0x17, 0xD0);

    w1160_WriteReg(_file, 0x3D, 0x01);

    // w1160_WriteReg(_file, 0x60, 0x20);
    w1160_WriteReg(_file, 0x60, 0x30);
    // w1160_WriteReg(_file, 0x60, 0xB0);
    w1160_WriteReg(_file, 0x61, 0x00);
    w1160_WriteReg(_file, 0x69, 0x30);
    w1160_WriteReg(_file, 0x6D, 0x0A);

#ifdef USE_HW_AGC
    w1160_WriteReg(_file, 0x6A, 0x80);
    w1160_WriteReg(_file, 0x6B, 0x00);
    w1160_WriteReg(_file, 0x6C, 0x0C);
    w1160_WriteReg(_file, 0xA4, 0x07);
#endif
}

#if 0
static uint16_t w1160_Read_ALS(int _file)
{
    uint16_t als_data;
    als_data = (w1160_ReadReg(_file, 0x13) << 8) | w1160_ReadReg(_file, 0x14);
    return als_data;
}
#endif

#ifdef USE_HW_AGC
static uint8_t GainTable[] = {20, 18, 16, 15, 14, 13, 12, 11, 10, 8, 6, 4, 3, 2, 1, 0};
#endif

static bool ReadFifoData(int _file, uint32_t *alsData)
{
    uint16_t i = 0;
    uint8_t buffVal[2 * ALS_TARGET_FIFO_DATA] = {0};
    uint8_t fifocnt[2] = {0};
    uint16_t FrameCnt = 0;

    if (!alsData)
    {
        return false;
    }

#ifdef SCL_DEBUG
    uint8_t value  = w1160_ReadReg(_file, 0x63);
    uint8_t value1 = w1160_ReadReg(_file, 0x64);
    uint8_t value2 = w1160_ReadReg(_file, 0x65);
    scl_log_debug("fifo count = 0x%x - %x\n", value1, value2);
#endif

    w1160_MultiReadReg(_file, 0x64, fifocnt, 2);
    FrameCnt = ((fifocnt[0] & 0x1) << 8) | fifocnt[1];

#ifdef SCL_DEBUG
    scl_log_debug("fifo ctrl = 0x%x, fcnt = %d\n", value, FrameCnt);
#endif

    if (FrameCnt >= ALS_TARGET_FIFO_DATA) {
        w1160_WriteReg(_file, 0x63, 0x10);

        w1160_MultiReadReg(_file, 0x66, buffVal, 2 * ALS_TARGET_FIFO_DATA);
        // scl_log_debug("USE_HW_AGC buffData %x %x--%x %x\n", buffVal[0],buffVal[1],buffVal[2],buffVal[3]);
        for (i = 0; i < ALS_TARGET_FIFO_DATA; i++) {
#ifdef USE_HW_AGC
            uint16_t raw = ((uint16_t)buffVal[2 * i] << 8) |
                buffVal[2 * i + 1];
            uint32_t scaled = (uint32_t)(raw & 0x0FFF) <<
                GainTable[buffVal[2 * i] >> 4];
            alsData[i] = scaled;
            scl_log_debug("USE_HW_AGC buffData[%d] = %u\n", i, buffData[i]);
#else
            alsData[i] = (uint32_t)((buffVal[2 * i] << 8) | buffVal[2 * i + 1]);
            // scl_log_debug("buffData[%d] = %u\n", i, alsData[i]);
#endif
//            scl_log_debug("buffData[%d] = %u\n", i, buffData[i]);
        }

        w1160_WriteReg(_file, 0x68, 0x01);
        w1160_WriteReg(_file, 0x63, 0x00);
        return true;
    } else {
        return false;
    }
}

static float getAls(int _file, float *last_lux)
{
    uint32_t als_FifoData[ALS_TARGET_FIFO_DATA];
    ChannelDataSet fifoData;
    ChannelData ambientData;
    float luxData = 0;
    uint16_t scale = 1000;  // calibration factor, default 72
    uint32_t ret = 0;
    float last_luxData = last_lux ? *last_lux : 0;

    // read FIFO data, save into als_FifoData[0-128]
    if (ReadFifoData(_file, als_FifoData)) {
        // Convert Sensor FIFO Data to ChannelDataSet Structure
        fifoData.ChannelG = als_FifoData;
        fifoData.Size = ALS_TARGET_FIFO_DATA;

        ret = STK_calcAmbientInfobyFIFO(STK_ALGO_INSTANCE_ID, &fifoData, &ambientData, 255, true);

        if (ret == 0 || ret == 0x10 || ret == 0x20 || ret == 0x2) {
            luxData = ambientData.ChannelG * (float)scale / (float)1000;
            if (last_lux) *last_lux = luxData;
            scl_log_debug("Ambient Raw: %u,Lux: %f, Scale: %d\n", ambientData.ChannelG, luxData, scale);

            scl_log_debug("Ambient ALS: %u\n", ambientData.ChannelG);
            scl_log_debug("Scale: %d\n", scale);
            scl_log_debug("Lux: %f\n", luxData);

            ChannelData maxData, minData, midData;
            STK_outputDebugInfo(&fifoData, &maxData, &minData, &midData);

            scl_log_debug("Max-Min: %u, %u\n", maxData.ChannelG, minData.ChannelG);
        } else {
            scl_log_debug("ret =0x%0x\n", ret);
            luxData = last_luxData;
            scl_log_debug("Last Lux: %f\n", luxData);
        }
    } else {
        scl_log_debug("[ALS] FIFO not ready!\n");
        luxData = last_luxData;
    }
    return luxData;
}

static uint32_t w1160_enable_ALS(int _file)
{
    uint32_t ALS_en;
    uint8_t state = 0;

    state = 0x02;
    ALS_en = w1160_WriteReg(_file, 0x00, state);

    scl_log_debug("w1160_enable_ALS,  0x00 = 0x%x\r\n", state);

    return ALS_en;
}

static uint32_t w1160_DisEnable_ALS(int _file)
{
    uint32_t ALS_en;
    uint8_t state = 0;

    scl_log_debug("w1160_DisEnable_ALS \n");

    state = 0x00;
    ALS_en = w1160_WriteReg(_file, 0x00, state);

    return ALS_en;
}

static void w1160_Delay(uint16_t ms)
{
    usleep(ms * 1000);
}

/* ========== light_sensor driver framework adaptation ========== */

static int w1160_i2c_init(struct light_sensor_dev *dev)
{
    struct w1160_i2c_priv *priv;
    int fd;

    if (!dev || !dev->priv_data)
        return -1;

    priv = (struct w1160_i2c_priv *)dev->priv_data;

    fd = open(priv->dev_path, O_RDWR);
    if (fd < 0) {
        scl_log_err("open %s failed: %s\n", priv->dev_path, strerror(errno));
        return -1;
    }

    if (ioctl(fd, I2C_SLAVE, priv->addr) < 0) {
        scl_log_err("ioctl I2C_SLAVE 0x%02X failed: %s\n", priv->addr, strerror(errno));
        close(fd);
        return -1;
    }

    priv->fd = fd;

    /* W1160 specific initialization - acquire lock for setup sequence */
    int lock_fd = w1160_lock_acquire(priv->dev_path);
    if (lock_fd < 0) {
        scl_log_err("Failed to acquire lock for init\n");
        close(fd);
        return -1;
    }

    w1160_PowerOn(fd);
    usleep(30000);
    w1160_enable_ALS(fd);
    usleep(300000);  /* Wait for first data ready */

    /* Release lock after initialization is done */
    w1160_lock_release(lock_fd);

    priv->initialized = true;
    priv->last_lux_data = 0;

    scl_log_info("W1160 init OK: dev=%s addr=0x%02X fd=%d\n", priv->dev_path, priv->addr, fd);
    return 0;
}

static int w1160_i2c_poll(struct light_sensor_dev *dev, uint32_t *light_value)
{
    struct w1160_i2c_priv *priv;
    float lux;
    int lock_fd;

    if (!dev || !dev->priv_data || !light_value)
        return -1;

    priv = (struct w1160_i2c_priv *)dev->priv_data;

    if (priv->fd < 0 || !priv->initialized)
        return -1;

    /* Acquire lock before each polling to serialize hardware access */
    lock_fd = w1160_lock_acquire(priv->dev_path);
    if (lock_fd < 0) {
        scl_log_err("Failed to acquire lock for poll\n");
        return -1;
    }

    lux = getAls(priv->fd, &priv->last_lux_data);
    *light_value = (uint32_t)lux;

    scl_log_debug("poll: lux=%f => %u\n", lux, *light_value);

    /* Release lock after reading complete */
    w1160_lock_release(lock_fd);

    return 0;
}

static void w1160_i2c_free(struct light_sensor_dev *dev)
{
    struct w1160_i2c_priv *priv;

    if (!dev)
        return;

    priv = (struct w1160_i2c_priv *)dev->priv_data;
    if (priv) {
        if (priv->fd >= 0) {
            w1160_DisEnable_ALS(priv->fd);
            close(priv->fd);
            priv->fd = -1;
        }
        free(priv);
    }

    if (dev->name)
        free((void *)dev->name);

    free(dev);
}

static const struct light_sensor_ops w1160_i2c_ops = {
    .init = w1160_i2c_init,
    .poll = w1160_i2c_poll,
    .free = w1160_i2c_free,
};

/* ---------- factory function ---------- */

static struct light_sensor_dev *w1160_i2c_factory(void *args)
{
    struct light_sensor_args_i2c *a = (struct light_sensor_args_i2c *)args;
    struct light_sensor_dev *dev;
    struct w1160_i2c_priv *priv;

    if (!a || !a->dev_path)
        return NULL;

    dev = light_sensor_dev_alloc(a->instance, sizeof(struct w1160_i2c_priv));
    if (!dev)
        return NULL;

    priv = (struct w1160_i2c_priv *)dev->priv_data;
    strncpy(priv->dev_path, a->dev_path, sizeof(priv->dev_path) - 1);
    priv->dev_path[sizeof(priv->dev_path) - 1] = '\0';
    priv->addr = a->addr;
    priv->fd = -1;
    priv->initialized = false;
    priv->last_lux_data = 0;

    dev->ops = &w1160_i2c_ops;

    scl_log_info("W1160 factory: instance=%s dev=%s addr=0x%02X\n",
        a->instance ? a->instance : "(null)",
        a->dev_path, a->addr);

    return dev;
}

REGISTER_LIGHT_SENSOR_DRIVER("W1160", LIGHT_SENSOR_DRV_I2C, w1160_i2c_factory);
