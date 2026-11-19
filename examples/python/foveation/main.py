# Copyright (c) 2026, NVIDIA CORPORATION. All rights reserved.
"""Compare disabled, locally configured, and runtime-provided foveation."""

import argparse
import math
from pathlib import Path
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "common"))
import xr_sample as sample

import ovxr


def fraction(value):
    number = float(value)
    if not math.isfinite(number) or not 0 < number <= 1:
        raise argparse.ArgumentTypeError("must be in (0, 1]")
    return number


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--frames", type=sample.nonnegative, default=0, help="submitted frames; 0 until Ctrl+C")
    parser.add_argument("--mode", choices=("off", "local", "runtime"), default="local")
    parser.add_argument("--multiplier", type=fraction, help="local warped resolution multiplier (default 0.2)")
    parser.add_argument("--inset", type=fraction, help="local full-resolution inset size (default 0.1)")
    args = parser.parse_args()
    if args.mode != "local" and (args.multiplier is not None or args.inset is not None):
        parser.error("--multiplier and --inset apply only to --mode local")
    args.no_foveation = False
    # [snippet:foveation-policy]
    modes = {"off": ovxr.LocalFoveationMode.DISABLED, "local": ovxr.LocalFoveationMode.WARPED,
             "runtime": ovxr.LocalFoveationMode.AUTO}
    context_options = dict(
        local_foveation_mode=modes[args.mode],
        runtime_foveated_submission_policy=(ovxr.FeaturePolicy.AUTO if args.mode == "runtime"
                                           else ovxr.FeaturePolicy.DISABLED),
        # Deliberately strong demo settings make peripheral detail loss visible.
        foveation_warped_resolution_multiplier=args.multiplier or 0.2,
        foveation_warped_inset_size=args.inset or 0.1,
        enable_frame_readback=True,
        readback_foveation_unwarp=False,
    )
    # Runtime PQW takes precedence over local tuning. AUTO allows an unfoveated fallback.
    # [/snippet:foveation-policy]
    print(f"Requested foveation mode: {args.mode}", flush=True)
    with sample.application("foveation", args, scene=sample.SCENE.with_name("foveation-grid.usda"),
                            context_options=context_options) as (_, context):
        progress = sample.Progress(args.frames)
        previous = None
        while progress.running():
            result, frame, _ = context.render_frame(sample.DELTA_TIME)
            if result == ovxr.Result.NOT_READY:
                time.sleep(0.01)
                continue
            sample.check(result, context, "render_frame")
            progress.record(frame)
            if not frame.submitted:
                continue
            # [snippet:resolved-foveation]
            # Query AFTER locating/submitting: AUTO may resolve during that frame.
            features = context.get_device_info().features
            local = "warped" if features.local_foveated_rendering_enabled else "disabled"
            runtime = "yes" if features.foveated_submission_enabled else "no"
            report = f"Effective local rendering: {local}; runtime foveated submission enabled: {runtime}"
            view = context.acquire_frame()
            if view is not None:
                report += f"; rendered readback: {view.width}x{view.height}"
            if report != previous:
                print(report, flush=True)
                previous = report
            # [/snippet:resolved-foveation]
    print(f"foveation: submitted {progress.submitted} frames; clean shutdown.", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(sample.run(main))
