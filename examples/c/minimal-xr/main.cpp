// SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: LicenseRef-NvidiaProprietary
//
// NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
// property and proprietary rights in and to this material, related
// documentation and any modifications thereto. Any use, reproduction,
// disclosure or distribution of this material and related documentation
// without an express license agreement from NVIDIA CORPORATION or
// its affiliates is strictly prohibited.

// Minimal ovxr example: load a USD stage and run a VR session (C twin of
// examples/python/minimal-xr).
//
// ovxr does not own scene data. The application creates an ovrtx renderer, an
// ovstage holding the scene, attaches the stage to the renderer, and hands ovxr
// both to borrow. ovxr then authors its own stereo render products, owns the
// OpenXR session, and drives the per-frame render and submit.
//
// The example fails fast: any unexpected API failure prints and exits, so the
// happy path reads straight through. A real application would propagate errors.
//
// Run with a headset or simulator connected and an OpenXR runtime installed:
//
//     minimal-xr                        # VR session, default sample scene
//     minimal-xr --frames 300           # stop after 300 submitted frames
//     minimal-xr --usd path/to/scene.usda --up-axis y --meters-per-unit 0.01

#include <ovrtx/ovrtx.h>
#include <ovrtx/ovrtx_config.h>
#include <ovstage/ovstage.h>
#include <ovstage/ovstage_population.h>
#include <ovxr/ovxr.h>

#include <cmath>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace
{

const char kDefaultUsd[] =
    "https://omniverse-content-production.s3.us-west-2.amazonaws.com/Samples/Robot-OVRTX/robot-ovrtx.usda";

// The prim whose transform anchors the tracking space. OpenXR poses are applied
// relative to this, so the headset starts where this camera sits in the stage.
const char kTrackingAnchor[] = "/Render/Camera";

// Consecutive NOT_READY results tolerated before giving up, matching the
// Python sample and OVXRSmokeApp.
const int kMaxNotReadyStreak = 300;

volatile std::sig_atomic_t g_running = 1;

void onInterrupt(int)
{
    g_running = 0;
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
    ovx_string_or_token_t globalFloor = {};
    checkOvstage(stage, ovstage_get_attribute_write_floor(stage, globalFloor, &handle).status,
                 "ovstage_get_attribute_write_floor");
    ovstage_ordinal_t ordinal = 0;
    checkOvstage(stage, ovstage_fetch_ordinal(stage, handle, OVSTAGE_TIMEOUT_INFINITE, &ordinal),
                 "ovstage_fetch_ordinal");
    waitOvstageOp(stage, ovstage_release_ordinal_query(stage, handle), "ovstage_release_ordinal_query");
    return ordinal + 1;
}

struct Options
{
    std::string usd = kDefaultUsd;
    int frames = 0; // 0 = run until Ctrl-C
    bool noFoveation = false;
    // Descriptor fallbacks for stages without upAxis/metersPerUnit metadata;
    // under OVXR_STAGE_SCENE_INFO_AUTO the attached ovstage's metadata wins.
    ovxr_stage_up_axis_t upAxis = OVXR_STAGE_UP_AXIS_Z;
    float metersPerUnit = 1.0f;
};

Options parseOptions(int argc, char** argv)
{
    Options options;
    for (int i = 1; i < argc; ++i)
    {
        const char* arg = argv[i];
        const bool hasValue = i + 1 < argc;
        if (std::strcmp(arg, "--help") == 0)
        {
            // logging: allow printf - public sample cannot depend on internal logging APIs.
            std::fprintf(stdout,
                         "usage: minimal-xr [--usd PATH] [--frames N] [--no-foveation]"
                         " [--up-axis y|z] [--meters-per-unit F]\n");
            std::exit(0);
        }
        else if (std::strcmp(arg, "--usd") == 0 && hasValue)
        {
            options.usd = argv[++i];
        }
        else if (std::strcmp(arg, "--frames") == 0 && hasValue)
        {
            options.frames = std::atoi(argv[++i]);
        }
        else if (std::strcmp(arg, "--no-foveation") == 0)
        {
            options.noFoveation = true;
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
            std::fprintf(stderr,
                         "usage: minimal-xr [--usd PATH] [--frames N] [--no-foveation]"
                         " [--up-axis y|z] [--meters-per-unit F]\n");
            std::exit(2);
        }
    }
    return options;
}

} // namespace

