# SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: LicenseRef-NvidiaProprietary
#
# NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
# property and proprietary rights in and to this material, related
# documentation and any modifications thereto. Any use, reproduction,
# disclosure or distribution of this material and related documentation
# without an express license agreement from NVIDIA CORPORATION or
# its affiliates is strictly prohibited.

"""Minimal ovxr example: load a USD stage and run a VR or AR session.

ovxr does not own scene data. The application creates an ovrtx renderer, an
ovstage holding the scene, attaches the stage to the renderer, and hands ovxr
both to borrow. ovxr then authors its own stereo render products, owns the
OpenXR session, and drives the per-frame render and submit.

Run with a headset or simulator connected and an OpenXR runtime installed:

    python main.py                  # VR session
    python main.py --session-mode ar
    python main.py --frames 300     # stop after 300 submitted frames
"""

import argparse
import signal
import sys

import ovrtx
import ovstage
import ovxr

USD_URL = "https://omniverse-content-production.s3.us-west-2.amazonaws.com/Samples/Robot-OVRTX/robot-ovrtx.usda"

# The prim whose transform anchors the tracking space. OpenXR poses are applied
# relative to this, so the headset starts where this camera sits in the stage.
TRACKING_ANCHOR = "/Render/Camera"

# Consecutive NOT_READY results tolerated before giving up, matching OVXRSmokeApp.
MAX_NOT_READY_STREAK = 300

_running = True


def _stop(_signum, _frame):
    global _running
    _running = False


def parse_args():
    parser = argparse.ArgumentParser(description="Minimal ovxr Python example")
    parser.add_argument(
        "--session-mode",
        choices=("vr", "ar", "auto"),
        default="vr",
        help="OpenXR session mode. Default: vr",
    )
    parser.add_argument(
        "--blend-mode",
        choices=("auto", "opaque", "alpha", "additive"),
        default="auto",
        help="Environment blend mode. AR usually wants alpha or additive. Default: auto",
    )
    parser.add_argument("--usd", default=USD_URL, help=f"USD stage to load. Default: {USD_URL}")
    # Descriptor fallbacks for stages that do not declare upAxis/metersPerUnit.
    # Under SceneInfoPolicy.AUTO ovxr reads both from the attached ovstage's
    # metadata when present, so for well-formed stages these flags are unused.
    # The defaults match the default scene above (upAxis = "Z",
    # metersPerUnit = 1) for stages that omit the metadata.
    parser.add_argument(
        "--up-axis",
        choices=("y", "z"),
        default="z",
        help="Fallback stage up axis when the stage omits upAxis metadata. Default: z",
    )
    parser.add_argument(
        "--meters-per-unit",
        type=float,
        default=1.0,
        help="Fallback metersPerUnit when the stage omits the metadata. Default: 1.0",
    )
    parser.add_argument("--frames", type=int, default=0, help="Stop after N submitted frames (0 = until Ctrl-C)")
    parser.add_argument("--no-foveation", action="store_true", help="Disable local foveation")
    return parser.parse_args()


