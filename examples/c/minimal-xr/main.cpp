// SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: LicenseRef-NvidiaProprietary
//
// NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
// property and proprietary rights in and to this material, related
// documentation and any modifications thereto. Any use, reproduction,
// disclosure or distribution of this material and related documentation
// without an express license agreement from NVIDIA CORPORATION or
// its affiliates is strictly prohibited.

// Minimal ovxr example: load a USD stage and run a VR or AR session (C twin of
// examples/python/minimal-xr).
//
// ovxr does not own scene data. The application creates an ovrtx renderer, an
// ovstage holding the scene, attaches the stage to the renderer, and hands ovxr
// both to borrow. ovxr then authors its own render products (stereo on a
// headset, mono on a handheld display such as an iPad), owns the
// OpenXR session, and drives the per-frame render and submit.
//
// The example fails fast: any unexpected API failure prints and exits, so the
// happy path reads straight through. A real application would propagate errors.
//
// Run with an OpenXR runtime installed and started. Without a usable runtime
// the example exits with status 1 before loading the scene. A headset or
// streaming client may connect before or after it starts: the scene loads
// meanwhile, and the frame loop waits for the session.
//
//     minimal-xr                        # VR session, default sample scene
//     minimal-xr --session-mode ar --blend-mode alpha
//     minimal-xr --frames 300           # stop after 300 submitted frames
//     minimal-xr --wait-timeout 120     # give up after 120 s without frames
//     minimal-xr --usd path/to/scene.usda --up-axis y --meters-per-unit 0.01

#include <ovrtx/ovrtx.h>
#include <ovrtx/ovrtx_config.h>
#include <ovstage/ovstage.h>
#include <ovstage/ovstage_population.h>
#include <ovxr/ovxr.h>

#include "../common/sample_assets.h"
#include "scene_selection.h"

#include <curl/curl.h>

#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>

