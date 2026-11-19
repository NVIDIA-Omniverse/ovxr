# Frame capture and PiP (C++)

**App type:** stereo VR (opaque). Mono and AR are not configured by this sample. The PiP feed itself is monoscopic. See the [compatibility matrix](../../COMPATIBILITY.md).

[Preview this sample in the gallery](../../gallery/README.md#frame-capture-and-pip).

See [Adapting this sample](#adapting-this-sample) for guidance on using the
canonical skills in your application.

Display a separate scene camera in a small desktop picture-in-picture (PiP) window, a
floating panel inside the scene, or both. Optionally save a full-resolution BMP
and JSON metadata while the previews keep running.

After the [CMake build](../../README.md#running-samples), run from the executable directory
(PowerShell; on Linux use `./frame-capture`):
```powershell
# Both live previews; no files saved until you press S.
.\frame-capture.exe --pip both

# Both previews plus an automatic snapshot at submitted frame 60.
.\frame-capture.exe --pip both --output "$PWD\observer.bmp"

# Each preview can also run independently.
.\frame-capture.exe --pip desktop
.\frame-capture.exe --pip headset

# Existing save-only usage remains valid and exits after 120 submitted frames.
.\frame-capture.exe --eye right --raw --output "$PWD\right-raw.bmp"
```

On Linux use `--pip headset` or `--pip off --output /absolute/path.bmp`;
the desktop window requires Windows. Linux execution has not been validated.
Relative save paths, including the S-key default `ovxr-frame.bmp`, are relative
to the current working directory. The sample prints the absolute save path.
PiP modes use the example-only native `ovxr-sample-support` library, built with
the C++ samples. Python needs `OVXR_SAMPLE_SUPPORT_LIBRARY` set to its absolute
path; CMake stages it beside the C++ executable.

Press **S** in the desktop PiP or launching console to save the next fresh image.
Press **Q**, close the desktop window, or use Ctrl+C to exit. Use Q to write a
run report on normal shutdown. The window must have desktop focus to receive
keys; headset controllers do not trigger these commands. Each save replaces
the BMP and its JSON sidecar at the same path.

| Option | Default | Purpose |
| --- | --- | --- |
| `--pip both\|desktop\|headset\|off` | both, or off with only `--output` | Choose previews. `off` requires an output path. |
| `--source camera\|eye` | camera with PiP, eye for save-only | Choose a separate scene viewpoint or XR eye readback. |
| `--frames N` | 0 with PiP; 120 save-only | Stop after N submitted frames; 0 runs until quit. |
| `--preview-every N` | 3 | Update previews every N submitted frames, plus the first fresh capture. |
| `--capture-at N` | 60 | First eligible frame for the automatic `--output` save. |
| `--output PATH.bmp` | no automatic save | Save once automatically; also sets the S-key destination. Parent directories are created. |
| `--report PATH.json` | none | Save frame, preview-update and snapshot counts on normal exit. Use a distinct path from snapshot metadata. |
| `--eye left\|right` | left | With `--source eye`, select render product 0 or 1. |
| `--raw` | off | With `--source eye`, preserve rendered image size/warp. |
| `--no-foveation` | off | Disable both local foveation and runtime PQW. |

## What each preview demonstrates

The default `--source camera` renders a **different viewpoint** from
`/World/PipCamera`, looking across the animated cube from the right. Its own
640 x 480 mono RenderProduct uses the same renderer and committed scene as XR.
The camera is positioned with the floating panel out of view, avoiding recursive
feedback. Moving the headset changes the view of the panel, not the feed camera.
Edit its pose/lens in [pip-camera.usda](../../assets/pip-camera.usda).

The desktop PiP is a resizable, always-on-top Win32 window. It preserves the
source aspect ratio; optional BMP saving retains the source resolution. The
headset PiP is a world-locked, 0.9 m wide panel to the left of the cube in
[pip-room.usda](../../assets/pip-room.usda). Both eyes see the same mono feed,
with normal perspective and scene-depth occlusion.

The headset panel downsamples to **128 x 96 emissive face colors** using public
OVStage writes. It is a diagnostic mesh, not a dynamic video texture or native
OpenXR quad layer. Tone mapping can change its appearance. The previous owned
capture is published before the next XR bind, adding at least one frame of
delay; the three-frame refresh interval can add more.

Camera and headset modes use application-owned ordinals: publish animation and
panel writes, bind/seal, submit XR, then render the observer at that committed
ordinal. A sealed bind advances the ordinal; an unsealed `NOT_READY` retry
retains it. Camera mappings/results and panel queries are released before their
owners. Eye-source desktop/save-only modes retain OVXR's private ordinal ledger.

Use `--source eye` to learn XR readback, including `--eye right` and `--raw`.
Only this explicit mode can show recursive feedback when the eye sees its own
panel. `--eye` and `--raw` are rejected for camera source. Save-only invocations
continue to default to eye capture; add `--source camera` to save the observer.

## Capture ownership and saving

Camera mode maps OVRTX `LdrColor` to CPU storage and copies it before unmapping
and releasing the render result. The helper lives in
[observer.cpp](../sample-support/observer.cpp). Saving, the desktop window
and the headset panel share those owned pixels. Its metadata identifies the
camera and RenderProduct instead of inventing XR eye or predicted-time fields.

The following readback rules apply to `--source eye`:

`ovxr_acquire_frame` fills a borrowed `ovxr_frame_view_t`; `FrameCopy` loads the CUDA Driver API dynamically.
Both implementations require the NVIDIA display driver, with no extra CUDA
toolkit, NumPy, Pillow, or image-library dependency.

Readback is best-effort: a successful acquisition with a repeated sequence still
describes an old image. The sample makes **one synchronous copy per selected
fresh sequence**, then shares the owned CPU bytes among desktop display, panel
sampling and saving. It retains only those bytes and scalar dimensions for the
next panel update.

The borrowed CUDA allocation must never be freed or written by the application.
It expires when capture sequence N+2 starts or the OVXR context is destroyed.
An asynchronous consumer must finish and synchronize its GPU work within that
lifetime, or first make an owned copy. The copy helper temporarily pushes the
allocation's CUDA context and restores the previous context even on failure.

Readback unwarp affects both previews and the snapshot; it does not alter the
runtime's submission mode or render products. With foveation disabled it has no
effect. Raw capture can look compressed at the edges and be smaller than the
unwarped image. The source is one eye's render-product color, not a compositor
screenshot or stereo packing.

BMP stores RGB without preserving alpha. Its JSON sidecar records dimensions,
source pitch, CUDA device, capture sequence, submitted frame, predicted display
time, eye index, and the requested readback-unwarp setting. File encoding and
preview updates run on the frame thread and can stall submissions. A production
mirror or recorder would queue owned copies and handle backpressure separately.

If the session ends before a requested fresh save or any preview update, the
sample fails. A finite frame target must also complete before exit. Increase
`--frames` for slow loading. The 60-second no-readback/no-submission checks run
between API calls, so they cannot interrupt a blocked native call.

See the [validation checklist](../../VALIDATION.md) for emulator commands and
pending Quest Pro / AVP checks with the official CloudXR OpenXR client.

## Adapting this sample

The [shared adaptation guide](../../python/frame-capture/README.md#adapting-this-sample)
covers application design, API ownership and validation for both implementations.
Use this README's C++ build/run instructions with that guidance and the
[shared project setup](../../README.md#shared-project-setup-and-ownership).

## Prompt context (FYI)

The [Python README](../../python/frame-capture/README.md#prompt-context-fyi)
records the shared request history and reconstructed prompt for both implementations.
These notes are informational; use the checked-in source for exact reproduction.
