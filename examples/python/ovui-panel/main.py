# Copyright (c) 2026, NVIDIA CORPORATION. All rights reserved.
"""An OVUI canvas composited into stereo color and depth, with XR pointer input."""
from contextlib import ExitStack
import json
import math
from pathlib import Path
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "common"))
from ovui_panel import PanelSurface
from panel_compositor import PanelCompositor
from projection_layer import ProjectionLayer, NEAR_Z, FAR_Z
from panel_model import panel_pointer, panel_hit, box_hit
from pointer_visuals import beam_transforms, ray_endpoint
from sample_support import Input, Mirror
from xr_pointer import tracked_pointer
import xr_sample as sample
import ovxr


class Controls:
    def __init__(self):
        self.paused = False
        self.speed = 1.0
        self.clicks = self.slider_changes = 0
        self.widgets = {}

    # [snippet:ovui-widgets]
    def build(self, ui):
        style = {"Label": {"color": 0xffeee6dc, "font_size": 24},
                 "Button": {"background_color": 0xff40372b, "border_radius": 8},
                 "Button:hovered": {"background_color": 0xff625039},
                 "Button:pressed": {"background_color": 0xff87602c},
                 "Button.Label": {"font_size": 26},
                 "Slider": {"background_color": 0xff40372b, "font_size": 24, "secondary_color": 0xffad7738}}
        with ui.ZStack(style=style):
            ui.Rectangle(style={"background_color": 0xff241d16})
            with ui.HStack():
                ui.Spacer(width=28)
                with ui.VStack(spacing=12):
                    ui.Spacer(height=12)
                    ui.Label("OVUI  /  SPATIAL CONTROLS", height=54, style={"font_size": 32})
                    ui.Label("Point and select. The cube blocks the panel.", height=38)
                    self.widgets["button"] = ui.Button("Pause motion", height=64, clicked_fn=self.toggle)
                    ui.Label("Motion speed", height=32)
                    slider = ui.FloatSlider(min=0.1, max=2.0, height=54)
                    slider.model.set_value(self.speed)
                    slider.model.add_value_changed_fn(self.change_speed)
                    self.widgets["slider"] = slider
                    self.status = ui.Label("Running  |  1.00x", height=38)
                    ui.Label("Stereo color + depth  /  unfoveated", height=32,
                             style={"font_size": 20, "color": 0xffaca296})
                    ui.Spacer()
                ui.Spacer(width=28)

    def toggle(self):
        self.paused = not self.paused
        self.clicks += 1
        self.widgets["button"].text = "Resume motion" if self.paused else "Pause motion"
        self.update_status()

    def change_speed(self, model):
        self.speed = model.as_float
        self.slider_changes += 1
        self.update_status()

    def update_status(self):
        self.status.text = f"{'Paused' if self.paused else 'Running'}  |  {self.speed:.2f}x"
    # [/snippet:ovui-widgets]

    def demo_pointer(self, frame):
        """User-like presses/drags through widget coordinates and the XR ray path."""
        slider = 30 <= frame <= 40
        widget = self.widgets["slider" if slider else "button"]
        fraction = 0.25 + 0.05 * min(10, frame - 30) if slider else 0.5
        x = widget.screen_position_x + widget.computed_width * fraction
        y = widget.screen_position_y + widget.computed_height * 0.5
        if 50 <= frame < 60:
            x, y = 384, 440  # Point at the lower panel region hidden by the cube.
        target = ((x / 768 - 0.5) * 1.2, 1.4 + (0.5 - y / 480) * 0.75, 0)
        origin = (.3, 1.25, 2.5)
        direction = tuple(target[i] - origin[i] for i in range(3))
        down = frame in (8, 9, 18, 19, 52, 53) or 31 <= frame <= 39
        return origin, direction, down, True, "demo"


