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

#ifndef SENSORS_VHAL_CLIENT_SENSOR_DEF_H
#define SENSORS_VHAL_CLIENT_SENSOR_DEF_H

#include <hardware/sensors.h>

#define  SENSOR_VHAL_PORT      8772
#define  SENSOR_SOCK_TYPE_PROP "ro.vendor.sensors.sock.type"
#define  SENSOR_VHAL_PORT_PROP "virtual.sensor.tcp.port"

struct sensorEventHeader {
    int32_t  sensorType; // Sensor type
    union {
        int32_t userId; // User Id for multiclient use case
        int32_t dataCount; // Number of data fields(clientSensorEvent::value).
    };
    int64_t timeStamp; // Timestamp is in nanoseconds
};

/*
 * clientSensorEvent structure is used to receive data from remote-client/Streamer.
 */
struct clientSensorEvent {
    struct sensorEventHeader header; // Sensor event header message
    union
    {
        float value[16];
        char  byteData[128];
    } data; // Sensor data
};

/*
 * sensorConfigMsg structure is used to send each sensor configuration to remote-client/Streamer.
 */
struct sensorConfigMsg {
    uint32_t sensorType;       // Sensor type
    int32_t  enabled;          // Sensor enable/disable flag
    int32_t  samplingPeriodMs; // Sensor sample period in milli seconds
};

#endif // SENSORS_VHAL_CLIENT_SENSOR_DEF_H
