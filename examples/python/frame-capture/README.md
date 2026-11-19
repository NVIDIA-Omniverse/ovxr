# Frame capture and PiP (Python)

**App type:** stereo VR (opaque). Mono and AR are not configured by this sample. The PiP feed itself is monoscopic. See the [compatibility matrix](../../COMPATIBILITY.md).

[Preview this sample in the gallery](../../gallery/README.md#frame-capture-and-pip).

See [Adapting this sample](#adapting-this-sample) for guidance on using the
canonical skills in your application.

Display a separate scene camera in a small desktop picture-in-picture (PiP) window, a
floating panel inside the scene, or both. Optionally save a full-resolution BMP
and JSON metadata while the previews keep running.

After [environment setup](../../README.md#running-samples), run from this sample directory:
```powershell
# Both live previews; no files saved until you press S.
python main.py --pip both

# Both previews plus an automatic snapshot at submitted frame 60.
python main.py --pip both --output "$PWD\observer.bmp"

# Each preview can also run independently.
python main.py --pip desktop
python main.py --pip headset

# Existing save-only usage remains valid and exits after 120 submitted frames.
python main.py --eye right --raw --output "$PWD\right-raw.bmp"
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
[observer.cpp](../../c/sample-support/observer.cpp). Saving, the desktop window
and the headset panel share those owned pixels. Its metadata identifies the
camera and RenderProduct instead of inventing XR eye or predicted-time fields.

The following readback rules apply to `--source eye`:

`Context.acquire_frame()` returns a guarded `Frame`; `FrameCopy` calls the CUDA Driver API through standard-library `ctypes`.
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

**Goal:** Create live XR PiP and image capture

Use the canonical skills: [frame-readback](../../../skills/frame-readback/SKILL.md).
See the [prompt context (FYI)](#prompt-context-fyi) and [validation coverage](../../../skills/COVERAGE.md).
The matched SDK contracts take precedence over example implementation choices.

Build the customer's own camera/eye-preview application with desktop, headset, both or
save-only modes. Keep optional image saving useful while live previews continue.
Read the API limits below before promising a full-resolution headset video panel.

### Read first

Read [customer project setup](../../README.md#shared-project-setup-and-ownership), then:

- [Python main](main.py) and
  [C++ main](../../c/frame-capture/main.cpp), especially
  `publish-pip-panel` and `copy-borrowed-frame`.
- [Python capture helper](../common/frame_copy.py),
  [C++ capture helper](../../c/common/frame_copy.h), and
  [panel implementation](../../c/sample-support/panel.cpp).
- [Desktop window](../../c/sample-support/mirror.cpp),
  [Python bridge](../common/sample_support.py),
  [panel scene](../../assets/pip-room.usda), and
  [CMake project](../../c/frame-capture/CMakeLists.txt).
- [Run/options guide](README.md).

### Configure the customer view

Choose the scene, initial anchor, camera/eye source, preview modes, panel pose/size,
refresh cadence and optional output path. Expose `--pip both|desktop|headset|off`
or equivalent application controls. The example uses both live previews by
default, updates every three submitted frames and saves only on S or explicit
output configuration. PiP defaults to a separate camera; save-only defaults to
eye readback. Preserve those useful defaults unless the request differs.

For `--source camera`, use [the observer helper](../../c/sample-support/observer.cpp)
and [camera scene](../../assets/pip-camera.usda). Render the mono
`/Render/PipObserver` at the application's committed ordinal after XR submission,
map `LdrColor` to CPU, copy it, then unmap and release the results. Keep the
borrowed renderer alive until the observer is closed. Camera animation/scene
writes must use OVStage writes at the unsealed ordinal; the OVXR transform
convenience call seals an explicit ordinal itself and cannot precede a bind at
that same ordinal. Reuse the sample's matrix writer. Frame/eye ownership below
applies to the optional eye source, not to the separate camera.

For `--source eye`, set `enable_frame_readback=true`, choose render product 0/1 for the selected
stereo eye, and request `readback_foveation_unwarp=true` for an ordinary eye
preview. A raw option turns only readback unwarp off; it must not reconfigure
headset submission. Keep run-report and snapshot-metadata paths distinct.

### Implement one owned copy with several consumers

1. Submit an XR frame, acquire readback, and require a sequence greater than the
   last one observed. A successful acquisition can return an old image.
2. At the chosen refresh/save point, copy RGBA8 from the borrowed CUDA pointer
   synchronously before advancing XR. Honor `pitch_bytes`, format and CUDA
   device/context. Reuse the helpers that push/restore the pointer's CUDA context
   even on failure; no additional CUDA toolkit is needed for their driver calls.
3. Share that single owned CPU copy between the selected consumers. Never retain
   a borrowed device pointer for a later panel update or free/write it. Sequence
   N's native buffer expires when capture N+2 starts; context destruction expires
   every view. Python enforces an additional conservative call-entry boundary.
4. Desktop: pass owned pixels to the small resizable Win32 PiP window. Preserve
   aspect ratio, handle close and S/Q events, and pump the window between frames.
5. Headset: retain owned bytes and scalar dimensions for the next frame. In
   APPLICATION ordinal mode, update panel attributes before binding the next
   frame with that same ordinal. Handle sealing/retries as in the
   [live scene adaptation guide](../live-scene/README.md#adapting-this-sample). Destroy the panel queries before
   the stage. Eye-source desktop-only/save-only can retain PRIVATE mode; camera
   source uses APPLICATION mode even without a headset panel.
6. Saving: wait for a fresh capture after the request, write a full-resolution
   BMP plus dimensions/pitch/sequence/source JSON metadata, and
   continue previewing. BMP drops alpha. Make repeated-save overwrite behavior
   explicit, or choose unique filenames if requested. Camera metadata identifies
   the camera/RenderProduct; only eye metadata includes device/timing/eye/unwarp.
7. Detect a prolonged lack of fresh frames and requested snapshots that never
   complete. Finite runs must reach their submission target. Quit normally to
   write the run report and clean up resources.

### Implement the panel honestly

The sample uses a **128 x 96 emissive face-color mesh** populated through public
OVStage attribute writes, so the panel participates in scene occlusion. OVXR
also accepts application-owned OpenXR layers through its submit descriptor;
using those requires application-managed swapchains and graphics uploads. The mesh's
`/World/PipPanel/Screen` path is also used in the native helper; if renamed,
change the asset and helper together or parameterize the helper.

Preserve image orientation, source aspect ratio, uniform face-color interpolation
and the emissive material's displayColor reader. Decode sRGB bytes to scene-linear
emission as the helper does, but do not claim a calibrated display: tone mapping
can change appearance. Customize placement/size in stage units, adapting the
helper's geometry dimensions as well as the initial mesh.

Both eyes see one monoscopic captured image on world-locked geometry. It has
at least one frame of delay and lower detail than desktop/saved output. Capturing
an eye that sees this panel produces recursive mirror feedback. The default
separate camera keeps the panel out of its frustum; check that when changing its
pose or lens. `--eye` and `--raw` require the eye source.

Treat it as a diagnostic CPU-copy preview. Do not invent a public dynamic-texture,
zero-copy streaming or compositor-layer API to improve it. The desktop helper
requires Windows; use headset/save-only modes on other hosts and report actual
platform validation.

### Validate and hand off

Check both, desktop-only, headset-only and save-only modes when supported. Verify
fresh sequence progression, correct orientation/colors, S-key saving without
stopping previews, and clean Q/window-close shutdown. Inspect a compositor image
after the panel has updated, not only the initial blank panel frame.

Test both sources, stale sequences, failed copy context restoration, panel-write failure, and
unsealed bind retry. Confirm each fresh sequence is copied once and panel writes
precede bind. Test right-eye/raw capture when foveation is available.

Return the custom app, scene/panel configuration, exact build/run commands and
observed results. State the panel resolution/latency limits and which physical
headset checks remain.

### App type and preview

Read the [per-sample compatibility matrix](../../COMPATIBILITY.md) and
[gallery](../../gallery/README.md) before choosing display/session modes.
The shipped sample uses stereo VR; a mono mirror/feed does not make it a mono XR
app. Do not claim AR, passthrough, hand tracking or physical headset validation
from emulator captures. Preserve these distinctions in the customer's README.

## Prompt context (FYI)

<details>
<summary>Request history and reconstructed prompt</summary>

References to a recipe in the recorded prompt now correspond to the
[adaptation guidance above](#adapting-this-sample).

### Provenance

Recovered request excerpt (2026-09-30):

> can we add a pip sample? maybe we could change frame-capture as I am not sure it's useful to save a BMP (we could do both: PIP and save)

This is historical context, not a complete generation transcript or a claim
that this request alone produced the current code. The implementation evolved
through follow-up requests, review and test fixes.

### Reconstruction prompt

Written on 2026-10-06 to describe the current example; it is not the original
verbatim generation prompt. Language coverage: Python and C++.
Use the canonical skills: [frame-readback](../../../skills/frame-readback/SKILL.md), plus the language's project-setup skill.

```text
Create an application demonstrating frame-capture using the matched OVXR, OVRTX and OVStage SDKs
provided with this request. Read the listed skills and the linked sample recipe.
Provide desktop and in-headset picture-in-picture plus optional saving. Support a separate observer camera and XR-eye source. Copy borrowed captures before advancing; retain shared assets and native helper dependencies.
Use the recipe's bundled scene and controls as the baseline, keeping all shared
helpers and asset references coherent. Do not change the SDK APIs or weaken the
tests to accommodate the result. Provide build/run commands and report actual
validation results separately from unrun hardware checks.
```

Read the [frame-capture adaptation guide](#adapting-this-sample) for source, snippet regions
and the precise feature contract. The adaptation guide links the checked-in implementation;
using it is reference-assisted adaptation, not a blind skill-generation evaluation.
For exact bytes or independent generation checks, follow
[reproduction and evaluation](../../VALIDATION.md#source-reproduction-and-generation-evaluation). No independent generation pass is
claimed by this prompt record.

</details>
