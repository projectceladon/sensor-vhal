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
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <utils/SystemClock.h>

#include <cmath>

#include "ClientSensorDef.h"
#include "Sensor.h"

namespace android {
namespace hardware {
namespace sensors {
namespace V2_X {
namespace implementation {

using ::android::hardware::sensors::V1_0::MetaDataEventType;
using ::android::hardware::sensors::V1_0::OperationMode;
using ::android::hardware::sensors::V1_0::Result;
using ::android::hardware::sensors::V1_0::SensorFlagBits;
using ::android::hardware::sensors::V1_0::SensorStatus;
using ::android::hardware::sensors::V2_1::Event;
using ::android::hardware::sensors::V1_0::SensorInfo;
using ::android::hardware::sensors::V1_0::SensorType;

Sensor::Sensor(ISensorsEventCallback *callback) : mCallback(callback) {
    mSocketServer = SockServer::getInstance(std::bind(&Sensor::clientConnectedCallback, this,
                                            std::placeholders::_1, std::placeholders::_2),
                                            std::bind(&Sensor::sensorEventCallback, this,
                                            std::placeholders::_1, std::placeholders::_2));
    // Set status as disable to all sensors
    for (int i = 0; i < MAX_NUM_SENSORS; i++) {
        mSensor[i].isEnabled = false;
        mMode[i] = OperationMode::NORMAL;
    }
    mRunThread = std::thread(startThread, this);
    mIsClientConnected = false;
    mPreviousEventSet = false;
    memset(mPreviousEvent, 0, sizeof(mPreviousEvent));
    memset(mSensor, 0, sizeof(mSensor));
}

Sensor::~Sensor() {
    mStopThread = true;
    mRunThread.join();
}

const std::vector<V1_0::SensorInfo> Sensor::getSensorInfo() const {
    std::vector<V1_0::SensorInfo> sensors;
    for (auto& sensorInfo : info)
        sensors.push_back(sensorInfo);
    return sensors;
}

void Sensor::clientConnectedCallback(SockServer *sock __unused,
                    sock_client_proxy_t *client __unused) {
    mIsClientConnected = true;
    ALOGI("%s client got connected", __func__);
    for (int i = 0; i < MAX_NUM_SENSORS; i++) {
        std::lock_guard<std::mutex> lock(mConfigMsgMutex);
        mSocketServer->sendConfigMsg(mSocketServer, &mSensor[i].configMsg, sizeof(mSensor[i].configMsg));
    }
}

void Sensor::sensorEventCallback(SockServer *sock __unused, sock_client_proxy_t *client __unused) {
    sensorEventHeader eventHeader;
    int len = mSocketServer->recv_data(client, &eventHeader, sizeof(sensorEventHeader),
                                       SOCK_BLOCK_MODE);
    if (len <= 0) {
        ALOGE("sensors vhal receive sensor header message failed: %s ", strerror(errno));
        return;
    }

    const int payloadLen = getPayloadLen(eventHeader.sensorType);
    if (payloadLen == 0) {
        return;
    }

    clientSensorEvent event;
    len = mSocketServer->recv_data(client, event.data.byteData, payloadLen, SOCK_BLOCK_MODE);
    if (len <= 0) {
        ALOGE("sensors vhal receive sensor data failed: %s", strerror(errno));
        return;
    }

    event.header = eventHeader;
    {
        std::unique_lock<std::mutex> lck(mSensorMsgQueueMtx);
        mSensorMsgQueue.push(event);
        mSensorMsgQueueReadyCV.notify_all();
    }
}

void Sensor::batch(int32_t sensorHandle, int64_t samplingPeriodNs) {
    sensorConfigMsg msg = {};
    msg.sensorType = getTypeFromHandle(sensorHandle);
    msg.enabled = true;
    std::lock_guard<std::mutex> lock(mConfigMsgMutex);
    mSensor[sensorHandle].configMsg = msg;
    mSensor[sensorHandle].isEnabled = true;
    mSensor[sensorHandle].samplingPeriodMs = ConvertNsToMs(samplingPeriodNs);
    // convert sampling period to milli seconds
    msg.samplingPeriodMs = mSensor[sensorHandle].samplingPeriodMs;
    mSocketServer->sendConfigMsg(mSocketServer, &msg, sizeof(msg));
}

void Sensor::activate(int32_t sensorHandle, bool enable) {
    sensorConfigMsg msg = {};
    msg.sensorType = getTypeFromHandle(sensorHandle);
    msg.enabled = enable;
    // convert sampling period to milli seconds
    msg.samplingPeriodMs = mSensor[sensorHandle].samplingPeriodMs;
    std::lock_guard<std::mutex> lock(mConfigMsgMutex);
    mSensor[sensorHandle].isEnabled = enable;
    mSensor[sensorHandle].configMsg = msg;
    mSocketServer->sendConfigMsg(mSocketServer, &msg, sizeof(msg));
}

Result Sensor::flush(int32_t sensorHandle) {
    // Only generate a flush complete event if the sensor is enabled and if the sensor is not a
    // one-shot sensor.
    if (!mSensor[sensorHandle].isEnabled) {
        return Result::BAD_VALUE;
    }

    // Note: If a sensor supports batching, write all of the currently batched events for the sensor
    // to the Event FMQ prior to writing the flush complete event.
    Event ev{};
    ev.sensorHandle = sensorHandle;
    ev.sensorType = ::android::hardware::sensors::V2_1::SensorType::META_DATA;
    ev.u.meta.what = MetaDataEventType::META_DATA_FLUSH_COMPLETE;
    std::vector<Event> evs{ev};
    mCallback->postEvents(evs, isWakeUpSensor(sensorHandle));
    return Result::OK;
}

Event Sensor::convertClientEvent(clientSensorEvent *cliEvent) {
    Event event{};
    event.sensorHandle = getHandleFromType(cliEvent->header.sensorType);
    if (event.sensorHandle < 0) {
        ALOGW("%s Invalid sensorHandle: %d for sensorType: %d", __func__, event.sensorHandle, cliEvent->header.sensorType);
        return event;
    }
    event.sensorType =  Sensor2_1_Types[event.sensorHandle];
    event.timestamp = getEventTimeStamp();
    switch (cliEvent->header.sensorType) {
        case SENSOR_TYPE_ACCELEROMETER:
        case SENSOR_TYPE_MAGNETIC_FIELD:
        case SENSOR_TYPE_GYROSCOPE:
            event.u.vec3.x = cliEvent->data.value[0];
            event.u.vec3.y = cliEvent->data.value[1];
            event.u.vec3.z = cliEvent->data.value[2];
            event.u.vec3.status = SensorStatus::ACCURACY_HIGH;
            break;
        case SENSOR_TYPE_ACCELEROMETER_UNCALIBRATED:
        case SENSOR_TYPE_GYROSCOPE_UNCALIBRATED:
        case SENSOR_TYPE_MAGNETIC_FIELD_UNCALIBRATED:
            event.u.uncal.x = cliEvent->data.value[0];
            event.u.uncal.y = cliEvent->data.value[1];
            event.u.uncal.z = cliEvent->data.value[2];
            event.u.uncal.x_bias = cliEvent->data.value[3];
            event.u.uncal.y_bias = cliEvent->data.value[4];
            event.u.uncal.z_bias = cliEvent->data.value[5];
            break;
        case SENSOR_TYPE_LIGHT:
        case SENSOR_TYPE_PROXIMITY:
        case SENSOR_TYPE_AMBIENT_TEMPERATURE:
            event.u.scalar = cliEvent->data.value[0];
            if (event.u.scalar != mPreviousEvent[event.sensorHandle].u.scalar || !mPreviousEventSet) {
                mPreviousEvent[event.sensorHandle] = event;
                mPreviousEventSet = true;
            }
            break;
        default:
            ALOGW("%s Client Event has invalid sensor type: %d", __func__, cliEvent->header.sensorType);
    }
    return event;
    
}

void Sensor::startThread(Sensor* sensor) {
    sensor->run();
}

void Sensor::run() {
    clientSensorEvent cliEvent;
    while (!mStopThread) {
        // TODO: Do not send events at data injection mode
        {
           {
                std::unique_lock<std::mutex> lock(mCVMutex);
                if (mSensorMsgQueue.empty()) {
                    mSensorMsgQueueReadyCV.wait(lock);
                }
            }

            std::unique_lock<std::mutex> lock(mSensorMsgQueueMtx);
            cliEvent = std::move(mSensorMsgQueue.front());
            mSensorMsgQueue.pop();
            Event event = convertClientEvent(&cliEvent);
            if (event.sensorHandle < 0) {
                ALOGE("%s Received invalid sensor handle(%d) from client", __func__, event.sensorHandle);
                continue;
            }
            mCallback->postEvents(std::vector<Event>{event}, isWakeUpSensor(event.sensorHandle));
        }
    }
}

void Sensor::setOperationMode(OperationMode mode) {
    for (int i = 0; i < MAX_NUM_SENSORS; i++) {
        if (mMode[i] != mode) {
            mMode[i] = mode;
        }
    }
}

bool Sensor::supportsDataInjection(int32_t sensorHandle) const {
    return info[sensorHandle].flags & static_cast<uint32_t>(SensorFlagBits::DATA_INJECTION);
}

Result Sensor::injectEvent(const Event &event) {
    Result result = Result::OK;
    if (event.sensorType == ::android::hardware::sensors::V2_1::SensorType::ADDITIONAL_INFO) {
        // When in OperationMode::NORMAL, SensorType::ADDITIONAL_INFO is used to push operation
        // environment data into the device.
    } else if (!supportsDataInjection(event.sensorHandle)) {
        result = Result::INVALID_OPERATION;
    } else if (mMode[event.sensorHandle] == OperationMode::DATA_INJECTION) {
        mCallback->postEvents(std::vector<Event>{event}, isWakeUpSensor(event.sensorHandle));
    } else {
        result = Result::BAD_VALUE;
    }

    return result;
}

}  // namespace implementation
}  // namespace V2_X
}  // namespace sensors
}  // namespace hardware
}  // namespace android