def main():
    parser = sample.parser(__doc__)
    parser.set_defaults(no_foveation=True)
    parser.add_argument("--input", choices=("controllers", "hands", "demo"), default="controllers")
    parser.add_argument("--mirror", action="store_true", help="Windows desktop preview of the composed XR eye")
    parser.add_argument("--report", type=Path)
    parser.add_argument("--evidence", type=Path, help="save OVUI canvas images during the demo")
    args = parser.parse_args()
    options = dict(depth_submission_policy=ovxr.FeaturePolicy.REQUIRED, depth_near_z=NEAR_Z, depth_far_z=FAR_Z)
    if args.input == "hands":
        options["extra_openxr_extensions"] = ["XR_EXT_hand_tracking"]
    if args.evidence:
        args.evidence.mkdir(parents=True, exist_ok=True)
    controls = Controls()
    # The UI and context outlive application-owned projection resources.
    with ExitStack() as cleanup:
        surface = PanelSurface(controls.build)
        cleanup.callback(surface.close)
        surface.tick()
        compositor = PanelCompositor(surface)
        _, context, renderer = cleanup.enter_context(sample.application("ovui-panel", args, context_options=options,
                                                                         with_renderer=True))
        projection = ProjectionLayer(renderer, context, compositor, surface.device)
        cleanup.callback(projection.close)
        read_key = cleanup.enter_context(sample.keyboard())
        mirror = Mirror("OVUI in XR | Q quit") if args.mirror else None
        if mirror:
            cleanup.callback(mirror.close)
        progress = sample.Progress(args.frames)
        native = None
        pinch = [False, False]
        phase = math.pi / 2
        anchor_ready = False
        blocked = tracked_frames = 0
        print("Select / pinch: pause or resume; drag the speed slider. Q quits. Foveation is disabled.", flush=True)
        if args.input == "demo":
            print("Demo uses synthetic rays, presses and drags; it does not verify headset input.", flush=True)
        # [snippet:panel-frame-loop]
        while progress.running():
            if mirror and not mirror.update(context, capture=False):
                break
            if ((mirror.key() if mirror else "") or read_key()) == "q":
                break
            result, plan = context.begin_frame()
            if result == ovxr.Result.NOT_READY:
                surface.pointer(None, active=False)
                surface.tick()
                time.sleep(0.01)
                continue
            sample.check(result, context, "begin_frame")
            try:
                index = progress.submitted
                cube = (0.95 * math.sin(phase), 1.0, 0.5)
                if args.input == "demo" and 50 <= index < 60:
                    cube = (0, 1.0, 0.5)  # Cover only the panel's bottom for the blocked-click check.
                if args.input == "demo":
                    origin, direction, down, active, identity = controls.demo_pointer(index)
                else:
                    if native is None:
                        handles = context.get_openxr_handles()
                        if handles is not None:
                            native = Input(handles, hands=args.input == "hands")
                            cleanup.callback(native.close)
                    state = native.poll(plan.timing.predicted_display_time_ns) if native and anchor_ready else None
                    origin, direction, down, active, identity = tracked_pointer(
                        context, state, args.input == "hands", pinch)
                    tracked_frames += int(active)
                pixels = panel_pointer(origin, direction, cube, captured=surface.captured) if active else None
                blocked += int(active and down and pixels is None)
                surface.pointer(pixels, down, active=active, identity=identity)
                surface.tick()
                hit = panel_hit(origin, direction)
                endpoint = ray_endpoint(origin, direction, [hit[0] if hit else None, box_hit(origin, direction, cube)])
                updates = beam_transforms(origin, endpoint, active, down)
                updates["/World/AnimatedCube"] = sample.transform(*cube)
                context.update_prim_transforms(updates)
                result, _ = context.bind_frame(plan, sample.DELTA_TIME)
                if result == ovxr.Result.NOT_READY:
                    continue
                sample.check(result, context, "bind_frame")
                # Resolve anchor conversion after this frame's bind.
                try:
                    compositor.set_tracking_transform(context.transform_openxr_pose((0, 0, 0)))
                    anchor_ready = True
                except ovxr.OvxrError as error:
                    if error.result != ovxr.Result.NOT_READY:
                        raise
                    # Non-rendering startup frames do not seed an anchor and
                    # have no render outputs. Still submit/end the plan.
                    anchor_ready = False
                layers = projection.layers(plan)
                result, frame = context.submit_frame(plan, additional_layers=layers, replace_scene_layer=bool(layers))
                sample.check(result, context, "submit_frame")
                progress.record(frame)
                if frame.submitted and not controls.paused:
                    phase += 2 * sample.DELTA_TIME * controls.speed
                if frame.submitted and args.evidence and index in (0, 12, 24, 42, 56, 90):
                    surface.screenshot(args.evidence / f"ui-{index:03d}.png")
            finally:
                if plan.frame:
                    context.cancel_frame(plan)
            if mirror and not projection.update_mirror(mirror, progress.submitted % 3 == 0):
                break
        # [/snippet:panel-frame-loop]
        if args.frames and progress.submitted < args.frames:
            raise RuntimeError("Session stopped before the requested number of frames")
        evidence = dict(submitted_frames=progress.submitted, input_mode=args.input, headset_verified=False,
                        button_clicks=controls.clicks, slider_changes=controls.slider_changes, speed=controls.speed,
                        blocked_pressed_frames=blocked, tracked_input_frames=tracked_frames,
                        mirror_frames=mirror.presented if mirror else 0, **compositor.report())
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(evidence, indent=2) + "\n", encoding="utf-8")
        print(f"Report: {args.report}", flush=True)
    print(f"ovui-panel: submitted {progress.submitted} frames; clean shutdown.", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(sample.run(main))
