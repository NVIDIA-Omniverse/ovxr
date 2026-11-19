# Copyright (c) 2026, NVIDIA CORPORATION. All rights reserved.
"""A Python OVUI scene inspector: XR selection, transform tools and shared undo history."""
from contextlib import ExitStack
import json
from pathlib import Path
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "common"))
from inspector_model import PANEL_CENTER, PANEL_SIZE, RESOLUTION, PointerRouter, SceneModel, selection_transforms
from ovui_panel import PanelSurface
from panel_compositor import PanelCompositor
from projection_layer import ProjectionLayer, NEAR_Z, FAR_Z
from panel_model import panel_hit
from inspector_model import object_hit
from pointer_visuals import beam_transforms, ray_endpoint
from sample_support import Input, Mirror
from xr_pointer import tracked_pointer
import xr_sample as sample
from controls import Controls
import ovstage
import ovxr


SCENE = Path(__file__).resolve().parents[2] / "assets/scene-inspector.usda"
# Each gesture has a preceding and a following canvas capture.
CHECKPOINTS = (7, 12, 19, 30, 31, 41, 44, 49, 53, 57, 59, 64, 69, 75, 79, 85, 89, 95)


def scene_transforms(model):
    transforms = {path: value.matrix() for path, value in model.objects.items()}
    transforms.update(selection_transforms(model))
    return transforms


def verify_scene(stage, model):
    """Read rendered world transforms at the sealed floor, outside the view callback."""
    import numpy as np

    matrices = {}
    ordinal = sample.next_ordinal(stage) - 1
    with ovstage.PathDictionary(stage) as paths:
        expected = {path: value.matrix() for path, value in model.objects.items()}
        expected.update(selection_transforms(model))
        for path, matrix in expected.items():
            path_list = paths.create_path_list_from_strings([path])
            try:
                with (stage.query_from_path_list(path_list) as query,
                      stage.read_attributes(query, [paths.intern_token("omni:fabric:worldMatrix")],
                                            ovstage.OrdinalRange.latest(ordinal)) as read):
                    read.wait()
                    group = read.fetch_next()
                    if group is None:
                        raise RuntimeError(f"No world transform read back for {path}")
                    try:
                        if group.column_count != 1:
                            raise RuntimeError(f"Unexpected transform readback for {path}")
                        column = group.columns[0]
                        if column.is_delete or column.tensor_count != 1:
                            raise RuntimeError(f"Unexpected transform readback for {path}")
                        row = column.data_row_index(0) if column.has_data_index_map else 0
                        actual = np.asarray(column.array(0)).reshape(-1, 16)[row].copy()
                        if not np.allclose(actual, matrix, atol=1e-5, rtol=0):
                            raise RuntimeError(f"Published scene transform disagrees with the model: {path}: {actual}")
                        matrices[path] = actual.tolist()
                    finally:
                        stage.release_group(group)
            finally:
                paths.destroy_path_list(path_list)
    return matrices


