/*
 * Copyright (C) 2022 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "sensors@2.0-service.intel"

#include <log/log.h>

#include "CommDef.h"

namespace android {
namespace hardware {
namespace sensors {
namespace V2_X {
namespace implementation {

int getTypeFromHandle(int32_t handle) {
    int id = -1;
    switch (handle) {
        case HANDLE_ACCEL:
            id = SENSOR_TYPE_ACCELEROMETER;
            break;
        case HANDLE_GYRO:
            id = SENSOR_TYPE_GYROSCOPE;
            break;
        case HANDLE_MAGN:
            id = SENSOR_TYPE_MAGNETIC_FIELD;
            break;
        case HANDLE_ACCEL_UNCALIB:
            id = SENSOR_TYPE_ACCELEROMETER_UNCALIBRATED;
            break;
        case HANDLE_GYRO_UNCALIB:
            id = SENSOR_TYPE_GYROSCOPE_UNCALIBRATED;
            break;
        case HANDLE_MAGN_UNCALIB:
            id = SENSOR_TYPE_MAGNETIC_FIELD_UNCALIBRATED;
            break;
        case HANDLE_LIGHT:
            id = SENSOR_TYPE_LIGHT;
            break;
        case HANDLE_PROXIMITY:
            id = SENSOR_TYPE_PROXIMITY;
            break;
        case HANDLE_ABS_TEMP:
            id = SENSOR_TYPE_AMBIENT_TEMPERATURE;
            break;
        default:
            ALOGW("unknown handle (%d)", handle);
            return -EINVAL;
    }
    return id;
}

int getHandleFromType(int32_t sensor_type) {
    int index = -1;
    switch (sensor_type) {
        case SENSOR_TYPE_ACCELEROMETER:
            index = HANDLE_ACCEL;
            break;
        case SENSOR_TYPE_MAGNETIC_FIELD:
            index = HANDLE_MAGN;
	    break;
        case SENSOR_TYPE_GYROSCOPE:
            index = HANDLE_GYRO;
            break;
        case SENSOR_TYPE_ACCELEROMETER_UNCALIBRATED:
            index = HANDLE_ACCEL_UNCALIB;
            break;
        case SENSOR_TYPE_GYROSCOPE_UNCALIBRATED:
            index = HANDLE_GYRO_UNCALIB;
            break;
        case SENSOR_TYPE_MAGNETIC_FIELD_UNCALIBRATED:
            index = HANDLE_MAGN_UNCALIB;
            break;
        case SENSOR_TYPE_LIGHT:
            index = HANDLE_LIGHT;
            break;
        case SENSOR_TYPE_AMBIENT_TEMPERATURE:
            index = HANDLE_ABS_TEMP;
            break;
        case SENSOR_TYPE_PROXIMITY:
            index = HANDLE_PROXIMITY;
            break;
        default:
            ALOGW("%s unsupported sensor type: %d", __func__, sensor_type);
            index = -1;
    }
    return index;
}

int getPayloadLen(uint32_t sensor_type) {
    int payload_len = 0;
    switch (sensor_type) {
        case SENSOR_TYPE_ACCELEROMETER:
        case SENSOR_TYPE_GYROSCOPE:
        case SENSOR_TYPE_MAGNETIC_FIELD:
            payload_len = 3 * sizeof(float);
            break;
        case SENSOR_TYPE_ACCELEROMETER_UNCALIBRATED:
        case SENSOR_TYPE_GYROSCOPE_UNCALIBRATED:
        case SENSOR_TYPE_MAGNETIC_FIELD_UNCALIBRATED:
            payload_len = 6 * sizeof(float);
            break;
        case SENSOR_TYPE_LIGHT:
        case SENSOR_TYPE_PROXIMITY:
        case SENSOR_TYPE_AMBIENT_TEMPERATURE:
            payload_len = 1 * sizeof(float);
            break;
        default:
            payload_len = 0;
            ALOGW("%s unsupported sensor type %d", __func__, sensor_type);
            break;
    }
    return payload_len;
}

int64_t getEventTimeStamp() {
    struct timespec ts;
    clock_gettime(CLOCK_BOOTTIME, &ts);
    return(ts.tv_sec * 1000 * 1000 * 1000 + ts.tv_nsec);
}

bool isWakeUpSensor(int32_t sensorHandle) {
    return info[sensorHandle].flags & static_cast<uint32_t>(SensorFlagBits::WAKE_UP);
}

}  // namespace implementation
}  // namespace V2_X
}  // namespace sensors
}  // namespace hardware
}  // namespace android
