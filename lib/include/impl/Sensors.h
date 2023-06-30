/*
 * Copyright (C) 2018 The Android Open Source Project
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

#ifndef SENSORS_VHAL_SENSORS_H
#define SENSORS_VHAL_SENSORS_H

#include <android/hardware/sensors/2.0/ISensors.h>
#include <android/hardware/sensors/2.0/types.h>
#include <fmq/MessageQueue.h>
#include <hardware_legacy/power.h>
#include <hidl/MQDescriptor.h>
#include <hidl/Status.h>
#include <log/log.h>

#include <atomic>
#include <memory>
#include <thread>

#include "EventMessageQueueWrapper.h"
#include "SensorFactoryImpl.h"

namespace android {
namespace hardware {
namespace sensors {
namespace V2_X {
namespace implementation {

template <class ISensorsInterface>
struct Sensors : public ISensorsInterface, public ISensorsEventCallback {
    using Event = ::android::hardware::sensors::V1_0::Event;
    using OperationMode = ::android::hardware::sensors::V1_0::OperationMode;
    using RateLevel = ::android::hardware::sensors::V1_0::RateLevel;
    using Result = ::android::hardware::sensors::V1_0::Result;
    using SharedMemInfo = ::android::hardware::sensors::V1_0::SharedMemInfo;
    using EventQueueFlagBits = ::android::hardware::sensors::V2_0::EventQueueFlagBits;
    using SensorTimeout = ::android::hardware::sensors::V2_0::SensorTimeout;
    using WakeLockQueueFlagBits = ::android::hardware::sensors::V2_0::WakeLockQueueFlagBits;
    using ISensorsCallback = ::android::hardware::sensors::V2_0::ISensorsCallback;
    using EventMessageQueue = MessageQueue<Event, kSynchronizedReadWrite>;
    using WakeLockMessageQueue = MessageQueue<uint32_t, kSynchronizedReadWrite>;

    static constexpr const char* kWakeLockName = "SensorsHAL_WAKEUP";

    Sensors()
        : mEventQueueFlag(nullptr),
          mNextHandle(1),
          mOutstandingWakeUpEvents(0),
          mReadWakeLockQueueRun(false),
          mAutoReleaseWakeLockTime(0),
          mHasWakeLock(false) {
        mSensors = SensorFactory::getSensorInstance(this);
    }

    virtual ~Sensors() {
        deleteEventFlag();
        mReadWakeLockQueueRun = false;
        try {
            mWakeLockThread.join();
        } catch (const std::exception& excp) {
            ALOGE("%s Exception !!! %s", __func__, excp.what());
        }
    }

    // Methods from ::android::hardware::sensors::V2_0::ISensors follow.
    Return<void> getSensorsList(V2_0::ISensors::getSensorsList_cb _hidl_cb) override {
        std::vector<V1_0::SensorInfo> mSensorsInfo = mSensors->getSensorInfo();
        std::vector<V1_0::SensorInfo> sensors;
        for (const auto& sensor : mSensorsInfo) {
            sensors.push_back(sensor);
        }
        // Call the HIDL callback with the SensorInfo
        _hidl_cb(sensors);

        return Void();
    }

    Return<Result> setOperationMode(OperationMode mode) override {
        mSensors->setOperationMode(mode);
        return Result::OK;
    }

    Return<Result> activate(int32_t sensorHandle, bool enabled) override {
        mSensors->activate(sensorHandle, enabled);
        return Result::OK;
    }

    Return<Result> initialize(
            const ::android::hardware::MQDescriptorSync<Event>& eventQueueDescriptor,
            const ::android::hardware::MQDescriptorSync<uint32_t>& wakeLockDescriptor,
            const sp<ISensorsCallback>& sensorsCallback) override {
        auto eventQueue =
                std::make_unique<EventMessageQueue>(eventQueueDescriptor, true /* resetPointers */);
        std::unique_ptr<V2_1::implementation::EventMessageQueueWrapperBase> wrapper =
                std::make_unique<V2_1::implementation::EventMessageQueueWrapperV1_0>(eventQueue);
        return initializeBase(wrapper, wakeLockDescriptor, sensorsCallback);
    }

    Return<Result> initializeBase(
            std::unique_ptr<V2_1::implementation::EventMessageQueueWrapperBase>& eventQueue,
            const ::android::hardware::MQDescriptorSync<uint32_t>& wakeLockDescriptor,
            const sp<ISensorsCallback>& sensorsCallback) {
        Result result = Result::OK;

        // Ensure that all sensors are disabled
        for (auto& sensor : mSensorsInfo) {
            mSensors->activate(sensor.sensorHandle, false /* enable */);
        }

        // Stop the Wake Lock thread if it is currently running
        if (mReadWakeLockQueueRun.load()) {
            mReadWakeLockQueueRun = false;
            mWakeLockThread.join();
        }

        // Save a reference to the callback
        mCallback = sensorsCallback;

        // Save the event queue.
        mEventQueue = std::move(eventQueue);

        // Ensure that any existing EventFlag is properly deleted
        deleteEventFlag();

        // Create the EventFlag that is used to signal to the framework that sensor events have been
        // written to the Event FMQ
        if (EventFlag::createEventFlag(mEventQueue->getEventFlagWord(), &mEventQueueFlag) != OK) {
            result = Result::BAD_VALUE;
        }

        // Create the Wake Lock FMQ that is used by the framework to communicate whenever WAKE_UP
        // events have been successfully read and handled by the framework.
        mWakeLockQueue = std::make_unique<WakeLockMessageQueue>(wakeLockDescriptor,
                                                                true /* resetPointers */);

        if (!mCallback || !mEventQueue || !mWakeLockQueue || mEventQueueFlag == nullptr) {
            result = Result::BAD_VALUE;
        }

        // Start the thread to read events from the Wake Lock FMQ
        mReadWakeLockQueueRun = true;
        mWakeLockThread = std::thread(startReadWakeLockThread, this);

        return result;
    }

    Return<Result> batch(int32_t sensorHandle, int64_t samplingPeriodNs,
                         int64_t /* maxReportLatencyNs */) override {
        mSensors->batch(sensorHandle, samplingPeriodNs);
        return Result::OK;
    }

    Return<Result> flush(int32_t sensorHandle) override {
        mSensors->flush(sensorHandle);
        return Result::OK;
    }

    Return<Result> injectSensorData(const Event& event) override {
        mSensors->injectEvent(V2_1::implementation::convertToNewEvent(event));
        return Result::OK;
    }

    Return<void> registerDirectChannel(const SharedMemInfo& /* mem */,
                                       V2_0::ISensors::registerDirectChannel_cb _hidl_cb) override {
        _hidl_cb(Result::INVALID_OPERATION, -1 /* channelHandle */);
        return Return<void>();
    }

    Return<Result> unregisterDirectChannel(int32_t /* channelHandle */) override {
        return Result::INVALID_OPERATION;
    }

    Return<void> configDirectReport(int32_t /* sensorHandle */, int32_t /* channelHandle */,
                                    RateLevel /* rate */,
                                    V2_0::ISensors::configDirectReport_cb _hidl_cb) override {
        _hidl_cb(Result::INVALID_OPERATION, 0 /* reportToken */);
        return Return<void>();
    }

    void postEvents(const std::vector<V2_1::Event>& events, bool wakeup) override {
        std::lock_guard<std::mutex> lock(mWriteLock);
        if (mEventQueue->write(events)) {
            mEventQueueFlag->wake(static_cast<uint32_t>(EventQueueFlagBits::READ_AND_PROCESS));

            if (wakeup) {
                // Keep track of the number of outstanding WAKE_UP events in order to properly hold
                // a wake lock until the framework has secured a wake lock
                updateWakeLock(events.size(), 0 /* eventsHandled */);
            }
        }
    }

  protected:
    /**
     * Utility function to delete the Event Flag
     */
    void deleteEventFlag() {
        status_t status = EventFlag::deleteEventFlag(&mEventQueueFlag);
        if (status != OK) {
            ALOGI("Failed to delete event flag: %d", status);
        }
    }

    static void startReadWakeLockThread(Sensors* sensors) { sensors->readWakeLockFMQ(); }

    /**
     * Function to read the Wake Lock FMQ and release the wake lock when appropriate
     */
    void readWakeLockFMQ() {
        while (mReadWakeLockQueueRun.load()) {
            constexpr int64_t kReadTimeoutNs = 500 * 1000 * 1000;  // 500 ms
            uint32_t eventsHandled = 0;

            // Read events from the Wake Lock FMQ. Timeout after a reasonable amount of time to
            // ensure that any held wake lock is able to be released if it is held for too long.
            mWakeLockQueue->readBlocking(&eventsHandled, 1 /* count */, 0 /* readNotification */,
                                         static_cast<uint32_t>(WakeLockQueueFlagBits::DATA_WRITTEN),
                                         kReadTimeoutNs);
            updateWakeLock(0 /* eventsWritten */, eventsHandled);
        }
    }

    /**
     * Responsible for acquiring and releasing a wake lock when there are unhandled WAKE_UP events
     */
    void updateWakeLock(int32_t eventsWritten, int32_t eventsHandled) {
        std::lock_guard<std::mutex> lock(mWakeLockLock);
        int32_t newVal = mOutstandingWakeUpEvents + eventsWritten - eventsHandled;
        if (newVal < 0) {
            mOutstandingWakeUpEvents = 0;
        } else {
            mOutstandingWakeUpEvents = newVal;
        }

        if (eventsWritten > 0) {
            // Update the time at which the last WAKE_UP event was sent
            mAutoReleaseWakeLockTime =
                    ::android::uptimeMillis() +
                    static_cast<uint32_t>(SensorTimeout::WAKE_LOCK_SECONDS) * 1000;
        }

        if (!mHasWakeLock && mOutstandingWakeUpEvents > 0 &&
            acquire_wake_lock(PARTIAL_WAKE_LOCK, kWakeLockName) == 0) {
            mHasWakeLock = true;
        } else if (mHasWakeLock) {
            // Check if the wake lock should be released automatically if
            // SensorTimeout::WAKE_LOCK_SECONDS has elapsed since the last WAKE_UP event was written
            // to the Wake Lock FMQ.
            if (::android::uptimeMillis() > mAutoReleaseWakeLockTime) {
                ALOGD("No events read from wake lock FMQ for %d seconds, auto releasing wake lock",
                      SensorTimeout::WAKE_LOCK_SECONDS);
                mOutstandingWakeUpEvents = 0;
            }

            if (mOutstandingWakeUpEvents == 0 && release_wake_lock(kWakeLockName) == 0) {
                mHasWakeLock = false;
            }
        }
    }

    /**
     * The Event FMQ where sensor events are written
     */
    std::unique_ptr<V2_1::implementation::EventMessageQueueWrapperBase> mEventQueue;

    /**
     * The Wake Lock FMQ that is read to determine when the framework has handled WAKE_UP events
     */
    std::unique_ptr<WakeLockMessageQueue> mWakeLockQueue;

    /**
     * Event Flag to signal to the framework when sensor events are available to be read
     */
    EventFlag* mEventQueueFlag;

    /**
     * Callback for asynchronous events, such as dynamic sensor connections.
     */
    sp<ISensorsCallback> mCallback;

    /**
     * The next available sensor handle
     */
    int32_t mNextHandle;

    /**
     * Lock to protect writes to the FMQs
     */
    std::mutex mWriteLock;

    /**
     * Lock to protect acquiring and releasing the wake lock
     */
    std::mutex mWakeLockLock;

    /**
     * Track the number of WAKE_UP events that have not been handled by the framework
     */
    uint32_t mOutstandingWakeUpEvents;

    /**
     * A thread to read the Wake Lock FMQ
     */
    std::thread mWakeLockThread;

    /**
     * Flag to indicate that the Wake Lock Thread should continue to run
     */
    std::atomic_bool mReadWakeLockQueueRun;

    /**
     * Track the time when the wake lock should automatically be released
     */
    int64_t mAutoReleaseWakeLockTime;

    /**
     * Flag to indicate if a wake lock has been acquired
     */
    bool mHasWakeLock;

    /**
     * Sensors Base class instance
     */
    SensorsBase *mSensors;

    /**
     * Sensors info list
     */
    std::vector<V1_0::SensorInfo> mSensorsInfo;
};

}  // namespace implementation
}  // namespace V2_X
}  // namespace sensors
}  // namespace hardware
}  // namespace android

#endif  // SENSORS_VHAL_SENSORS_H
