// Copyright (c) 2026, NVIDIA CORPORATION. All rights reserved.
#include "../common/interactive_sample.h"

int main(int argc, char** argv)
{
    try
    {
        sample::Options options;
        sample::Configuration config;
        int reconnectAt = 0, retryAfter = 10, maxRestarts = 3;
        std::filesystem::path report;
        for (int i = 1; i < argc; ++i)
        {
            const std::string arg = argv[i];
            if (arg == "--help" || arg == "-h")
            {
                sample::message("Runtime lifecycle: --frames N, --mirror, --no-foveation, --reconnect-at N,");
                sample::message("                   --retry-after SECONDS (10), --max-restarts N (3), --report PATH.json");
                return 0;
            }
            if (arg == "--mirror") { config.readback = config.readbackUnwarp = true; continue; }
            if (arg == "--no-foveation") { options.noFoveation = true; continue; }
            if (++i >= argc) throw std::runtime_error("Missing value: " + arg);
            if (arg == "--report") { report = argv[i]; continue; }
            const int value = sample::nonnegative(argv[i]);
            if (arg == "--frames") options.frames = value;
            else if (arg == "--reconnect-at") reconnectAt = value;
            else if (arg == "--retry-after") retryAfter = value;
            else if (arg == "--max-restarts") maxRestarts = value;
            else throw std::runtime_error("Unknown option: " + arg);
        }
        if (!retryAfter || (reconnectAt && (!maxRestarts || (options.frames && reconnectAt >= options.frames))))
            throw std::runtime_error("Use positive --retry-after and leave frames/restarts available after --reconnect-at");
        constexpr char name[] = "ovxr.example.runtime-lifecycle";
        sample::Application app;
        app.open(argv[0], name, options, false, config);
        sample::Mirror mirror;
        if (config.readback) mirror.open("OVXR runtime lifecycle - R reconnect, Q quit");
        sample::Keyboard keyboard;
        int submitted = 0, restarts = 0, attempts = 0, consecutiveFailures = 0;
        bool exercised = false, reported = false;
        std::string reason;
        auto lastSubmit = std::chrono::steady_clock::now();
        while (sample::running && (!options.frames || submitted < options.frames))
        {
            if (!mirror.update(app.context, false)) break;
            const char windowKey = mirror.key(), key = windowKey ? windowKey : keyboard.read();
            if (key == 'q' || key == 'Q') break;
            if (key == 'r' || key == 'R' || (!exercised && reconnectAt && submitted >= reconnectAt))
            {
                exercised = true;
                reason = "requested session recreation";
            }
            ovxr_frame_result_t frame{};
            if (reason.empty())
            {
                const auto result = ovxr_render_frame(app.context, sample::kDeltaTime, 0, &frame, nullptr);
                // Copy the diagnostic before another OVXR call clears it.
                const std::string error = ovxr_get_last_error();
                if (result == OVXR_ERROR_OPENXR) reason = "OpenXR failure: " + error;
                else if (result != OVXR_SUCCESS && result != OVXR_NOT_READY && result != OVXR_TIMEOUT)
                    sample::check(result, "render_frame");
                if (frame.submitted)
                {
                    ++submitted;
                    consecutiveFailures = 0; // A submitted frame proves recovery.
                    lastSubmit = std::chrono::steady_clock::now();
                    if (!reported)
                    {
                        ovxr_openxr_probe_result_t probe{};
                        sample::check(ovxr_probe_openxr(app.context, &probe), "probe_openxr");
                        sample::message(std::string("Connected: ") + probe.runtime_name + " / " + probe.system_name);
                        reported = true;
                    }
                    if (!mirror.update(app.context, submitted % 3 == 0)) break;
                }
                else
                {
                    if (std::chrono::steady_clock::now() - lastSubmit > std::chrono::seconds(retryAfter))
                        reason = "no submitted frame within retry window: " + error;
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
            }
            if (!reason.empty())
            {
                if (consecutiveFailures >= maxRestarts) throw std::runtime_error("Reconnect budget exhausted: " + reason);
                sample::message("Reconnecting: " + reason);
                // Keep application scene and renderer. Discard all borrowed XR
                // objects; the old context's overlay is released before reuse.
                app.closeContext();
                mirror.sequence = 0;
                ++attempts;
                ++consecutiveFailures;
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                const auto result = app.createContext(name, options, config);
                if (result == OVXR_ERROR_OPENXR)
                {
                    reason = std::string("OpenXR creation failed: ") + ovxr_get_last_error();
                    continue; // Pump window/quit input before the next attempt.
                }
                sample::check(result, "create_context");
                ++restarts;
                reason.clear();
                reported = false;
                lastSubmit = std::chrono::steady_clock::now();
            }
        }
        if (options.frames && submitted < options.frames) throw std::runtime_error("Session stopped before frame target");
        if (config.readback && !mirror.presented) throw std::runtime_error("Mirror received no fresh frames");
        app.close();
        if (!report.empty())
        {
            report = std::filesystem::absolute(report);
            std::filesystem::create_directories(report.parent_path());
            std::ofstream file(report);
            file.exceptions(std::ios::badbit | std::ios::failbit);
            file << "{\"submitted_frames\":" << submitted << ",\"context_restarts\":" << restarts
                 << ",\"reconnect_attempts\":" << attempts
                 << ",\"mirror_frames\":" << mirror.presented << ",\"headset_verified\":false}\n";
            file.close();
            sample::message("Report: " + report.string());
        }
        sample::message("runtime-lifecycle: submitted " + std::to_string(submitted) + " frames; clean shutdown.");
        return 0;
    }
    catch (const std::exception& error) { sample::message(std::string("Sample failed: ") + error.what()); return 1; }
}
