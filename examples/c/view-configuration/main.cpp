// Copyright (c) 2026, NVIDIA CORPORATION. All rights reserved.
#include "../common/xr_sample.h"
#include <fstream>
#include <sstream>

int main(int argc, char** argv)
{
    try
    {
        sample::Options options;
        options.noFoveation = true;
        std::string display = "auto", reportPath;
        for (int i = 1; i < argc; ++i)
        {
            const std::string arg = argv[i];
            if (arg == "--help" || arg == "-h")
            {
                sample::message("View configuration: --display auto|mono|stereo (default auto), --frames N, --report FILE");
                sample::message("Opaque VR; foveation disabled. The runtime must support the requested view configuration.");
                return 0;
            }
            if (arg == "--no-foveation") continue;
            if (i + 1 >= argc) throw std::runtime_error("Missing value: " + arg);
            const std::string value = argv[++i];
            if (arg == "--frames") options.frames = sample::nonnegative(value);
            else if (arg == "--display") display = value;
            else if (arg == "--report") reportPath = value;
            else throw std::runtime_error("Unknown option: " + arg);
        }
        // [snippet:view-configuration]
        sample::Configuration configuration;
        configuration.readback = true; // Only view zero; no CUDA-to-CPU transfer.
        if (display == "auto") configuration.displayMode = OVXR_DISPLAY_MODE_AUTO;
        else if (display == "mono") configuration.displayMode = OVXR_DISPLAY_MODE_MONO;
        else if (display == "stereo") configuration.displayMode = OVXR_DISPLAY_MODE_STEREO;
        else throw std::runtime_error("--display must be auto, mono or stereo");
        sample::Application app;
        app.open(argv[0], "ovxr.example.view-configuration", options, false, configuration);
        ovxr_openxr_probe_result_t probe{};
        const auto probed = ovxr_probe_openxr(app.context, &probe);
        if (probed == OVXR_NOT_READY)
            throw std::runtime_error("Display " + display + " is unavailable or the runtime is not ready: " + ovxr_get_last_error());
        sample::check(probed, "probe_openxr");
        sample::message("Requested " + display + "; resolved views " + std::to_string(probe.view_count) +
                        "; recommended first-view size " + std::to_string(probe.recommended_swapchain_width) + "x" +
                        std::to_string(probe.recommended_swapchain_height));
        sample::Progress progress{options.frames};
        uint32_t width = 0, height = 0;
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
            if (frame.submitted && !width)
            {
                ovxr_frame_view_t view{};
                const auto acquired = ovxr_acquire_frame(app.context, &view);
                if (acquired == OVXR_NOT_READY) continue;
                sample::check(acquired, "acquire_frame");
                width = view.width; height = view.height;
                sample::message("Rendered view 0: " + std::to_string(width) + "x" + std::to_string(height));
            }
        }
        if (!width) throw std::runtime_error("Session ended without first-view readback");
        // [/snippet:view-configuration]
        app.close();
        std::ostringstream report;
        report << "{\"requested_display\":\"" << display << "\",\"view_count\":" << probe.view_count
               << ",\"recommended_width\":" << probe.recommended_swapchain_width
               << ",\"recommended_height\":" << probe.recommended_swapchain_height
               << ",\"recommended_size_uniform\":" << (probe.recommended_swapchain_size_uniform ? "true" : "false")
               << ",\"rendered_view\":0,\"rendered_width\":" << width << ",\"rendered_height\":" << height
               << ",\"submitted\":" << progress.submitted << ",\"headset_verified\":false}";
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
