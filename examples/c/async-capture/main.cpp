// Copyright (c) 2026, NVIDIA CORPORATION. All rights reserved.
#include "../common/async_capture.h"
#include "../common/frame_copy.h"
#include <sstream>

int main(int argc, char** argv)
{
    try
    {
        sample::Options options;
        options.noFoveation = true;
        int consumerMs = 200;
        std::string reportPath;
        for (int i = 1; i < argc; ++i)
        {
            const std::string arg = argv[i];
            if (arg == "--help" || arg == "-h")
            {
                sample::message("Async capture: --consumer-ms N (default 200), --frames N, --report FILE");
                sample::message("One pending owned frame; replace stale work without waiting for the consumer.");
                return 0;
            }
            if (arg == "--no-foveation") continue;
            if (i + 1 >= argc) throw std::runtime_error("Missing value: " + arg);
            const std::string value = argv[++i];
            if (arg == "--frames") options.frames = sample::nonnegative(value);
            else if (arg == "--consumer-ms") consumerMs = sample::nonnegative(value);
            else if (arg == "--report") reportPath = value;
            else throw std::runtime_error("Unknown option: " + arg);
        }
        sample::Configuration configuration;
        configuration.readback = true;
        sample::Application app;
        app.open(argv[0], "ovxr.example.async-capture", options, false, configuration);
        sample::FrameCopy copier;
        sample::CaptureConsumer consumer(consumerMs); // Joined before app teardown, including exceptions.
        sample::Progress progress{options.frames};
        uint64_t sequence = 0;
        while (progress.active())
        {
            ovxr_frame_result_t frame{};
            const auto result = ovxr_render_frame(app.context, sample::kDeltaTime, 0, &frame, nullptr);
            if (result == OVXR_NOT_READY)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
            sample::check(result, "render_frame");
            progress.record(frame);
            if (!frame.submitted) continue;
            // [snippet:owned-async-capture]
            ovxr_frame_view_t view{};
            const auto acquired = ovxr_acquire_frame(app.context, &view);
            if (acquired == OVXR_NOT_READY) continue;
            sample::check(acquired, "acquire_frame");
            if (view.sequence <= sequence) continue;
            // Copy synchronously before advancing XR. No borrowed pointer is
            // stored in the worker's frame, even while its simulated work sleeps.
            consumer.offer({view.sequence, view.width, view.height, view.pitch_bytes, copier.copy(view)});
            sequence = view.sequence;
            // [/snippet:owned-async-capture]
        }
        consumer.close();
        if (!consumer.consumed) throw std::runtime_error("Session ended without consuming a fresh capture");
        app.close();
        std::ostringstream report;
        report << "{\"submitted\":" << progress.submitted << ",\"offered\":" << consumer.offered
               << ",\"consumed\":" << consumer.consumed << ",\"dropped\":" << consumer.dropped
               << ",\"last_sequence\":" << consumer.lastSequence << ",\"checksum\":" << consumer.checksum
               << ",\"pending_capacity\":1,\"headset_verified\":false}";
        sample::message(report.str());
        if (!reportPath.empty())
        {
            std::ofstream file;
            file.exceptions(std::ios::failbit | std::ios::badbit);
            file.open(reportPath); file << report.str() << '\n'; file.close();
        }
        return 0;
    }
    catch (const std::exception& error) { sample::message(error.what()); return 1; }
}