def main():
    parser = sample.parser(__doc__)
    parser.set_defaults(no_foveation=True)
    parser.add_argument("--input", choices=("controllers", "hands", "demo"), default="controllers")
    parser.add_argument("--mirror", action="store_true", help="Windows desktop preview with shared editing shortcuts")
    parser.add_argument("--report", type=Path, help="JSON evidence, including a readback of rendered world transforms")
    parser.add_argument("--evidence", type=Path, help="OVUI canvas captures before/after each demo gesture")
    args = parser.parse_args()
    options = dict(depth_submission_policy=ovxr.FeaturePolicy.REQUIRED, depth_near_z=NEAR_Z, depth_far_z=FAR_Z)
    if args.input == "hands":
        options["extra_openxr_extensions"] = ["XR_EXT_hand_tracking"]
    if args.evidence:
        args.evidence.mkdir(parents=True, exist_ok=True)
    model, router = SceneModel(), PointerRouter()
    controls = Controls(model)
    checkpoints = {}
    with ExitStack() as cleanup:
        surface = PanelSurface(controls.build, width=RESOLUTION[0], height=RESOLUTION[1])
        cleanup.callback(surface.close)
        surface.tick()
        compositor = PanelCompositor(surface, center=PANEL_CENTER, size=PANEL_SIZE)
        stage, context, renderer = cleanup.enter_context(sample.application("scene-inspector", args, scene=SCENE,
                                                                  context_options=options, with_renderer=True))
        projection = ProjectionLayer(renderer, context, compositor, surface.device)
        cleanup.callback(projection.close)
        read_key = cleanup.enter_context(sample.keyboard())
        title = "XR inspector | 1/2 select | A/D move | U undo | Y redo | R reset | Q quit"
        mirror = Mirror(title) if args.mirror else None
        if mirror:
            cleanup.callback(mirror.close)
        native = None
        pinch = [False, False]
        anchor_ready = False
        tracked_frames = 0
        progress = sample.Progress(args.frames)

        def cancel_pointer():
            controls.accept_events = False
            router.route(model, (0, 0, 0), (0, 0, -1), False, False, None)
            model.finish(cancel=True)
            surface.pointer(None, active=False)
            controls.refresh()
            surface.tick()

        print("Point/select a part, then drag the OVUI sliders. "
              "1/2 select; A/D move; U undo; Y redo; R reset; Q quit.",
              flush=True)
        if args.input == "demo":
            print("Demo uses synthetic XR rays and pointer events; physical headset validation is separate.", flush=True)
        # [snippet:inspector-frame-loop]
        while progress.running():
            if mirror and not mirror.update(context, capture=False):
                break
            key = (mirror.key() if mirror else "") or read_key()
            if key == "q":
                break
            if key:
                cancel_pointer()
                model.key(key)
            result, plan = context.begin_frame()
            if result == ovxr.Result.NOT_READY:
                cancel_pointer()
                time.sleep(0.01)
                continue
            sample.check(result, context, "begin_frame")
            try:
                index = progress.submitted
                if args.input == "demo":
                    pointer = controls.demo_pointer(index)
                else:
                    if native is None:
                        handles = context.get_openxr_handles()
                        if handles is not None:
                            native = Input(handles, hands=args.input == "hands")
                            cleanup.callback(native.close)
                    state = native.poll(plan.timing.predicted_display_time_ns) if native and anchor_ready else None
                    pointer = tracked_pointer(context, state, args.input == "hands", pinch)
                    tracked_frames += int(pointer[3])
                route = router.route(model, *pointer)
                controls.accept_events = route.pixels is not None and not route.cancel
                if route.cancel:
                    model.finish(cancel=True)
                controls.refresh()
                surface.pointer(route.pixels, route.down, active=controls.accept_events, identity=pointer[4])
                surface.tick()
                if not pointer[2]:
                    model.finish()
                controls.refresh()
                controls.check_sync()
                origin, direction, down, active, _ = pointer
                panel = panel_hit(origin, direction, center=PANEL_CENTER, size=PANEL_SIZE)
                distances = [object_hit(origin, direction, value) for value in model.objects.values()]
                endpoint = ray_endpoint(origin, direction, distances + [panel[0] if panel else None])
                updates = scene_transforms(model)
                updates.update(beam_transforms(origin, endpoint, active, down))
                context.update_prim_transforms(updates)
                result, _ = context.bind_frame(plan, sample.DELTA_TIME)
                if result == ovxr.Result.NOT_READY:
                    cancel_pointer()
                    continue
                sample.check(result, context, "bind_frame")
                try:
                    compositor.set_tracking_transform(context.transform_openxr_pose((0, 0, 0)))
                    anchor_ready = True
                except ovxr.OvxrError as error:
                    if error.result != ovxr.Result.NOT_READY:
                        raise
                    anchor_ready = False  # Non-rendering startup frames still need submit/end.
                layers = projection.layers(plan)
                result, frame = context.submit_frame(plan, additional_layers=layers, replace_scene_layer=bool(layers))
                sample.check(result, context, "submit_frame")
                progress.record(frame)
                if frame.submitted and index in CHECKPOINTS and args.input == "demo":
                    checkpoints[str(index)] = model.snapshot()
                    if args.report:
                        checkpoints[str(index)]["scene_readback"] = verify_scene(stage, model)
                    if args.evidence:
                        surface.screenshot(args.evidence / f"ui-{index:03d}.png")
            finally:
                if plan.frame:
                    context.cancel_frame(plan)
            if mirror and not projection.update_mirror(mirror, progress.submitted % 3 == 0):
                break
        # [/snippet:inspector-frame-loop]
        if args.frames and progress.submitted < args.frames:
            raise RuntimeError("Session stopped before the requested number of frames")
        # Read the last published state before disposing the session. An unfinished
        # gesture is not saved or exported as an edit command on shutdown.
        evidence = dict(submitted_frames=progress.submitted, input_mode=args.input, headset_verified=False,
                        tracked_input_frames=tracked_frames, mirror_frames=mirror.presented if mirror else 0,
                        events=model.events.copy(), final=model.snapshot(), checkpoints=checkpoints,
                        ui_sync_checks=controls.sync_checks,
                        scene_readback=verify_scene(stage, model) if args.report and progress.submitted else {},
                        **compositor.report())
        cancel_pointer()
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(evidence, indent=2) + "\n", encoding="utf-8")
        print(f"Report: {args.report}", flush=True)
    print(f"scene-inspector: submitted {progress.submitted} frames; clean shutdown.", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(sample.run(main))
