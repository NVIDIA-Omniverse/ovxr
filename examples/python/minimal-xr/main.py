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
both to borrow. ovxr then authors its own render products (stereo on a
headset, mono on a handheld display such as an iPad), owns the
OpenXR session, and drives the per-frame render and submit.

Run with an OpenXR runtime installed and started. Without a usable runtime the
example exits with status 1 before loading the scene. A headset or streaming
client may connect before or after it starts: the scene loads meanwhile, and the
frame loop waits for the session.

    python main.py                    # VR session
    python main.py --session-mode ar
    python main.py --frames 300       # stop after 300 submitted frames
    python main.py --wait-timeout 120 # give up after 120 s without frames
"""

import argparse
from http.client import HTTPException
from pathlib import Path
import math
import signal
import sys
import time
from urllib.request import Request, urlopen

import ovrtx
import ovstage
import ovxr

DEFAULT_USD = "https://omniverse-content-production.s3.us-west-2.amazonaws.com/Samples/Robot-OVRTX/robot-ovrtx.usda"
DEFAULT_FALLBACK_USD = Path(__file__).resolve().parents[2] / "assets/minimal-xr.usda"
ROBOT_ASSETS = (
    DEFAULT_USD,
    "https://omniverse-content-production.s3-us-west-2.amazonaws.com/Assets/Isaac/5.1/Isaac/Robots/"
    "FourierIntelligence/GR-1/GR1T2_fourier_hand_6dof/GR1T2_fourier_hand_6dof.usd",
    "https://omniverse-content-production.s3.us-west-2.amazonaws.com/Environments/2023_1/DomeLights/"
    "Indoor/ZetoCGcom_ExhibitionHall_Interior1.hdr",
)

# Where the headset starts in the default scene, in its Z-up metres: on the
# floor (z = 0), 1 m from the robot on the line toward the scene's authored
# /World/Camera, facing the robot. The heading turns forward (+Y) toward -X
# about +Z. The runtime's floor-level reference space supplies the user's own
# eye height, so the virtual floor meets the real one.
DEFAULT_START_X = 0.3276
DEFAULT_START_Y = -0.9448
DEFAULT_START_HEADING_DEGREES = 19.12

# For other scenes, the prim whose transform anchors the tracking space. OpenXR
# poses are applied relative to this, so the headset starts where this camera
# sits in the stage.
TRACKING_ANCHOR = "/Render/Camera"

# While render_frame returns NOT_READY, retry at this interval (ovxr looks for the
# headset again on each call) and report the wait at most this often, matching
# the C sample.
NOT_READY_RETRY_SECONDS = 0.05
FIRST_WAIT_NOTICE_SECONDS = 1.0
WAIT_NOTICE_INTERVAL_SECONDS = 10.0

# --wait-timeout default for runs bounded by --frames, so a runtime that never
# starts a session still fails visibly. Unbounded runs wait until Ctrl-C.
BOUNDED_RUN_WAIT_TIMEOUT_SECONDS = 60.0

_running = True


def _stop(_signum, _frame):
    global _running
    _running = False


def default_start_anchor():
    """Row-major, row-vector anchor: the heading about +Z, then the floor position."""
    heading = math.radians(DEFAULT_START_HEADING_DEGREES)
    c, s = math.cos(heading), math.sin(heading)
    return (c, s, 0.0, 0.0, -s, c, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, DEFAULT_START_X, DEFAULT_START_Y, 0.0, 1.0)


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
    parser.add_argument("--usd", help="Explicit USD path or URL; disables automatic fallback")
    parser.add_argument("--fallback-usd", help="Fallback USD path or URL; default: bundled assets/minimal-xr.usda")
    parser.add_argument("--offline", action="store_true", help="Use the fallback directly without contacting Robot-OVRTX")
    # Descriptor fallbacks for stages that do not declare upAxis/metersPerUnit.
    # Under SceneInfoPolicy.AUTO ovxr reads both from the attached ovstage's
    # metadata when present, so for well-formed stages these flags are unused.
    # The defaults match the bundled scene (upAxis = "Y",
    # metersPerUnit = 1) for stages that omit the metadata.
    parser.add_argument(
        "--up-axis",
        choices=("y", "z"),
        default="y",
        help="Fallback stage up axis when the stage omits upAxis metadata. Default: y",
    )
    parser.add_argument(
        "--meters-per-unit",
        type=float,
        default=1.0,
        help="Fallback metersPerUnit when the stage omits the metadata. Default: 1.0",
    )
    parser.add_argument("--frames", type=int, default=0, help="Stop after N submitted frames (0 = until Ctrl-C)")
    parser.add_argument(
        "--wait-timeout",
        type=float,
        default=None,
        help="Give up after this many seconds without frames (0 = wait until Ctrl-C). "
        f"Default: 0, or {BOUNDED_RUN_WAIT_TIMEOUT_SECONDS:.0f} with --frames",
    )
    parser.add_argument("--no-foveation", action="store_true", help="Disable local foveation")
    args = parser.parse_args()
    if args.usd == "" or args.fallback_usd == "":
        parser.error("Scene paths must not be empty")
    if args.usd is not None and args.offline:
        parser.error("--offline cannot be combined with --usd")
    if args.wait_timeout is not None and not args.wait_timeout >= 0:
        parser.error("--wait-timeout must be >= 0")
    return args


def probe_robot_asset(url):
    """Check GET access, not just the root layer or a successful HEAD request."""
    request = Request(url, headers={"Range": "bytes=0-0"})
    with urlopen(request, timeout=10) as response:
        if not response.read(1):
            raise OSError(f"Empty remote asset: {url}")


def fallback_scene(args):
    if args.fallback_usd is not None:
        return args.fallback_usd
    for scene in (DEFAULT_FALLBACK_USD, DEFAULT_FALLBACK_USD.with_name("xr-room.usda")):
        if not scene.is_file():
            raise FileNotFoundError(f"Bundled scene file is missing: {scene}. Keep examples/assets beside "
                                    "examples/python, or pass --fallback-usd or --usd with your own scene.")
    return str(DEFAULT_FALLBACK_USD)


def select_scene(args):
    if args.usd is not None:
        return args.usd, ""
    if args.offline:
        return fallback_scene(args), "offline mode requested"
    for asset in ROBOT_ASSETS:
        try:
            probe_robot_asset(asset)
        except (OSError, HTTPException) as error:
            return fallback_scene(args), f"cannot read {asset}: {error}"
    return DEFAULT_USD, ""


def open_scene(path):
    # A failed population may have written part of a scene. Never reuse that
    # stage for a fallback or attach it to the renderer.
    stage = ovstage.Stage("ovxr.example.minimal")
    try:
        print(f"Opening {path}...", file=sys.stderr, flush=True)
        floor_query = stage.get_attribute_write_floor()
        initial_ordinal = stage.fetch_ordinal(floor_query) + 1
        stage.release_ordinal_query(floor_query).wait()
        # OVXR needs the coordinate metadata as well as rendering data.
        ovstage.population.open_usd_with_desc(
            stage, path, ordinal=initial_ordinal, time=ovstage.population.DEFAULT_TIME,
            descs=ovstage.population.Desc(
                domains=ovstage.population.PopulationDomain.RENDERING,
                stage_metadata_paths=["upAxis", "metersPerUnit"],
            ),
        )
        stage.advance_write_floor(initial_ordinal).wait()
        return stage
    except BaseException:
        stage.destroy()
        raise


def main():
    args = parse_args()
    signal.signal(signal.SIGINT, _stop)

    # [snippet:check-runtime]
    # Fail fast without a usable OpenXR runtime, before the comparatively slow
    # renderer and scene startup below. No headset or client yet is not fatal:
    # a streaming runtime reports that until its client connects. Start up
    # anyway so shaders compile and the scene loads before the device arrives;
    # the frame loop then waits for the session.
    try:
        runtime = ovxr.check_openxr_runtime("ovxr.example.minimal")
    except ovxr.OvxrError as error:
        # The check itself failed, for example because the OpenXR loader could not be loaded.
        print(f"minimal-xr: {error}", file=sys.stderr)
        return 1
    if runtime.status == ovxr.OpenXrRuntimeStatus.UNAVAILABLE:
        print(
            f"minimal-xr: {runtime.message}.\n"
            "Install and start an OpenXR runtime (for CloudXR, start the CloudXR service), "
            "make it the active runtime, and try again.",
            file=sys.stderr,
        )
        return 1
    print(f"{runtime.message}.", file=sys.stderr)
    if runtime.status == ovxr.OpenXrRuntimeStatus.NO_SYSTEM:
        print("Loading the scene meanwhile; connect a headset or client at any time.", file=sys.stderr)
    # [/snippet:check-runtime]

    try:
        if args.usd is None and not args.offline:
            print("Checking Robot-OVRTX scene availability...", file=sys.stderr, flush=True)
        scene, fallback_reason = select_scene(args)
    except FileNotFoundError as error:
        print(error, file=sys.stderr)
        raise SystemExit(2) from error

    # Track what has been created so a failure part-way through startup still
    # releases it in order, rather than leaving it to interpreter shutdown.
    renderer = stage = context = None
    try:
        # [snippet:create-renderer]
        # ovxr borrows a renderer; the application owns it and the scene behind it.
        print("Creating renderer. First use may compile and cache shaders; this can take several minutes...",
              file=sys.stderr, flush=True)
        renderer = ovrtx.Renderer()
        print("Renderer created.", file=sys.stderr, flush=True)
        # [/snippet:create-renderer]

        # [snippet:add-usd]
        # OVXR requires an application-owned ovstage attached to the renderer in
        # borrow mode (the default): the stage is the single scene representation,
        # and ovxr authors its runtime state (eye cameras, render products) through
        # it. Populate the stage at a fresh ordinal, then advance the write floor to
        # commit that population before ovxr reads it.
        if fallback_reason:
            print(f"Using fallback scene {scene}: {fallback_reason}", file=sys.stderr, flush=True)
        try:
            stage = open_scene(scene)
        except ovstage.OvstageError as error:
            if args.usd is not None or fallback_reason:
                raise
            scene = fallback_scene(args)
            print(f"Using fallback scene {scene}: cannot load Robot-OVRTX: {error}", file=sys.stderr, flush=True)
            stage = open_scene(scene)
        print("Attaching scene to renderer...", file=sys.stderr, flush=True)
        renderer.attach_ovstage(stage)
        print(
            f"USD loaded into the attached ovstage. Descriptor fallback: "
            f"{args.up_axis.upper()}-up at {args.meters_per_unit} m/unit; stage "
            "metadata wins when the scene declares upAxis/metersPerUnit.",
            file=sys.stderr,
        )
        # [/snippet:add-usd]

        # [snippet:create-context]
        # create_render_products lets ovxr author its own products sized to the
        # runtime's recommended swapchain, so the application does not have to.
        # AUTO display mode selects stereo when the runtime offers it and mono
        # otherwise, such as CloudXR serving an iPad.
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
        default_scene = scene == DEFAULT_USD
        desc = ovxr.ContextDesc(
            ovrtx_renderer=renderer,
            ovstage=stage,
            application_name="ovxr.example.minimal",
            display_mode=ovxr.DisplayMode.AUTO,
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
            tracking_anchor_path="" if default_scene else TRACKING_ANCHOR,
            up_axis=ovxr.StageUpAxis.Z if args.up_axis == "z" else ovxr.StageUpAxis.Y,
            meters_per_unit=args.meters_per_unit,
            # AUTO: scale/up-axis come from ovstage metadata when the scene declares
            # them, falling back to the descriptor values above otherwise.
            scene_info_policy=ovxr.SceneInfoPolicy.AUTO,
            foveation_unwarp=not args.no_foveation,
            # Extra OpenXR extensions an application needs go here; ovxr enables only
            # its own. e.g. extra_openxr_extensions=["XR_EXT_hand_tracking"].
        )
        print("Creating OpenXR session and render targets...", file=sys.stderr, flush=True)
        context = ovxr.Context(desc)
        if default_scene:
            # The default scene has no prim at the start pose, so place the user
            # with ovxr's managed anchor. It takes effect at the first frame.
            context.set_managed_anchor_transform(default_start_anchor())
        # [/snippet:create-context]

        # Name where the up axis and scale came from. A scene whose metadata
        # never reached OVStage still renders, on the descriptor fallback.
        scene_info = context.get_scene_info()
        sources = {
            ovxr.SceneInfoSource.OVSTAGE: "OVStage",
            ovxr.SceneInfoSource.DESCRIPTOR: "descriptor",
            ovxr.SceneInfoSource.DEFAULT: "default",
        }
        print(
            f"Scene info: {scene_info.up_axis.name}-up from {sources.get(scene_info.up_axis_source, 'default')}, "
            f"{scene_info.meters_per_unit:g} m/unit from {sources.get(scene_info.meters_per_unit_source, 'default')}",
            file=sys.stderr,
            flush=True,
        )

        # [snippet:probe]
        probe = context.probe_openxr()
        if probe.result == ovxr.Result.SUCCESS:
            print(
                f"OpenXR ready: runtime={probe.runtime_name!r} system={probe.system_name!r} "
                f"views={probe.view_count} "
                f"recommended={probe.recommended_swapchain_width}x{probe.recommended_swapchain_height}",
                file=sys.stderr,
            )
        elif probe.result == ovxr.Result.NOT_READY:
            # No headset or client yet; the frame loop below waits for one.
            print(f"OpenXR not ready yet: {context.last_error}", file=sys.stderr)
        else:
            print(f"probe_openxr: {ovxr.result_name(probe.result)} — {context.last_error}", file=sys.stderr)
        # [/snippet:probe]

        print("Warming up renderer. Scene shaders and ray-tracing pipelines may compile; "
              "first use can take several minutes...", file=sys.stderr, flush=True)
        # [snippet:warm-up]
        # Render once before the frame loop. ovRTX's first step (shader
        # compilation, scene loading) can take seconds; inside an OpenXR frame it
        # would make the runtime throttle the frames that follow. NOT_READY (no
        # renderable frame yet) leaves startup to the frame loop.
        result, _, _ = context.warm_up()
        if result not in (ovxr.Result.SUCCESS, ovxr.Result.NOT_READY):
            raise ovxr.OvxrError("warm_up", result, context.last_error)
        # [/snippet:warm-up]

        # [snippet:frame-loop]
        # render_frame drives the whole frame: wait, render the view products,
        # and submit to the compositor. NOT_READY means the runtime is not asking
        # for frames yet: no headset or client is connected, or the session is
        # still starting or idle. ovxr looks for the headset again on each call,
        # so keep calling at a modest rate and report the wait now and then.
        # Unbounded runs wait until Ctrl-C; --wait-timeout bounds the wait.
        wait_timeout = args.wait_timeout
        if wait_timeout is None:
            wait_timeout = BOUNDED_RUN_WAIT_TIMEOUT_SECONDS if args.frames > 0 else 0.0
        submitted = 0
        wait_start = None
        next_wait_notice = 0.0
        print("Preparing the first XR frame. Scene shaders and ray-tracing pipelines may compile; "
              "first use can take several minutes...", file=sys.stderr, flush=True)
        first_frame_start = time.monotonic()
        while _running and (args.frames == 0 or submitted < args.frames):
            # Sole writer: stage_ordinal defaults to 0 (OVXR's private ledger),
            # so there is no ordinal of ours to reconcile and the seal report is
            # discarded. A co-writer passing its own ordinal must read it.
            result, frame_result, _ = context.render_frame(1.0 / 90.0)
            if result not in (ovxr.Result.SUCCESS, ovxr.Result.NOT_READY):
                print(f"render_frame failed: {ovxr.result_name(result)} — {context.last_error}", file=sys.stderr)
                return 1
            if result == ovxr.Result.SUCCESS and frame_result.submitted:
                wait_start = None
                submitted += 1
                if submitted == 1:
                    print(f"First XR frame submitted after {time.monotonic() - first_frame_start:.1f} s. "
                          "Startup complete.", file=sys.stderr, flush=True)
                if submitted % 90 == 0:
                    print(f"submitted {submitted} frames", file=sys.stderr)
                continue
            # No frame submitted: NOT_READY, or a frame the runtime asked not to
            # render. Time the whole stretch without a submitted frame, so frames
            # that render nothing cannot keep postponing the timeout.
            now = time.monotonic()
            if wait_start is None:
                wait_start = now
                next_wait_notice = now + FIRST_WAIT_NOTICE_SECONDS
            if wait_timeout > 0 and now - wait_start >= wait_timeout:
                print(
                    f"No OpenXR frames after waiting {now - wait_start:.0f} s; giving up. "
                    f"Last status: {context.last_error}",
                    file=sys.stderr,
                )
                return 1
            if result == ovxr.Result.NOT_READY:
                if now >= next_wait_notice:
                    print(
                        f"Waiting for a headset or client to start the XR session ({context.last_error})...",
                        file=sys.stderr,
                    )
                    next_wait_notice = now + WAIT_NOTICE_INTERVAL_SECONDS
                time.sleep(NOT_READY_RETRY_SECONDS)
        # [/snippet:frame-loop]

        print(f"Stopped after {submitted} submitted frame(s).", file=sys.stderr)
    except ovxr.OvxrError as error:
        print(f"minimal-xr: {error}", file=sys.stderr)
        return 1
    finally:
        # ovxr first: it borrows the renderer and the stage, so it must not
        # outlive them. renderer.destroy() detaches the ovstage; the stage is
        # destroyed last because the renderer borrows it while attached.
        try:
            if context is not None:
                context.destroy()
        finally:
            if renderer is not None:
                renderer.destroy()
            if stage is not None:
                stage.destroy()
    return 0


if __name__ == "__main__":
    sys.exit(main())
