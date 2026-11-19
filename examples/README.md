# ovxr Examples

This directory contains example projects demonstrating the ovxr OpenXR session
API. Eleven samples have both C++ and Python implementations;
OVUI panel, scene inspector and spatial UI currently use Python. C++ projects
build with CMake against matched ovxr, ovrtx and ovstage packages. Keep the
`common/`, `cmake/` and `assets/` siblings when copying a feature sample out of
this tree. Per-example READMEs explain the APIs and expected behavior.

**[Browse the sample gallery](gallery/README.md)** to see screenshots and
animations and before/after comparisons before choosing a sample. Check the
[app compatibility matrix](COMPATIBILITY.md) for stereo/mono and VR/AR scope. Each entry links to its
source, run instructions and adaptation guidance.

All samples require an NVIDIA RTX GPU and a running OpenXR runtime. These
examples include stereo VR, AR blending and mono/stereo view selection. They use
bundled texture-free scenes. Minimal XR first tries Robot-OVRTX online and uses
its bundled room only if the robot cannot load; `--offline` selects that
fallback directly. `--usd` selects an explicit scene without fallback, while
`--fallback-usd` overrides the fallback scene.


## Running samples

With installed SDKs, prepare the matched Python packages and native libraries
using [project setup](#shared-project-setup-and-ownership). For Python, change to
the chosen `python/<sample>/` directory and run `python main.py --help`, then
use the options in that sample's README. The three OVUI samples also require
their [pinned environment](python/ovui-panel/README.md#requirements-and-launch).

For C++, build either `c/` (all samples) or `c/<sample>/` with the matching SDKs:

```sh
cmake -S c -B c/build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="<ovxr-sdk>;<ovrtx-sdk>;<ovstage-sdk>"
cmake --build c/build --config Release
```

Without OpenXR and Vulkan SDK headers, the aggregate build configures the seven
independent samples and warns that frame-capture, runtime-lifecycle,
spatial-workbench and composition-layers were skipped. To build all eleven, add
`-DSAMPLE_OPENXR_INCLUDE_DIR="<openxr-sdk>/include"` and
`-DSAMPLE_VULKAN_INCLUDE_DIR="<vulkan-sdk>/Include"` to the configure command
(OpenXR 1.1.53 or newer).
Configuring one of those four samples directly requires both header sets and
fails with an explicit diagnostic when either is missing.

Run these commands from this examples directory, replacing the SDK placeholders
with actual paths. With Visual Studio, the executable is
`c/build/<sample>/Release/<sample>.exe`; with a single-config Linux generator it
is `c/build/<sample>/<sample>`. A standalone sample build omits the `<sample>/`
subdirectory under `build/`. Change to the executable's directory before using
the per-sample commands. On Linux use `./<sample>` in place of `.\<sample>.exe`.
CMake stages the required runtime files beside each executable.

All samples inherit the active OpenXR runtime, or an explicit `XR_RUNTIME_JSON`
environment override. First startup can spend several minutes compiling driver
shaders. Use `--help` for options, Ctrl+C to exit, or `--frames N` to stop after N
**submitted** frames. Relative output paths are relative to the launching
terminal's working directory. Feature samples return a nonzero status for API
failures and, for bounded `--frames N` runs, if no frame is submitted for 60
seconds between frame calls; this does not detect a runtime blocked inside an
API call. Unlimited interactive runs wait through idle or no-render sessions,
including a headset that has not been worn or has been removed, and remain
quittable with Ctrl+C. `runtime-lifecycle` uses its configurable retry window
and restart budget instead.

Python examples use only the standard library beyond the three SDKs, except
for the OVUI examples. Input, PiP and mirroring also use the example-only native
`ovxr-sample-support` library. Build it with the C++ samples and set
`OVXR_SAMPLE_SUPPORT_LIBRARY` to its absolute path for Python. Its input code
requires OpenXR and Vulkan SDK headers; for standalone builds set
`SAMPLE_OPENXR_INCLUDE_DIR` and `SAMPLE_VULKAN_INCLUDE_DIR` to their respective
SDK include directories. The projection adapter loads Vulkan from the installed
graphics driver. Desktop windows use
Win32: omit `--mirror` on Linux and use `--pip headset` or save-only mode.

## Example Projects

- **Minimal XR** — *"Populate an application-owned ovstage from USD, attach it
  to an ovrtx renderer, hand both to ovxr, and run a VR session: probe the
  OpenXR runtime, render and submit stereo frames sized to the runtime's
  recommended swapchain, and tear down cleanly."*
  Build & run in: [C →](c/minimal-xr), [Python →](python/minimal-xr)

- **Live scene** — animate a cube directly in OVStage, use application-owned
  ordinals, and publish scene changes together with XR camera state through
  `begin_frame` → `bind_frame` → `submit_frame`. Demonstrates retrying
  `NOT_READY`, reading the seal result, and cancelling an outstanding frame.
  Build & run: [C++](c/live-scene), [Python](python/live-scene).

- **Anchors and teleport** — place the tracking origin in the room, teleport to
  another position and heading, follow a moving scene prim, and reset to the
  stage origin. Demonstrates floor alignment, meter units, live anchors and
  frame-boundary activation. Includes keyboard controls and a repeatable demo.
  Build & run: [C++](c/anchors-and-teleport), [Python](python/anchors-and-teleport).

- **Foveation** — compare full-resolution rendering, application-controlled
  foveation, and runtime PQW (piecewise quadratic warp). A fine grid exposes
  peripheral resampling. Reports effective feature state and rendered readback
  dimensions after submission, including runtime fallback.
  Build & run: [C++](c/foveation), [Python](python/foveation).

- **Frame capture and PiP** — choose a separate scene camera (default) or an XR
  eye and share owned pixels between
  a small desktop PiP window, a floating scene panel, and optional BMP + JSON
  saving. Press S to save while previews continue. Demonstrates readback unwarp,
  stale-sequence detection, buffer lifetime and panel updates at explicit
  publication ordinals. The headset panel uses a 128 x 96 emissive pixel mesh;
  desktop and files use full capture resolution. Defaults to both live previews;
  `--output` alone retains the 120-frame save-only behavior.
  Build & run: [C++](c/frame-capture), [Python](python/frame-capture).

- **Runtime lifecycle** — setup diagnostics, bounded context recreation while
  preserving the renderer and application scene, and an unwarped desktop mirror.
  Use a controlled restart before testing a physical CloudXR client disconnect.
  Build & run: [C++](c/runtime-lifecycle), [Python](python/runtime-lifecycle).

- **Spatial workbench** — OpenXR controller actions or tracked hand joints,
  selection/grabbing, scene edits and undo, teleport and snap turn, and scene
  panels whose occlusion also blocks input. Desktop and XR share application
  state. Includes a synthetic input mode and machine-readable reports.
  Build & run: [C++](c/spatial-workbench), [Python](python/spatial-workbench).

- **OVUI panel** — native OVUI button, speed slider and pointer feedback on a
  spatial canvas; per-eye GPU projection and scene depth occlusion; occlusion-aware
  input, controller select or hand pinch, and a composed desktop mirror.
  Build & run: [Python](python/ovui-panel) (first version: unfoveated, opaque, single GPU).

- **Scene inspector** — select parts in XR or an OVUI object list, edit position,
  Y rotation and scale, and use shared undo/redo/reset. Desktop shortcuts update
  the same model. Includes selection outlines, depth-aware pointer routing and
  rendered-transform readback checks.
  Build & run: [Python](python/scene-inspector) (reuses the OVUI panel bridge).

- **Spatial UI** — two panels, one button each: drag their headers to reposition
  them and route clicks to the nearest visible panel. A cube demonstrates scene
  occlusion. Focuses on panel movement and multiple-panel ordering.
  Build & run: [Python](python/spatial-ui) (reuses the OVUI panel bridge).

- **AR blending** — transparent background and opaque objects, requested versus
  resolved environment blending, and RGBA alpha inspection. Stereo AR; headset
  passthrough requires runtime/client support.
  Build & run: [C++](c/ar-blending), [Python](python/ar-blending).

- **View configuration** — choose mono, stereo or AUTO; report the negotiated
  view count, runtime recommendations and actual first-view image size.
  Build & run: [C++](c/view-configuration), [Python](python/view-configuration).

- **Asynchronous capture** — copy borrowed eye pixels into owned memory and
  feed a slow worker through a one-frame pending queue; drop stale work and
  join cleanly. The source copy remains synchronous.
  Build & run: [C++](c/async-capture), [Python](python/async-capture).

## Feature coverage and validation

| Area | Samples and coverage | Remaining boundary |
| --- | --- | --- |
| Runtime + CloudXR | Minimal XR setup; runtime lifecycle recreation, retry budget, desktop mirror; official OpenXR client instructions. | Real network/client reconnect needs headset testing. |
| Spatial input | Workbench controller actions, hand joints/pinch, floor teleport, snap turn; anchors sample for live tracking origin. | Client/runtime must expose the input capability; no eye tracking. |
| App interactions | Inspector object selection, property editing and undo/redo/reset shared by desktop/XR; workbench move/rotate/grab/undo; live scene explicit publication ordinals. | No external USD edit watching or distributed multiuser synchronization. |
| Spatial UI | OVUI panel and scene inspector; spatial UI adds movable panels and nearest-panel input; per-eye color/depth composition; workbench scene panels; live camera/eye PiP. | OVUI uses opaque, world-facing, unfoveated single-GPU panels. Rotation/resizing and attached menus are separate future work. Headset PiP remains a low-resolution diagnostic mesh. No real-world passthrough occlusion. |
| Validation | GPU-free interaction/lifecycle tests, bundled-runtime frame/report/capture checks, Quest Pro + AVP evidence checklist. | Emulator checks are separate from physical headset proof, which remains pending. |

See [validation and CloudXR setup](VALIDATION.md). Tablet samples remain deferred.

The feature samples share initialization and cleanup helpers, while their
feature loops live directly in each `main.cpp` / `main.py`. Live scene and frame
capture with a headset panel use application-owned ordinals; the other feature
loops use OVXR's private ordinal mode. They show two ownership models and should
not be combined without choosing one ledger.

## Shared project setup and ownership

Start with the canonical [Python setup](../skills/project-setup-python/SKILL.md)
or [C/C++ setup](../skills/project-setup-c/SKILL.md) skill. This guide adds
sample-specific helpers and ownership examples. Paths in these documents are
relative to the files, and resolve entirely inside the OVXR public tree.
Consult the [OVXR API guide](../README.md) and
[C API contract](../include/ovxr/ovxr.h) from the same SDK revision as the
application. Match the installed Python bindings and native binaries.

### Resolve the customer's project

Use the requested language, output directory, USD scene and runtime. If the
language is unspecified, follow the existing project, or default a new prototype
to Python and state that choice. Ask for missing SDK/asset locations when they
block execution; continue preparing source that does not depend on the answer.

Create the customer's own entrypoint and build/run instructions. Reuse or adapt
the relevant example helpers where useful, keep their license notices, and
expose the customer's scene path, required prim paths and output paths as
configuration. Example prim names and room dimensions are demonstration values,
not API requirements.

If no customer scene is available, use the bundled
[room scene](assets/xr-room.usda) as an explicit temporary default.
It declares Y-up meters and `/World/StartAnchor`. Feature scenes add required
prims; retain those or author equivalents. Changing only the USD filename is
insufficient when the loop queries named prims. Read sublayers too.

Deliver source, assets or asset references, dependency/build configuration,
exact run commands, controls, assumptions, and validation results. Mark missing
hardware/SDK checks as unrun, with the actual reason.

### Use matched SDKs

Require an NVIDIA RTX-capable host and a working OpenXR runtime, plus compatible
OVXR, OVRTX and OVStage SDKs. Read version requirements from the customer's
provided packages; do not invent package versions, download URLs, credentials,
or a published OVXR wheel. Python bindings alone do not provide the native XR
renderer. Do not substitute a second USD/fabric runtime from an unrelated SDK.

For C++, start with the matching example's `CMakeLists.txt` and the public
[CMake modules](c/cmake/ovxr.cmake). They implement package
discovery and runtime placement. A standalone sample build uses this form:

```text
cmake -S <sample-or-customer-project> -B <build-dir> -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="<ovxr-sdk>;<ovrtx-sdk>;<ovstage-sdk>"
cmake --build <build-dir> --config Release
```

Resolve placeholders to the customer's actual paths. Preserve C++17 and the
`ovxr::ovxr`, `ovrtx::ovrtx`, `ovstage::ovstage` dependencies. Keep
`ovrtx_setup_runtime`, `ovstage_setup_runtime`, and `ovxr_setup_runtime` when
using those modules. Find the resulting executable from the chosen generator:
single-config and multi-config builds use different output directories.
Do not add Carbonite/private renderer APIs to the public application.

For Python, make the three matching packages importable in the chosen
interpreter, and use their documented native-library configuration.
`ovxr.OVXR_LIBRARY_PATH_HINT` can identify the intended OVXR shim or its
directory; consult the [loader section](../README.md#locating-libovxr-dynamicso)
for the full discovery contract. Resolve OVRTX/OVStage dependencies from the same
SDK family. Use `python <customer-main.py> ...` with that prepared environment;
there is no generic pip command that guarantees a complete native runtime.

A customer skill must work from supplied SDK packages; do not make Kit's private
build tree, package registry, or internal test runtime a prerequisite.

### Shared example code

The feature samples use [C++ initialization](c/common/xr_sample.h)
or [Python initialization](python/common/xr_sample.py).
If copying a feature directory, retain or deliberately relocate its sibling
`common/`, `cmake/` (C++), and shared `assets/` references. Update source and
CMake paths together and keep scene paths independent of the launching directory.

Workbench input, Windows mirroring, and PiP use the
[native sample helper](c/sample-support/CMakeLists.txt) and its
[Python bridge](python/common/sample_support.py).
Build it against the same OVStage and OVRTX SDKs. Its source currently also requires
OpenXR SDK headers 1.1.53 or newer and Vulkan SDK headers. Set
`SAMPLE_VULKAN_INCLUDE_DIR` to the Vulkan include directory and `SAMPLE_OPENXR_INCLUDE_DIR` explicitly in
a customer build. C++ consumers link/copy the helper as their example does.
Python consumers set `OVXR_SAMPLE_SUPPORT_LIBRARY` to its actual library path.
Initialize the application stage before using this bridge so its OVStage native
dependency is loaded. The helper borrows the stage/session/renderer; it owns its
queries, window, action objects, hand trackers and observer CPU copies. Destroy
the helper objects before their borrowed owners.

### Application lifetime

Read the canonical [context-creation](../skills/context-creation/SKILL.md)
and [application-flow](../skills/application-flow/SKILL.md) guidance before
implementing startup and shutdown:

- Choose telemetry policy before the first successful OVXR create. To opt out,
  set `NVIDIA_OMNIVERSE_PRODUCT_TELEMETRY_DISABLED=1` at process launch, or configure native loading
  and call `ovxr_library_telemetry_opt_out()` / `ovxr.library_telemetry_opt_out()`
  before creation. See context-creation for early loading and reporting lifetime.
- After OVRTX initialization and before OVXR creation, use
  `ovxr_set_log_callback` to route library diagnostics into the host. Follow
  application-flow and the matched public header for callback lifetime,
  thread safety and replacement rules.
- Use `ovxr_request_exit()` / `Context.request_exit()` for cooperative shutdown,
  then let the owner thread pump session events and release live plans before
  destruction. An exit request may overlap frame calls, but must not overlap
  destroy; application-flow defines the complete teardown sequence.

1. Create the OVRTX renderer and the application-owned OVStage.
2. Attach that stage in borrow mode. Populate USD at a fresh ordinal greater
   than the current write floor. Wait for population and advance/wait the write
   floor so scene data is committed before OVXR consumes it.
3. Create OVXR with both handles, `create_render_products=true`, and stereo
   VR/Vulkan as the baseline. Use `SceneInfoPolicy.AUTO` so declared stage
   `upAxis` and `metersPerUnit` win over descriptor fallbacks.
4. Choose the anchor for the customer's scene. Example helpers use
   `/World/StartAnchor`; minimal-xr places the managed anchor for its default
   scene and uses `/Render/Camera` for `--usd` scenes. Use an appropriate
   authored prim or the managed anchor. Do not assume those paths exist.
5. Let OVXR own OpenXR wait/begin/end, session events, rendering and submission.
   Do not independently render the same products each frame.
6. On shutdown, release outstanding plans and application-owned queries/input
   children while their parent stage/session still exists. Destroy OVXR before
   detaching/destroying the renderer, and destroy the stage last. Cover partial
   initialization failures as well as normal quit.

Serialize frame and stage-changing calls on each context. Queue commands from
other UI threads into that owner rather than calling the renderer concurrently.
Readback acquisition is an explicit overlapping-call exception; destruction is
exclusive and must not race even a readback consumer. Check the matching SDK's
threading contract before introducing additional concurrency.

### Pick one ordinal ownership model

| Model | When to use | Rule |
| --- | --- | --- |
| PRIVATE | The application populates once, then all live scene writes use OVXR helpers. | Pass C ordinal 0 / omit Python `stage_ordinal`; OVXR allocates and seals. |
| APPLICATION | The application writes OVStage directly while OVXR writes camera state. | Set descriptor ordinal mode to APPLICATION and coordinate one nonzero ordinal across all writers, satisfying the applicable OVStage floors and admission rules. Zero selects OVXR's private ledger. |

In APPLICATION mode, publish scene edits before `bind_frame` at that frame's
ordinal. Bind seals the transaction. Inspect the seal result even on failure:
never reuse a sealed ordinal. An unsealed retry retains the ordinal and pending
writes. Start a new frame plan after cancelling the old outstanding plan.
Pass fresh explicit ordinals to other OVXR writers and to destruction too.
Do not independently advance the write floor before bind for edits intended to
share its transaction. See the [live scene adaptation guide](python/live-scene/README.md#adapting-this-sample).

After a successful begin, cancel any still-outstanding plan on every error or
early exit. A submit may already consume the plan even when it reports failure;
use the cleared plan state to avoid double cancellation.

### Baseline validation

- First run a bounded session with foveation disabled. Count submitted frames,
  not loop iterations: SUCCESS can occur without a submission.
- Treat NOT_READY as transient, with a small retry delay and a bounded
  no-submission interval. A native call that blocks cannot be interrupted by a
  timer checked only between calls.
- Report runtime/system, actual submitted count and errors. Permit extra startup
  time for shader compilation rather than silently declaring a blocked session
  successful.
- Inspect scene scale, up axis, initial placement, stereo and head motion with the
  chosen runtime. Exit cleanly and launch again.
- Automate what is available. The
  [validation guide](VALIDATION.md) includes a hardware checklist;
  its internal emulator commands are optional and may be absent from customer
  packages. Do not manufacture headset proof or turn unsupported input into
  synthetic input without labeling it.

## Snippets and skills

Use the single [OVXR skills catalog](../skills/README.md) for SDK and application
guidance. Each sample README has an **Adapting this sample** section and keeps
request history and reconstructed prompts in an optional FYI section.
The [sample-to-skill map](#skills-by-sample) below links the shared guidance.
Provide the selected skills, adaptation guide, public source tree and matched SDK packages.
The [coverage map](../skills/COVERAGE.md) identifies associated tests and gaps.

The example sources define the snippets; the matched public SDK contracts
remain authoritative for API behavior.
Snippet regions are marked `[snippet:name]` … `[/snippet:name]`; keep the markers
intact when editing example code.

## Skills by sample

Choose the relevant canonical skills and follow the sample README adaptation
section. Use the [shared project setup](#shared-project-setup-and-ownership)
when reusing helpers or moving an example into your own application.

| Sample adaptation guide | Canonical skills |
| --- | --- |
| [minimal-xr](python/minimal-xr/README.md#adapting-this-sample) | [project-setup-python](../skills/project-setup-python/SKILL.md), [project-setup-c](../skills/project-setup-c/SKILL.md), [application-flow](../skills/application-flow/SKILL.md), [context-creation](../skills/context-creation/SKILL.md), [frame-loop](../skills/frame-loop/SKILL.md) |
| [live-scene](python/live-scene/README.md#adapting-this-sample) | [stage-publication](../skills/stage-publication/SKILL.md), [frame-loop](../skills/frame-loop/SKILL.md) |
| [anchors-and-teleport](python/anchors-and-teleport/README.md#adapting-this-sample) | [tracking-anchor-and-scale](../skills/tracking-anchor-and-scale/SKILL.md), [openxr-input-and-extensions](../skills/openxr-input-and-extensions/SKILL.md) |
| [foveation](python/foveation/README.md#adapting-this-sample) | [foveation-and-display](../skills/foveation-and-display/SKILL.md) |
| [frame-capture](python/frame-capture/README.md#adapting-this-sample) | [frame-readback](../skills/frame-readback/SKILL.md) |
| [runtime-lifecycle](python/runtime-lifecycle/README.md#adapting-this-sample) | [context-creation](../skills/context-creation/SKILL.md), [runtime-adapters](../skills/runtime-adapters/SKILL.md), [application-flow](../skills/application-flow/SKILL.md) |
| [spatial-workbench](python/spatial-workbench/README.md#adapting-this-sample) | [spatial-interaction](../skills/spatial-interaction/SKILL.md), [openxr-input-and-extensions](../skills/openxr-input-and-extensions/SKILL.md), [tracking-anchor-and-scale](../skills/tracking-anchor-and-scale/SKILL.md) |
| [ovui-panel](python/ovui-panel/README.md#adapting-this-sample) | [scene-occluded-ui](../skills/scene-occluded-ui/SKILL.md), [composition-integration](../skills/composition-integration/SKILL.md) |
| [scene-inspector](python/scene-inspector/README.md#adapting-this-sample) | [scene-occluded-ui](../skills/scene-occluded-ui/SKILL.md), [spatial-interaction](../skills/spatial-interaction/SKILL.md) |
| [spatial-ui](python/spatial-ui/README.md#adapting-this-sample) | [scene-occluded-ui](../skills/scene-occluded-ui/SKILL.md), [spatial-interaction](../skills/spatial-interaction/SKILL.md) |
| [ar-blending](python/ar-blending/README.md#adapting-this-sample) | [session-modes-and-passthrough](../skills/session-modes-and-passthrough/SKILL.md) |
| [view-configuration](python/view-configuration/README.md#adapting-this-sample) | [foveation-and-display](../skills/foveation-and-display/SKILL.md) |
| [async-capture](python/async-capture/README.md#adapting-this-sample) | [frame-readback](../skills/frame-readback/SKILL.md) |
| [composition-layers](python/composition-layers/README.md#adapting-this-sample) | [composition-integration](../skills/composition-integration/SKILL.md), [frame-loop](../skills/frame-loop/SKILL.md) |

For an adapted application, provide the chosen skills, adaptation guide, matched
SDKs, scene and output directory. Preserve the application's intended layout and
defaults. Follow [source reproduction and evaluation](VALIDATION.md#source-reproduction-and-generation-evaluation)
before claiming that a generated application reproduces a checked-in example.

## Independent overlays

[Composition layers in Python](python/composition-layers/README.md) and
[C++](c/composition-layers/README.md) append a head-fixed or world-fixed quad and
verify its Vulkan image upload. Stereo tests compare the test runtime's composed
scene-plus-quad output against shared references, alongside texture readback and
submission checks. This complements scene-occluded UI; it does not use scene
depth or claim validation of physical headset output.

OVUI Panel, Scene Inspector and Spatial UI remain Python-only because their
examples use the Python OVUI/Warp widgets, editing and interaction helpers.
Native versions of that application code have not been implemented. This is
a sample-coverage limit, not a restriction of the OVXR C API.
