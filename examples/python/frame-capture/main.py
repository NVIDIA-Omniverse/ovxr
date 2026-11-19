# Copyright (c) 2026, NVIDIA CORPORATION. All rights reserved.
"""A separate scene camera in desktop/headset PiP; optional eye capture and saving."""

from contextlib import ExitStack
import json
import math
from pathlib import Path
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "common"))
from frame_copy import FrameCopy, save_bmp
from sample_support import Observer, PipPanel, PipWindow
from transform_writer import TransformWriter
import xr_sample as sample
import ovxr


def main():
    parser = sample.parser(__doc__)
    parser.set_defaults(frames=None)
    parser.add_argument("--pip", choices=("both", "desktop", "headset", "off"),
                        help="default: both, or off when only --output is supplied; desktop requires Windows")
    parser.add_argument("--preview-every", type=sample.nonnegative, default=3, help="submitted frames per preview update")
    parser.add_argument("--capture-at", type=sample.nonnegative, default=60, help="first eligible automatic save frame")
    parser.add_argument("--output", type=Path, help="optional automatic BMP + JSON save; S saves on demand")
    parser.add_argument("--report", type=Path, help="optional run statistics JSON")
    parser.add_argument("--source", choices=("camera", "eye"), help="default: camera with PiP, eye for save-only")
    parser.add_argument("--eye", choices=("left", "right"))
    parser.add_argument("--raw", action="store_true", help="keep rendered warp/size instead of requesting unwarp")
    args = parser.parse_args()
    args.pip = args.pip or ("off" if args.output else "both")
    args.source = args.source or ("eye" if args.pip == "off" else "camera")
    if args.source == "camera" and (args.eye or args.raw):
        parser.error("--eye and --raw require --source eye")
    args.eye = args.eye or "left"
    camera = args.source == "camera"
    if args.frames is None:
        args.frames = 120 if args.pip == "off" else 0
    if not args.preview_every or not args.capture_at:
        parser.error("--preview-every and --capture-at must be positive")
    if args.output and args.frames and args.capture_at > args.frames:
        parser.error("--capture-at must be no larger than a finite --frames limit when saving")
    if args.pip == "off" and args.output is None:
        parser.error("--pip off requires --output PATH.bmp")
    output = (args.output or Path("ovxr-frame.bmp")).resolve()
    if output.suffix != ".bmp":
        parser.error("--output must end in .bmp")
    if args.report and args.report.resolve() in (output, output.with_suffix(".json")):
        parser.error("--report must differ from the snapshot BMP and its JSON sidecar")
    desktop, headset = args.pip in ("both", "desktop"), args.pip in ("both", "headset")
    copy = FrameCopy()
    # [snippet:enable-frame-readback]
    context_options = dict(enable_frame_readback=not camera, readback_render_product_index=1 if args.eye == "right" else 0,
                           readback_foveation_unwarp=not args.raw)
    # Readback unwarp changes the copied image, not OpenXR submission.
    # [/snippet:enable-frame-readback]
    scene = sample.SCENE.with_name("pip-room.usda" if headset else "pip-camera.usda") if headset or camera else sample.SCENE
    with sample.application("frame-capture", args, explicit_ordinals=headset or camera, scene=scene,
                            context_options=context_options, with_renderer=True) as (stage, context, renderer), ExitStack() as cleanup, \
            sample.keyboard() as read_key:
        observer = Observer(renderer) if camera else None
        if observer:
            cleanup.callback(observer.close)
        writer = TransformWriter(stage, "/World/AnimatedCube") if camera else None
        if writer:
            cleanup.callback(writer.close)
        window = PipWindow(f"OVXR {args.source} PiP | S save, Q quit") if desktop else None
        if window:
            cleanup.callback(window.close)
        panel = PipPanel(stage) if headset else None
        if panel:
            cleanup.callback(panel.close)
        print(f"PiP: {args.pip}; S saves {output}; Q quits.", flush=True)
        progress = sample.Progress(args.frames)
        ordinal = sample.next_ordinal(stage) if panel or camera else None
        pending_panel = None  # Only CPU bytes + dimensions; never a borrowed Frame.
        last_sequence = previews = saves = 0
        last_capture = time.monotonic()
        save_requested = False
        automatic_save = args.output is not None
        while progress.running():
            if window and not window.present():
                break
            key = (window.key() if window else "") or read_key()
            if key == "q":
                break
            save_requested |= key == "s"
            if panel or camera:
                result, plan = context.begin_frame()
                if result == ovxr.Result.NOT_READY:
                    time.sleep(0.01)
                    continue
                sample.check(result, context, "begin_frame")
                try:
                    # [snippet:publish-pip-panel]
                    # Upload the previous owned capture BEFORE binding this
                    # frame. OVXR seals the same application ordinal as its eyes.
                    if pending_panel is not None:
                        panel.update(*pending_panel, ordinal)
                        pending_panel = None
                    if camera:
                        t = progress.submitted * sample.DELTA_TIME
                        writer.write(sample.transform(.45*math.sin(t), 1.3, 0, t), ordinal)
                    result, sealed = context.bind_frame(plan, sample.DELTA_TIME, stage_ordinal=ordinal)
                    if sealed:
                        ordinal = sample.next_ordinal(stage)
                    if result == ovxr.Result.NOT_READY:
                        continue  # New plan, same unsealed ordinal and pending writes.
                    sample.check(result, context, "bind_frame")
                    result, frame = context.submit_frame(plan)
                    sample.check(result, context, "submit_frame")
                    # [/snippet:publish-pip-panel]
                finally:
                    if plan.frame:
                        context.cancel_frame(plan)
            else:
                result, frame, _ = context.render_frame(sample.DELTA_TIME)
                if result == ovxr.Result.NOT_READY:
                    time.sleep(0.01)
                    continue
                sample.check(result, context, "render_frame")
            progress.record(frame)
            if not frame.submitted:
                continue
            want_preview = args.pip != "off" and (previews == 0 or progress.submitted % args.preview_every == 0)
            want_save = save_requested or (automatic_save and progress.submitted >= args.capture_at)
            if args.pip == "off" and not automatic_save and not save_requested:
                continue
            # [snippet:copy-borrowed-frame]
            if observer:
                view, rgba = observer.capture(ordinal - 1) if want_preview or want_save else (None, None)
            else:
                view = context.acquire_frame()
                rgba = None
            if view is not None and view.sequence > last_sequence:
                last_sequence = view.sequence
                last_capture = time.monotonic()
                if want_preview or want_save:
                    # One synchronous copy feeds all consumers, before XR advances.
                    if rgba is None:
                        rgba = copy.copy(view)
                    if want_save:
                        output.parent.mkdir(parents=True, exist_ok=True)
                        save_bmp(output, view, rgba)
                        metadata = {key: getattr(view, key) for key in ("width", "height", "pitch_bytes", "sequence")}
                        metadata.update(source=args.source, submitted_frame=progress.submitted)
                        if camera:
                            metadata.update(camera_path="/World/PipCamera", render_product_path="/Render/PipObserver")
                        else:
                            metadata.update(cuda_device_id=view.cuda_device_id, predicted_display_time_ns=view.predicted_display_time_ns,
                                            readback_foveation_unwarp=not args.raw,
                                            render_product_index=context_options["readback_render_product_index"])
                        output.with_suffix(".json").write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")
                        print(f"Saved {output} ({view.width}x{view.height}, sequence {view.sequence})", flush=True)
                        saves += 1
                        automatic_save = save_requested = False
                    if want_preview:
                        previews += 1
                        if panel:
                            pending_panel = (rgba, view.width, view.height, view.pitch_bytes)
                        if window and not window.present(rgba, view.width, view.height, view.pitch_bytes):
                            break
            # A repeated sequence is stale, even when acquire_frame succeeds.
            # [/snippet:copy-borrowed-frame]
            if (not observer or want_preview or want_save) and time.monotonic() - last_capture > 60:
                raise RuntimeError("No fresh readback for 60 seconds")
        if automatic_save or save_requested:
            raise RuntimeError("Session ended without saving a fresh capture; increase --frames")
        if args.frames and progress.submitted < args.frames:
            raise RuntimeError("Session stopped before frame target")
        if args.pip != "off" and not previews:
            raise RuntimeError("PiP received no fresh frames")
        if panel and not panel.presented:
            raise RuntimeError("No PiP panel update was rendered; increase --frames")
        evidence = dict(submitted_frames=progress.submitted, pip_mode=args.pip, preview_frames=previews,
                        desktop_frames=window.presented if window else 0, panel_updates=panel.presented if panel else 0,
                        saved_frames=saves, source=args.source, eye=None if camera else args.eye,
                        readback_foveation_unwarp=None if camera else not args.raw, headset_verified=False)
    if args.report:
        report = args.report.resolve()
        report.parent.mkdir(parents=True, exist_ok=True)
        report.write_text(json.dumps(evidence, indent=2) + "\n", encoding="utf-8")
    print(f"PiP frames: desktop={evidence['desktop_frames']}, panel={evidence['panel_updates']}; saved={saves}.", flush=True)
    print(f"frame-capture: submitted {progress.submitted} frames; clean shutdown.", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(sample.run(main))