def main():
    args = parse_args()
    signal.signal(signal.SIGINT, _stop)

    # [snippet:create-renderer]
    # ovxr borrows a renderer; the application owns it and the scene behind it.
    print("Creating renderer (first run compiles and caches shaders)...", file=sys.stderr)
    renderer = ovrtx.Renderer()
    # [/snippet:create-renderer]

    # [snippet:add-usd]
    # OVXR requires an application-owned ovstage attached to the renderer in
    # borrow mode (the default): the stage is the single scene representation,
    # and ovxr authors its runtime state (eye cameras, render products) through
    # it. Populate the stage at a fresh ordinal, then advance the write floor to
    # commit that population before ovxr reads it.
    stage = ovstage.Stage("ovxr.example.minimal")
    renderer.attach_ovstage(stage)

    print(f"Opening {args.usd}...", file=sys.stderr)
    floor_query = stage.get_attribute_write_floor()
    initial_ordinal = stage.fetch_ordinal(floor_query) + 1
    stage.release_ordinal_query(floor_query).wait()
    ovstage.population.open_usd(stage, args.usd, ordinal=initial_ordinal)
    stage.advance_write_floor(initial_ordinal).wait()
    print(
        f"USD loaded into the attached ovstage. Descriptor fallback: "
        f"{args.up_axis.upper()}-up at {args.meters_per_unit} m/unit; stage "
        "metadata wins when the scene declares upAxis/metersPerUnit.",
        file=sys.stderr,
    )
    # [/snippet:add-usd]

    # [snippet:create-context]
    # create_render_products lets ovxr author its own stereo products sized to the
    # runtime's recommended swapchain, so the application does not have to.
    session_mode = {
        "vr": ovxr.SessionMode.VR,
        "ar": ovxr.SessionMode.AR,
        "auto": ovxr.SessionMode.AUTO,
    }[args.session_mode]
    blend_mode = {
        "auto": ovxr.EnvironmentBlendMode.AUTO,
        "opaque": ovxr.EnvironmentBlendMode.OPAQUE,
        "alpha": ovxr.EnvironmentBlendMode.ALPHA_BLEND,
        "additive": ovxr.EnvironmentBlendMode.ADDITIVE,
    }[args.blend_mode]
    desc = ovxr.ContextDesc(
        ovrtx_renderer=renderer,
        ovstage=stage,
        application_name="ovxr.example.minimal",
        display_mode=ovxr.DisplayMode.STEREO,
        session_mode=session_mode,
        environment_blend_mode=blend_mode,
        local_foveation_mode=(
            ovxr.LocalFoveationMode.DISABLED if args.no_foveation else ovxr.LocalFoveationMode.WARPED
        ),
        runtime_foveated_submission_policy=(
            ovxr.FeaturePolicy.DISABLED if args.no_foveation else ovxr.FeaturePolicy.AUTO
        ),
        graphics_api=ovxr.GraphicsApi.VULKAN,
        create_render_products=True,
        tracking_anchor_path=TRACKING_ANCHOR,
        up_axis=ovxr.StageUpAxis.Z if args.up_axis == "z" else ovxr.StageUpAxis.Y,
        meters_per_unit=args.meters_per_unit,
        # AUTO: scale/up-axis come from ovstage metadata when the scene declares
        # them, falling back to the descriptor values above otherwise.
        scene_info_policy=ovxr.SceneInfoPolicy.AUTO,
        foveation_unwarp=not args.no_foveation,
        # Extra OpenXR extensions an application needs go here; ovxr enables only
        # its own. e.g. extra_openxr_extensions=["XR_EXT_hand_tracking"].
    )
    context = ovxr.Context(desc)
    # [/snippet:create-context]

    try:
        # [snippet:probe]
        probe = context.probe_openxr()
        if probe.result == ovxr.Result.SUCCESS:
            print(
                f"OpenXR ready: runtime={probe.runtime_name!r} system={probe.system_name!r} "
                f"views={probe.view_count} "
                f"recommended={probe.recommended_swapchain_width}x{probe.recommended_swapchain_height}",
                file=sys.stderr,
            )
        else:
            print(f"probe_openxr: {ovxr.result_name(probe.result)} — {context.last_error}", file=sys.stderr)
        # [/snippet:probe]

        # [snippet:frame-loop]
        # render_frame drives the whole frame: wait, render the stereo products,
        # and submit to the compositor. NOT_READY simply means the runtime is not
        # asking for frames yet (session still starting, or headset not worn).
        submitted = 0
        not_ready_streak = 0
        while _running and (args.frames == 0 or submitted < args.frames):
            # Sole writer: stage_ordinal defaults to 0 (OVXR's private ledger),
            # so there is no ordinal of ours to reconcile and the seal report is
            # discarded. A co-writer passing its own ordinal must read it.
            result, frame_result, _ = context.render_frame(1.0 / 90.0)
            if result == ovxr.Result.NOT_READY:
                # Transient while the session starts or the headset is idle, but
                # bound it so a runtime that never becomes ready fails visibly
                # instead of spinning forever.
                not_ready_streak += 1
                if not_ready_streak >= MAX_NOT_READY_STREAK:
                    print(
                        f"render_frame returned NOT_READY {not_ready_streak} times in a row; "
                        f"giving up. Last error: {context.last_error}",
                        file=sys.stderr,
                    )
                    break
                continue
            not_ready_streak = 0
            if result != ovxr.Result.SUCCESS:
                print(f"render_frame failed: {ovxr.result_name(result)} — {context.last_error}", file=sys.stderr)
                break
            if frame_result.submitted:
                submitted += 1
                if submitted % 90 == 0:
                    print(f"submitted {submitted} frames", file=sys.stderr)
        # [/snippet:frame-loop]

        print(f"Stopped after {submitted} submitted frame(s).", file=sys.stderr)
    finally:
        # ovxr first: it borrows the renderer and the stage, so it must not
        # outlive them. renderer.destroy() detaches the ovstage; the stage is
        # destroyed last because the renderer borrows it while attached.
        context.destroy()
        renderer.destroy()
        stage.destroy()


if __name__ == "__main__":
    main()
