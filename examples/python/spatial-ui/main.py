# Copyright (c) 2026, NVIDIA CORPORATION. All rights reserved.
"""Two small OVUI panels: drag their headers and click only the visible button."""
from contextlib import ExitStack
import json
from pathlib import Path
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "common"))
from ovui_panel import PanelSurface
from panel_compositor import PanelCompositor
from projection_layer import ProjectionLayer, NEAR_Z, FAR_Z
from panel_layout import INITIAL, RESOLUTION, SIZE, Panels, ray_to_pixel
from panel_layout import CUBE
from panel_model import box_hit
from pointer_visuals import beam_transforms, ray_endpoint
from sample_support import Input, Mirror
from xr_pointer import tracked_pointer
import xr_sample as sample
import ovxr


class Buttons:
    def __init__(self):
        self.clicks, self.widgets, self.labels = [0, 0], [], []
        self.focus = None

    def build(self, ui):
        style = {"Label": {"font_size": 24, "color": 0xffeee6dc},
                 "Button.Label": {"font_size": 28, "color": 0xffeee6dc},
                 "Button": {"border_radius": 7, "background_color": 0xff40372b},
                 "Button:hovered": {"background_color": 0xff625039},
                 "Button:pressed": {"background_color": 0xff87602c}}
        # One OVUI canvas, two equal regions. Each region becomes one XR panel.
        with ui.HStack(spacing=0, style=style):
            for index, name in enumerate(("A", "B")):
                with ui.ZStack():
                    ui.Rectangle(style={"background_color": (0xff302219, 0xff252b20)[index]})
                    with ui.VStack(spacing=0):
                        with ui.ZStack(height=56):
                            ui.Rectangle(style={"background_color": (0xff755134, 0xff466d43)[index]})
                            ui.Label(f"    Panel {name}  /  drag here")
                        with ui.HStack():
                            ui.Spacer(width=24)
                            with ui.VStack(spacing=12):
                                ui.Spacer(height=4)
                                ui.Label("Near panel" if index == 0 else "Far panel", height=28)
                                self.widgets.append(ui.Button(f"Click {name}", height=72,
                                                              clicked_fn=lambda i=index: self.click(i)))
                                self.labels.append(ui.Label("Clicks: 0", height=36))
                                ui.Label("Point and select / pinch", height=28, style={"font_size": 20})
                                ui.Spacer()
                            ui.Spacer(width=24)

    def click(self, index):
        if self.focus == index:
            self.clicks[index] += 1
            self.labels[index].text = f"Clicks: {self.clicks[index]}"

    def demo_pointer(self, frame):
        index = 0 if frame < 14 or 26 <= frame < 44 else 1
        widget = self.widgets[index]
        pixels = (widget.screen_position_x + widget.computed_width * (0.9 if frame >= 60 else 0.5)
                  - index * RESOLUTION[0], widget.screen_position_y + widget.computed_height * 0.5)
        centers = list(INITIAL)
        if 26 <= frame < 44:
            pixels = (256, 28)
            fraction = max(0, min(1, (frame - 30) / 10))
            centers[0] = (INITIAL[0][0] - 0.75 * fraction, INITIAL[0][1] + 0.15 * fraction, INITIAL[0][2])
        down = frame in (8, 9, 18, 19, 50, 51, 66, 67) or 30 <= frame <= 40
        return (*ray_to_pixel(index, pixels, centers), down, True, "demo")


