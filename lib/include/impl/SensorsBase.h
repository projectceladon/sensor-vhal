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

#ifndef ANDROID_HARDWARE_SENSORS_V2_X_SENSOR_BASE_H
#define ANDROID_HARDWARE_SENSORS_V2_X_SENSOR_BASE_H

#include <android/hardware/sensors/1.0/types.h>
#include <android/hardware/sensors/2.1/types.h>

namespace android {
namespace hardware {
namespace sensors {
namespace V2_X {
namespace implementation {

class ISensorsEventCallback {
    public:
      using Event = ::android::hardware::sensors::V2_1::Event;

      virtual ~ISensorsEventCallback(){};
      virtual void postEvents(const std::vector<Event>& events, bool wakeup) = 0;
};

class SensorsBase {
    public:
        using OperationMode = ::android::hardware::sensors::V1_0::OperationMode;
        using Result = ::android::hardware::sensors::V1_0::Result;
        using Event = ::android::hardware::sensors::V2_1::Event;
        using SensorInfo = ::android::hardware::sensors::V2_1::SensorInfo;

        virtual ~SensorsBase() {};
        virtual const std::vector<V1_0::SensorInfo> getSensorInfo() const;
        virtual void batch(int32_t sensorHandle, int64_t samplingPeriodNs);
        virtual void activate( int32_t sensorHandle, bool enable);
        virtual Result flush(int32_t sensorHandle);
        virtual void setOperationMode(OperationMode mode);
        virtual Result injectEvent(const Event& event);
};

}  // namespace implementation
}  // namespace V2_X
}  // namespace sensors
}  // namespace hardware
}  // namespace android

#endif  // ANDROID_HARDWARE_SENSORS_V2_X_SENSOR_BASE_H
