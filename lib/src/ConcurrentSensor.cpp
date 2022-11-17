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
 */

#include <utils/SystemClock.h>

#include <cmath>

#include "ClientSensorDef.h"
#include "ConcurrentSensor.h"

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

ConcurrentSensor::ConcurrentSensor(ISensorsEventCallback *callback) : mCallback(callback) {
    mSocketServer = SockServer::getInstance(
                                    std::bind(&ConcurrentSensor::clientConnectedCallback,
                                    this, std::placeholders::_1, std::placeholders::_2),
                                    std::bind(&ConcurrentSensor::concurrentSensorEventCallback,
                                    this, std::placeholders::_1, std::placeholders::_2));
    // Set status as disable to all sensors
    for (int userId = 0; userId < MAX_NUM_USERS; userId++) {
        for (int sensorIndex = 0; sensorIndex < MAX_NUM_SENSORS; sensorIndex++) {
            sensorConfigMsg msg = {};
            msg.sensorType = getTypeFromHandle(sensorIndex);
            msg.enabled = false;
            mSensor[userId][sensorIndex].isEnabled = false;
            mSensor[userId][sensorIndex].configMsg = msg;
        }
    }

    // Set default mode as normal for each sensor type
    for (int i = 0; i < MAX_NUM_SENSORS; i++) {
        mMode[i] = OperationMode::NORMAL;
    }

    mRunThread = std::thread(startThread, this);
}

ConcurrentSensor::~ConcurrentSensor() {
    mStopThread = true;
    mRunThread.join();
}

const std::vector<V1_0::SensorInfo> ConcurrentSensor::getSensorInfo() const {
    std::vector<V1_0::SensorInfo> sensors;
    for (auto sensorInfo : info)
        sensors.push_back(sensorInfo);
    return sensors;
}

void ConcurrentSensor::clientConnectedCallback(SockServer *sock __unused,
                                sock_client_proxy_t *client __unused) {
    mIsClientConnected = true;
    ALOGI("%s client got connected", __func__);
}

void ConcurrentSensor::concurrentSensorEventCallback(SockServer *sock __unused,
                                    sock_client_proxy_t *client __unused) {
    sensorEventHeader eventHeader;
    int len = mSocketServer->recv_data(client, &eventHeader, sizeof(sensorEventHeader),
                                                                    SOCK_BLOCK_MODE);
    if (len <= 0) {
        ALOGE("sensors vhal receive sensor header message failed: %s ", strerror(errno));
        return;
    }

    if (eventHeader.sensorType == SENSOR_TYPE_ADDITIONAL_INFO) {
        if ((eventHeader.userId < 0) || (eventHeader.userId >= MAX_NUM_USERS)) {
            ALOGE("%s user-id(%d) received from client is not valid", __func__, client->userId);
            return;
        }

        client->userId = GET_USERID(eventHeader.userId);
        for (int i = 0; i < MAX_NUM_SENSORS; i++) {
            std::unique_lock<std::mutex> lck(mConfigMsgMutex);
            sendConfigMsg(&mSensor[eventHeader.userId][i].configMsg, sizeof(sensorConfigMsg),
                                                                        client->userId);
        }
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
    event.header.userId = client->userId;
    {
        std::unique_lock<std::mutex> lck(mSensorMsgQueueMtx);
        mSensorMsgQueue.push(event);
        mSensorMsgQueueReadyCV.notify_all();
    }
}

void ConcurrentSensor::batch(int32_t sensorHandle, int64_t samplingPeriodNs) {
    int userId = GET_USERID_FROM_HANDLE(sensorHandle);
    if (userId < USER0_ID || userId > USER_ID_MAX) {
        ALOGE("%s Concurrent-user won't support user-id: %d\n", __func__, userId);
        return;
    }

    int uIndex = GET_INDEX_OF_USERID(userId);
    sensorHandle = GET_ACTUAL_HANDLE(sensorHandle);
    struct sensorInfo *sensor = &mSensor[uIndex][sensorHandle];
    sensorConfigMsg msg = {};
    std::unique_lock<std::mutex> lck(mConfigMsgMutex);
    msg.sensorType = getTypeFromHandle(sensorHandle);
    msg.enabled = true;
    // convert sampling period into milli seconds
    msg.samplingPeriodMs = ConvertNsToMs(samplingPeriodNs);
    sensor->configMsg = msg;
    sensor->isEnabled = true;
    sensor->samplingPeriodMs = ConvertNsToMs(samplingPeriodNs);
    sendConfigMsg(&msg, sizeof(msg), userId);
}

void ConcurrentSensor::activate(int32_t sensorHandle, bool enable) {
    int userId = GET_USERID_FROM_HANDLE(sensorHandle);
    if (userId < USER0_ID || userId > USER_ID_MAX) {
        ALOGE("%s Concurrent-user won't support user-id: %d\n", __func__, userId);
        return;
    }

    int uIndex = GET_INDEX_OF_USERID(userId);
    sensorHandle = GET_ACTUAL_HANDLE(sensorHandle);
    struct sensorInfo *sensor = &mSensor[uIndex][sensorHandle];
    sensorConfigMsg msg = {};

    std::unique_lock<std::mutex> lck(mConfigMsgMutex);
    msg.sensorType = getTypeFromHandle(sensorHandle);
    msg.enabled = enable;
    msg.samplingPeriodMs = sensor->samplingPeriodMs;
    sensor->configMsg = msg;
    sensor->isEnabled = enable;
    sendConfigMsg(&msg, sizeof(msg), userId);
}

Result ConcurrentSensor::flush(int32_t sensorHandle) {
    int userId = GET_USERID_FROM_HANDLE(sensorHandle);
    if (userId < USER0_ID || userId > USER_ID_MAX) {
        ALOGE("%s Concurrent-user won't support user-id: %d\n", __func__, userId);
        return Result::BAD_VALUE;
    }

    int uIndex = GET_INDEX_OF_USERID(userId);
    sensorHandle = GET_ACTUAL_HANDLE(sensorHandle);
    // Only generate a flush complete event if the sensor is enabled and if the sensor is not a
    // one-shot sensor.
    if (mSensor[uIndex][sensorHandle].isEnabled) {
        return Result::BAD_VALUE;
    }

    // Note: If a sensor supports batching, write all of the currently batched events for the sensor
    // to the Event FMQ prior to writing the flush complete event.
    Event ev;
    ev.sensorHandle = sensorHandle;
    ev.sensorType = ::android::hardware::sensors::V2_1::SensorType::META_DATA;
    ev.u.meta.what = MetaDataEventType::META_DATA_FLUSH_COMPLETE;
    std::vector<Event> evs{ev};
    mCallback->postEvents(evs, isWakeUpSensor(GET_ACTUAL_HANDLE(sensorHandle)));
    return Result::OK;
}

Event ConcurrentSensor::convertClientEvent(clientSensorEvent *cliEvent) {
    Event event;
    int32_t sensorHandle = getHandleFromType(cliEvent->header.sensorType);
    event.sensorHandle = (cliEvent->header.userId << 8) | sensorHandle;
    event.sensorType = Sensor2_1_Types[sensorHandle];
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
            break;
        default:
            ALOGW("%s Client Event has invalid sensor type: %d", __func__,
                                            cliEvent->header.sensorType);
    }

    return event;
    
}

