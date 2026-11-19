# Copyright (c) 2026, NVIDIA CORPORATION. All rights reserved.
"""Select mono/stereo/AUTO and inspect the resolved views and first-view image size."""
import json
from pathlib import Path
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "common"))
import xr_sample as sample
import ovxr


def main():
    parser = sample.parser(__doc__)
    parser.add_argument("--display", choices=("auto", "mono", "stereo"), default="auto")
    parser.add_argument("--report", type=Path, help="write negotiated views and observed image dimensions as JSON")
    args = parser.parse_args()
    args.no_foveation = True
    # [snippet:view-configuration]
    modes = dict(auto=ovxr.DisplayMode.AUTO, mono=ovxr.DisplayMode.MONO, stereo=ovxr.DisplayMode.STEREO)
    options = dict(display_mode=modes[args.display], enable_frame_readback=True, readback_render_product_index=0)
    report = dict(requested_display=args.display, headset_verified=False)
    with sample.application("view-configuration", args, context_options=options) as (_, context):
        probe = context.probe_openxr()
        if probe.result == ovxr.Result.NOT_READY:
            raise RuntimeError(f"Display {args.display} is unavailable or the runtime is not ready: {context.last_error}")
        sample.check(probe.result, context, "probe_openxr")
        report.update(view_count=probe.view_count,
                      recommended_width=probe.recommended_swapchain_width,
                      recommended_height=probe.recommended_swapchain_height,
                      recommended_size_uniform=bool(probe.raw.recommended_swapchain_size_uniform))
        print(json.dumps(report), flush=True)
        progress = sample.Progress(args.frames)
        while progress.running():
            result, frame, _ = context.render_frame(sample.DELTA_TIME)
            if result == ovxr.Result.NOT_READY:
                time.sleep(0.01)
                continue
            sample.check(result, context, "render_frame")
            progress.record(frame)
            if frame.submitted and "rendered_width" not in report:
                view = context.acquire_frame()
                if view is not None:
                    # Only view zero is captured. Do not label its extent as the
                    # dimensions of every eye on a runtime with nonuniform views.
                    report.update(rendered_view=0, rendered_width=view.width, rendered_height=view.height)
                    print(json.dumps(report), flush=True)
        if "rendered_width" not in report:
            raise RuntimeError("Session ended without first-view readback")
    # [/snippet:view-configuration]
    report["submitted"] = progress.submitted
    if args.report:
        args.report.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(f"view-configuration: submitted {progress.submitted} frames; clean shutdown.", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(sample.run(main))
