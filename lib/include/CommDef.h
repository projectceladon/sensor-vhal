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

#ifndef SENSORS_VHAL_COMM_DEF_H
#define SENSORS_VHAL_COMM_DEF_H

#include <android/hardware/sensors/1.0/types.h>
#include <android/hardware/sensors/2.1/types.h>
#include <hardware/sensors.h>

#include "ClientSensorDef.h"

namespace android {
namespace hardware {
namespace sensors {
namespace V2_X {
namespace implementation {

using ::android::hardware::sensors::V1_0::SensorFlagBits;
using ::android::hardware::sensors::V1_0::SensorType;
using ::android::hardware::sensors::V1_0::SensorType;
using ::android::hardware::sensors::V1_0::SensorInfo;
using SensorType2 = ::android::hardware::sensors::V2_1::SensorType;

#define ConvertNsToMs(X) (X / 1000000)
#define DefaultMaxDelayUs (500 * 1000)

enum sensorHandle { HANDLE_ACCEL,
                    HANDLE_GYRO,
                    HANDLE_MAGN,
                    HANDLE_ACCEL_UNCALIB,
                    HANDLE_GYRO_UNCALIB,
                    HANDLE_MAGN_UNCALIB,
                    HANDLE_LIGHT,
                    HANDLE_PROXIMITY,
                    HANDLE_ABS_TEMP,
                    MAX_NUM_SENSORS
};

static const SensorInfo info[MAX_NUM_SENSORS] = {
    {
        .sensorHandle = HANDLE_ACCEL,
        .name = "AIC 3-axis Accelerometer",
        .vendor = "Intel ACGSS",
        .version = 1,
        .type = SensorType::ACCELEROMETER,
        .typeAsString = "android.sensor.accelerometer",
        .maxRange = 2.8f,  // +/- 8g
        .resolution = 1.0f / 4032.0f,
        .power = 3.0f,        // mA
        .minDelay = 10000,  // microseconds
        .maxDelay = DefaultMaxDelayUs,
        .fifoReservedEventCount = 0,
        .fifoMaxEventCount = 0,
        .requiredPermission = "",
        .flags = static_cast<uint32_t>(SensorFlagBits::DATA_INJECTION | SensorFlagBits::CONTINUOUS_MODE),
    },

    {
        .sensorHandle = HANDLE_GYRO,
        .name = "AIC 3-axis Gyroscope",
        .vendor = "Intel ACGSS",
        .version = 1,
        .type = SensorType::GYROSCOPE,
        .typeAsString = "android.sensor.gyroscope",
        .maxRange = 11.1111111,
        .resolution = 1.0f / 1000.0f,
        .power = 3.0f,
        .minDelay = 10000,  // microseconds
        .maxDelay = DefaultMaxDelayUs,
        .fifoReservedEventCount = 0,
        .fifoMaxEventCount = 0,
        .requiredPermission = "",
        .flags = 0,
    },

    {
        .sensorHandle = HANDLE_MAGN,
        .name = "AIC 3-axis Magnetic field sensor",
        .vendor = "Intel ACGSS",
        .version = 1,
        .type = SensorType::MAGNETIC_FIELD,
        .typeAsString = "android.sensor.magnetic_field",
        .maxRange = 2000.0f,
        .resolution = 1.01f,
        .power = 6.7f,        // mA
        .minDelay = 10000,  // microseconds
        .maxDelay = DefaultMaxDelayUs,
        .fifoReservedEventCount = 0,
        .fifoMaxEventCount = 0,
        .requiredPermission = "",
        .flags = static_cast<uint32_t>(SensorFlagBits::CONTINUOUS_MODE),
    },

    {
        .sensorHandle = HANDLE_ACCEL_UNCALIB,
        .name = "AIC 3-axis uncalibrated accelerometer",
        .vendor = "Intel ACGSS",
        .version = 1,
        .type = SensorType::ACCELEROMETER_UNCALIBRATED,
        .typeAsString = "android.sensor.accelerometer_uncalibrated",
        .maxRange = 2.8f,  // +/- 8g
        .resolution = 1.0f / 4032.0f,
        .power = 3.0f,        // mA
        .minDelay = 10000,  // microseconds
        .maxDelay = DefaultMaxDelayUs,
        .fifoReservedEventCount = 0,
        .fifoMaxEventCount = 0,
        .requiredPermission = "",
        .flags = static_cast<uint32_t>(SensorFlagBits::CONTINUOUS_MODE),
    },

    {
        .sensorHandle = HANDLE_GYRO_UNCALIB,
        .name = "AIC 3-axis uncalibrated gyroscope",
        .vendor = "Intel ACGSS",
        .version = 1,
        .type = SensorType::GYROSCOPE_UNCALIBRATED,
        .typeAsString = "android.sensor.gyroscope_uncalibrated",
        .maxRange = 11.1111111,
        .resolution = 1.0f / 1000.0f,
        .power = 3.0f,
        .minDelay = 10000,  // microseconds
        .maxDelay = DefaultMaxDelayUs,
        .fifoReservedEventCount = 0,
        .fifoMaxEventCount = 0,
        .requiredPermission = "",
        .flags = 0,
    },

    {
        .sensorHandle = HANDLE_MAGN_UNCALIB,
        .name = "AIC 3-axis Magnetic field uncalibrated sensor",
        .vendor = "Intel ACGSS",
        .version = 1,
        .type = SensorType::MAGNETIC_FIELD_UNCALIBRATED,
        .typeAsString = "android.sensor.magnetic_field_uncalibrated",
        .maxRange = 2000.0f,
        .resolution = 1.01f,
        .power = 6.7f,        // mA
        .minDelay = 10000,  // microseconds
        .maxDelay = DefaultMaxDelayUs,
        .fifoReservedEventCount = 0,
        .fifoMaxEventCount = 0,
        .requiredPermission = "",
        .flags = static_cast<uint32_t>(SensorFlagBits::CONTINUOUS_MODE),
    },

    {
        .sensorHandle = HANDLE_LIGHT,
        .name = "AIC Light senso",
        .vendor = "Intel ACGSS",
        .version = 1,
        .type = SensorType::LIGHT,
        .typeAsString = "android.sensor.light",
        .maxRange = 40000.0f,
        .resolution = 1.0f,
        .power = 20.0f,         // mA
        .minDelay = 10000,  // microseconds
        .maxDelay = DefaultMaxDelayUs,
        .fifoReservedEventCount = 0,
        .fifoMaxEventCount = 0,
        .requiredPermission = "",
        .flags = static_cast<uint32_t>(SensorFlagBits::ON_CHANGE_MODE),
    },

    {
        .sensorHandle = HANDLE_PROXIMITY,
        .name = "AIC Proximity Sensor",
        .vendor = "Intel ACGSS",
        .version = 1,
        .type = SensorType::PROXIMITY,
        .typeAsString = "android.sensor.proximity",
        .maxRange = 5.0f,
        .resolution = 1.0f,
        .power = 20.0f,         // mA
        .minDelay = 10000,  // microseconds
        .maxDelay = DefaultMaxDelayUs,
        .fifoReservedEventCount = 0,
        .fifoMaxEventCount = 0,
        .requiredPermission = "",
        .flags =
            static_cast<uint32_t>(SensorFlagBits::ON_CHANGE_MODE | SensorFlagBits::WAKE_UP),
    },

    {
        .sensorHandle = HANDLE_ABS_TEMP,
        .name = "AIC Ambient Temperature Sensor",
        .vendor = "Intel ACGSS",
        .version = 1,
        .type = SensorType::AMBIENT_TEMPERATURE,
        .typeAsString = "android.sensor.ambient_temperature",
        .maxRange = 80.0f,
        .resolution = 1.0f,
        .power = 0.0f,
        .minDelay = 10000,  // microseconds
        .maxDelay = DefaultMaxDelayUs,
        .fifoReservedEventCount = 0,
        .fifoMaxEventCount = 0,
        .requiredPermission = "",
        .flags = static_cast<uint32_t>(SensorFlagBits::ON_CHANGE_MODE),
    }
};

/*
 * SensorInfo structure is used to maintain each sensor's information in HAL
 */
struct sensorInfo {
    bool isEnabled;             // Flag to check sensor enabled/disabled
    int64_t samplingPeriodMs;   // Sensor's sampling period in milli seconds
    sensorConfigMsg configMsg;  // Sensor's config message
};

std::vector<SensorType2> const Sensor2_1_Types = { SensorType2::ACCELEROMETER,
                                                    SensorType2::GYROSCOPE,
                                                    SensorType2::MAGNETIC_FIELD,
                                                    SensorType2::ACCELEROMETER_UNCALIBRATED,
                                                    SensorType2::GYROSCOPE_UNCALIBRATED,
                                                    SensorType2::MAGNETIC_FIELD_UNCALIBRATED,
                                                    SensorType2::LIGHT,
                                                    SensorType2::PROXIMITY,
                                                    SensorType2::AMBIENT_TEMPERATURE
                                                };

bool isWakeUpSensor(int32_t sensorHandle);
int getTypeFromHandle(int32_t handle);
int getHandleFromType(int32_t sensor_type);
int getPayloadLen(uint32_t sensor_type);
int64_t getEventTimeStamp();

}  // namespace implementation
}  // namespace V2_X
}  // namespace sensors
}  // namespace hardware
}  // namespace android

#endif  // SENSORS_VHAL_COMM_DEF_H
