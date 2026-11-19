// Copyright (c) 2026, NVIDIA CORPORATION. All rights reserved.
#include "../common/xr_sample.h"
#include <algorithm>

int main(int argc, char** argv)
{
    try
    {
        const auto options = sample::options(argc, argv, true);
        if (options.help)
            return 0;
        sample::Application app;
        app.open(argv[0], "ovxr.example.anchors-and-teleport", options);
        sample::Progress progress{ options.frames };
        sample::Keyboard keyboard;
        sample::message("Keys: 1 = start, 2 = teleport, 3 = follow moving rig, R = reset origin, Q = quit.");
        char mode = 0;
        while (progress.active())
        {
            const char key = keyboard.read();
            if (key == 'q' || key == 'Q')
                break;
            // The first submitted frame initializes the managed anchor.
            if (progress.submitted)
            {
                const char requested = options.demo ? "123r"[(progress.submitted / options.demoInterval) % 4] : key;
                if (requested != mode && (requested == '1' || requested == '2' || requested == '3' ||
                                         requested == 'r' || requested == 'R'))
                {
                    // [snippet:select-anchor]
                    if (requested == '1' || requested == '2')
                    {
                        const auto matrix = requested == '1' ? sample::transform(0, 0, 3) :
                                                              sample::transform(3, 0, 1, std::acos(-1.0) / 2);
                        ovxr_anchor_transform_t anchor{};
                        std::copy(matrix.begin(), matrix.end(), anchor.matrix);
                        sample::check(ovxr_set_managed_anchor_transform(app.context, &anchor, 0), "set_managed_anchor");
                    }
                    else if (requested == '3')
                    {
                        constexpr char path[] = "/World/MovingRig";
                        sample::check(ovxr_set_anchor_prim(app.context, { path, sizeof(path) - 1 }), "set_anchor_prim");
                    }
                    else
                        sample::check(ovxr_clear_anchor(app.context, 0), "clear_anchor");
                    mode = requested;
                    sample::message(std::string("Requested anchor mode ") + mode);
                    // [/snippet:select-anchor]
                }
                if (mode == '3')
                {
                    // [snippet:live-anchor]
                    // OVXR owns every write after population here. Publish the
                    // moving rig BEFORE the frame reads its live anchor path.
                    const double t = progress.submitted * sample::kDeltaTime;
                    const auto matrix = sample::transform(std::sin(t * 0.5), 0, 3);
                    ovxr_prim_transform_t update{};
                    update.prim_path = "/World/MovingRig";
                    std::copy(matrix.begin(), matrix.end(), update.matrix);
                    sample::check(ovxr_update_prim_transforms(app.context, &update, 1, 0), "update_prim_transforms");
                    // [/snippet:live-anchor]
                }
            }
            ovxr_frame_result_t frame{};
            const auto result = ovxr_render_frame(app.context, sample::kDeltaTime, 0, &frame, nullptr);
            if (result == OVXR_NOT_READY)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
            sample::check(result, "render_frame");
            progress.record(frame);
        }
        app.close();
        sample::message("anchors-and-teleport: submitted " + std::to_string(progress.submitted) + " frames; clean shutdown.");
        return 0;
    }
    catch (const std::exception& error)
    {
        sample::message(std::string("Sample failed: ") + error.what());
        return 1;
    }
}
