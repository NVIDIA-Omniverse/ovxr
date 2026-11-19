# Copyright (c) 2026, NVIDIA CORPORATION. All rights reserved.
"""Move the tracking origin, teleport, and follow a live scene anchor."""

import math
from pathlib import Path
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "common"))
import xr_sample as sample

import ovxr


def main():
    parser = sample.parser(__doc__)
    parser.add_argument("--demo", action="store_true", help="cycle fixed, teleport, follow, reset automatically")
    parser.add_argument("--demo-interval", type=int, default=180, help="submitted frames per demo mode")
    args = parser.parse_args()
    if args.demo_interval < 1:
        parser.error("--demo-interval must be positive")
    print("Keys: 1 = start, 2 = teleport, 3 = follow moving rig, R = reset origin, Q = quit.", flush=True)
    with sample.application("anchors-and-teleport", args) as (_, context), sample.keyboard() as read_key:
        progress = sample.Progress(args.frames)
        mode = ""
        while progress.running():
            key = read_key()
            if key == "q":
                break
            # The first submitted frame initializes the managed tracking anchor.
            if progress.submitted:
                requested = "123r"[(progress.submitted // args.demo_interval) % 4] if args.demo else key
                if requested in ("1", "2", "3", "r") and requested != mode:
                    # [snippet:select-anchor]
                    if requested == "1":
                        context.set_managed_anchor_transform(sample.transform(z=3.0))
                    elif requested == "2":
                        context.set_managed_anchor_transform(sample.transform(x=3.0, z=1.0, yaw=math.pi / 2))
                    elif requested == "3":
                        context.set_anchor_prim("/World/MovingRig")
                    else:
                        context.clear_anchor()
                    mode = requested
                    print(f"Requested anchor mode {mode}", flush=True)
                    # [/snippet:select-anchor]
                if mode == "3":
                    # [snippet:live-anchor]
                    # OVXR owns all writes after population in this sample.
                    # Its helper publishes the rig BEFORE the frame reads it.
                    # No per-frame set_anchor_prim is needed: it is a live path.
                    t = progress.submitted * sample.DELTA_TIME
                    context.update_prim_transforms({
                        "/World/MovingRig": sample.transform(x=math.sin(t * 0.5), z=3.0),
                    })
                    # [/snippet:live-anchor]
            result, frame, _ = context.render_frame(sample.DELTA_TIME)
            if result == ovxr.Result.NOT_READY:
                time.sleep(0.01)
                continue
            sample.check(result, context, "render_frame")
            progress.record(frame)
    print(f"anchors-and-teleport: submitted {progress.submitted} frames; clean shutdown.", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(sample.run(main))
