# Copyright (c) 2026, NVIDIA CORPORATION. All rights reserved.
"""Setup shared by the feature samples; feature code stays in each main.py."""

import argparse
from contextlib import contextmanager, ExitStack
import math
import os
from pathlib import Path
import select
import sys
import time

import ovrtx
import ovstage
import ovxr


SCENE = Path(__file__).resolve().parents[2] / "assets/xr-room.usda"
DELTA_TIME = 1.0 / 90.0


def nonnegative(value):
    number = int(value)
    if number < 0:
        raise argparse.ArgumentTypeError("must be nonnegative")
    return number


def parser(description):
    result = argparse.ArgumentParser(description=description)
    result.add_argument("--frames", type=nonnegative, default=0, help="submitted frames; 0 runs until Ctrl+C")
    result.add_argument("--no-foveation", action="store_true", help="disable local and runtime foveation")
    return result


def transform(x=0.0, y=0.0, z=0.0, yaw=0.0):
    """Row-vector, Y-up, meters: translation lives in elements 12..14."""
    c, s = math.cos(yaw), math.sin(yaw)
    return [c, 0.0, -s, 0.0, 0.0, 1.0, 0.0, 0.0, s, 0.0, c, 0.0, x, y, z, 1.0]


def next_ordinal(stage):
    query = stage.get_attribute_write_floor()
    try:
        return stage.fetch_ordinal(query) + 1
    finally:
        stage.release_ordinal_query(query).wait()


def check(result, context, operation):
    if result != ovxr.Result.SUCCESS:
        raise RuntimeError(f"{operation}: {result.name}: {context.last_error}")


SCENE_INFO_SOURCES = {
    ovxr.SceneInfoSource.OVSTAGE: "OVStage",
    ovxr.SceneInfoSource.DESCRIPTOR: "descriptor",
    ovxr.SceneInfoSource.DEFAULT: "default",
}


def report_scene_info(context):
    """Name where the up axis and scale came from.

    A scene whose metadata never reached OVStage still renders, on the
    descriptor fallback; this line is how that shows, and the sample tests
    require OVStage here.
    """
    info = context.get_scene_info()
    print(f"Scene info: {info.up_axis.name}-up from {SCENE_INFO_SOURCES.get(info.up_axis_source, 'default')}, "
          f"{info.meters_per_unit:g} m/unit from {SCENE_INFO_SOURCES.get(info.meters_per_unit_source, 'default')}",
          flush=True)


def warm_up(context, ordinal):
    """Render once before the frame loop so ovRTX's slow first step stays out of OpenXR frame pacing."""
    result, warm_up_result, _ = context.warm_up(ordinal)
    if result == ovxr.Result.NOT_READY:
        # The frame loop waits for the session instead.
        print("No renderable XR frame yet; skipping warm-up.", flush=True)
        return
    check(result, context, "warm_up")
    print(f"Warm-up: {warm_up_result.duration_seconds:.1f} s, {warm_up_result.step_count} renderer step(s)",
          flush=True)