void ConcurrentSensor::startThread(ConcurrentSensor* sensor) {
    sensor->run();
}

void ConcurrentSensor::run() {
    clientSensorEvent cliEvent;
    while (!mStopThread) {
        // TODO: Do not send events at data injection mode
        {
            {
                std::unique_lock<std::mutex> lock(mCVMutex);
                if (mSensorMsgQueue.empty())
                {
                    mSensorMsgQueueReadyCV.wait(lock);
                }
            }

            std::unique_lock<std::mutex> lock(mSensorMsgQueueMtx);
            cliEvent = std::move(mSensorMsgQueue.front());
            mSensorMsgQueue.pop();
            Event event = convertClientEvent(&cliEvent);
            mCallback->postEvents(std::vector<Event>{event}, isWakeUpSensor(GET_ACTUAL_HANDLE(event.sensorHandle)));
        }
    }
}

void ConcurrentSensor::setOperationMode(OperationMode mode) {
    for (int i = 0; i < MAX_NUM_SENSORS; i++) {
        if (mMode[i] != mode) {
            mMode[i] = mode;
        }
    }
}

bool ConcurrentSensor::supportsDataInjection(int32_t sensorHandle) const {
    sensorHandle = GET_ACTUAL_HANDLE(sensorHandle);
    return info[sensorHandle].flags & static_cast<uint32_t>(SensorFlagBits::DATA_INJECTION);
}

Result ConcurrentSensor::injectEvent(const Event &event) {
    Result result = Result::OK;
    if (event.sensorType == ::android::hardware::sensors::V2_1::SensorType::ADDITIONAL_INFO) {
        // When in OperationMode::NORMAL, SensorType::ADDITIONAL_INFO is used to push operation
        // environment data into the device.
    } else if (!supportsDataInjection(event.sensorHandle)) {
        result = Result::INVALID_OPERATION;
    } else if (mMode[event.sensorHandle] == OperationMode::DATA_INJECTION) {
        mCallback->postEvents(std::vector<Event>{event}, isWakeUpSensor(GET_ACTUAL_HANDLE(event.sensorHandle)));
    } else {
        result = Result::BAD_VALUE;
    }

    return result;
}

void ConcurrentSensor::sendConfigMsg(const void *msg, int len, int32_t userId) {
    sock_client_proxy_t** clients = mSocketServer->get_connected_clients();
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i]) {
            if (clients[i]->userId == userId) {
                int ret = mSocketServer->sendConfigMsg(mSocketServer, msg, len, clients[i]);
                if (ret < 0) {
                    ret = -errno;
                    ALOGE("%s: ERROR: %s", __FUNCTION__, strerror(errno));
                }
                break;
            }
        }
    }
}

}  // namespace implementation
}  // namespace V2_X
}  // namespace sensors
}  // namespace hardware
}  // namespace android
