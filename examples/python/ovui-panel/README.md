# OVUI panel (Python)

**App type:** stereo VR (opaque). Mono and AR are not configured by this sample. Python only; opaque UI, scene depth, no foveation. See the [compatibility matrix](../../COMPATIBILITY.md).

[Preview this sample in the gallery](../../gallery/README.md#ovui-panel).

A real `omni.ui` button pauses a moving cube; an `omni.ui.FloatSlider` changes
its speed. The 768 × 480 canvas appears as a 1.2 × 0.75 m panel in the room.
Each XR eye projects the same canvas from its own pose and FOV. A foreground
cube occludes the panel's lower portion in both color and depth and also blocks
pointer input there, leaving the main controls and pointing beam visible.
The desktop mirror receives the same composed eye image.

Create your own version with [the OVUI panel skill](#adapting-this-sample).

## Requirements and launch

This sample targets **Windows, Python 3.12, NVIDIA RTX GPUs, and unfoveated stereo**.
The adapter composites on OVUI's selected CUDA device and transfers the result
to application-owned OpenXR swapchains through synchronous host staging. It uses
OVUI 0.2.0, Warp 1.15.0, the matched
OVRTX/OVStage SDKs, and an OVXR build exposing `plan.step_result` and `additional_layers`.
An older release payload cannot run this sample merely by updating Python files.
Build the example `ovxr-sample-support` library for controller/hand input and
the optional Win32 mirror. See the [gallery setup](../../README.md).


For an extracted SDK, install the Python packages and native libraries as
described in [project setup](../../README.md#shared-project-setup-and-ownership). From this
sample directory, use `python -m pip install -r requirements-windows.txt` in your
Python 3.12 environment. Set `OVXR_SAMPLE_SUPPORT_LIBRARY` to the built example
helper, then run `python main.py --input controllers --mirror`. Set `CUDA_PATH`
to a toolkit providing `bin/cudart64_12.dll` and add its `bin/` directory to
`PATH`. First renderer startup may compile shaders for several minutes. Keep
the sibling `common/` and `assets/` directories. OVUI uses private tick/input
entrypoints in this version; they are isolated in `common/ovui_panel.py`.

Use the official OpenXR CloudXR client with Quest Pro or AVP and your active
CloudXR runtime. `--input hands` explicitly requests `XR_EXT_hand_tracking` and
uses index-finger direction plus thumb/index pinch. AVP gaze-and-pinch is not
implemented; the client/runtime must expose the requested tracked input.
Press Q or Ctrl+C to quit; `--frames N` stops after N submitted frames.

## How the bridge works

1. OVUI owns layout, text, widgets, value models and interaction feedback. Its
   headless Vulkan canvas is exported and copied into an owned CUDA image, with
   producer-ready and consumer-complete synchronization.
2. An XR aim ray intersects the panel. A matching analytic cube collider rejects
   blocked hits. Pixel coordinates and select/pinch become mouse events for OVUI.
   Loss of tracking/focus, changed source or occlusion cancels the pointer;
   release is required before a new press. Drags can continue outside the canvas.
3. After bind resolves the anchor, the app caches the OpenXR-to-scene transform.
   The app reads color and `XRDepth` from the borrowed `plan.step_result`, copies
   them to owned storage, then unmaps the outputs. The sample point-resamples
   depth to color resolution when needed (for example with DLSS).
4. Warp projects the canvas using that eye's asymmetric FOV and pose. It compares
   panel depth with scene reverse-Z depth and updates color **and depth** where
   the panel is visible. The compositor finishes GPU work before upload.
5. The app uploads both eyes into its own OpenXR color/depth swapchains, waits
   for Vulkan completion and releases them. It submits the projection through
   `additional_layers` with `replace_scene_layer=True` (the image already contains
   the scene), and uses its composed image for the optional mirror.
   Errors cancel the unconsumed plan. Renderer-owned outputs remain read-only.

Relevant code: [widgets and frame loop](main.py), [OVUI surface/input adapter](../common/ovui_panel.py),
[GPU composition](../common/panel_compositor.py), [pointer hit test](../common/panel_model.py),
and [projection bridge](../common/projection_layer.py).

The panel is opaque and axis-aligned in the bundled Y-up, meter-sized room.
This is a direct eye-image composition example, not an OpenXR quad overlay.
OpenXR depth submission alone does not depth-test an independently submitted UI
layer. Foveated images, transparent panels, arbitrary-scene collision,
and multi-panel focus require additional work. Host transfers work even when
OVUI and XR select different GPUs, but add latency. This is a correctness reference.

## Validation

`--input demo` routes synthetic rays, button presses, a slider drag and a blocked
click through the same pointer adapter. It never counts as tracked hardware.
`--report result.json` records eye indices, composed/occluded pixels, UI events,
mirror updates and `headset_verified: false`. `--evidence output/` saves canvas
images before and after these interactions.


On each physical headset, verify readable text and stable stereo placement while
moving your head; correct cube/panel ordering in both eyes and the mirror; button
release and slider dragging; blocked clicks; tracking/focus loss while pressed;
and controller or hand availability. Record the runtime/client versions and the
input mode. Emulator success does not establish comfort, latency or hardware input.

## Visible pointing

A magenta beam follows the same origin/direction used for input and ends at the
nearest sample collider (or the ray's range limit). The white tip marks the hit;
the beam gets thicker while select/pinch is held. Invalid or lost tracking hides
the ray. `--input demo` uses synthetic rays from a visible off-center origin.
These markers explain the interaction; they are not a separate picking system.

## Adapting this sample

**Goal:** Build an OVUI panel in XR

Use the canonical skills: [scene-occluded-ui](../../../skills/scene-occluded-ui/SKILL.md), [composition-integration](../../../skills/composition-integration/SKILL.md).
See the [prompt context (FYI)](#prompt-context-fyi) and [validation coverage](../../../skills/COVERAGE.md).
The matched SDK contracts take precedence over example implementation choices.

Build the customer's controls with real `omni.ui` widgets and models. Begin with
one opaque panel and unfoveated stereo. This sample is Python only;
do not promise a C++ OVUI binding or foveated compositor that is not present.

### Read and reuse

- [Project setup](../../README.md#shared-project-setup-and-ownership) and [sample requirements/launch](README.md).
- [Widgets and lifecycle](main.py), especially
  `ovui-widgets` and `panel-frame-loop` snippet markers.
- [OVUI adapter](../common/ovui_panel.py): `ovui-pointer-routing`
  and `ovui-gpu-export`; reuse this version-pinned adapter.
- [Warp compositor](../common/panel_compositor.py):
  `panel-depth-composite` and `process-xr-eye`.
- [Analytic picking](../common/panel_model.py) and
  [native OpenXR input bridge](../../c/sample-support/input.cpp).
- [Projection bridge](../common/projection_layer.py) and its
  [native adapter](../../c/sample-support/projection.cpp).
- [C contract](../../../include/ovxr/ovxr.h), borrowed `plan.step_result` and
  `ovxr_submit_desc_t`, plus [Python wrapper](../../../python/ovxr/_src/context.py).

### Adapt to the customer

Use the scene, tools, widget labels, panel pose and inputs already requested.
Ask only for missing choices that materially affect the app; keep working on
independent parts. Preserve customer work and work within their chosen directory.

Replace `Controls` with the customer's data model and button/slider callbacks.
Use OVUI layout, text and model/view APIs; do not redraw those widgets as a pixel
mesh. UI events should update application state, and scene publication should
read that state. Keep GPU transport, input routing and widgets separate.

Use matched OVXR/OVRTX/OVStage headers, bindings and binaries and the pinned
OVUI/Warp environment. Keep private OVUI entrypoints inside the adapter and
validate them on any version change. Export and compose on OVUI's selected CUDA
device. The example uses synchronous host staging to reach the XR Vulkan device,
which may be on a different GPU. Profile before replacing this with GPU interop.

### Preserve composition and input contracts

1. Require depth and disable local/runtime foveation. Keep one renderer, one
   application stage and one OVXR session.
2. Locate and convert tracked input after begin using the plan's latched
   anchor, as described by the canonical input skill. The sample gates input
   until its first renderable bind resolves startup anchor state; do not turn
   that startup check into a rule to defer all input until after bind. Keep
   units consistent. This scene uses Y-up meters and a fixed world-Z panel plane.
3. Locate controller aim/actions or valid hand joints at predicted display time.
   Map rays to canvas pixels, block hits behind geometry, cancel on tracking/focus
   loss or source changes, and require release after cancellation. Preserve drag
   capture outside panel bounds. Arbitrary geometry requires real picking.
4. Tick/export OVUI once per frame; wait for its producer, finish CUDA reads and
   signal consumption. Keep the copied canvas alive until both eyes are composed.
5. After bind, read color and XRDepth from the borrowed `plan.step_result` using
   public OVRTX output mapping. Copy to owned storage and unmap before submitting
   or canceling. Never modify renderer outputs or destroy the borrowed result.
6. Process each eye with its own pose, asymmetric FOV and clip planes. Resample
   depth to the color extent as needed. Compare panel depth with scene depth in
   the same reverse-Z convention; update both owned color and depth images only
   where the panel is visible. Independent layers do not automatically depth-test
   their pixels against scene geometry.
7. Upload to application-owned color/depth swapchains using the borrowed Vulkan
   binding, finish GPU work, release acquired images and submit the projection
   through `additional_layers` with `replace_scene_layer=True`: this image already
   contains the scene, so it replaces the original projection. Keep the layer, view/depth chains and swapchains
   alive through submit. The mirror uses the composed owned color image.
8. Cancel unconsumed plans on errors. Destroy swapchains/input children before
   their borrowed session, then the context, renderer and stage in ownership order.

The reference prioritizes simple ownership over transfer performance. Transparent
composition, foveation and arbitrary panel rotations require changes to projection
and hit testing; do not claim them from this sample.

### Validate and deliver

Render the actual UI, inspect a screenshot, then simulate pointer presses/drags
and inspect the resulting state/image. Do not substitute direct callback calls
for interaction evidence. Use demo input for deterministic emulator checks;
report its synthetic nature and keep `headset_verified` false.

Inspect both eyes with a foreground occluder and a clear panel region, plus the
desktop mirror. Verify that button and slider changes reach the scene and blocked
input does not. Test tracking loss while pressed and release-to-rearm. On Quest
Pro/AVP with the official OpenXR CloudXR client, validate the actual exposed input
mode; joint-based hands are not AVP gaze-and-pinch. Record hardware checks
separately from emulator/GPU results.

Deliver source, reproducible dependencies, a launch command, evidence paths and
remaining limitations. Keep the scene and linked helpers with the customer's
copy. Do not claim headset comfort, latency or reconnection from emulator results.

### App type and preview

Read the [per-sample compatibility matrix](../../COMPATIBILITY.md) and
[gallery](../../gallery/README.md) before choosing display/session modes.
The shipped sample uses stereo VR; a mono mirror/feed does not make it a mono XR
app. Do not claim AR, passthrough, hand tracking or physical headset validation
from emulator captures. Preserve these distinctions in the customer's README.

Keep the visible beam tied to the same ray and nearest-hit colliders as input;
hide it on tracking loss. Reuse [pointer visuals](../common/pointer_visuals.py)
for the shared math. Demo ray origins should be off-center so the beam is visible.
Workbench's demo hands use joint spheres, finger cylinders and rounded palms.
Tracked pinch uses OpenXR thumb tip 5, index proximal 7 and index tip 10. Synthetic joints are never
tracked-hand evidence. Preview motion using genuine emulator frame sequences.

## Prompt context (FYI)

<details>
<summary>Request history and reconstructed prompt</summary>

References to a recipe in the recorded prompt now correspond to the
[adaptation guidance above](#adapting-this-sample).

### Provenance

Recovered request excerpt (2026-09-30):

> some samples can be python only at least to start with
> also i think we can get the depth somehow and have proper compositing
> do we need to create a lot of glue code to connect ovxr to ovui though ?

This is historical context, not a complete generation transcript or a claim
that this request alone produced the current code. The implementation evolved
through follow-up requests, review and test fixes.

### Reconstruction prompt

Written on 2026-10-06 to describe the current example; it is not the original
verbatim generation prompt. Language coverage: Python.
Use the canonical skills: [scene-occluded-ui](../../../skills/scene-occluded-ui/SKILL.md), [composition-integration](../../../skills/composition-integration/SKILL.md), plus the language's project-setup skill.

```text
Create an application demonstrating ovui-panel using the matched OVXR, OVRTX and OVStage SDKs
provided with this request. Read the listed skills and the linked sample recipe.
Build real OVUI button/slider controls on a world panel. Composite per-eye color and depth so a foreground cube hides the panel and blocks input. Draw the pointer beam and support tracking-loss cancellation and release-to-rearm.
Use the recipe's bundled scene and controls as the baseline, keeping all shared
helpers and asset references coherent. Do not change the SDK APIs or weaken the
tests to accommodate the result. Provide build/run commands and report actual
validation results separately from unrun hardware checks.
```

Read the [ovui-panel adaptation guide](#adapting-this-sample) for source, snippet regions
and the precise feature contract. The adaptation guide links the checked-in implementation;
using it is reference-assisted adaptation, not a blind skill-generation evaluation.
For exact bytes or independent generation checks, follow
[reproduction and evaluation](../../VALIDATION.md#source-reproduction-and-generation-evaluation). No independent generation pass is
claimed by this prompt record.

</details>
