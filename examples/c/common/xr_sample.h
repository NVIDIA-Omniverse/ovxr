// Copyright (c) 2026, NVIDIA CORPORATION. All rights reserved.
#pragma once

#include <ovrtx/ovrtx.h>
#include <ovrtx/ovrtx_config.h>
#include <ovstage/ovstage.h>
#include <ovstage/ovstage_population.h>
#include <ovxr/ovxr.h>

#include <array>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

#ifdef _WIN32
#    include <conio.h>
#    include <io.h>
#else
#    include <sys/select.h>
#    include <termios.h>
#    include <unistd.h>
#endif

namespace sample
{
constexpr double kDeltaTime = 1.0 / 90.0;
inline volatile std::sig_atomic_t running = 1;
inline void interrupt(int) { running = 0; }

inline void message(const std::string& text)
{
    // logging: allow fprintf - Carbonite-free public example; stderr is its only log sink
    std::fprintf(stderr, "%s\n", text.c_str());
}

inline std::string string(ovx_string_t value)
{
    return value.ptr ? std::string(value.ptr, value.length) : std::string();
}

inline void check(ovxr_result_t result, const char* operation)
{
    if (result != OVXR_SUCCESS)
        throw std::runtime_error(std::string(operation) + ": " + ovxr_get_last_error());
}

inline void checkStage(ovstage_api_status_t status, const char* operation)
{
    if (status != OVSTAGE_OK)
        throw std::runtime_error(std::string(operation) + ": " + string(ovstage_get_last_error()));
}

inline const char* sceneInfoSourceName(ovxr_scene_info_source_t source)
{
    switch (source)
    {
    case OVXR_SCENE_INFO_SOURCE_OVSTAGE: return "OVStage";
    case OVXR_SCENE_INFO_SOURCE_DESCRIPTOR: return "descriptor";
    default: return "default";
    }
}

// Names where the up axis and scale came from. A scene whose metadata never
// reached OVStage still renders, on the descriptor fallback; this line is how
// that shows, and the sample tests require OVStage here.
inline void reportSceneInfo(ovxr_context_t* context)
{
    ovxr_scene_info_t info{};
    check(ovxr_get_scene_info(context, &info), "get_scene_info");
    char text[128];
    std::snprintf(text, sizeof(text), "Scene info: %s-up from %s, %g m/unit from %s",
                  info.up_axis == OVXR_STAGE_UP_AXIS_Z ? "Z" : "Y", sceneInfoSourceName(info.up_axis_source),
                  static_cast<double>(info.meters_per_unit), sceneInfoSourceName(info.meters_per_unit_source));
    message(text);
}

inline void checkRenderer(ovrtx_result_t result, const char* operation)
{
    if (result.status != OVRTX_API_SUCCESS)
        throw std::runtime_error(std::string(operation) + ": " + string(ovrtx_get_last_error()));
}

inline void wait(ovstage_instance_t* stage, ovstage_enqueue_result_t result)
{
    checkStage(result.status, "enqueue");
    ovstage_op_wait_result_t waited{};
    const auto status = ovstage_wait_op(stage, result.op_index, OVSTAGE_TIMEOUT_INFINITE, &waited);
    const std::string error = waited.error_op_id_count ? string(ovstage_get_last_op_error(stage, waited.error_op_ids[0])) : "";
    // Release even when the asynchronous operation failed.
    const auto release = ovstage_release_op(stage, result.op_index);
    checkStage(status, "wait_op");
    checkStage(release, "release_op");
    if (!error.empty())
        throw std::runtime_error(error);
}

inline ovstage_ordinal_t nextOrdinal(ovstage_instance_t* stage)
{
    ovstage_ordinal_query_handle_t query = OVSTAGE_INVALID_ORDINAL_QUERY_HANDLE;
    checkStage(ovstage_get_attribute_write_floor(stage, {}, &query).status, "get_write_floor");
    ovstage_ordinal_t floor = 0;
    const auto status = ovstage_fetch_ordinal(stage, query, OVSTAGE_TIMEOUT_INFINITE, &floor);
    wait(stage, ovstage_release_ordinal_query(stage, query));
    checkStage(status, "fetch_ordinal");
    return floor + 1;
}

// Render once before the frame loop so ovRTX's slow first step (shader
// compilation, scene loading) stays out of OpenXR frame pacing. NOT_READY
// leaves startup to the frame loop, which waits for the session.
inline void warmUp(ovxr_context_t* context, ovstage_ordinal_t ordinal)
{
    bool sealed = false;
    ovxr_warm_up_result_t result{};
    const ovxr_result_t status = ovxr_warm_up(context, ordinal, &sealed, &result);
    if (status == OVXR_NOT_READY)
    {
        message("No renderable XR frame yet; skipping warm-up.");
        return;
    }
    check(status, "warm_up");
    message("Warm-up: " + std::to_string(result.duration_seconds) + " s, " + std::to_string(result.step_count) +
            " renderer step(s)");
}

using Matrix = std::array<double, 16>;
inline Matrix transform(double x = 0, double y = 0, double z = 0, double yaw = 0)
{
    const double c = std::cos(yaw), s = std::sin(yaw);
    // Row vectors, Y-up, meters; translation lives in elements 12..14.
    return { c, 0, -s, 0, 0, 1, 0, 0, s, 0, c, 0, x, y, z, 1 };
}

struct Options
{
    int frames = 0;
    bool noFoveation = false;
    bool demo = false;
    int demoInterval = 180;
    bool help = false;
};

inline int nonnegative(const std::string& value)
{
    size_t consumed = 0;
    int number = std::stoi(value, &consumed);
    if (number < 0 || consumed != value.size())
        throw std::runtime_error("Expected a nonnegative integer: " + value);
    return number;
}

inline Options options(int argc, char** argv, bool anchors = false)
{
    Options result;
    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h")
            result.help = true;
        else if (arg == "--no-foveation")
            result.noFoveation = true;
        else if (arg == "--frames" && i + 1 < argc)
            result.frames = nonnegative(argv[++i]);
        else if (anchors && arg == "--demo")
            result.demo = true;
        else if (anchors && arg == "--demo-interval" && i + 1 < argc)
        {
            result.demoInterval = nonnegative(argv[++i]);
            if (!result.demoInterval)
                throw std::runtime_error("--demo-interval must be positive");
        }
        else
            throw std::runtime_error("Unknown option or missing value: " + arg);
    }
    if (result.help)
    {
        message("Options: --frames N (submitted frames, 0 = unlimited), --no-foveation, --help");
        if (anchors)
            message("         --demo (cycle anchor modes), --demo-interval N (default 180)");
    }
    return result;
}

