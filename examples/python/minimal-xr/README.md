# Minimal ovxr Example

**App type:** stereo VR by default. AR/blend options are runtime-dependent and not headset-validated; mono is not exposed. See the [compatibility matrix](../../COMPATIBILITY.md).

[Preview this sample in the gallery](../../gallery/README.md#minimal-xr).

See [Adapting this sample](#adapting-this-sample) for guidance on using the
canonical skills in your application.

The smallest useful ovxr Python example: load a USD stage and run a VR or AR
session against a connected headset or simulator.

1. Create an ovrtx `Renderer` and load a USD stage into it
2. Create an ovxr `Context` that borrows the renderer and authors its own stereo
   render products
3. Probe the OpenXR runtime, then drive `render_frame` until stopped

ovxr does not own scene data or the renderer — it borrows both, owns the OpenXR
session, and drives the per-frame render and submit. The scene lives in an
application-owned ovstage attached to the renderer in borrow mode (OVXR requires
one): the example uses `ovstage.population.open_usd_with_desc()` to populate
rendering data and the `upAxis`/`metersPerUnit` stage metadata, commits the
population by advancing the stage's write floor, and passes the stage to ovxr
through `ContextDesc.ovstage`. At startup it prints the scene's resolved up
axis and scale and where each came from (`OVStage` when the scene declares
them, otherwise the `--up-axis`/`--meters-per-unit` fallback).

## Prerequisites

- Python 3.10-3.13
- [uv](https://docs.astral.sh/uv/)
- NVIDIA RTX-capable GPU and a supported NVIDIA driver
- An installed OpenXR runtime, with a headset or simulator connected
- Network access for the initial Python/SDK dependency installation and the default robot scene; a bundled room is available offline
- Unsandboxed runtime execution

## Running


With the packaged dependencies:

```bash
uv run main.py                      # VR session, runs until Ctrl-C
uv run main.py --session-mode ar --blend-mode alpha
uv run main.py --frames 300         # stop after 300 submitted frames
uv run main.py --wait-timeout 120   # exit 1 after 120 s without frames
uv run main.py --no-foveation       # submit full-size views
uv run main.py --offline           # load the bundled room without probing the robot
uv run main.py --fallback-usd /path/to/fallback.usda  # override the fallback
uv run main.py --usd /path/to/scene.usda --up-axis y --meters-per-unit 0.01
```

## Default scene

Without `--usd`, the sample tries the Robot-OVRTX scene. It checks GET access to
its root layer, GR1T2 robot payload and dome HDR before opening it: the root
layer alone can open while the robot or lighting is unavailable. These bounded
probes check availability, not every transitive asset's contents. OVStage then
loads the scene through its usual resolver and cache.

If a probe or the robot's USD population fails, the sample logs the reason and
loads the bundled [minimal-xr.usda](../../assets/minimal-xr.usda) and texture-free
[room](../../assets/xr-room.usda). A failed population is discarded; the fallback
is populated into a fresh stage before attaching it to the renderer.

- `--usd PATH_OR_URL` selects exactly that scene and disables automatic fallback.
  Use `--usd ../../assets/minimal-xr.usda` to choose the basic scene explicitly.
- `--fallback-usd PATH_OR_URL` overrides the scene used after a robot load failure.
- `--offline` selects the fallback directly, without robot network probes. It
  cannot be combined with `--usd`.

The bundled fallback resolves relative to this script, independent of the
working directory. Keep `examples/assets` beside `examples/python` when
relocating it. Only fallback use requires these assets; online robot loads,
explicit scenes and `--help` do not. A missing bundle selected by `--offline`
is diagnosed before renderer initialization.

The fallback declares Y-up and one meter per unit, with a floor-level
`/Render/Camera` at `(0, 0, 3)` looking along -Z. The runtime adds head height.
Materials use the SDK's built-in `OmniPBR.mdl`; this scene needs no downloads.

## What to expect

Before probing remote assets, the example checks for a usable OpenXR runtime with
`ovxr.check_openxr_runtime()`. If none is installed, selected, or running (for
example, the CloudXR service is stopped), it prints one line saying so and
exits with status 1 before creating the renderer or loading the scene. A
running runtime with no headset or client connected yet is not fatal: the
example says so, compiles shaders and loads the scene meanwhile, and the frame
loop waits for the session. Connect the headset or client at any time.

Startup messages identify availability checks, renderer creation, scene loading,
XR setup and first-frame preparation. Shader and ray-tracing pipeline compilation
can take several minutes on first use, including during the first render call.
The messages are flushed before each blocking phase; the first actual submitted
frame reports its wait time and confirms startup is complete. The example also
prints the runtime, system and swapchain size, then progress every 90 submitted frames.

`render_frame` returning `NOT_READY` is normal and not an error: the runtime is
not asking for frames yet, because no headset or client is connected or the
session is still starting or idle. The loop retries every 50 ms and prints a
waiting notice at most every 10 s. It waits until Ctrl-C, or for
`--wait-timeout` seconds without frames (default 60 when `--frames` is given),
after which it exits with status 1.

## Notes

- **Stage up axis and scale.** ovxr maps OpenXR's +Y-up, metres-based tracking
  poses into stage space using the stage's `upAxis` and `metersPerUnit`. Under
  `SceneInfoPolicy.AUTO` (what this example passes) both are read from the
  attached ovstage's metadata when the scene declares them, so well-formed stages
  need no flags. `--up-axis` and `--meters-per-unit` are the descriptor fallback
  for stages that omit the metadata; get a fallback wrong and the session still
  runs but the view is rotated and mis-scaled.

  The flag defaults match the bundled fallback. The robot declares Z-up;
  AUTO uses that authored metadata instead. To check your own, read the metadata
  block at the top of the `.usda`:

  ```
  (
      metersPerUnit = 1
      upAxis = "Y"
  )
  ```

  USD's default `upAxis` is also `Y`. Only `metersPerUnit` differs: USD defaults
  to `0.01` rather than the bundled fallback's `1`. For a stage that omits both metadata
  fields, use `--up-axis y --meters-per-unit 0.01`.

- **Tracking anchor.** When the robot actually loads (also with an explicit
  `--usd` of the robot URL), the user starts on its floor
  1 m from the robot, facing it from the scene camera's side: `default_start_anchor()`
  passes that floor position and heading to `set_managed_anchor_transform`.
  ovxr keeps only the anchor's heading. When ovxr selects `STAGE` or `LOCAL_FLOOR`,
  the runtime's floor-level reference space adds the user's real eye height, so
  the virtual and real floors align. If only `LOCAL` is available, calibrate the
  runtime start pose before relying on floor alignment.
  For the fallback, `--offline`, or another `--usd`, `TRACKING_ANCHOR` names the prim whose
  transform anchors the tracking space instead. Change it to match your scene.
- **AR mode.** With the robot scene, `--session-mode ar` and an alpha blend mode, ovxr renders
  the background transparent, so the default scene's exhibition-hall dome
  becomes passthrough. Its ground plane is a matte object
  (`primvars:isMatteObject`), so it stays transparent apart from the robot's
  contact shadow, and only the robot is visible. These are properties of the
  robot scene; the bundled room does not provide a passthrough dome or matte ground.
- **Extra OpenXR extensions.** ovxr enables only the extensions it needs itself.
  If your application resolves its own entry points (hand tracking, CloudXR
  action or data-channel functions) against ovxr's instance, declare them via
  `ContextDesc.extra_openxr_extensions` or `xrGetInstanceProcAddr` will fail with
  `XR_ERROR_FUNCTION_UNSUPPORTED`.
- **Shutdown order.** The ovxr context borrows the renderer, so destroy the
  context before detaching the stage and destroying the renderer.

## Adapting this sample

**Goal:** Create a minimal XR application

Use the canonical skills: [project-setup-python](../../../skills/project-setup-python/SKILL.md), [project-setup-c](../../../skills/project-setup-c/SKILL.md), [application-flow](../../../skills/application-flow/SKILL.md), [context-creation](../../../skills/context-creation/SKILL.md), [frame-loop](../../../skills/frame-loop/SKILL.md).
See the [prompt context (FYI)](#prompt-context-fyi) and [validation coverage](../../../skills/COVERAGE.md).
The matched SDK contracts take precedence over example implementation choices.

Build the customer's own small application that loads USD, opens a stereo
session and shuts down cleanly. Start with VR and opaque blending; enable AR or
other display modes only when requested and supported by the selected runtime.

### Read first

Read [customer project setup](../../README.md#shared-project-setup-and-ownership), then the requested
language's source:

- [Python main](main.py) and
  [README](README.md).
- [C++ main](../../c/minimal-xr/main.cpp),
  [CMake project](../../c/minimal-xr/CMakeLists.txt) and
  [README](../../c/minimal-xr/README.md).

In either main, read snippet regions `create-renderer`, `add-usd`,
`create-context`, `probe`, and `frame-loop`. Use the matching public
[header](../../../include/ovxr/ovxr.h) for exact C signatures.

### Build the customer's version

1. Resolve language, local scene path, stage metadata and initial anchor from the
   request/project. Replace the sample's remote robot scene, its start pose
   and `/Render/Camera` when using different assets. Offer a scene-path option and
   an anchor setting in the generated application.
2. Own the renderer and stage, attach the stage in borrow mode, populate the USD
   at a fresh ordinal, then commit its write floor. In Python use
   `ovstage.population.open_usd_with_desc`; in C++ follow the
   `ovstage_population_open_usd_from_file_with_desc` setup and operation cleanup.
   Select the rendering domain and request `upAxis` and `metersPerUnit` through
   the description's stage metadata paths.
3. Construct `ContextDesc` / `ovxr_desc_t` with the borrowed handles,
   `create_render_products`, stereo, Vulkan and the customer's application
   name. Configure units/up-axis as fallbacks under AUTO. A new customer's
   well-authored scene should not inherit the sample's Y-up, meter fallback
   by accident.
4. Use PRIVATE ordinals for this sample: there are no direct scene writes after
   population. Probe using `Context.probe_openxr` / `ovxr_probe_openxr` and
   print the runtime, system and recommended view dimensions.
5. Drive `Context.render_frame` / `ovxr_render_frame`, count only submitted
   frames, and handle NOT_READY with a paced, time-based wait. Add a finite-frame
   option and a normal quit path. A finite run that ends early or an API error
   should return failure; do not blindly copy the minimal example's diagnostic
   loop breaks as a successful validation result.
6. Use structured cleanup for every partially created resource. Keep OVXR's
   borrowed renderer and stage alive until after context destruction. The
   [feature helper](../common/xr_sample.py) and
   [C++ helper](../../c/common/xr_sample.h) demonstrate stronger failure
   cleanup and submitted-frame accounting if needed.

### Customize without changing the ownership model

Keep the customer's scene, lights/materials and initial pose configurable.
For the minimal sample's local baseline use
[minimal-xr.usda](../../assets/minimal-xr.usda) with its `/Render/Camera` anchor
and Y-up, meter metadata. The other feature samples use the same room through
[xr-room.usda](../../assets/xr-room.usda) and its `/World/StartAnchor`.
Keep scene downloads explicit.

The API can support other session/display configurations, but this recipe's
baseline is stereo. AR blending does not establish real-world occlusion support.
For direct live scene edits, use the
[live scene adaptation guide](../live-scene/README.md#adapting-this-sample) to change ordinal ownership
deliberately.

### Validate and hand off

Build/import against the supplied SDKs. Run at least 100 submitted frames with
the customer's runtime and foveation disabled; include a failed-runtime or
not-ready startup check when practical. Confirm floor scale and initial view,
then quit and relaunch. If only a simulator is available, state that physical
head motion and headset presentation are still unverified.

Return the created files, resolved build/run command, scene/anchor choices,
submitted-frame result and remaining hardware checks. The deliverable is a
working customer application, not just instructions for launching the stock
sample.

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

The minimal C++ and Python examples already existed when this work began. No original generation prompt is available in this task.

### Reconstruction prompt

Written on 2026-10-06 to describe the current example; it is not the original
verbatim generation prompt. Language coverage: Python and C++.
Use the canonical skills: [project-setup-python](../../../skills/project-setup-python/SKILL.md), [project-setup-c](../../../skills/project-setup-c/SKILL.md), [application-flow](../../../skills/application-flow/SKILL.md), [context-creation](../../../skills/context-creation/SKILL.md), [frame-loop](../../../skills/frame-loop/SKILL.md), plus the language's project-setup skill.

```text
Create an application demonstrating minimal-xr using the matched OVXR, OVRTX and OVStage SDKs
provided with this request. Read the listed skills and the linked sample recipe.
Load USD into an application-owned OVStage and borrowed OVRTX renderer, create a stereo VR context, probe the runtime, render a bounded session and clean up partial initialization. Honor authored units/up axis and configurable scene/anchor paths.
Use the recipe's bundled scene and controls as the baseline, keeping all shared
helpers and asset references coherent. Do not change the SDK APIs or weaken the
tests to accommodate the result. Provide build/run commands and report actual
validation results separately from unrun hardware checks.
```

Read the [minimal-xr adaptation guide](#adapting-this-sample) for source, snippet regions
and the precise feature contract. The adaptation guide links the checked-in implementation;
using it is reference-assisted adaptation, not a blind skill-generation evaluation.
For exact bytes or independent generation checks, follow
[reproduction and evaluation](../../VALIDATION.md#source-reproduction-and-generation-evaluation). No independent generation pass is
claimed by this prompt record.

</details>