def main():
    parser = sample.parser(__doc__)
    parser.set_defaults(no_foveation=True)
    parser.add_argument("--input", choices=("controllers", "hands", "demo"), default="controllers")
    parser.add_argument("--mirror", action="store_true")
    parser.add_argument("--report", type=Path)
    parser.add_argument("--evidence", type=Path)
    args = parser.parse_args()
    if args.evidence:
        args.evidence.mkdir(parents=True, exist_ok=True)
    options = dict(depth_submission_policy=ovxr.FeaturePolicy.REQUIRED, depth_near_z=NEAR_Z, depth_far_z=FAR_Z)
    if args.input == "hands":
        options["extra_openxr_extensions"] = ["XR_EXT_hand_tracking"]
    panels, buttons = Panels(), Buttons()
    checkpoints = {}
    with ExitStack() as cleanup:
        surface = PanelSurface(buttons.build, width=2 * RESOLUTION[0], height=RESOLUTION[1])
        cleanup.callback(surface.close)
        surface.tick()
        compositors = [PanelCompositor(surface.horizontal_region(i * RESOLUTION[0], RESOLUTION[0]),
                                       center=panels.centers[i], size=SIZE) for i in range(2)]

        # [snippet:two-panel-composition]
        def compose(view):
            for index, compositor in enumerate(compositors):
                compositor.center = panels.centers[index]
                compositor(view)  # Both read/write the same eye depth: nearest pixels win.
        # [/snippet:two-panel-composition]

        scene = Path(__file__).resolve().parents[2] / "assets/spatial-ui.usda"
        _, context, renderer = cleanup.enter_context(sample.application("spatial-ui", args, scene=scene,
                                                                        context_options=options, with_renderer=True))
        projection = ProjectionLayer(renderer, context, compose, surface.device)
        cleanup.callback(projection.close)
        read_key = cleanup.enter_context(sample.keyboard())
        mirror = Mirror("Spatial UI | drag headers | R reset positions | Q quit") if args.mirror else None
        if mirror:
            cleanup.callback(mirror.close)
        native, anchor_ready = None, False
        pinch, tracked_frames = [False, False], 0
        progress = sample.Progress(args.frames)
        print("Drag a header to move its panel. Click either button. R resets positions; Q quits.", flush=True)
        if args.input == "demo":
            print("Synthetic rays exercise overlap, dragging and blocked clicks; no headset proof.", flush=True)
        while progress.running():
            if mirror and not mirror.update(context, capture=False):
                break
            key = (mirror.key() if mirror else "") or read_key()
            if key == "q":
                break
            if key == "r":
                panels.reset()
            result, plan = context.begin_frame()
            if result == ovxr.Result.NOT_READY:
                panels.cancel()
                buttons.focus = None
                surface.pointer(None, active=False)
                surface.tick()
                time.sleep(0.01)
                continue
            sample.check(result, context, "begin_frame")
            try:
                index = progress.submitted
                if args.input == "demo":
                    pointer = buttons.demo_pointer(index)
                else:
                    if native is None and (handles := context.get_openxr_handles()) is not None:
                        native = Input(handles, hands=args.input == "hands")
                        cleanup.callback(native.close)
                    state = native.poll(plan.timing.predicted_display_time_ns) if native and anchor_ready else None
                    pointer = tracked_pointer(context, state, args.input == "hands", pinch)
                    tracked_frames += int(pointer[3])
                pixels, down, buttons.focus = panels.pointer(*pointer)
                surface.pointer(pixels, down, active=buttons.focus is not None, identity=(pointer[4], buttons.focus))
                surface.tick()
                origin, direction, pressed, active, _ = pointer
                distances = [hit[0] for i in range(2) if (hit := panels.hit(origin, direction, i))]
                endpoint = ray_endpoint(origin, direction, distances + [box_hit(origin, direction, CUBE)])
                context.update_prim_transforms(beam_transforms(origin, endpoint, active, pressed))
                result, _ = context.bind_frame(plan, sample.DELTA_TIME)
                if result == ovxr.Result.NOT_READY:
                    panels.cancel()
                    continue
                sample.check(result, context, "bind_frame")
                try:
                    transform = context.transform_openxr_pose((0, 0, 0))
                    for compositor in compositors:
                        compositor.set_tracking_transform(transform)
                    anchor_ready = True
                except ovxr.OvxrError as error:
                    if error.result != ovxr.Result.NOT_READY:
                        raise
                    anchor_ready = False
                layers = projection.layers(plan)
                result, frame = context.submit_frame(plan, additional_layers=layers, replace_scene_layer=bool(layers))
                sample.check(result, context, "submit_frame")
                progress.record(frame)
                if frame.submitted and index in (7, 12, 17, 24, 29, 44, 49, 56, 65, 72):
                    checkpoints[str(index)] = dict(clicks=list(buttons.clicks), centers=list(panels.centers))
                    if args.evidence:
                        surface.screenshot(args.evidence / f"ui-{index:03d}.png")
            finally:
                if plan.frame:
                    context.cancel_frame(plan)
            if mirror and not projection.update_mirror(mirror, progress.submitted % 3 == 0):
                break
        if args.frames and progress.submitted < args.frames:
            raise RuntimeError("Session stopped before the requested number of frames")
        evidence = dict(submitted_frames=progress.submitted, input_mode=args.input, headset_verified=False,
                        clicks=buttons.clicks, panel_moves=panels.moves, cancelled_moves=panels.cancels,
                        centers=panels.centers, checkpoints=checkpoints, tracked_input_frames=tracked_frames,
                        mirror_frames=mirror.presented if mirror else 0,
                        panels=[compositor.report() for compositor in compositors])
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(evidence, indent=2) + "\n", encoding="utf-8")
    print(f"spatial-ui: submitted {progress.submitted} frames; clean shutdown.", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(sample.run(main))
