// Copyright (c) 2026, NVIDIA CORPORATION. All rights reserved.
#include "../common/interactive_sample.h"
#include <cctype>
#include "../common/transform_writer.h"
#include <memory>

namespace
{
// Both consumers take CPU-owned bytes. The panel's queries must die before the stage.
struct Preview
{
    void* window = nullptr;
    void* panel = nullptr;
    void* observer = nullptr;
    uint64_t desktopFrames = 0, panelUpdates = 0;
    ~Preview() { close(); }
    void close()
    {
        sample_mirror_destroy(std::exchange(window, nullptr));
        sample_panel_destroy(std::exchange(panel, nullptr));
        sample_observer_destroy(std::exchange(observer, nullptr));
    }
    bool present(const std::vector<uint8_t>& rgba = {}, uint32_t width = 0, uint32_t height = 0, uint32_t pitch = 0)
    {
        if (!window) return true;
        const int result = sample_mirror_present(window, rgba.empty() ? nullptr : rgba.data(), width, height, pitch);
        sample::supportCheck(result >= 0);
        if (result && !rgba.empty()) ++desktopFrames;
        return result != 0;
    }
};
void save(const std::filesystem::path& output, const ovxr_frame_view_t& view, const std::vector<uint8_t>& rgba,
          int submitted, const sample::Configuration& config, bool camera)
{
    std::filesystem::create_directories(output.parent_path());
    sample::saveBmp(output, view, rgba);
    auto path = output; path.replace_extension(".json");
    std::ofstream metadata(path);
    metadata.exceptions(std::ios::failbit | std::ios::badbit);
    metadata << "{\n  \"width\": " << view.width << ",\n  \"height\": " << view.height
             << ",\n  \"pitch_bytes\": " << view.pitch_bytes
             << ",\n  \"sequence\": " << view.sequence << ",\n  \"submitted_frame\": " << submitted;
    if (camera)
        metadata << ",\n  \"source\": \"camera\",\n  \"camera_path\": \"/World/PipCamera\","
                 << "\n  \"render_product_path\": \"/Render/PipObserver\"";
    else
        metadata << ",\n  \"source\": \"eye\",\n  \"cuda_device_id\": " << view.cuda_device_id
             << ",\n  \"predicted_display_time_ns\": " << view.predicted_display_time_ns
             << ",\n  \"render_product_index\": " << config.readbackIndex
             << ",\n  \"readback_foveation_unwarp\": " << (config.readbackUnwarp ? "true" : "false");
    metadata << "\n}\n";
    metadata.close();
    sample::message("Saved " + output.string() + " (" + std::to_string(view.width) + "x" +
                    std::to_string(view.height) + ", sequence " + std::to_string(view.sequence) + ")");
}
}
int main(int argc, char** argv)
{
    try
    {
        sample::Options options;
        sample::Configuration configuration;
        configuration.readback = configuration.readbackUnwarp = true;
        int captureAt = 60, previewEvery = 3;
        bool framesSet = false, automaticSave = false, eyeSet = false;
        std::string mode, source;
        std::filesystem::path output = "ovxr-frame.bmp", report;
        for (int i = 1; i < argc; ++i)
        {
            const std::string arg = argv[i];
            if (arg == "--help" || arg == "-h")
            {
                sample::message("Frame capture + PiP: --pip both|desktop|headset|off, --preview-every N (3),");
                sample::message("  --frames N (unlimited with PiP, 120 save-only), --capture-at N (60), --output PATH.bmp,");
                sample::message("  --eye left|right, --raw, --no-foveation, --report PATH.json; S save, Q quit.");
                sample::message("  --source camera|eye (camera with PiP, eye for save-only); --eye/--raw require eye.");
                sample::message("Default: both previews, or save-only when only --output is supplied. Desktop PiP requires Windows.");
                return 0;
            }
            if (arg == "--raw") { configuration.readbackUnwarp = false; continue; }
            if (arg == "--no-foveation") { options.noFoveation = true; continue; }
            if (i + 1 >= argc) throw std::runtime_error("Missing value: " + arg);
            const std::string value = argv[++i];
            if (arg == "--frames") { options.frames = sample::nonnegative(value); framesSet = true; }
            else if (arg == "--capture-at") captureAt = sample::nonnegative(value);
            else if (arg == "--preview-every") previewEvery = sample::nonnegative(value);
            else if (arg == "--pip") mode = value;
            else if (arg == "--source") source = value;
            else if (arg == "--output") { output = value; automaticSave = true; }
            else if (arg == "--report") report = value;
            else if (arg == "--eye")
            {
                if (value != "left" && value != "right") throw std::runtime_error("--eye must be left or right");
                configuration.readbackIndex = value == "right" ? 1 : 0;
                eyeSet = true;
            }
            else throw std::runtime_error("Unknown option: " + arg);
        }
        if (mode.empty()) mode = automaticSave ? "off" : "both";
        if (source.empty()) source = mode == "off" ? "eye" : "camera";
        if (source != "camera" && source != "eye") throw std::runtime_error("--source must be camera or eye");
        const bool camera = source == "camera";
        if (camera && (eyeSet || !configuration.readbackUnwarp)) throw std::runtime_error("--eye and --raw require --source eye");
        configuration.readback = !camera;
        if (mode != "both" && mode != "desktop" && mode != "headset" && mode != "off")
            throw std::runtime_error("--pip must be both, desktop, headset or off");
        if (!framesSet) options.frames = mode == "off" ? 120 : 0;
        if (captureAt < 1 || previewEvery < 1) throw std::runtime_error("--capture-at and --preview-every must be positive");
        if (automaticSave && options.frames && captureAt > options.frames)
            throw std::runtime_error("--capture-at must be no larger than finite --frames when saving");
        if (mode == "off" && !automaticSave) throw std::runtime_error("--pip off requires --output PATH.bmp");
        if (output.extension() != ".bmp") throw std::runtime_error("--output must end in .bmp");
        output = std::filesystem::absolute(output);
        if (!report.empty())
        {
            report = std::filesystem::weakly_canonical(report);
            auto metadata = output; metadata.replace_extension(".json");
            if (report == std::filesystem::weakly_canonical(output) || report == std::filesystem::weakly_canonical(metadata))
                throw std::runtime_error("--report must differ from the snapshot BMP and its JSON sidecar");
        }
        const bool desktop = mode == "both" || mode == "desktop", headset = mode == "both" || mode == "headset";
        if (headset) configuration.scene = "pip-room.usda";
        else if (camera) configuration.scene = "pip-camera.usda";
        sample::FrameCopy copy;
        sample::Application app;
        app.open(argv[0], "ovxr.example.frame-capture", options, headset || camera, configuration);
        Preview preview;
        std::unique_ptr<sample::TransformWriter> writer;
        if (camera)
        {
            writer = std::make_unique<sample::TransformWriter>(app.stage);
            writer->open();
            preview.observer = sample_observer_create(reinterpret_cast<uint64_t>(app.renderer));
            sample::supportCheck(preview.observer != nullptr);
        }
        if (desktop)
        {
            preview.window = sample_pip_create(camera ? "OVXR scene camera PiP | S save, Q quit" : "OVXR eye PiP | S save, Q quit");
            sample::supportCheck(preview.window != nullptr);
        }
        if (headset)
        {
            preview.panel = sample_panel_create(reinterpret_cast<uint64_t>(app.stage));
            sample::supportCheck(preview.panel != nullptr);
        }
        sample::message("PiP: " + mode + "; S saves " + output.string() + "; Q quits.");
        sample::Keyboard keyboard;
        sample::Progress progress{options.frames};
        auto ordinal = headset || camera ? sample::nextOrdinal(app.stage) : 0;
        std::vector<uint8_t> pending;
        uint32_t width = 0, height = 0, pitch = 0;
        uint64_t lastSequence = 0, previews = 0, saves = 0;
        auto lastCapture = std::chrono::steady_clock::now();
        bool saveRequested = false;
        while (progress.active())
        {
            if (!preview.present()) break;
            char key = preview.window ? char(sample_mirror_key(preview.window)) : 0;
            if (!key) key = keyboard.read();
            key = char(std::tolower(static_cast<unsigned char>(key)));
            if (key == 'q') break;
            saveRequested |= key == 's';
            ovxr_frame_result_t frame{};
            if (headset || camera)
            {
                ovxr_frame_plan_t plan{};
                auto result = ovxr_begin_frame(app.context, &plan);
                if (result == OVXR_NOT_READY) { std::this_thread::sleep_for(std::chrono::milliseconds(10)); continue; }
                sample::check(result, "begin_frame");
                sample::FrameGuard guard{app.context, &plan};
                // [snippet:publish-pip-panel]
                // Previous capture is CPU-owned. Publish it at the same ordinal
                // that bind uses for this frame's eyes, then seal exactly once.
                if (!pending.empty())
                {
                    sample::supportCheck(sample_panel_update(preview.panel, pending.data(), width, height, pitch, ordinal) == 0);
                    ++preview.panelUpdates;
                    pending.clear();
                }
                if (camera)
                {
                    const double t = progress.submitted * sample::kDeltaTime;
                    auto matrix = sample::transform(.45*std::sin(t), 1.3, 0, t);
                    writer->write(matrix, ordinal);
                }
                bool sealed = false;
                result = ovxr_bind_frame(app.context, &plan, sample::kDeltaTime, ordinal, &sealed);
                if (sealed) ordinal = sample::nextOrdinal(app.stage);
                if (result == OVXR_NOT_READY) continue; // Retain an unsealed ordinal, retry a new plan.
                sample::check(result, "bind_frame");
                sample::check(ovxr_submit_frame(app.context, &plan, nullptr, &frame), "submit_frame");
                // [/snippet:publish-pip-panel]
            }
            else
            {
                const auto result = ovxr_render_frame(app.context, sample::kDeltaTime, 0, &frame, nullptr);
                if (result == OVXR_NOT_READY) { std::this_thread::sleep_for(std::chrono::milliseconds(10)); continue; }
                sample::check(result, "render_frame");
            }
            progress.record(frame);
            if (!frame.submitted) continue;
            const bool wantPreview = mode != "off" && (!previews || progress.submitted % previewEvery == 0);
            const bool wantSave = saveRequested || (automaticSave && progress.submitted >= captureAt);
            if (mode == "off" && !automaticSave && !saveRequested) continue;
            // [snippet:copy-borrowed-frame]
            ovxr_frame_view_t view{};
            std::vector<uint8_t> rgba;
            auto acquired = OVXR_NOT_READY;
            if (camera && (wantPreview || wantSave))
            {
                SampleImage image{};
                const int status = sample_observer_capture(preview.observer, ordinal-1, &image);
                sample::supportCheck(status >= 0);
                if (!status)
                {
                    view.width = image.width; view.height = image.height; view.pitch_bytes = image.pitch;
                    view.sequence = lastSequence + 1;
                    const auto* pixels = static_cast<const uint8_t*>(image.rgba);
                    rgba.assign(pixels, pixels + size_t(image.pitch)*image.height);
                    acquired = OVXR_SUCCESS;
                }
            }
            else if (!camera) acquired = ovxr_acquire_frame(app.context, &view);
            if (acquired != OVXR_SUCCESS && acquired != OVXR_NOT_READY) sample::check(acquired, "acquire_frame");
            if (acquired == OVXR_SUCCESS && view.sequence > lastSequence)
            {
                lastSequence = view.sequence;
                lastCapture = std::chrono::steady_clock::now();
                if (wantPreview || wantSave)
                {
                    // Exactly one owned copy feeds saving and both displays.
                    if (!camera) rgba = copy.copy(view);
                    if (wantSave)
                    {
                        save(output, view, rgba, progress.submitted, configuration, camera);
                        ++saves; automaticSave = saveRequested = false;
                    }
                    if (wantPreview)
                    {
                        ++previews;
                        if (!preview.present(rgba, view.width, view.height, view.pitch_bytes)) break;
                        if (headset)
                        {
                            width = view.width; height = view.height; pitch = view.pitch_bytes;
                            pending = std::move(rgba); // Retain only CPU bytes and scalar dimensions.
                        }
                    }
                }
            }
            // A repeated sequence is stale even when acquire_frame succeeds.
            // [/snippet:copy-borrowed-frame]
            if ((!camera || wantPreview || wantSave) && std::chrono::steady_clock::now() - lastCapture > std::chrono::seconds(60))
                throw std::runtime_error("No fresh readback for 60 seconds");
        }
        if (automaticSave || saveRequested) throw std::runtime_error("Session ended without saving a fresh capture; increase --frames");
        if (options.frames && progress.submitted < options.frames) throw std::runtime_error("Session stopped before frame target");
        if (mode != "off" && !previews) throw std::runtime_error("PiP received no fresh frames");
        if (headset && !preview.panelUpdates) throw std::runtime_error("No PiP panel update was rendered; increase --frames");
        writer.reset();
        preview.close();
        app.close();
        if (!report.empty())
        {
            report = std::filesystem::absolute(report);
            std::filesystem::create_directories(report.parent_path());
            std::ofstream file(report);
            file.exceptions(std::ios::failbit | std::ios::badbit);
            file << "{\"submitted_frames\":" << progress.submitted << ",\"pip_mode\":\"" << mode
                 << "\",\"preview_frames\":" << previews << ",\"desktop_frames\":" << preview.desktopFrames
                 << ",\"panel_updates\":" << preview.panelUpdates << ",\"saved_frames\":" << saves
                 << ",\"source\":\"" << source << "\",\"eye\":"
                 << (camera ? "null" : (configuration.readbackIndex ? "\"right\"" : "\"left\""))
                 << ",\"readback_foveation_unwarp\":" << (camera ? "null" : (configuration.readbackUnwarp ? "true" : "false"))
                 << ",\"headset_verified\":false}\n";
        }
        sample::message("PiP frames: desktop=" + std::to_string(preview.desktopFrames) + ", panel=" +
                        std::to_string(preview.panelUpdates) + "; saved=" + std::to_string(saves) + ".");
        sample::message("frame-capture: submitted " + std::to_string(progress.submitted) + " frames; clean shutdown.");
        return 0;
    }
    catch (const std::exception& error) { sample::message(std::string("Sample failed: ") + error.what()); return 1; }
}