namespace
{

// Where the headset starts in the default scene, in its Z-up metres: on the
// floor (z = 0), 1 m from the robot on the line toward the scene's authored
// /World/Camera, facing the robot. The heading turns forward (+Y) toward -X
// about +Z. The runtime's floor-level reference space supplies the user's own
// eye height, so the virtual floor meets the real one.
const double kDefaultStartX = 0.3276;
const double kDefaultStartY = -0.9448;
const double kDefaultStartHeadingDegrees = 19.12;

// For other scenes, the prim whose transform anchors the tracking space. OpenXR
// poses are applied relative to this, so the headset starts where this camera
// sits in the stage.
const char kTrackingAnchor[] = "/Render/Camera";

// While render_frame returns NOT_READY, retry at this interval (ovxr looks for
// the headset again on each call) and report the wait at most this often,
// matching the Python sample.
constexpr std::chrono::milliseconds kNotReadyRetryInterval{ 50 };
constexpr std::chrono::seconds kFirstWaitNotice{ 1 };
constexpr std::chrono::seconds kWaitNoticeInterval{ 10 };

// --wait-timeout default for runs bounded by --frames, so a runtime that never
// starts a session still fails visibly. Unbounded runs wait until Ctrl-C.
constexpr double kBoundedRunWaitTimeoutSeconds = 60.0;

volatile std::sig_atomic_t g_running = 1;

void onInterrupt(int)
{
    g_running = 0;
}

void startupMessage(const char* message)
{
    // logging: allow fprintf - Carbonite-free public sample; report slow startup phases to the user.
    std::fprintf(stderr, "%s\n", message);
    std::fflush(stderr);
}

// ovxr_get_last_error() returns a plain C string.
[[noreturn]] void die(const char* call, const char* detail)
{
    std::fprintf(stderr, "%s failed: %s\n", call, detail && detail[0] ? detail : "(no error detail)");
    std::exit(1);
}

// ovx_string_t carries {ptr, length} and is not guaranteed null-terminated.
[[noreturn]] void die(const char* call, ovx_string_t detail)
{
    if (detail.ptr && detail.length)
    {
        std::fprintf(stderr, "%s failed: %.*s\n", call, static_cast<int>(detail.length), detail.ptr);
    }
    else
    {
        std::fprintf(stderr, "%s failed (no error detail)\n", call);
    }
    std::exit(1);
}

void checkOvrtx(ovrtx_result_t result, const char* call)
{
    if (result.status != OVRTX_API_SUCCESS)
    {
        die(call, ovrtx_get_last_error());
    }
}

void checkOvstage(ovstage_instance_t* stage, ovstage_api_status_t status, const char* call)
{
    if (status != OVSTAGE_OK)
    {
        die(call, stage ? ovstage_get_last_error() : ovx_string_t{});
    }
}

// Wait for an enqueued ovstage op to finish, then release it.
void waitOvstageOp(ovstage_instance_t* stage, ovstage_enqueue_result_t result, const char* call)
{
    checkOvstage(stage, result.status, call);
    ovstage_op_wait_result_t waitResult = {};
    checkOvstage(stage, ovstage_wait_op(stage, result.op_index, OVSTAGE_TIMEOUT_INFINITE, &waitResult), call);
    if (waitResult.error_op_id_count != 0)
    {
        die(call, ovstage_get_last_op_error(stage, waitResult.error_op_ids[0]));
    }
    checkOvstage(stage, ovstage_release_op(stage, result.op_index), call);
}

// The next free ordinal: one past the stage-wide attribute write floor.
ovstage_ordinal_t nextOvstageOrdinal(ovstage_instance_t* stage)
{
    ovstage_ordinal_query_handle_t handle = OVSTAGE_INVALID_ORDINAL_QUERY_HANDLE;
    ovx_token_or_string_t globalFloor = {};
    checkOvstage(stage, ovstage_get_attribute_write_floor(stage, globalFloor, &handle).status,
                 "ovstage_get_attribute_write_floor");
    ovstage_ordinal_t ordinal = 0;
    checkOvstage(stage, ovstage_fetch_ordinal(stage, handle, OVSTAGE_TIMEOUT_INFINITE, &ordinal),
                 "ovstage_fetch_ordinal");
    waitOvstageOp(stage, ovstage_release_ordinal_query(stage, handle), "ovstage_release_ordinal_query");
    return ordinal + 1;
}

// Row-major, row-vector anchor: the heading about the stage's +Z up axis, then
// the floor position.
ovxr_anchor_transform_t defaultStartAnchor()
{
    const double heading = kDefaultStartHeadingDegrees * std::acos(-1.0) / 180.0;
    const double c = std::cos(heading);
    const double s = std::sin(heading);
    return { { c, s, 0, 0, -s, c, 0, 0, 0, 0, 1, 0, kDefaultStartX, kDefaultStartY, 0, 1 } };
}

struct Options
{
    std::string usd; // Nonempty means an explicit scene: no automatic fallback.
    std::string fallbackUsd;
    bool offline = false;
    int frames = 0; // 0 = run until Ctrl-C
    // Seconds without a frame before giving up; 0 = wait until Ctrl-C.
    // Negative = default: 0 for unbounded runs, kBoundedRunWaitTimeoutSeconds with --frames.
    double waitTimeout = -1.0;
    bool noFoveation = false;
    ovxr_session_mode_t sessionMode = OVXR_SESSION_MODE_VR;
    ovxr_environment_blend_mode_t blendMode = OVXR_ENVIRONMENT_BLEND_MODE_AUTO;
    // Descriptor fallbacks for stages without upAxis/metersPerUnit metadata;
    // under OVXR_STAGE_SCENE_INFO_AUTO the attached ovstage's metadata wins.
    ovxr_stage_up_axis_t upAxis = OVXR_STAGE_UP_AXIS_Y;
    float metersPerUnit = 1.0f;
};

[[noreturn]] void printUsageAndExit(int exitCode)
{
    // logging: allow fprintf - Carbonite-free public example; CLI help uses stdout, errors use stderr.
    std::fprintf(exitCode == 0 ? stdout : stderr,
                 "usage: minimal-xr [--usd PATH] [--fallback-usd PATH] [--offline] [--frames N] [--wait-timeout SECONDS] [--no-foveation]\n"
                 "                  [--session-mode vr|ar|auto] [--blend-mode auto|opaque|alpha|additive]\n"
                 "                  [--up-axis y|z] [--meters-per-unit F]\n");
    std::exit(exitCode);
}

Options parseOptions(int argc, char** argv)
{
    Options options;
    for (int i = 1; i < argc; ++i)
    {
        const char* arg = argv[i];
        const bool hasValue = i + 1 < argc;
        if (std::strcmp(arg, "--help") == 0 || std::strcmp(arg, "-h") == 0)
        {
            printUsageAndExit(0);
        }
        else if (std::strcmp(arg, "--usd") == 0 && hasValue)
        {
            options.usd = argv[++i];
            if (options.usd.empty()) printUsageAndExit(2);
        }
        else if (std::strcmp(arg, "--fallback-usd") == 0 && hasValue)
        {
            options.fallbackUsd = argv[++i];
            if (options.fallbackUsd.empty()) printUsageAndExit(2);
        }
        else if (std::strcmp(arg, "--offline") == 0)
        {
            options.offline = true;
        }
        else if (std::strcmp(arg, "--frames") == 0 && hasValue)
        {
            options.frames = std::atoi(argv[++i]);
        }
        else if (std::strcmp(arg, "--wait-timeout") == 0 && hasValue)
        {
            options.waitTimeout = std::atof(argv[++i]);
            if (!(options.waitTimeout >= 0.0)) printUsageAndExit(2);
        }
        else if (std::strcmp(arg, "--no-foveation") == 0)
        {
            options.noFoveation = true;
        }
        else if (std::strcmp(arg, "--session-mode") == 0 && hasValue)
        {
            const std::string mode = argv[++i];
            if (mode == "vr") options.sessionMode = OVXR_SESSION_MODE_VR;
            else if (mode == "ar") options.sessionMode = OVXR_SESSION_MODE_AR;
            else if (mode == "auto") options.sessionMode = OVXR_SESSION_MODE_AUTO;
            else printUsageAndExit(2);
        }
        else if (std::strcmp(arg, "--blend-mode") == 0 && hasValue)
        {
            const std::string mode = argv[++i];
            if (mode == "auto") options.blendMode = OVXR_ENVIRONMENT_BLEND_MODE_AUTO;
            else if (mode == "opaque") options.blendMode = OVXR_ENVIRONMENT_BLEND_MODE_OPAQUE;
            else if (mode == "alpha") options.blendMode = OVXR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND;
            else if (mode == "additive") options.blendMode = OVXR_ENVIRONMENT_BLEND_MODE_ADDITIVE;
            else printUsageAndExit(2);
        }
        else if (std::strcmp(arg, "--up-axis") == 0 && hasValue)
        {
            const char* axis = argv[++i];
            options.upAxis = (axis[0] == 'z' || axis[0] == 'Z') ? OVXR_STAGE_UP_AXIS_Z : OVXR_STAGE_UP_AXIS_Y;
        }
        else if (std::strcmp(arg, "--meters-per-unit") == 0 && hasValue)
        {
            options.metersPerUnit = static_cast<float>(std::atof(argv[++i]));
        }
        else
        {
            printUsageAndExit(2);
        }
    }
    if (options.offline && !options.usd.empty()) printUsageAndExit(2);
    return options;
}

// Stop after the first received bytes even if a server ignores Range. The
// public scene is then loaded through OVStage, using its normal resolver/cache.
bool probeRobotAsset(const char* url, std::string& detail)
{
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK)
    {
        detail = "HTTP initialization failed";
        return false;
    }
    CURL* request = curl_easy_init();
    if (!request)
    {
        curl_global_cleanup();
        detail = "HTTP request allocation failed";
        return false;
    }
    bool received = false;
    char error[CURL_ERROR_SIZE] = {};
    curl_easy_setopt(request, CURLOPT_URL, url);
    curl_easy_setopt(request, CURLOPT_RANGE, "0-0");
    curl_easy_setopt(request, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(request, CURLOPT_PROTOCOLS_STR, "http,https");
    curl_easy_setopt(request, CURLOPT_REDIR_PROTOCOLS_STR, "http,https");
    curl_easy_setopt(request, CURLOPT_CONNECTTIMEOUT, 3L);
    curl_easy_setopt(request, CURLOPT_TIMEOUT, 10L);
#if defined(_WIN32)
    curl_easy_setopt(request, CURLOPT_SSL_OPTIONS, static_cast<long>(CURLSSLOPT_NATIVE_CA));
#endif
    curl_easy_setopt(request, CURLOPT_ERRORBUFFER, error);
    curl_easy_setopt(request, CURLOPT_WRITEDATA, &received);
    curl_easy_setopt(request, CURLOPT_WRITEFUNCTION,
                     +[](char*, size_t size, size_t count, void* data) -> size_t {
                         *static_cast<bool*>(data) = size * count != 0;
                         return 0;
                     });
    const CURLcode result = curl_easy_perform(request);
    long status = 0;
    curl_easy_getinfo(request, CURLINFO_RESPONSE_CODE, &status);
    curl_easy_cleanup(request);
    curl_global_cleanup();
    const bool readable = received && status >= 200 && status < 300 &&
                          (result == CURLE_OK || result == CURLE_WRITE_ERROR);
    if (!readable)
        detail = "HTTP " + std::to_string(status) + ": " + (error[0] ? error : curl_easy_strerror(result));
    return readable;
}

} // namespace