@contextmanager
def application(name, args, *, explicit_ordinals=False, scene=SCENE, context_options=None, reconnectable=False,
                with_renderer=False):
    # ExitStack also unwinds partial initialization. OVXR goes first, then the
    # renderer detaches and dies, then its borrowed stage is destroyed.
    with ExitStack() as cleanup:
        print("Creating renderer (the first run may compile shaders)...", flush=True)
        renderer = ovrtx.Renderer()
        cleanup.callback(renderer.destroy)
        stage = ovstage.Stage(f"ovxr.example.{name}")
        # Now both exist, reorder callbacks so the borrowed stage outlives the
        # renderer. The earlier callback covers a failure creating the stage.
        cleanup.pop_all()
        cleanup.callback(stage.destroy)
        cleanup.callback(renderer.destroy)
        renderer.attach_ovstage(stage)
        cleanup.callback(renderer.detach_ovstage)
        print(f"Loading {scene.name}...", flush=True)
        ordinal = next_ordinal(stage)
        # OVXR needs the coordinate metadata as well as rendering data.
        ovstage.population.open_usd_with_desc(
            stage, str(scene), ordinal=ordinal, time=ovstage.population.DEFAULT_TIME,
            descs=ovstage.population.Desc(
                domains=ovstage.population.PopulationDomain.RENDERING,
                stage_metadata_paths=["upAxis", "metersPerUnit"],
            ),
        )
        stage.advance_write_floor(ordinal).wait()
        desc_options = dict(
            ovrtx_renderer=renderer,
            ovstage=stage,
            application_name=f"ovxr.example.{name}",
            display_mode=ovxr.DisplayMode.STEREO,
            session_mode=ovxr.SessionMode.VR,
            environment_blend_mode=ovxr.EnvironmentBlendMode.OPAQUE,
            local_foveation_mode=(ovxr.LocalFoveationMode.DISABLED if args.no_foveation
                                  else ovxr.LocalFoveationMode.WARPED),
            runtime_foveated_submission_policy=(ovxr.FeaturePolicy.DISABLED if args.no_foveation
                                               else ovxr.FeaturePolicy.AUTO),
            graphics_api=ovxr.GraphicsApi.VULKAN,
            create_render_products=True,
            tracking_anchor_path="/World/StartAnchor",
            floor_aligned_anchor=True,
            up_axis=ovxr.StageUpAxis.Y,
            meters_per_unit=1.0,
            scene_info_policy=ovxr.SceneInfoPolicy.AUTO,
            ordinal_mode=(ovxr.StageOrdinalMode.APPLICATION if explicit_ordinals else ovxr.StageOrdinalMode.PRIVATE),
            foveation_unwarp=not args.no_foveation,
        )
        desc_options.update(context_options or {})
        current = [ovxr.Context(ovxr.ContextDesc(**desc_options))]
        def close_context():
            context = current[0]
            current[0] = None
            if context is None:
                return
            ordinal = 1
            try:
                if explicit_ordinals:
                    ordinal = next_ordinal(stage)
            finally:
                # A positive fallback passes Python validation and allows
                # native teardown to drain even if publication is rejected.
                # After failure this stage is disposed, never published/reused.
                context.destroy(ordinal if explicit_ordinals else None)

        cleanup.callback(close_context)
        # Report only once the context's teardown is registered, so a failed
        # report still destroys it before the renderer and stage.
        report_scene_info(current[0])
        def recreate_context():
            try:
                close_context()
            except ovxr.OvxrError as error:
                # Only creation failures are retryable; teardown must complete
                # before the scene can be reused by a replacement context.
                raise RuntimeError("Could not close the old XR context") from error
            current[0] = ovxr.Context(ovxr.ContextDesc(**desc_options))
            report_scene_info(current[0])
            return current[0]

        warm_up(current[0], next_ordinal(stage) if explicit_ordinals else None)
        print("Scene ready; waiting for XR frames...", flush=True)
        if reconnectable:
            yield stage, current[0], recreate_context
        elif with_renderer:
            yield stage, current[0], renderer
        else:
            yield stage, current[0]


class Progress:
    """Bound test runs; interactive sessions wait through idle/no-render periods."""

    def __init__(self, frame_limit):
        self.limit = frame_limit
        self.submitted = 0
        self.last_submission = time.monotonic()
        self.last_waiting_notice = self.last_submission

    def running(self):
        if self.limit and self.submitted >= self.limit:
            return False
        now = time.monotonic()
        if now - self.last_submission > 60:
            if self.limit:
                raise RuntimeError("No frame submitted for 60 seconds; check the OpenXR runtime and headset.")
            if now - self.last_waiting_notice > 60:
                print("No frame submitted for 60 seconds; waiting for the OpenXR session (Ctrl+C to quit).",
                      flush=True)
                self.last_waiting_notice = now
        return True

    def record(self, frame):
        if frame.submitted:
            self.submitted += 1
            self.last_submission = time.monotonic()
            self.last_waiting_notice = self.last_submission


@contextmanager
def keyboard():
    """Nonblocking single-key input; redirected stdin requires no terminal setup."""
    saved = None
    if os.name != "nt" and sys.stdin.isatty():
        import termios
        import tty

        saved = termios.tcgetattr(sys.stdin)
        tty.setcbreak(sys.stdin.fileno())
    try:
        yield read_key
    finally:
        if saved is not None:
            termios.tcsetattr(sys.stdin, termios.TCSADRAIN, saved)


def read_key():
    if not sys.stdin.isatty():
        return ""
    if os.name == "nt":
        import msvcrt

        return msvcrt.getwch().lower() if msvcrt.kbhit() else ""
    return sys.stdin.read(1).lower() if select.select([sys.stdin], [], [], 0)[0] else ""


def run(main):
    try:
        return main()
    except KeyboardInterrupt:
        print("Stopped by Ctrl+C.", flush=True)
        return 0
    except Exception as error:
        print(f"Sample failed: {error}", file=sys.stderr, flush=True)
        return 1
