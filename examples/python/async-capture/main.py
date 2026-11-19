# Copyright (c) 2026, NVIDIA CORPORATION. All rights reserved.
"""Feed owned XR captures to a slow worker using a one-frame pending queue."""
import json
from pathlib import Path
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "common"))
import xr_sample as sample
from frame_copy import FrameCopy
from consumer import Consumer, OwnedFrame

import ovxr


def main():
    parser = sample.parser(__doc__)
    parser.add_argument("--consumer-ms", type=sample.nonnegative, default=200, help="simulated worker delay (default 200 ms)")
    parser.add_argument("--report", type=Path, help="write offered/consumed/dropped counts as JSON")
    args = parser.parse_args()
    args.no_foveation = True
    with sample.application("async-capture", args, context_options=dict(enable_frame_readback=True)) as (_, context):
        copier, consumer = FrameCopy(), Consumer(args.consumer_ms)
        progress = sample.Progress(args.frames)
        sequence = 0
        try:
            while progress.running():
                result, frame, _ = context.render_frame(sample.DELTA_TIME)
                if result == ovxr.Result.NOT_READY:
                    time.sleep(0.01)
                    continue
                sample.check(result, context, "render_frame")
                progress.record(frame)
                if not frame.submitted:
                    continue
                # [snippet:owned-async-capture]
                view = context.acquire_frame()
                if view is None or view.sequence <= sequence:
                    continue
                # The synchronous source copy finishes on the render thread,
                # before the next submission. Only owned bytes cross threads.
                owned = OwnedFrame(view.sequence, view.width, view.height, view.pitch_bytes, copier.copy(view))
                consumer.offer(owned)
                sequence = view.sequence
                # [/snippet:owned-async-capture]
        finally:
            consumer.close()
        if not consumer.consumed:
            raise RuntimeError("Session ended without consuming a fresh capture")
    report = dict(submitted=progress.submitted, offered=consumer.offered, consumed=consumer.consumed,
                  dropped=consumer.dropped, last_sequence=consumer.last_sequence, checksum=consumer.checksum,
                  pending_capacity=1, headset_verified=False)
    print(json.dumps(report), flush=True)
    if args.report:
        args.report.write_text(json.dumps(report, indent=2), encoding="utf-8")
    return 0


if __name__ == "__main__":
    sys.exit(sample.run(main))