struct Configuration
{
    const char* scene = "xr-room.usda";
    ovxr_local_foveation_mode_t localFoveation = OVXR_LOCAL_FOVEATION_WARPED;
    ovxr_feature_policy_t runtimeFoveation = OVXR_FEATURE_AUTO;
    float multiplier = 0.5f;
    float inset = 0.4f;
    bool readback = false;
    bool readbackUnwarp = false;
    uint32_t readbackIndex = 0;
    const ovx_string_t* extensions = nullptr;
    size_t extensionCount = 0;
    ovxr_feature_policy_t depthPolicy = OVXR_FEATURE_AUTO;
    ovxr_display_mode_t displayMode = OVXR_DISPLAY_MODE_STEREO;
    ovxr_session_mode_t sessionMode = OVXR_SESSION_MODE_VR;
    ovxr_environment_blend_mode_t blendMode = OVXR_ENVIRONMENT_BLEND_MODE_OPAQUE;
};

// Members are created in open(), so the destructor also covers partial setup.
// The stage is disposed after a failure; it is never republished or reused.
struct Application
{
    ovrtx_renderer_t* renderer = nullptr;
    ovstage_instance_t* stage = nullptr;
    ovxr_context_t* context = nullptr;
    bool attached = false;
    bool explicitOrdinals = false;

    Application() = default;
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    ~Application()
    {
        try { close(); }
        catch (const std::exception& error) { message(std::string("Cleanup failed: ") + error.what()); }
    }

