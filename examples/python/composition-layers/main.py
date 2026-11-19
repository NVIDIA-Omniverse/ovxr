# Copyright (c) 2026, NVIDIA CORPORATION. All rights reserved.
"""Append a head-fixed or world-fixed OpenXR quad after the rendered scene."""
from contextlib import ExitStack
import json
from pathlib import Path
import sys
import time

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "common"))
import xr_sample as sample
from quad_layer import QuadLayer
import ovxr


def main():
    parser = sample.parser(__doc__)
    parser.set_defaults(no_foveation=True)
    parser.add_argument("--placement", choices=("head", "world"), default="head")
    parser.add_argument("--verify-upload", action="store_true", help="read back actual Vulkan swapchain pixels")
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()
    progress = sample.Progress(args.frames)
    layer_submissions = 0
    with ExitStack() as cleanup:
        _, context, renderer = cleanup.enter_context(sample.application("composition-layers", args, with_renderer=True))
        quad = QuadLayer(renderer, context, args.placement, args.verify_upload)
        cleanup.callback(quad.close)
        read_key = cleanup.enter_context(sample.keyboard())
        # [snippet:append-quad]
        while progress.running():
            if read_key() == "q":
                break
            result, plan = context.begin_frame()
            if result == ovxr.Result.NOT_READY:
                time.sleep(0.01)
                continue
            sample.check(result, context, "begin_frame")
            try:
                result, _ = context.bind_frame(plan, sample.DELTA_TIME)
                if result == ovxr.Result.NOT_READY:
                    continue
                sample.check(result, context, "bind_frame")
                # Upload, synchronize and release app images before submitting.
                # Keep the original scene projection. This quad does NOT use its depth.
                layers = quad.layers(plan)
                result, frame = context.submit_frame(plan, additional_layers=layers)
                sample.check(result, context, "submit_frame")
                if frame.submitted and layers:
                    layer_submissions += 1
                progress.record(frame)
            finally:
                if plan.frame:
                    context.cancel_frame(plan)
        # [/snippet:append-quad]
        if args.frames and progress.submitted != args.frames:
            raise RuntimeError("Session ended before the requested submitted-frame count")
        if args.report:
            args.report.parent.mkdir(parents=True, exist_ok=True)
            if quad.readback is not None and quad.verified_uploads:
                np.save(args.report.with_name("overlay-readback.npy"), quad.readback)
            args.report.write_text(json.dumps({"submitted": progress.submitted,
                "layer_submissions": layer_submissions, "verified_uploads": quad.verified_uploads,
                "placement": args.placement, "scene_depth_tested": False,
                "headset_verified": False}, indent=2), encoding="utf-8")
    print(f"Submitted {progress.submitted} frames, {layer_submissions} with an appended quad.", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(sample.run(main))
