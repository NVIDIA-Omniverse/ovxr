# Copyright (c) 2026, NVIDIA CORPORATION. All rights reserved.
"""Inspect AR blend negotiation and transparent-background RGBA readback."""
import json
from pathlib import Path
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "common"))
import xr_sample as sample
from frame_copy import FrameCopy
import ovxr


def alpha_counts(frame, rgba):
    # Ignore row padding; black RGB alone says nothing about transparency.
    transparent = opaque = 0
    for y in range(frame.height):
        alpha = rgba[y * frame.pitch_bytes + 3:y * frame.pitch_bytes + frame.width * 4:4]
        transparent += alpha.count(0)
        opaque += alpha.count(255)
    return dict(transparent_pixels=transparent, opaque_pixels=opaque,
                total_pixels=frame.width * frame.height)


def main():
    parser = sample.parser(__doc__)
    parser.add_argument("--blend", choices=("auto", "alpha", "opaque", "additive"), default="alpha")
    parser.add_argument("--report", type=Path, help="write observed blend mode and alpha counts as JSON")
    args = parser.parse_args()
    args.no_foveation = True
    modes = dict(auto=ovxr.EnvironmentBlendMode.AUTO, alpha=ovxr.EnvironmentBlendMode.ALPHA_BLEND,
                 opaque=ovxr.EnvironmentBlendMode.OPAQUE, additive=ovxr.EnvironmentBlendMode.ADDITIVE)
    report = dict(requested_blend=args.blend, session="ar", headset_verified=False)
    # [snippet:ar-blending]
    # AR lets OVXR author the renderer's compositing settings. The scene has no
    # floor/background geometry, and its dome illuminates without filling primary rays.
    options = dict(session_mode=ovxr.SessionMode.AR, environment_blend_mode=modes[args.blend],
                   enable_frame_readback=True)
    with sample.application("ar-blending", args, scene=sample.SCENE.with_name("ar-objects.usda"),
                            context_options=options) as (_, context):
        copier = FrameCopy()
        progress = sample.Progress(args.frames)
        captured = False
        while progress.running():
            result, frame, _ = context.render_frame(sample.DELTA_TIME)
            if result == ovxr.Result.NOT_READY:
                time.sleep(0.01)
                continue
            sample.check(result, context, "render_frame")
            progress.record(frame)
            if not frame.submitted:
                continue
            report["resolved_blend"] = ovxr.EnvironmentBlendMode(frame.environment_blend_mode).name
            if not captured and progress.submitted >= min(args.frames or 30, 30):
                view = context.acquire_frame()
                if view is not None:
                    report.update(alpha_counts(view, copier.copy(view)))
                    report.update(width=view.width, height=view.height)
                    captured = True
                    print(json.dumps(report), flush=True)
        # [/snippet:ar-blending]
        if not captured:
            raise RuntimeError("Session ended without RGBA readback")
    report["submitted"] = progress.submitted
    if args.report:
        args.report.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(f"ar-blending: submitted {progress.submitted} frames; clean shutdown.", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(sample.run(main))
