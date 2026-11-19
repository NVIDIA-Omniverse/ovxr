// Copyright (c) 2026, NVIDIA CORPORATION. All rights reserved.
#pragma once
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <exception>
#include <mutex>
#include <numeric>
#include <optional>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

namespace sample
{
struct OwnedFrame
{
    uint64_t sequence;
    uint32_t width, height, pitch;
    std::vector<uint8_t> rgba;
};

class CaptureConsumer
{
    std::mutex mutex;
    std::condition_variable changed;
    std::optional<OwnedFrame> pending;
    bool stopping = false;
    std::exception_ptr error;
    std::chrono::milliseconds delay;

    void run()
    {
        try
        {
            for (;;)
            {
                std::unique_lock<std::mutex> lock(mutex);
                changed.wait(lock, [&] { return pending.has_value() || stopping; });
                if (!pending) return;
                auto frame = std::move(*pending);
                pending.reset();
                // Simulate slow work. Shutdown interrupts the delay and drains
                // the one remaining pending frame. Real work belongs outside the lock.
                changed.wait_for(lock, delay, [&] { return stopping; });
                lock.unlock();
                checksum = std::accumulate(frame.rgba.begin(), frame.rgba.end(), uint32_t(0));
                lastSequence = frame.sequence;
                ++consumed;
            }
        }
        catch (...) { std::lock_guard<std::mutex> lock(mutex); error = std::current_exception(); }
    }
    std::thread worker; // Start only after all state, including counters, is initialized.

public:
    uint64_t offered = 0, dropped = 0, consumed = 0, lastSequence = 0;
    uint32_t checksum = 0;
    explicit CaptureConsumer(int delayMs) : delay(delayMs) { worker = std::thread([this] { run(); }); }
    CaptureConsumer(const CaptureConsumer&) = delete;
    CaptureConsumer& operator=(const CaptureConsumer&) = delete;
    ~CaptureConsumer() { try { close(); } catch (...) {} }

    void offer(OwnedFrame frame)
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (error) std::rethrow_exception(error);
        if (stopping) throw std::runtime_error("Capture consumer is closed");
        if (pending) ++dropped;
        pending = std::move(frame);
        ++offered;
        changed.notify_one();
    }
    void close()
    {
        {
            std::lock_guard<std::mutex> lock(mutex);
            stopping = true;
            changed.notify_one();
        }
        if (worker.joinable()) worker.join();
        if (error) std::rethrow_exception(error);
    }
    // Read counters only after close() joins the worker.
};
} // namespace sample