int main(int argc, char** argv)
{
    const Options options = parseOptions(argc, argv);
    std::signal(SIGINT, onInterrupt);

    // [snippet:create-renderer]
    // ovxr borrows a renderer; the application owns it and the scene behind it.
    // The default (borrow) attach mode renders zero-copy out of the attached
    // ovstage, which is what ovxr requires.
    std::fprintf(stderr, "Creating renderer (first run compiles and caches shaders)...\n");
    ovrtx_config_t rendererConfig = {};
    ovrtx_renderer_t* renderer = NULL;
    checkOvrtx(ovrtx_create_renderer(&rendererConfig, &renderer), "ovrtx_create_renderer");
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
    checkOvstage(NULL, ovstage_create_instance(&stageDesc, &stage), "ovstage_create_instance");
    checkOvrtx(ovrtx_attach_ovstage(renderer, stage), "ovrtx_attach_ovstage");

    std::fprintf(stderr, "Opening %s...\n", options.usd.c_str());
    const ovstage_ordinal_t initialOrdinal = nextOvstageOrdinal(stage);
    ovx_string_t usdPath = { options.usd.c_str(), options.usd.size() };
    ovstage_population_enqueue_result_t populateResult = ovstage_population_open_usd_from_file(
        stage, usdPath, initialOrdinal, NAN, OVSTAGE_POPULATION_DOMAIN_ALL);
    if (populateResult.status != OVSTAGE_OK ||
        ovstage_population_wait_op(stage, populateResult.op_index, OVSTAGE_TIMEOUT_INFINITE, NULL) != OVSTAGE_OK)
    {
        die("ovstage_population_open_usd_from_file", ovstage_population_get_last_error());
    }

    ovstage_write_floor_desc_t writeFloor = {};
    writeFloor.ordinal = initialOrdinal;
    writeFloor.scope = OVSTAGE_SCOPE_ALL;
    waitOvstageOp(stage, ovstage_advance_write_floor(stage, &writeFloor), "ovstage_advance_write_floor");
    std::fprintf(stderr, "USD loaded into the attached ovstage.\n");
    // [/snippet:add-usd]

    // [snippet:create-context]
    // create_render_products lets ovxr author its own stereo products sized to
    // the runtime's recommended swapchain, so the application does not have to.
    ovxr_setting_entry_t settingEntries[] = {
        ovxr_setting_entry_int(OVXR_SETTING_DISPLAY_MODE, OVXR_DISPLAY_MODE_STEREO),
        ovxr_setting_entry_int(OVXR_SETTING_SESSION_MODE, OVXR_SESSION_MODE_VR),
        ovxr_setting_entry_int(OVXR_SETTING_ENVIRONMENT_BLEND_MODE, OVXR_ENVIRONMENT_BLEND_MODE_AUTO),
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
    desc.tracking_anchor_path = { kTrackingAnchor, sizeof(kTrackingAnchor) - 1 };
    // AUTO: scale/up-axis come from ovstage metadata when the scene declares
    // them, falling back to the descriptor values below otherwise.
    desc.scene_info_policy = OVXR_STAGE_SCENE_INFO_AUTO;
    desc.up_axis = options.upAxis;
    desc.meters_per_unit = options.metersPerUnit;
    desc.foveation_unwarp = !options.noFoveation;

    ovxr_context_t* context = NULL;
    if (ovxr_create(&desc, &context) != OVXR_SUCCESS)
    {
        die("ovxr_create", ovxr_get_last_error());
    }
    // [/snippet:create-context]

    // [snippet:probe]
    ovxr_openxr_probe_result_t probe = {};
    if (ovxr_probe_openxr(context, &probe) == OVXR_SUCCESS)
    {
        std::fprintf(stderr, "OpenXR ready: runtime='%s' system='%s' views=%u recommended=%ux%u\n",
                     probe.runtime_name, probe.system_name, probe.view_count,
                     probe.recommended_swapchain_width, probe.recommended_swapchain_height);
    }
    else
    {
        std::fprintf(stderr, "ovxr_probe_openxr failed: %s\n", ovxr_get_last_error());
    }
    // [/snippet:probe]

    // [snippet:frame-loop]
    // render_frame drives the whole frame: wait, render the stereo products,
    // and submit to the compositor. NOT_READY simply means the runtime is not
    // asking for frames yet (session still starting, or headset not worn) --
    // but bound it so a runtime that never becomes ready fails visibly.
    int submitted = 0;
    int notReadyStreak = 0;
    while (g_running && (options.frames == 0 || submitted < options.frames))
    {
        ovxr_frame_result_t frameResult = {};
        /* Sole writer: stage_ordinal 0 uses OVXR's private ledger, so there is
           no ordinal of ours to reconcile and the seal report is not needed. */
        const ovxr_result_t result = ovxr_render_frame(context, 1.0 / 90.0, 0, &frameResult, NULL);
        if (result == OVXR_NOT_READY)
        {
            if (++notReadyStreak >= kMaxNotReadyStreak)
            {
                // logging: allow fprintf - Carbonite-free public example; stderr is its only log sink
                std::fprintf(stderr, "render_frame returned NOT_READY %d times in a row; giving up. Last error: %s\n",
                             notReadyStreak, ovxr_get_last_error());
                break;
            }
            continue;
        }
        notReadyStreak = 0;
        if (result != OVXR_SUCCESS)
        {
            std::fprintf(stderr, "render_frame failed: %s\n", ovxr_get_last_error());
            break;
        }
        if (frameResult.submitted)
        {
            ++submitted;
            if (submitted % 90 == 0)
            {
                std::fprintf(stderr, "submitted %d frames\n", submitted);
            }
        }
    }
    // [/snippet:frame-loop]

    std::fprintf(stderr, "Stopped after %d submitted frame(s).\n", submitted);

    // ovxr first: it borrows the renderer and the stage, so it must not outlive
    // them. Detach before destroying either side; the stage is destroyed last
    // because the renderer borrows it while attached.
    int exitCode = 0;
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
