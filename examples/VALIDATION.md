# XR sample validation

Use the **official OpenXR client** with your CloudXR runtime on Quest Pro and
Apple Vision Pro (AVP). These samples are server-side OpenXR applications; they
do not require a custom browser or JavaScript client. Physical results are
pending tester execution. A successful emulator run is not headset proof.

## CloudXR setup

1. Install the matched SDKs and prepare the [launching environment](README.md#shared-project-setup-and-ownership).
2. Start your installed CloudXR runtime and connect the official OpenXR client
   using your existing connection and authentication settings. Record the
   runtime version, client name/version, headset OS version and host GPU driver.
3. Select that runtime through your existing OpenXR registration, or set
   `XR_RUNTIME_JSON` in the launching shell to its actual runtime manifest.
   The samples inherit this variable and do not alter system registration.
4. Start `runtime-lifecycle --mirror --no-foveation`. Allow several minutes for
   first-run renderer shader compilation. Check the reported runtime/system,
   stereo view, head motion, floor height and desktop mirror before adding input.

For an explicit manifest override (substitute your installed runtime manifest):

```powershell
$env:XR_RUNTIME_JSON = 'C:\path\to\your\cloudxr-runtime.json'
# Run from the public OVXR repository root.
python .\examples\python\runtime-lifecycle\main.py --mirror --no-foveation
```

NVIDIA documents runtime startup and process-local manifest selection in
[CloudXR runtime getting started](https://docs.nvidia.com/cloudxr-sdk/release/6/usr_guide/cloudxr_runtime/getting_started.html).
Capabilities depend on the runtime and client versions. The
[controller and hand tracking guide](https://docs.nvidia.com/cloudxr-sdk/release/6/usr_guide/cloudxr_runtime/controller_hand_tracking.html)
describes action paths and `XR_EXT_hand_tracking`. Workbench selects either
controllers or hands; it does not consume both simultaneously. Hand mode uses
a finger ray and thumb/index pinch derived from raw joint positions, not AVP's
platform gaze-and-pinch selection. If joint tracking is unavailable, record the
error as unsupported for that runtime/client combination.

## Emulator and model checks


## Visual sample updates recorded on 2026-10-01

Workbench hands now use 26 joint spheres, 24 cylinders and a rounded palm each,
with the same visualization helper for synthetic and tracked positions. The
focused Python suite passed 23 tests, including joint loss/focus loss; both C++
model/hand-geometry tests passed. Python and C++ demo runs each submitted 100
frames, with four tool commands, one blocked selection, dragging and one
teleport. The gallery's 96-frame GIF was refreshed from the Python run. Physical
hand tracking remains pending Quest Pro/AVP validation.

The foveation sample's stronger local defaults (`--multiplier 0.2 --inset 0.1`)
were checked in both Python and rebuilt C++. All three modes submitted 100 frames
and shut down cleanly in each language with the bundled `sanity-avp` emulator:

| Mode | Effective local rendering | Runtime foveated submission | Raw rendered readback |
| --- | --- | --- | --- |
| `off` | Disabled | No | 2000 × 1600 |
| `local` | Warped | No | 400 × 320 |
| `runtime` | Warped | Yes | 800 × 640 |

The refreshed [gallery comparison](gallery/README.md#foveation) uses Python's
full-size compositor captures from a fixed head pose. The local image shows a
sharp central grid and visibly softer peripheral detail. Runtime mode uses the
emulator's PQW parameters, not the sample's local preset. This is image-quality
evidence; headset comfort and performance remain unmeasured.

All seven C++ samples built against the staged release SDKs; the native workbench
model test passed. The Python API/sample suite passed **163 tests and 2 subtests**,
including the opt-in OVUI GPU tests. Its existing explicit-context-GC warning
remains. Added checks cover exact beam endpoints, hiding on tracking loss,
OpenXR thumb-tip pinch, camera ownership, and capture-after-submit ordering.

The C++ and Python workbench demos each submitted 100 frames and verified tool
commands, dragging, blocked input, teleporting and mirror updates. Simulated
hand spheres and rays were visible; tracked-input counters remained zero.
All three Python OVUI samples verified their scripted interaction results and
200 processed eye views, including scene-depth occlusion. Scene inspector also
read back scene transforms to verify selection, edits and shared undo/redo.

The PiP camera path rendered a distinct 640 × 480 mono RenderProduct at a
committed scene ordinal. Both languages exercised desktop/headset previews and
BMP saving; the explicit eye-source path still exercised borrowed readback.
Both languages also passed the camera/PiP check with their default foveation
settings (observer-camera PiP with saving and default foveation).
The pinned renderer can emit a transform-history warning when interleaving the
observer and XR products. This synchronous diagnostic sample does not validate
long-exposure or motion-blur correctness, or real-time streaming performance.

The [gallery](gallery/README.md) includes seven GIFs captured from actual Python
sample runs and refreshed stills.
Each GIF samples a 100-submission run; playback is deliberately slowed.
Compatibility labels describe the configured app type, not a broader runtime
certification. Quest Pro/AVP and AR/mono headset checks remain pending.

## OVUI panel validation recorded on 2026-09-30

These September records describe the earlier callback-based implementation.
The current samples use borrowed bound-frame outputs and application-owned
projection layers; rerun the current suite when assessing the migrated bridge.

Windows / RTX 4090, OVUI 0.2.0 and Warp 1.15.0: 279 mandatory native tests
and 122 Python tests passed, including GPU depth projection, native copy ownership,
lower-resolution depth alignment, callback errors and non-rendering startup.
The 100-frame emulator demo processed both eyes on every submitted frame, produced
33 mirror updates, exercised two button clicks and nine slider updates, and blocked
two pressed frames behind the cube. Clear and occluded stereo captures were inspected.
The report deliberately keeps physical headset validation pending. This validates
the synchronous reference bridge; no latency or comfort measurement was performed.

## Scene inspector validation recorded on 2026-09-30

The Python scene inspector reuses the same OVUI/Warp GPU bridge. Its 100-frame
emulator demo processed 200 eye views and produced 33 mirror updates. Synthetic
rays selected both scene objects and selected one through the OVUI list; two
slider drags produced 16 value updates. The run exercised two undo operations,
one reset and one redo. Every checkpoint verified all 16 matrix elements for
both objects and all twelve selection-cage bars by reading OVStage after
submission. Native OVUI values also matched the selected object's model.
Both stereo captures showed the corrected selection outline and depth occlusion.

The updated Python suite passed 142 tests including GPU projection, history,
rotated/scaled picking, release-to-rearm, cancellation and startup/failure cleanup.
The native OVUI test also dragged actual sliders through tracking/source loss,
occlusion and back-facing rays, checked rollback, then rearmed and used Undo.
One existing ResourceWarning test reports an expected unraisable-warning message.
Hardware input counts remain zero in demo mode; Quest Pro and AVP results remain
pending. This sample adds application-level tools using the shared composition bridge.


## Spatial UI validation recorded on 2026-09-30

The small Python sample has two buttons, header dragging and nearest-visible-panel
input. Its 100-frame emulator demo produced A=1, B=1 and one completed move.
Each panel was processed for all 200 eye views, with 33 desktop mirror updates.
Both eyes were inspected before/after moving A; B was correctly hidden by A and
the scene cube. The updated Python suite passed 155 tests, including real OVUI
button events, canvas-region row strides, both panel draw orders, capture and
move cancellation. The existing ResourceWarning test still reports its warning.
Synthetic input frames are not counted as tracked hardware; physical headset
validation remains pending.


## Physical headset checklist

Run both available languages on each device. For C++, pass the same options to
the built executable (see [run instructions](README.md#running-samples)).
OVUI panel, scene inspector and spatial UI have Python implementations and
require unfoveated rendering. For the other samples, start with `--no-foveation`;
repeat with the default foveation policy after the
baseline works. Use an unlimited run for
interactive checks, and Q to exit cleanly and write the report. Mirror keyboard
commands require its window to have desktop focus; console keys work in the
launching terminal. The window is a diagnostic left-eye view and adds synchronous
readback overhead.

```powershell
# Run from the public OVXR repository root after project setup.
# For the OVUI samples, activate the environment described in their README.
python .\examples\python\runtime-lifecycle\main.py --mirror --no-foveation --retry-after 30 --max-restarts 6 --report "$PWD\quest-runtime-python.json"
python .\examples\python\spatial-workbench\main.py --input controllers --mirror --no-foveation --report "$PWD\quest-input-python.json"
python .\examples\python\spatial-workbench\main.py --input hands --mirror --no-foveation --report "$PWD\avp-input-python.json"
python .\examples\python\frame-capture\main.py --pip both --no-foveation --output "$PWD\pip-eye.bmp" --report "$PWD\pip-run-python.json"
python .\examples\python\scene-inspector\main.py --input controllers --mirror --report "$PWD\quest-inspector-python.json"
python .\examples\python\spatial-ui\main.py --input controllers --mirror --report "$PWD\quest-spatial-ui-python.json"
```

| Check | Quest Pro | AVP | Expected observation / evidence |
| --- | --- | --- | --- |
| Setup and stereo | Pending | Pending | Correct runtime/system; stable stereo, floor and head motion; matching mirror. |
| AR blending | Pending | Pending | Run `ar-blending --blend alpha --frames 180 --report ar.json` in each language. Environment is visible around the objects; the dark cube stays opaque. Record resolved blend and alpha counts; compare with opaque only if supported. |
| View configuration | Pending | Pending | Run `view-configuration --display auto --frames 180 --report views.json`, then explicit stereo/mono if exposed by the runtime. Record resolved view count and actual view-0 dimensions. An unsupported mode is not a successful mono check. |
| Asynchronous capture | Pending | Pending | Run `async-capture --frames 180 --consumer-ms 200 --report capture.json` in each language. Frames keep submitting; the final report has consumed frames and offered = consumed + dropped. Increase the worker delay to observe drops. |
| OVUI panel | Pending | Pending | Real button and slider control cube motion; cursor/hover feedback; cube occludes panel in both eyes and mirror; hidden controls cannot receive clicks; tracking/focus loss cancels drags. Use `python main.py --input controllers --mirror` from `python/ovui-panel/` or exposed hand tracking. |
| Scene inspector | Pending | Pending | Select both parts in XR and the object list; outline and properties follow selection. Edit XYZ, yaw and scale; undo/redo/reset match in scene, panel and mirror. Desktop 1/2, A/D, U/Y/R use the same state. Occluded controls reject input; focus/tracking loss rolls back the active drag. |
| Spatial UI | Pending | Pending | Drag either header to reposition its panel. Buttons increment only their own counters; nearer panels and the cube block input. Lost tracking cancels a move; held input cannot transfer between panels. R restores placement. Check overlap in both eyes and mirror. |
| Explicit restart | Pending | Pending | R in lifecycle recreates XR, reconnects and resumes fresh mirror frames without recreating the renderer or reloading the room. |
| Client reconnect | Pending | Pending | Disconnect/reconnect the official client; record whether the existing session resumes or a retry recreates it. Room and mirror return. Record pauses/failures and elapsed time. |
| Runtime restart | Pending | Pending | Stop/restart CloudXR during lifecycle; observe bounded recovery or the precise reported failure. A blocked native call needs separate diagnosis. |
| Controller interaction | Pending | Optional, capability dependent | In controllers mode, aim marker follows tracked device; trigger selects a panel/cube; right-hand preference and left fallback work. Record active frame count. |
| Hand interaction | Optional, capability dependent | Pending | In hands mode, joint markers follow hands; finger ray/pinch selects and drags; release drops. Record active frame count and unsupported-extension errors. |
| Input loss / focus | Pending | Pending | While grabbing, hide hand/controller or change focus. Drag stops; resuming while held does not click or continue dragging until release and a new press. |
| Navigation | Pending | Pending | Select floor to teleport tracking origin; controller stick or Z/X snap turns once until centered. Physical tracking offset remains. |
| Shared app state | Pending | Pending | Desktop 1/2/3 edits appear in headset; XR commands update mirror and revision; 4 undoes the same command history. |
| Panel depth and input | Pending | Pending | Violet pillar hides part of button 4 and blocks selection. O removes it; button 4 works and undo restores the occluder. Move viewpoint to see the parallax. |
| Foveation / capture | Pending | Pending | Compare off/local/runtime grid modes; save frame capture and compare against view. Record effective fallback and depth enablement, not just requested settings. |
| Live PiP and save | Pending | Pending | Desktop PiP and the floating panel keep updating as the head moves. Panel stays fixed in the scene and has depth/parallax. Press S in the focused desktop window or console; a fresh full-resolution BMP + JSON is saved while previews continue. Record visible delay and color/detail differences. |
| Clean exit | Pending | Pending | Q exits without error and produces report; next launch succeeds. |

Scene panels are composed in the projection image. Their depth ordering is
renderer geometry; `depth_submission_enabled` in the workbench report records
whether runtime depth submission was also enabled. This does not establish
runtime reprojection quality, compositor quad-layer support, or occlusion by
real-world objects. The separate [composition-layer example](python/composition-layers/README.md)
uses `additional_layers` for independent quads and has its own validation limits.
The headset PiP is a 128 x 96 emissive face-color mesh using public stage writes,
not a dynamic video texture. It shows a delayed monoscopic eye image, including
itself, so recursive mirror feedback is expected. Desktop/save retain the full
capture resolution. PiP performance and appearance still need physical checks.

## Evidence record

For [independent composition layers](python/composition-layers/README.md), run
both `--placement head` and `--placement world` with `--verify-upload`. Check the
transparent surround and partial-alpha strip in both eyes on the actual runtime.
Move the head to distinguish the two placements. The quad remains an overlay
when scene geometry is in front of it. Local/CI stereo goldens check the isolated
test runtime's composition of every submitted scene/quad layer. Independent
swapchain readback checks texture contents and submission. Neither replaces
physical tracking, lens distortion or final headset-display validation.

Copy this record per device/language and attach the application log, JSON report,
and a short headset recording or screenshots. Do not mark pass from the mirror
alone; it cannot establish in-headset stereo or physical tracking behavior.

```text
Result: Pending / Pass / Fail / Unsupported
Date and tester:
Git commit and local changes:
Language and sample / exact command:
Headset and OS version:
Official OpenXR client name and version:
CloudXR runtime version and selected manifest:
Host GPU / driver / OS:
Controller or hand tracking mode:
Checklist rows exercised and observed results:
Disconnect/reconnect duration and recovery path:
Effective depth/foveation state:
Log / report / headset recording paths:
Failures, unsupported capabilities and reproduction steps:
```


## Replacement projection regression checks

OVUI Panel, Scene Inspector and Spatial UI compose over the bound renderer
color and XRDepth. Their replacement projection uses
`Context.get_frame_projection_views(plan)` (C:
`ovxr_get_frame_projection_views`) to copy the poses and FOV used by that
frame. Repeated `xrLocateViews` calls at the same display time can predict
different views; attaching a later pose to previously rendered pixels breaks
occlusion and reprojection. Native tests vary successive predictions, and the
sample CPU test checks that composition and submission use the copied views.

`LdrColor` is sRGB RGBA8. The native sample adapter negotiates
`VK_FORMAT_R8G8B8A8_SRGB` and uploads these bytes unchanged. An unsupported
format fails explicitly; it does not silently reinterpret the bytes as linear.
The procedural Composition Layers quad remains in its existing linear format.

Each of the three UI examples also runs a separate private composed-output
color regression. It captures the actual replacement projection plus a
test-owned half-transparent black quad, verifies Vulkan quad readback, and
compares both composed eyes against the IEC sRGB decode/blend/encode result at
every scripted checkpoint. This distinguishes the linear-format error from
correct sRGB interpretation. These checks add no approved reference names and
do not replace the existing image comparisons. They do not validate physical
headset optics, latency or late-stage reprojection.

## Source reproduction and generation evaluation

The FYI sections in sample READMEs distinguish recovered request excerpts from
reconstructed prompts. They record context; the checked-in source and its test
results establish what has been validated.

### Exact source reproduction

Use the checked-in source from a pinned full Git commit or the matching SDK
source archive. Copy the example with its common helpers, CMake modules and USD
assets; do not ask a model to recreate exact bytes. From the root of the public
OVXR repository, export the pinned revision to preserve those relative dependencies:

```text
git archive --format=zip --output=ovxr-public.zip <full-commit-id>
```



For an SDK source archive, retain the supplied archive and record its checksum;
the Git commands above require a repository checkout.

Record the resolved commit and archive SHA-256 alongside the SDK package versions.
This reproduces tracked source, not driver behavior, dependencies or timing.
An LLM prompt, even with fixed settings, is not an exact-source guarantee.

### Evaluating generated applications

1. Pin the full repository revision, SDK packages, skill files and their linked
   references, prompt text, assets, model/version and generation settings. Record
   file hashes, tool versions and allowed context. Do not put credentials in logs.
2. Generate into a clean disposable checkout. For a blind evaluation withhold
   sample implementation and helper solutions, and record the reference material
   actually supplied. For reference-assisted adaptation, allow the README guidance/source
   and label that mode. They answer different questions.
3. Keep the evaluator's tests and approved golden references fixed. Build the
   generated C++ entrypoints and run Python behavioral checks, then the existing
   runtime/visual scenarios against the generated application at the canonical
   paths in that disposable checkout. Test retained helpers as well. Compilation
   or importing alone is insufficient.
4. Verify that the executable/source under test is the generated output, not an
   earlier build or the reference checkout. Report selected scenario identities,
   actual submissions, image metrics, missing/skipped tests and visible stereo
   content. No threshold changes, reference approvals or new self-generated
   baselines are part of evaluation. Preserve failures and image differences.
5. Save the generated source, context hashes, exact commands, reports and images
   with the evaluation result. Repeat independent generations to measure success
   rate; one passing run proves only that run. Do not require text equality for
   a behaviorally equivalent implementation.

The [coverage map](../skills/COVERAGE.md) identifies existing checks and gaps.
Private test sources are maintainer evaluation tooling, not an SDK prerequisite.

Samples currently have 60
Python and 44 C++ eye comparisons, plus four composition-layer runtime checks
with actual swapchain readback; this covers all eleven C++ examples, not physical
headset behavior.

The checked-in examples are regression-tested. README prompt context and
coverage links do not claim that a fresh model has
regenerated and passed every sample, nor that every sentence of a skill has
runtime coverage. Such a claim requires a recorded generation evaluation above.
