/*
 * Copyright (c) 2016-2025 Sensortek Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Sensortek Technologies, Inc.
 *
 * Modified by SpacemiT (Hangzhou) Technology Co. Ltd.
 * Copyright 2026 SpacemiT (Hangzhou) Technology Co. Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef DRV_I2C_W1160_H
#define DRV_I2C_W1160_H

// #include <linux/types.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct FloatParameter
{
    // float ParameterF;
    // float ParameterR;
    float ParameterG;
    // float ParameterB;
    // float ParameterC;
} FloatParameter;

typedef struct IntParameter
{
    // int32_t ParameterF;
    // int32_t ParameterR;
    int32_t ParameterG;
    // int32_t ParameterB;
    // int32_t ParameterC;
} IntParameter;

typedef struct ChannelData
{
    // uint32_t ChannelF;
    // uint32_t ChannelR;
    uint32_t ChannelG;
    // uint32_t ChannelB;
    // uint32_t ChannelC;
} ChannelData;

typedef struct ChannelDataSet
{
    // uint32_t *ChannelF;
    // uint32_t *ChannelR;
    uint32_t *ChannelG;
    // uint32_t *ChannelB;
    // uint32_t *ChannelC;
    uint16_t Size;
} ChannelDataSet;

typedef struct CNode
{
    FloatParameter Alpha;
    FloatParameter Beta;
    FloatParameter Bias;
    ChannelData HighTHD;
    ChannelData LowTHD;
    uint16_t Brightness;
} CNode;

typedef struct AlgoParam
{
    CNode DisplayNode[13];
    float luxBase;
    float luxCode[8];
    float FilterPct[1];
    float DebouncePct;
    uint8_t DebounceCount;
    ChannelData DebounceTHD;
} AlgoParam;

#ifdef __cplusplus
extern "C"
{
#endif
    uint32_t STK_getVersion(void);
    uint32_t STK_getBuildDate(void);
    void STK_enableLog(bool enabled);
    uint32_t STK_initAlgo(uint8_t instanceID, AlgoParam *param);
    uint32_t STK_updateAlgo(uint8_t instanceID, AlgoParam *param);
    uint32_t STK_deInitAlgo(uint8_t instanceID);
    uint32_t STK_resetAlgo(uint8_t instanceID);
    uint32_t STK_updateDebounceParam(uint8_t instanceID, float percent, uint8_t count);
    float STK_calcAmbientLuxbyFIFO(uint8_t instanceID, ChannelDataSet *fifo,
        uint16_t brightness, bool enableFilter);
    float STK_calcAmbientLuxbyMaxMin(uint8_t instanceID, ChannelData *max,
        ChannelData *min, uint16_t brightness, bool enableFilter);
    uint32_t STK_calcAmbientInfobyFIFO(uint8_t instanceID, ChannelDataSet *fifo,
        ChannelData *ambient, uint16_t brightness, bool enableFilter);
    uint32_t STK_calcAmbientInfobyMaxMin(uint8_t instanceID, ChannelData *max,
        ChannelData *min, ChannelData *ambient, uint16_t brightness, bool enableFilter);
    uint32_t STK_calcAmbientInfobyFIFOSet(uint8_t instanceID, ChannelDataSet *fifo,
        uint8_t sectionSize, uint8_t dataSize, ChannelData *ambient,
        uint16_t brightness, bool enableFilter);
    uint32_t STK_calcAmbientInfobyMaxMinSet(uint8_t instanceID, ChannelData *max,
        ChannelData *min, ChannelData *ambient, uint16_t brightness, bool enableFilter);
    uint32_t STK_extrFeaturebyFIFOSet(ChannelDataSet *fifo, uint8_t sectionSize,
        uint8_t dataSize, ChannelData *max, ChannelData *min);
    uint32_t STK_outputDebugInfo(ChannelDataSet *fifo, ChannelData *max,
        ChannelData *min, ChannelData *mid);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
#endif /* DRV_I2C_W1160_H */
