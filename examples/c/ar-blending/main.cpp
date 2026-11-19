// Copyright (c) 2026, NVIDIA CORPORATION. All rights reserved.
#include "../common/frame_copy.h"
#include <algorithm>
#include <sstream>

int main(int argc, char** argv)
{
    try
    {
        sample::Options options;
        options.noFoveation = true;
        std::string blend = "alpha", reportPath;
        for (int i = 1; i < argc; ++i)
        {
            const std::string arg = argv[i];
            if (arg == "--help" || arg == "-h")
            {
                sample::message("AR blending: --blend auto|alpha|opaque|additive (default alpha), --frames N, --report FILE");
                sample::message("Stereo AR; foveation disabled. Reports observed RGBA alpha, not headset passthrough proof.");
                return 0;
            }
            if (arg == "--no-foveation") continue;
            if (i + 1 >= argc) throw std::runtime_error("Missing value: " + arg);
            const std::string value = argv[++i];
            if (arg == "--frames") options.frames = sample::nonnegative(value);
            else if (arg == "--blend") blend = value;
            else if (arg == "--report") reportPath = value;
            else throw std::runtime_error("Unknown option: " + arg);
        }
        // [snippet:ar-blending]
        sample::Configuration configuration;
        configuration.scene = "ar-objects.usda";
        configuration.sessionMode = OVXR_SESSION_MODE_AR;
        configuration.readback = true;
        if (blend == "alpha") configuration.blendMode = OVXR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND;
        else if (blend == "opaque") configuration.blendMode = OVXR_ENVIRONMENT_BLEND_MODE_OPAQUE;
        else if (blend == "additive") configuration.blendMode = OVXR_ENVIRONMENT_BLEND_MODE_ADDITIVE;
        else if (blend == "auto") configuration.blendMode = OVXR_ENVIRONMENT_BLEND_MODE_AUTO;
        else throw std::runtime_error("--blend must be auto, alpha, opaque or additive");
        // AR lets OVXR author compositing settings. The scene leaves background
        // rays empty and lights objects with a dome hidden from primary rays.
        sample::Application app;
        app.open(argv[0], "ovxr.example.ar-blending", options, false, configuration);
        sample::FrameCopy copier;
        sample::Progress progress{options.frames};
        uint64_t transparent = 0, opaque = 0;
        uint32_t width = 0, height = 0;
        std::string resolved;
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
            resolved = frame.environment_blend_mode == OVXR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND ? "ALPHA_BLEND" :
                       frame.environment_blend_mode == OVXR_ENVIRONMENT_BLEND_MODE_ADDITIVE ? "ADDITIVE" : "OPAQUE";
            if (!width && progress.submitted >= std::min(options.frames ? options.frames : 30, 30))
            {
                ovxr_frame_view_t view{};
                const auto acquired = ovxr_acquire_frame(app.context, &view);
                if (acquired == OVXR_NOT_READY) continue;
                sample::check(acquired, "acquire_frame");
                const auto rgba = copier.copy(view);
                width = view.width; height = view.height;
                for (uint32_t y = 0; y < height; ++y)
                    for (uint32_t x = 0; x < width; ++x)
                    {
                        const auto alpha = rgba[size_t(y) * view.pitch_bytes + x * 4 + 3];
                        transparent += alpha == 0;
                        opaque += alpha == 255;
                    }
                sample::message("Requested " + blend + "; resolved " + resolved + "; transparent pixels " +
                                std::to_string(transparent) + "; opaque pixels " + std::to_string(opaque));
            }
        }
        // [/snippet:ar-blending]
        if (!width) throw std::runtime_error("Session ended without RGBA readback");
        app.close();
        std::ostringstream report;
        report << "{\"requested_blend\":\"" << blend << "\",\"resolved_blend\":\"" << resolved
               << "\",\"session\":\"ar\",\"width\":" << width << ",\"height\":" << height
               << ",\"transparent_pixels\":" << transparent << ",\"opaque_pixels\":" << opaque
               << ",\"total_pixels\":" << uint64_t(width) * height << ",\"submitted\":" << progress.submitted
               << ",\"headset_verified\":false}";
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
