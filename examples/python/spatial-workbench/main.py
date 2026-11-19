# Copyright (c) 2026, NVIDIA CORPORATION. All rights reserved.
"""Controllers or hand joints drive the same scene tools as desktop keys."""

from contextlib import ExitStack
import json
import math
from pathlib import Path
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "common"))
from sample_support import Input, Mirror
from workbench_model import Model, add, scale, normalize, demo_pointer
from pointer_visuals import beam_transforms, demo_hand, hand_transforms
import xr_sample as sample
import ovxr


def anchor_ready(context):
    try:
        context.get_anchor_transform()
        return True
    except ovxr.OvxrError as error:
        if error.result != ovxr.Result.NOT_READY:
            raise
        return False


def main():
    parser = sample.parser(__doc__)
    parser.add_argument("--input", choices=("controllers", "hands", "demo"), default="controllers")
    parser.add_argument("--mirror", action="store_true", help="Windows desktop mirror and keyboard controls")
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()
    print("Panel / keys: 1 left, 2 right, 3 rotate, 4 undo; R reset, O occluder, Z/X turn, Q quit.", flush=True)
    print("Select / pinch cube to drag; select floor to teleport. Demo input is synthetic.", flush=True)
    options = dict(enable_frame_readback=args.mirror, readback_foveation_unwarp=True,
                   depth_submission_policy=ovxr.FeaturePolicy.AUTO)
    if args.input == "hands":
        options["extra_openxr_extensions"] = ["XR_EXT_hand_tracking"]
    scene = sample.SCENE.with_name("spatial-workbench.usda")
    with sample.application("spatial-workbench", args, scene=scene, context_options=options) as (_, context), \
            ExitStack() as cleanup, sample.keyboard() as read_key:
        progress = sample.Progress(args.frames)
        model = Model()
        mirror = Mirror("OVXR workbench") if args.mirror else None
        if mirror:
            cleanup.callback(mirror.close)
        native = None
        pinch = [False, False]
        selected = None
        turn_armed = False
        anchor = [0.0, 0.0, 3.0]
        yaw = 0.0
        controller_frames = hand_frames = navigation = 0
        revision = -1
        anchor_transition = False
        while progress.running():
            if mirror and not mirror.update(context, capture=False):
                break
            key = (mirror.key() if mirror else "") or read_key()
            if key == "q":
                break
            model.command(key)
            result, plan = context.begin_frame()
            if result == ovxr.Result.NOT_READY:
                model.pointer((0, 0, 0), (0, 0, -1), False, False)
                turn_armed = False
                time.sleep(0.01)
                continue
            sample.check(result, context, "begin_frame")
            try:
                updates = {}
                ready = anchor_ready(context)
                next_anchor = None
                origin, direction, active, down, stick = (0, 0, 0), (0, 0, -1), False, False, 0.0
                if args.input == "demo":
                    origin, direction, down = demo_pointer(progress.submitted)
                    active = True
                    for side, name in enumerate(("Left", "Right")):
                        tip = (-.35, 1.35, 2.1) if side == 0 else origin
                        ray = (0, .1, -1) if side == 0 else direction
                        points = dict(enumerate(demo_hand(tip, ray, down, left=side == 0)))
                        updates.update(hand_transforms(name, points))
                else:
                    if native is None:
                        handles = context.get_openxr_handles()
                        if handles is not None:
                            native = Input(handles, hands=args.input == "hands")
                            cleanup.callback(native.close)  # Children must die before the borrowed OVXR session.
                    # The first renderable bind resolves the anchor. A navigation
                    # update also latches at bind, so hide tracked input for that
                    # transition frame rather than draw it with the old mapping.
                    state = native.poll(plan.timing.predicted_display_time_ns) if native and ready and not anchor_transition else None
                    candidates = []
                    # With a stable anchor, map located input after begin_frame.
                    for side, name in enumerate(("Left", "Right")):
                        if args.input == "controllers":
                            controller = state.controllers[side] if state else None
                            matrix = sample.transform(y=-100)
                            if controller and controller.active and state.focused:
                                matrix = context.transform_openxr_pose(controller.aim.position, controller.aim.orientation)
                                candidates.append((side, tuple(matrix[12:15]), tuple(-v for v in matrix[8:11]),
                                                   bool(controller.select), controller.stick[0]))
                            updates[f"/World/Controller{name}"] = matrix
                        else:
                            hand = state.hands[side] if state else None
                            points = {}
                            for joint in range(26):
                                if hand and hand.active and state.focused and hand.valid[joint]:
                                    # Position validity does not imply a valid joint orientation.
                                    matrix = context.transform_openxr_pose(hand.joints[joint].position)
                                    points[joint] = tuple(matrix[12:15])
                            updates.update(hand_transforms(name, points))
                            if all(j in points for j in (5, 7, 10)):
                                ray = normalize(add(points[10], scale(points[7], -1)))
                                distance = math.dist(points[5], points[10])
                                pinch[side] = distance < (0.04 if pinch[side] else 0.025)
                                if ray:
                                    candidates.append((side, points[10], ray, pinch[side], 0.0))
                            else:
                                pinch[side] = False
                    identity = candidates[-1][0] if candidates else None  # Prefer right when both are located.
                    if identity != selected:
                        model.pointer(origin, direction, False, False)
                        turn_armed = False
                        selected = identity
                    if candidates:
                        _, origin, direction, down, stick = candidates[-1]
                        active = True
                        controller_frames += args.input == "controllers"
                        hand_frames += args.input == "hands"
                cursor, teleport = model.pointer(origin, direction, active, down)
                updates.update(beam_transforms(origin, cursor, active, down))
                turn = (-1 if key == "z" else 1 if key == "x" else 0)
                if abs(stick) < 0.3:
                    turn_armed = active
                elif active and turn_armed and abs(stick) > 0.7:
                    turn, turn_armed = (1 if stick > 0 else -1), False
                if ready and (teleport is not None or turn):
                    if teleport is not None:
                        anchor = [teleport[0], 0.0, teleport[2]]
                    yaw -= turn * math.pi / 6
                    next_anchor = sample.transform(*anchor, yaw)
                updates["/World/AnimatedCube"] = sample.transform(*model.position, model.yaw)
                updates["/World/Occluder"] = sample.transform(0.66, 1.1 if model.occluder else -100, 1.55)
                context.update_prim_transforms(updates)
                result, _ = context.bind_frame(plan, sample.DELTA_TIME)
                if result == ovxr.Result.NOT_READY:
                    continue
                sample.check(result, context, "bind_frame")
                result, frame = context.submit_frame(plan)
                sample.check(result, context, "submit_frame")
                progress.record(frame)
                anchor_transition = False
                if next_anchor is not None:
                    # Queue only after this frame; bind will latch it next frame.
                    context.set_managed_anchor_transform(next_anchor)
                    anchor_transition = True
                    navigation += 1
            finally:
                if plan.frame:
                    context.cancel_frame(plan)
            if model.revision != revision:
                revision = model.revision
                title = f"OVXR {args.input}: revision {revision}, cube x={model.position[0]:.2f} | 1/2 move 3 rotate 4 undo O occluder"
                print(title, flush=True)
                if mirror:
                    mirror.title(title)
            if mirror and not mirror.update(context, progress.submitted % 3 == 0):
                break
        if args.frames and progress.submitted < args.frames:
            raise RuntimeError("Session stopped before frame target")
        if mirror and not mirror.presented:
            raise RuntimeError("Mirror received no fresh frames")
        evidence = dict(input_mode=args.input, submitted_frames=progress.submitted,
                        controller_active_frames=controller_frames, hand_active_frames=hand_frames,
                        revision=model.revision, commands=model.commands, occlusion_blocks=model.blocked,
                        navigation_commands=navigation, mirror_frames=mirror.presented if mirror else 0,
                        depth_submission_enabled=bool(context.get_device_info().features.depth_submission_enabled)
                        if progress.submitted else False,
                        headset_verified=False)
    if args.report:
        report = args.report.resolve()
        report.parent.mkdir(parents=True, exist_ok=True)
        report.write_text(json.dumps(evidence, indent=2) + "\n", encoding="utf-8")
        print(f"Report: {report}", flush=True)
    print(f"spatial-workbench: submitted {progress.submitted} frames; clean shutdown.", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(sample.run(main))
