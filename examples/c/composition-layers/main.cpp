// Copyright (c) 2026, NVIDIA CORPORATION. All rights reserved.
#include "../common/xr_sample.h"
#include "../common/sample_support.h"
#include <algorithm>
#include <fstream>
#include <memory>
#include <vector>

namespace
{
constexpr uint32_t kWidth = 256, kHeight = 128;
std::vector<uint8_t> overlayPixels()
{
    std::vector<uint8_t> pixels(kWidth * kHeight * 4, 0);
    const auto rectangle = [&](int x0, int y0, int x1, int y1, std::array<uint8_t, 4> rgba) {
        for (int y = y0; y < y1; ++y)
            for (int x = x0; x < x1; ++x)
                std::copy(rgba.begin(), rgba.end(), pixels.begin() + (y * kWidth + x) * 4);
    };
    rectangle(8, 8, 248, 120, {24, 32, 48, 192});
    rectangle(24, 24, 80, 72, {235, 72, 54, 255});
    rectangle(100, 24, 156, 72, {48, 210, 112, 255});
    rectangle(176, 24, 232, 72, {50, 120, 245, 255});
    rectangle(24, 88, 232, 104, {245, 190, 48, 128});
    return pixels;
}
} // namespace

int main(int argc, char** argv)
{
    try
    {
        sample::Options options;
        options.noFoveation = true;
        std::string placement = "head", reportPath;
        bool verifyUpload = false;
        for (int i = 1; i < argc; ++i)
        {
            const std::string arg = argv[i];
            if (arg == "--help" || arg == "-h")
            {
                sample::message("Composition layers: --placement head|world, --frames N, --verify-upload, --report FILE");
                sample::message("Appends a quad after the scene; no scene depth test. Vulkan runtime required.");
                return 0;
            }
            if (arg == "--no-foveation") continue;
            if (arg == "--verify-upload") { verifyUpload = true; continue; }
            if (i + 1 >= argc) throw std::runtime_error("Missing value: " + arg);
            const std::string value = argv[++i];
            if (arg == "--frames") options.frames = sample::nonnegative(value);
            else if (arg == "--placement") placement = value;
            else if (arg == "--report") reportPath = value;
            else throw std::runtime_error("Unknown option: " + arg);
        }
        if (placement != "head" && placement != "world")
            throw std::runtime_error("--placement must be head or world");
        sample::Application app;
        app.open(argv[0], "ovxr.example.composition-layers", options);
        // Declared after app: owned resources die before the borrowed session/device,
        // including when upload, submission or report writing throws.
        std::unique_ptr<void, decltype(&sample_projection_destroy)> quad(nullptr, sample_projection_destroy);
        const auto pixels = overlayPixels();
        std::vector<uint8_t> readback(verifyUpload ? pixels.size() : 0);
        sample::Progress progress{options.frames};
        uint32_t layered = 0, verified = 0;
        // [snippet:append-quad]
        while (progress.active())
        {
            ovxr_frame_plan_t plan{};
            auto result = ovxr_begin_frame(app.context, &plan);
            if (result == OVXR_NOT_READY)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
            sample::check(result, "begin_frame");
            sample::FrameGuard guard{app.context, &plan};
            result = ovxr_bind_frame(app.context, &plan, sample::kDeltaTime, 0, nullptr);
            if (result == OVXR_NOT_READY) continue;
            sample::check(result, "bind_frame");
            const void* layer = nullptr;
            if (plan.step_result != OVRTX_INVALID_STEP_RESULT_HANDLE)
            {
                if (!quad)
                {
                    ovxr_openxr_handles_t xr{};
                    ovxr_vulkan_handles_t vk{};
                    sample::check(ovxr_get_openxr_handles(app.context, &xr), "get_openxr_handles");
                    sample::check(ovxr_get_vulkan_handles(app.context, &vk), "get_vulkan_handles");
                    quad.reset(sample_projection_create(reinterpret_cast<uint64_t>(app.renderer),
                        xr.xr_instance, xr.xr_session, xr.xr_space, xr.xr_get_instance_proc_addr,
                        vk.vk_instance, vk.vk_physical_device, vk.vk_device, vk.vk_queue, vk.queue_family_index));
                    if (!quad) throw std::runtime_error(sample_support_error());
                }
                // The adapter finishes GPU work and releases the acquired XR image
                // before returning. Its layer descriptor stays alive through submit.
                layer = sample_quad_upload(quad.get(), pixels.data(), verifyUpload ? readback.data() : nullptr,
                                           kWidth, kHeight, placement == "head");
                if (!layer) throw std::runtime_error(sample_support_error());
                if (verifyUpload)
                {
                    if (pixels != readback) throw std::runtime_error("OpenXR swapchain upload/readback differs");
                    ++verified;
                }
            }
            ovxr_submit_desc_t submit{};
            const auto* header = static_cast<const XrCompositionLayerBaseHeader*>(layer);
            submit.additional_layers = layer ? &header : nullptr;
            submit.additional_layer_count = layer ? 1 : 0;
            // Keep the original scene projection; this independent quad is an overlay.
            ovxr_frame_result_t frame{};
            sample::check(ovxr_submit_frame(app.context, &plan, &submit, &frame), "submit_frame");
            if (frame.submitted && layer) ++layered;
            progress.record(frame);
        }
        // [/snippet:append-quad]
        if (options.frames && progress.submitted != options.frames)
            throw std::runtime_error("Session ended before the requested submitted-frame count");
        if (!reportPath.empty())
        {
            const auto path = std::filesystem::u8path(reportPath);
            if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path());
            if (verifyUpload && verified)
            {
                // Raw RGBA byte order and fixed dimensions are recorded below.
                std::ofstream image;
                image.exceptions(std::ios::failbit | std::ios::badbit);
                image.open(path.parent_path() / "overlay-readback.rgba", std::ios::binary);
                image.write(reinterpret_cast<const char*>(readback.data()), readback.size());
                image.close();
            }
            std::ofstream report;
            report.exceptions(std::ios::failbit | std::ios::badbit);
            report.open(path);
            report << "{\"submitted\":" << progress.submitted << ",\"layer_submissions\":" << layered
                   << ",\"verified_uploads\":" << verified << ",\"placement\":\"" << placement
                   << "\",\"readback_width\":" << kWidth << ",\"readback_height\":" << kHeight
                   << ",\"scene_depth_tested\":false,\"headset_verified\":false}\n";
            report.close();
        }
        sample::message("composition-layers: submitted " + std::to_string(progress.submitted) + " frames.");
        return 0;
    }
    catch (const std::exception& error) { sample::message(error.what()); return 1; }
}
