// Copyright (c) 2026, NVIDIA CORPORATION. All rights reserved.
#include "../common/xr_sample.h"

int main(int argc, char** argv)
{
    try
    {
        sample::Options options;
        sample::Configuration configuration;
        configuration.scene = "foveation-grid.usda";
        configuration.readback = true; // Inspect image dimensions; no CPU transfer.
        configuration.readbackUnwarp = false;
        // Deliberately strong demo settings make peripheral detail loss visible.
        configuration.multiplier = 0.2f;
        configuration.inset = 0.1f;
        std::string mode = "local";
        bool tuned = false;
        for (int i = 1; i < argc; ++i)
        {
            const std::string arg = argv[i];
            if (arg == "--help" || arg == "-h")
            {
                sample::message("Foveation: --mode off|local|runtime, --multiplier F, --inset F, --frames N");
                sample::message("Local tuning values must be in (0, 1]; runtime mode allows an unfoveated fallback.");
                sample::message("Local demo defaults: --multiplier 0.2 --inset 0.1.");
                return 0;
            }
            if (i + 1 >= argc)
                throw std::runtime_error("Missing value: " + arg);
            const std::string value = argv[++i];
            if (arg == "--frames") options.frames = sample::nonnegative(value);
            else if (arg == "--mode") mode = value;
            else if (arg == "--multiplier" || arg == "--inset")
            {
                size_t consumed = 0;
                const float number = std::stof(value, &consumed);
                if (consumed != value.size() || !std::isfinite(number) || number <= 0 || number > 1)
                    throw std::runtime_error(arg + " must be in (0, 1]");
                (arg == "--multiplier" ? configuration.multiplier : configuration.inset) = number;
                tuned = true;
            }
            else throw std::runtime_error("Unknown option: " + arg);
        }
        if (mode != "off" && mode != "local" && mode != "runtime")
            throw std::runtime_error("--mode must be off, local or runtime");
        if (tuned && mode != "local")
            throw std::runtime_error("--multiplier and --inset apply only to --mode local");

        // [snippet:foveation-policy]
        configuration.localFoveation = mode == "off" ? OVXR_LOCAL_FOVEATION_DISABLED :
                                      mode == "local" ? OVXR_LOCAL_FOVEATION_WARPED : OVXR_LOCAL_FOVEATION_AUTO;
        configuration.runtimeFoveation = mode == "runtime" ? OVXR_FEATURE_AUTO : OVXR_FEATURE_DISABLED;
        // Runtime PQW data takes precedence over local multiplier/inset values.
        // AUTO deliberately falls back when no usable runtime PQW is available.
        // [/snippet:foveation-policy]
        sample::message("Requested foveation mode: " + mode);
        sample::Application app;
        app.open(argv[0], "ovxr.example.foveation", options, false, configuration);
        sample::Progress progress{ options.frames };
        std::string previous;
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
            // [snippet:resolved-foveation]
            // Query AFTER locating/submitting: AUTO may resolve during that frame.
            ovxr_device_info_t info{};
            sample::check(ovxr_get_device_info(app.context, &info), "get_device_info");
            const auto& features = info.features;
            std::string report = "Effective local rendering: " +
                std::string(features.local_foveated_rendering_enabled ? "warped" : "disabled") +
                "; runtime foveated submission enabled: " + (features.foveated_submission_enabled ? "yes" : "no");
            ovxr_frame_view_t view{};
            const auto acquired = ovxr_acquire_frame(app.context, &view);
            if (acquired == OVXR_SUCCESS)
                report += "; rendered readback: " + std::to_string(view.width) + "x" + std::to_string(view.height);
            else if (acquired != OVXR_NOT_READY)
                sample::check(acquired, "acquire_frame");
            if (report != previous)
            {
                sample::message(report);
                previous = report;
            }
            // [/snippet:resolved-foveation]
        }
        app.close();
        sample::message("foveation: submitted " + std::to_string(progress.submitted) + " frames; clean shutdown.");
        return 0;
    }
    catch (const std::exception& error)
    {
        sample::message(std::string("Sample failed: ") + error.what());
        return 1;
    }
}
