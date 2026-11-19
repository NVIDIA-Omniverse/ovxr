# Copyright (c) 2026, NVIDIA CORPORATION. All rights reserved.
"""Keep the application scene while recreating a stalled or explicitly reset XR context."""

from contextlib import ExitStack
import json
from pathlib import Path
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "common"))
from sample_support import Mirror
import xr_sample as sample

import ovxr


def main():
    parser = sample.parser(__doc__)
    parser.add_argument("--mirror", action="store_true", help="Windows desktop mirror; R reconnect, Q quit")
    parser.add_argument("--reconnect-at", type=sample.nonnegative, default=0, help="exercise one context restart")
    parser.add_argument("--retry-after", type=sample.nonnegative, default=10, help="seconds without submission")
    parser.add_argument("--max-restarts", type=sample.nonnegative, default=3,
                        help="consecutive recreation attempts without a submitted frame; 0 disables retries")
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()
    if (not args.retry_after or (args.reconnect_at and
            (not args.max_restarts or (args.frames and args.reconnect_at >= args.frames)))):
        parser.error("Use positive --retry-after and leave frames/restarts available after --reconnect-at")
    context_options = dict(enable_frame_readback=args.mirror, readback_foveation_unwarp=True)
    with sample.application("runtime-lifecycle", args, context_options=context_options, reconnectable=True) as session, \
            ExitStack() as cleanup, sample.keyboard() as read_key:
        _, context, recreate = session
        mirror = Mirror("OVXR runtime lifecycle - R reconnect, Q quit") if args.mirror else None
        if mirror:
            cleanup.callback(mirror.close)
        submitted = restarts = attempts = consecutive_failures = 0
        exercised = reported = False
        reason = ""
        last_submit = time.monotonic()
        while not args.frames or submitted < args.frames:
            if mirror and not mirror.update(context, capture=False):
                break
            key = (mirror.key() if mirror else "") or read_key()
            if key == "q":
                break
            if key == "r" or (not exercised and args.reconnect_at and submitted >= args.reconnect_at):
                exercised = True
                reason = "requested session recreation"
            if not reason:
                result, frame, _ = context.render_frame(sample.DELTA_TIME)
                error = context.last_error  # Copy before another OVXR call clears it.
                if result == ovxr.Result.ERROR_OPENXR:
                    reason = f"OpenXR failure: {error}"
                elif result not in (ovxr.Result.SUCCESS, ovxr.Result.NOT_READY, ovxr.Result.TIMEOUT):
                    sample.check(result, context, "render_frame")
                if frame.submitted:
                    submitted += 1
                    consecutive_failures = 0  # A submitted frame proves recovery.
                    last_submit = time.monotonic()
                    if not reported:
                        probe = context.probe_openxr()
                        sample.check(probe.result, context, "probe_openxr")
                        print(f"Connected: {probe.runtime_name} / {probe.system_name}", flush=True)
                        reported = True
                    if mirror and not mirror.update(context, submitted % 3 == 0):
                        break
                else:
                    if time.monotonic() - last_submit > args.retry_after:
                        reason = f"no submitted frame within retry window: {error}"
                    time.sleep(0.01)
            if reason:
                if consecutive_failures >= args.max_restarts:
                    raise RuntimeError(f"Reconnect budget exhausted: {reason}")
                print(f"Reconnecting: {reason}", flush=True)
                # Scene and renderer remain alive. No borrowed XR handle/view
                # survives; private ordinal allocation resumes above the floor.
                attempts += 1
                consecutive_failures += 1
                time.sleep(0.5)
                context = None  # The old context is destroyed even if creation fails.
                try:
                    context = recreate()
                except ovxr.OvxrError as error:
                    if error.result != ovxr.Result.ERROR_OPENXR:
                        raise
                    reason = f"OpenXR creation failed: {error}"
                    continue  # Pump window/quit input before the next attempt.
                if mirror:
                    mirror.reset()
                restarts += 1
                reason = ""
                reported = False
                last_submit = time.monotonic()
        if args.frames and submitted < args.frames:
            raise RuntimeError("Session stopped before frame target")
        if mirror and not mirror.presented:
            raise RuntimeError("Mirror received no fresh frames")
        evidence = dict(submitted_frames=submitted, context_restarts=restarts, reconnect_attempts=attempts,
                        mirror_frames=mirror.presented if mirror else 0, headset_verified=False)
    if args.report:
        report = args.report.resolve()
        report.parent.mkdir(parents=True, exist_ok=True)
        report.write_text(json.dumps(evidence, indent=2) + "\n", encoding="utf-8")
        print(f"Report: {report}", flush=True)
    print(f"runtime-lifecycle: submitted {submitted} frames; clean shutdown.", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(sample.run(main))