    void open(const char* executable, const char* name, const Options& options, bool explicitMode = false,
              const Configuration& configuration = {})
    {
        explicitOrdinals = explicitMode;
        std::signal(SIGINT, interrupt);
        message("Creating renderer (the first run may compile shaders)...");
        ovrtx_config_t config{};
        checkRenderer(ovrtx_create_renderer(&config, &renderer), "create_renderer");
        ovstage_instance_desc_t stageDesc{};
        stageDesc.name = name;
        checkStage(ovstage_create_instance(&stageDesc, &stage), "create_stage");
        checkRenderer(ovrtx_attach_ovstage(renderer, stage), "attach_ovstage");
        attached = true;

        message(std::string("Loading ") + configuration.scene + "...");
        const std::string usd =
            (std::filesystem::absolute(executable).parent_path() / "assets" / configuration.scene).string();
        const auto ordinal = nextOrdinal(stage);
        // OVXR reads the scene's up axis and scale from OVStage stage metadata,
        // which the rendering domain does not populate, so request both.
        const ovx_string_t sceneMetadata[] = { { "upAxis", 6 }, { "metersPerUnit", 13 } };
        ovstage_population_desc_t populationDesc{};
        populationDesc.domains = OVSTAGE_POPULATION_DOMAIN_RENDERING;
        populationDesc.stage_metadata_paths = sceneMetadata;
        populationDesc.stage_metadata_path_count = std::size(sceneMetadata);
        auto population = ovstage_population_open_usd_from_file_with_desc(
            stage, { usd.c_str(), usd.size() }, ordinal, OVSTAGE_POPULATION_DEFAULT_TIME, &populationDesc, 1);
        checkStage(population.status, "open_usd");
        ovstage_population_op_wait_result_t populated{};
        checkStage(ovstage_population_wait_op(stage, population.op_index, OVSTAGE_TIMEOUT_INFINITE, &populated), "wait_population");
        if (populated.error_op_id_count)
            throw std::runtime_error(string(ovstage_population_get_last_op_error(populated.error_op_ids[0])));
        ovstage_write_floor_desc_t floor{};
        floor.ordinal = ordinal;
        floor.scope = OVSTAGE_SCOPE_ALL;
        wait(stage, ovstage_advance_write_floor(stage, &floor));

        check(createContext(name, options, configuration), "create_context");
        warmUp(context, explicitOrdinals ? nextOrdinal(stage) : 0);
        message("Scene ready; waiting for XR frames...");
    }

    ovxr_result_t createContext(const char* name, const Options& options, const Configuration& configuration)
    {
        if (context) throw std::runtime_error("Close the old context before recreating it");
        ovxr_setting_entry_t entries[] = {
            ovxr_setting_entry_int(OVXR_SETTING_DISPLAY_MODE, configuration.displayMode),
            ovxr_setting_entry_int(OVXR_SETTING_SESSION_MODE, configuration.sessionMode),
            ovxr_setting_entry_int(OVXR_SETTING_ENVIRONMENT_BLEND_MODE, configuration.blendMode),
            ovxr_setting_entry_int(OVXR_SETTING_GRAPHICS_API, OVXR_GRAPHICS_API_VULKAN),
            ovxr_setting_entry_int(OVXR_SETTING_LOCAL_FOVEATION_MODE,
                                  options.noFoveation ? OVXR_LOCAL_FOVEATION_DISABLED : configuration.localFoveation),
            ovxr_setting_entry_int(OVXR_SETTING_RUNTIME_FOVEATED_SUBMISSION_POLICY,
                                  options.noFoveation ? OVXR_FEATURE_DISABLED : configuration.runtimeFoveation),
            ovxr_setting_entry_int(OVXR_SETTING_FLOOR_ALIGNED_ANCHOR, 1),
            ovxr_setting_entry_int(OVXR_SETTING_DEPTH_SUBMISSION_POLICY, configuration.depthPolicy),
        };
        ovxr_settings_t settings{};
        settings.entries = entries;
        settings.entry_count = std::size(entries);
        ovxr_desc_t desc{};
        desc.ovrtx_renderer = renderer;
        desc.ovstage = stage;
        desc.application_name = name;
        desc.runtime_prim_root = "/OVXR";
        desc.settings = &settings;
        desc.max_frames_in_flight = 1;
        desc.create_render_products = true;
        constexpr char anchor[] = "/World/StartAnchor";
        desc.tracking_anchor_path = { anchor, sizeof(anchor) - 1 };
        desc.scene_info_policy = OVXR_STAGE_SCENE_INFO_AUTO;
        desc.up_axis = OVXR_STAGE_UP_AXIS_Y;
        desc.meters_per_unit = 1.0;
        desc.ordinal_mode = explicitOrdinals ? OVXR_STAGE_ORDINAL_APPLICATION : OVXR_STAGE_ORDINAL_PRIVATE;
        desc.foveation_unwarp = !options.noFoveation;
        desc.foveation_warped_resolution_multiplier = configuration.multiplier;
        desc.foveation_warped_inset_size = configuration.inset;
        desc.enable_frame_readback = configuration.readback;
        desc.readback_foveation_unwarp = configuration.readbackUnwarp;
        desc.readback_render_product_index = configuration.readbackIndex;
        desc.extra_openxr_extensions = configuration.extensions;
        desc.extra_openxr_extension_count = configuration.extensionCount;
        const ovxr_result_t result = ovxr_create(&desc, &context);
        if (result == OVXR_SUCCESS)
            reportSceneInfo(context);
        return result;
    }