int main(int argc, char** argv)
{
    Options options = parseOptions(argc, argv);
    auto fallback = [&options]() -> std::string {
        if (!options.fallbackUsd.empty())
            return options.fallbackUsd;
        const auto executable = ovxr_sample::executableDirectory();
        const auto assets = executable / "assets";
        std::error_code error;
        if (executable.empty() || !std::filesystem::is_regular_file(assets / "minimal-xr.usda", error) ||
            !std::filesystem::is_regular_file(assets / "xr-room.usda", error))
            throw std::runtime_error("Keep assets beside minimal-xr, or pass --fallback-usd or --usd with your own scene.");
        const auto utf8 = (assets / "minimal-xr.usda").u8string();
        // C++20 uses char8_t here; OVStage accepts the same UTF-8 bytes as char.
        return { reinterpret_cast<const char*>(utf8.data()), utf8.size() };
    };
    std::signal(SIGINT, onInterrupt);

    // [snippet:check-runtime]
    // Fail fast without a usable OpenXR runtime, before the comparatively slow
    // renderer and scene startup below. No headset or client yet is not fatal:
    // a streaming runtime reports that until its client connects. Start up
    // anyway so shaders compile and the scene loads before the device arrives;
    // the frame loop then waits for the session.
    ovxr_openxr_runtime_check_t runtimeCheck = {};
    if (ovxr_check_openxr_runtime("ovxr.example.minimal-xr", &runtimeCheck) != OVXR_SUCCESS)
    {
        die("ovxr_check_openxr_runtime", ovxr_get_last_error());
    }
    if (runtimeCheck.status == OVXR_OPENXR_RUNTIME_UNAVAILABLE)
    {
        // logging: allow fprintf - Carbonite-free public example; stderr is its only log sink
        std::fprintf(stderr,
                     "minimal-xr: %s.\n"
                     "Install and start an OpenXR runtime (for CloudXR, start the CloudXR service), make it the "
                     "active runtime, and try again.\n",
                     runtimeCheck.message);
        return 1;
    }
    std::fprintf(stderr, "%s.\n", runtimeCheck.message);
    if (runtimeCheck.status == OVXR_OPENXR_RUNTIME_NO_SYSTEM)
    {
        std::fprintf(stderr, "Loading the scene meanwhile; connect a headset or client at any time.\n");
    }
    // [/snippet:check-runtime]

    ovxr_sample::SceneSelection scene;
    try
    {
        if (options.usd.empty() && !options.offline)
            startupMessage("Checking Robot-OVRTX scene availability...");
        scene = ovxr_sample::selectScene(options.usd, options.offline, probeRobotAsset, fallback);
    }
    catch (const std::exception& error)
    {
        die("select scene", error.what());
    }

    // [snippet:create-renderer]
    // ovxr borrows a renderer; the application owns it and the scene behind it.
    // The default (borrow) attach mode renders zero-copy out of the attached
    // ovstage, which is what ovxr requires.
    startupMessage("Creating renderer. First use may compile and cache shaders; this can take several minutes...");
    ovrtx_config_t rendererConfig = {};
    ovrtx_renderer_t* renderer = NULL;
    checkOvrtx(ovrtx_create_renderer(&rendererConfig, &renderer), "ovrtx_create_renderer");
    startupMessage("Renderer created.");
    // [/snippet:create-renderer]

    // [snippet:add-usd]
    // OVXR requires an application-owned ovstage attached to the renderer in
    // borrow mode: the stage is the single scene representation, and ovxr
    // authors its runtime state (eye cameras, render products) through it.
    // Populate the stage at a fresh ordinal, then advance the write floor to
    // commit that population before ovxr reads it.
    ovstage_instance_desc_t stageDesc = {};
    stageDesc.name = "ovxr.example.minimal-xr";
    ovstage_instance_t* stage = NULL;
    auto load = [&stage, &stageDesc](const std::string& path, std::string& detail) {
        checkOvstage(NULL, ovstage_create_instance(&stageDesc, &stage), "ovstage_create_instance");
        // logging: allow fprintf - Carbonite-free public sample; stderr identifies the scene being loaded.
        std::fprintf(stderr, "Opening %s...\n", path.c_str());
        std::fflush(stderr);
        const ovstage_ordinal_t initialOrdinal = nextOvstageOrdinal(stage);
        const ovx_string_t usdPath = { path.c_str(), path.size() };
        // OVXR needs the coordinate metadata as well as rendering data: it reads
        // the scene's upAxis and metersPerUnit from the stage metadata, which the
        // rendering domain does not populate on its own.
        const ovx_string_t sceneMetadata[] = { { "upAxis", 6 }, { "metersPerUnit", 13 } };
        ovstage_population_desc_t populationDesc = {};
        populationDesc.domains = OVSTAGE_POPULATION_DOMAIN_RENDERING;
        populationDesc.stage_metadata_paths = sceneMetadata;
        populationDesc.stage_metadata_path_count = sizeof(sceneMetadata) / sizeof(sceneMetadata[0]);
        const auto result = ovstage_population_open_usd_from_file_with_desc(
            stage, usdPath, initialOrdinal, OVSTAGE_POPULATION_DEFAULT_TIME, &populationDesc, 1);
        if (result.status != OVSTAGE_OK ||
            ovstage_population_wait_op(stage, result.op_index, OVSTAGE_TIMEOUT_INFINITE, NULL) != OVSTAGE_OK)
        {
            const auto error = ovstage_population_get_last_error();
            detail.assign(error.ptr ? error.ptr : "", error.ptr ? error.length : 0);
            checkOvstage(stage, ovstage_destroy_instance(stage), "ovstage_destroy_instance");
            stage = NULL;
            return false;
        }
        ovstage_write_floor_desc_t floor = {};
        floor.ordinal = initialOrdinal;
        floor.scope = OVSTAGE_SCOPE_ALL;
        waitOvstageOp(stage, ovstage_advance_write_floor(stage, &floor), "ovstage_advance_write_floor");
        return true;
    };
    try
    {
        scene = ovxr_sample::loadScene(scene, load, fallback);
    }
    catch (const std::exception& error)
    {
        ovrtx_destroy_renderer(renderer);
        die("load scene", error.what());
    }
    if (!scene.fallbackReason.empty())
    {
        // logging: allow fprintf - Carbonite-free public sample; explain the selected fallback and load failure.
        std::fprintf(stderr, "Using fallback scene %s: %s\n", scene.path.c_str(), scene.fallbackReason.c_str());
    }
    startupMessage("Attaching scene to renderer...");
    checkOvrtx(ovrtx_attach_ovstage(renderer, stage), "ovrtx_attach_ovstage");
    std::fprintf(stderr, "USD loaded into the attached ovstage.\n");
    // [/snippet:add-usd]

    // [snippet:create-context]
    // create_render_products lets ovxr author its own products sized to the
    // runtime's recommended swapchain, so the application does not have to.
    // AUTO display mode selects stereo when the runtime offers it and mono
    // otherwise, such as CloudXR serving an iPad.
    ovxr_setting_entry_t settingEntries[] = {
        ovxr_setting_entry_int(OVXR_SETTING_DISPLAY_MODE, OVXR_DISPLAY_MODE_AUTO),
        ovxr_setting_entry_int(OVXR_SETTING_SESSION_MODE, options.sessionMode),
        ovxr_setting_entry_int(OVXR_SETTING_ENVIRONMENT_BLEND_MODE, options.blendMode),
        ovxr_setting_entry_int(OVXR_SETTING_LOCAL_FOVEATION_MODE,
                               options.noFoveation ? OVXR_LOCAL_FOVEATION_DISABLED : OVXR_LOCAL_FOVEATION_WARPED),
        ovxr_setting_entry_int(OVXR_SETTING_RUNTIME_FOVEATED_SUBMISSION_POLICY,
                               options.noFoveation ? OVXR_FEATURE_DISABLED : OVXR_FEATURE_AUTO),
        ovxr_setting_entry_int(OVXR_SETTING_GRAPHICS_API, OVXR_GRAPHICS_API_VULKAN),
    };
    ovxr_settings_t settings = {};
    settings.entries = settingEntries;
    settings.entry_count = sizeof(settingEntries) / sizeof(settingEntries[0]);

    ovxr_desc_t desc = {};
    desc.ovrtx_renderer = renderer;
    desc.ovstage = stage;
    desc.application_name = "ovxr.example.minimal-xr";
    desc.runtime_prim_root = "/OVXR";
    desc.settings = &settings;
    desc.max_frames_in_flight = 1;
    desc.create_render_products = true;
    const bool defaultScene = ovxr_sample::usesRobotStartAnchor(scene);
    if (!defaultScene)
    {
        desc.tracking_anchor_path = { kTrackingAnchor, sizeof(kTrackingAnchor) - 1 };
    }
    // AUTO: scale/up-axis come from ovstage metadata when the scene declares
    // them, falling back to the descriptor values below otherwise.
    desc.scene_info_policy = OVXR_STAGE_SCENE_INFO_AUTO;
    desc.up_axis = options.upAxis;
    desc.meters_per_unit = options.metersPerUnit;
    desc.foveation_unwarp = !options.noFoveation;

    ovxr_context_t* context = NULL;
    startupMessage("Creating OpenXR session and render targets...");
    if (ovxr_create(&desc, &context) != OVXR_SUCCESS)
    {
        die("ovxr_create", ovxr_get_last_error());
    }
    if (defaultScene)
    {
        // The default scene has no prim at the start pose, so place the user
        // with ovxr's managed anchor. It takes effect at the first frame.
        const ovxr_anchor_transform_t anchor = defaultStartAnchor();
        if (ovxr_set_managed_anchor_transform(context, &anchor, 0) != OVXR_SUCCESS)
        {
            die("ovxr_set_managed_anchor_transform", ovxr_get_last_error());
        }
    }
    // [/snippet:create-context]

    {
        // Name where the up axis and scale came from. A scene whose metadata
        // never reached OVStage still renders, on the descriptor fallback.
        ovxr_scene_info_t info = {};
        if (ovxr_get_scene_info(context, &info) != OVXR_SUCCESS)
        {
            die("ovxr_get_scene_info", ovxr_get_last_error());
        }
        const auto source = [](ovxr_scene_info_source_t value) {
            return value == OVXR_SCENE_INFO_SOURCE_OVSTAGE    ? "OVStage" :
                   value == OVXR_SCENE_INFO_SOURCE_DESCRIPTOR ? "descriptor" :
                                                                "default";
        };
        // logging: allow fprintf - Carbonite-free public sample; report the resolved scene units.
        std::fprintf(stderr, "Scene info: %s-up from %s, %g m/unit from %s\n",
                     info.up_axis == OVXR_STAGE_UP_AXIS_Z ? "Z" : "Y", source(info.up_axis_source),
                     static_cast<double>(info.meters_per_unit), source(info.meters_per_unit_source));
    }

    // [snippet:probe]
    ovxr_openxr_probe_result_t probe = {};
    const ovxr_result_t probed = ovxr_probe_openxr(context, &probe);
    if (probed == OVXR_SUCCESS)
    {
        std::fprintf(stderr, "OpenXR ready: runtime='%s' system='%s' views=%u recommended=%ux%u\n",
                     probe.runtime_name, probe.system_name, probe.view_count,
                     probe.recommended_swapchain_width, probe.recommended_swapchain_height);
    }
    else if (probed == OVXR_NOT_READY)
    {
        // No headset or client yet; the frame loop below waits for one.
        std::fprintf(stderr, "OpenXR not ready yet: %s\n", ovxr_get_last_error());
    }
    else
    {
        std::fprintf(stderr, "ovxr_probe_openxr failed: %s\n", ovxr_get_last_error());
    }
    // [/snippet:probe]

    startupMessage("Warming up renderer. Scene shaders and ray-tracing pipelines may compile; "
                   "first use can take several minutes...");
    // [snippet:warm-up]
    // Render once before the frame loop. ovRTX's first step (shader compilation,
    // scene loading) can take seconds; inside an OpenXR frame it would make the
    // runtime throttle the frames that follow. NOT_READY (no renderable frame
    // yet) leaves startup to the frame loop.
    {
        bool warmUpSealed = false;
        const ovxr_result_t warmUp = ovxr_warm_up(context, 0, &warmUpSealed, NULL);
        if (warmUp != OVXR_SUCCESS && warmUp != OVXR_NOT_READY)
        {
            die("ovxr_warm_up", ovxr_get_last_error());
        }
    }
    // [/snippet:warm-up]

    // [snippet:frame-loop]
    // render_frame drives the whole frame: wait, render the view products,
    // and submit to the compositor. NOT_READY means the runtime is not asking
    // for frames yet: no headset or client is connected, or the session is
    // still starting or idle. ovxr looks for the headset again on each call, so
    // keep calling at a modest rate and report the wait now and then. Unbounded
    // runs wait until Ctrl-C; --wait-timeout bounds the wait without frames.
    using Clock = std::chrono::steady_clock;
    const double waitTimeout =
        options.waitTimeout >= 0.0 ? options.waitTimeout : (options.frames > 0 ? kBoundedRunWaitTimeoutSeconds : 0.0);
    int submitted = 0;
    int exitCode = 0;
    bool waiting = false;
    Clock::time_point waitStart;
    Clock::time_point nextWaitNotice;
    startupMessage("Preparing the first XR frame. Scene shaders and ray-tracing pipelines may compile; "
                   "first use can take several minutes...");
    const auto firstFrameStart = std::chrono::steady_clock::now();
    while (g_running && (options.frames == 0 || submitted < options.frames))
    {
        ovxr_frame_result_t frameResult = {};
        /* Sole writer: stage_ordinal 0 uses OVXR's private ledger, so there is
           no ordinal of ours to reconcile and the seal report is not needed. */
        const ovxr_result_t result = ovxr_render_frame(context, 1.0 / 90.0, 0, &frameResult, NULL);
        if (result != OVXR_SUCCESS && result != OVXR_NOT_READY)
        {
            std::fprintf(stderr, "render_frame failed: %s\n", ovxr_get_last_error());
            exitCode = 1;
            break;
        }
        if (result == OVXR_SUCCESS && frameResult.submitted)
        {
            waiting = false;
            ++submitted;
            if (submitted == 1)
            {
                const double seconds = std::chrono::duration<double>(
                    std::chrono::steady_clock::now() - firstFrameStart).count();
                // logging: allow fprintf - Carbonite-free public sample; confirm actual first-frame submission.
                std::fprintf(stderr, "First XR frame submitted after %.1f s. Startup complete.\n", seconds);
                std::fflush(stderr);
            }
            if (submitted % 90 == 0)
            {
                std::fprintf(stderr, "submitted %d frames\n", submitted);
            }
            continue;
        }
        // No frame submitted: NOT_READY, or a frame the runtime asked not to
        // render. Time the whole stretch without a submitted frame, so frames
        // that render nothing cannot keep postponing the timeout.
        const Clock::time_point now = Clock::now();
        if (!waiting)
        {
            waiting = true;
            waitStart = now;
            nextWaitNotice = now + kFirstWaitNotice;
        }
        const double waitedSeconds = std::chrono::duration<double>(now - waitStart).count();
        if (waitTimeout > 0.0 && waitedSeconds >= waitTimeout)
        {
            // logging: allow fprintf - Carbonite-free public example; stderr is its only log sink
            std::fprintf(stderr, "No OpenXR frames after waiting %.0f s; giving up. Last status: %s\n", waitedSeconds,
                         ovxr_get_last_error());
            exitCode = 1;
            break;
        }
        if (result == OVXR_NOT_READY)
        {
            if (now >= nextWaitNotice)
            {
                std::fprintf(stderr, "Waiting for a headset or client to start the XR session (%s)...\n",
                             ovxr_get_last_error());
                nextWaitNotice = now + kWaitNoticeInterval;
            }
            std::this_thread::sleep_for(kNotReadyRetryInterval);
        }
    }
    // [/snippet:frame-loop]

    std::fprintf(stderr, "Stopped after %d submitted frame(s).\n", submitted);

    // ovxr first: it borrows the renderer and the stage, so it must not outlive
    // them. Detach before destroying either side; the stage is destroyed last
    // because the renderer borrows it while attached.
    if (ovxr_destroy(context, 0) != OVXR_SUCCESS)
    {
        // logging: allow fprintf - Carbonite-free public example; stderr is its only log sink
        std::fprintf(stderr, "ovxr_destroy failed after draining the context: %s\n", ovxr_get_last_error());
        exitCode = 1;
    }
    checkOvrtx(ovrtx_detach_ovstage(renderer), "ovrtx_detach_ovstage");
    ovrtx_destroy_renderer(renderer);
    checkOvstage(stage, ovstage_destroy_instance(stage), "ovstage_destroy_instance");
    return exitCode;
}
