/*
 * Copyright (C) 2022 Intel Corportation
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
 */

#ifndef ANDROID_HARDWARE_SENSORS_V2_X_SENSOR_H
#define ANDROID_HARDWARE_SENSORS_V2_X_SENSOR_H

#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>
#include <queue>

#include "CommDef.h"
#include "SensorsBase.h"
#include "SockUtils.h"

namespace android {
namespace hardware {
namespace sensors {
namespace V2_X {
namespace implementation {

class Sensor : public SensorsBase {
    public:
        using OperationMode = ::android::hardware::sensors::V1_0::OperationMode;
        using Result = ::android::hardware::sensors::V1_0::Result;
        using Event = ::android::hardware::sensors::V2_1::Event;
        using SensorInfo = ::android::hardware::sensors::V1_0::SensorInfo;
        using SensorType = ::android::hardware::sensors::V1_0::SensorType;

        Sensor(ISensorsEventCallback *callback);
        virtual ~Sensor();

        const std::vector<V1_0::SensorInfo> getSensorInfo() const;
        bool supportsDataInjection(int32_t sensorHandle) const;
        Event convertClientEvent(clientSensorEvent *cliEvent);
        Result flush(int32_t sensorHandle);
        Result injectEvent(const Event &event);
        SockServer *mSocketServer;
        void activate(int32_t sensorHandle, bool enable);
        void batch(int32_t sensorHandle, int64_t samplingPeriodNs);
        void clientConnectedCallback(SockServer *sock __unused, sock_client_proxy_t *client);
        void run();
        void sensorEventCallback(SockServer *sock __unused, sock_client_proxy_t *client);
        void setOperationMode(OperationMode mode);

    protected:
        static void startThread(Sensor *sensor);

        bool mIsClientConnected;
        bool mPreviousEventSet;
        Event mPreviousEvent[MAX_NUM_SENSORS];
        ISensorsEventCallback *mCallback;
        std::atomic_bool mStopThread;
        std::condition_variable mSensorMsgQueueReadyCV;
        std::mutex mCVMutex;
        std::mutex mConfigMsgMutex;
        std::mutex mSensorMsgQueueMtx;
        std::thread mRunThread;
        std::queue<clientSensorEvent> mSensorMsgQueue;
        struct sensorInfo mSensor[MAX_NUM_SENSORS];
        OperationMode mMode[MAX_NUM_SENSORS];
};

}  // namespace implementation
}  // namespace V2_X
}  // namespace sensors
}  // namespace hardware
}  // namespace android

#endif  // ANDROID_HARDWARE_SENSORS_V2_X_SENSOR_H
