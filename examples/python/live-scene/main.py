# Copyright (c) 2026, NVIDIA CORPORATION. All rights reserved.
"""Animate scene data and bind XR cameras at the same application-owned ordinal."""

import ctypes
import math
from pathlib import Path
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "common"))
import xr_sample as sample

import ovstage
import ovxr


def main():
    args = sample.parser(__doc__).parse_args()
    with sample.application("live-scene", args, explicit_ordinals=True) as (stage, context):
        progress = sample.Progress(args.frames)
        paths = ovstage.PathDictionary(stage)
        path_list = paths.create_path_list_from_strings(["/World/AnimatedCube"])
        query = stage.query_from_path_list(path_list)
        try:
            ordinal = sample.next_ordinal(stage)
            pending = False
            while progress.running():
                result, plan = context.begin_frame()
                if result == ovxr.Result.NOT_READY:
                    time.sleep(0.01)
                    continue
                sample.check(result, context, "begin_frame")
                try:
                    # [snippet:publish-live-scene]
                    # This single thread owns the publication rendezvous. A
                    # co-writer must use this same ledger and synchronization.
                    if not pending:
                        t = progress.submitted * sample.DELTA_TIME
                        matrix = (ctypes.c_double * 16)(*sample.transform(math.sin(t), 1.3, 0.0, t))
                        shape = (ctypes.c_int64 * 1)(1)
                        # One CPU element with 16 double lanes, matching the C++ sample.
                        tensor = ovstage.DLTensor(
                            data=ctypes.cast(matrix, ctypes.c_void_p),
                            device=(ovstage.DLDeviceType.kDLCPU, 0), ndim=1,
                            dtype=(ovstage.DLDataTypeCode.kDLFloat, 64, 16), shape=shape,
                        )
                        stage.write_attribute(query, "omni:xform", ordinal=ordinal,
                                              tensors=tensor, is_array=False,
                                              semantic=ovstage.AttributeSemantic.MATRIX).wait()
                        pending = True
                    result, sealed = context.bind_frame(plan, sample.DELTA_TIME, stage_ordinal=ordinal)
                    # Sealing can precede a render failure. Never reuse a sealed
                    # ordinal. NOT_READY retains the pending write for a NEW plan.
                    if sealed:
                        ordinal = sample.next_ordinal(stage)
                        pending = False
                    if result == ovxr.Result.NOT_READY:
                        continue
                    sample.check(result, context, "bind_frame")
                    result, frame = context.submit_frame(plan)
                    sample.check(result, context, "submit_frame")
                    progress.record(frame)
                    # [/snippet:publish-live-scene]
                finally:
                    if plan.frame:
                        context.cancel_frame(plan)
        finally:
            query.release().wait()
            paths.destroy_path_list(path_list)
            paths.destroy()
    print(f"live-scene: submitted {progress.submitted} frames; clean shutdown.", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(sample.run(main))