    void closeContext()
    {
        if (!context) return;
        ovstage_ordinal_t ordinal = 0;
        try { if (explicitOrdinals) ordinal = nextOrdinal(stage); }
        catch (...) { ovxr_destroy(std::exchange(context, nullptr), 0); throw; }
        check(ovxr_destroy(std::exchange(context, nullptr), ordinal), "destroy_context");
    }

    void close()
    {
        std::string failure;
        const auto attempt = [&failure](auto operation) {
            try { operation(); }
            catch (const std::exception& error) { if (failure.empty()) failure = error.what(); }
        };
        if (context)
        {
            // Always destroy native resources, even if reading the floor fails.
            ovstage_ordinal_t ordinal = 0;
            if (explicitOrdinals)
                attempt([&] { ordinal = nextOrdinal(stage); });
            auto* old = std::exchange(context, nullptr);
            attempt([&] { check(ovxr_destroy(old, ordinal), "destroy_context"); });
        }
        if (attached)
        {
            attached = false;
            attempt([&] { checkRenderer(ovrtx_detach_ovstage(renderer), "detach_ovstage"); });
        }
        if (renderer)
            ovrtx_destroy_renderer(std::exchange(renderer, nullptr));
        if (stage)
            attempt([&] { checkStage(ovstage_destroy_instance(std::exchange(stage, nullptr)), "destroy_stage"); });
        if (!failure.empty())
            throw std::runtime_error(failure);
    }
};

struct FrameGuard
{
    ovxr_context_t* context;
    ovxr_frame_plan_t* plan;
    ~FrameGuard()
    {
        if (plan->frame)
            ovxr_cancel_frame(context, plan);
    }
};

struct Progress
{
    int limit;
    int submitted = 0;
    std::chrono::steady_clock::time_point lastSubmission = std::chrono::steady_clock::now();
    std::chrono::steady_clock::time_point lastWaitingNotice = lastSubmission;
    bool active()
    {
        if (!running || (limit && submitted >= limit))
            return false;
        const auto now = std::chrono::steady_clock::now();
        if (now - lastSubmission > std::chrono::seconds(60))
        {
            if (limit)
                throw std::runtime_error("No frame submitted for 60 seconds; check the OpenXR runtime and headset.");
            if (now - lastWaitingNotice > std::chrono::seconds(60))
            {
                message("No frame submitted for 60 seconds; waiting for the OpenXR session (Ctrl+C to quit).");
                lastWaitingNotice = now;
            }
        }
        return true;
    }
    void record(const ovxr_frame_result_t& frame)
    {
        if (frame.submitted)
        {
            ++submitted;
            lastSubmission = std::chrono::steady_clock::now();
            lastWaitingNotice = lastSubmission;
        }
    }
};

struct Keyboard
{
#ifndef _WIN32
    termios saved{};
    bool configured = false;
    Keyboard()
    {
        if (isatty(STDIN_FILENO) && tcgetattr(STDIN_FILENO, &saved) == 0)
        {
            auto current = saved;
            current.c_lflag &= ~(ICANON | ECHO);
            configured = tcsetattr(STDIN_FILENO, TCSANOW, &current) == 0;
        }
    }
    ~Keyboard() { if (configured) tcsetattr(STDIN_FILENO, TCSANOW, &saved); }
#endif
    char read()
    {
#ifdef _WIN32
        return _isatty(_fileno(stdin)) && _kbhit() ? static_cast<char>(_getch()) : 0;
#else
        if (!configured)
            return 0;
        fd_set input;
        FD_ZERO(&input);
        FD_SET(STDIN_FILENO, &input);
        timeval timeout{};
        char key = 0;
        if (select(STDIN_FILENO + 1, &input, nullptr, nullptr, &timeout) > 0)
            ::read(STDIN_FILENO, &key, 1);
        return key;
#endif
    }
};
} // namespace sample
