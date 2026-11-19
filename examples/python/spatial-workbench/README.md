# Spatial workbench (Python)

**App type:** stereo VR (opaque). Mono and AR are not configured by this sample. See the [compatibility matrix](../../COMPATIBILITY.md).

[Preview this sample in the gallery](../../gallery/README.md#spatial-workbench).

See [Adapting this sample](#adapting-this-sample) for guidance on using the
canonical skills in your application.

A shared application model drives an editable cube, four world-space buttons,
undo, and a movable tracking origin. Desktop keys and tracked input dispatch the
same commands. The mirror displays the rendered state and a revision in its title.

After [environment setup](../../README.md#running-samples), run from this sample directory:
```powershell
# Quest Pro: controller aim and trigger, thumbstick snap turn.
python main.py --input controllers --mirror
# AVP: requires XR_EXT_hand_tracking from the runtime/client.
python main.py --input hands --mirror
# Synthetic emulator sequence; never counts as headset proof.
python main.py --input demo --frames 100 --mirror --report "$PWD\workbench-python.json"
```

Use the [official-client setup and Quest Pro / AVP checklist](../../VALIDATION.md).
Both hardware targets are pending user validation.

| Input | Behavior |
| --- | --- |
| Buttons with 1 / 2 / 3 / 4 bars, or keys 1 / 2 / 3 / 4 | Move cube left / right, rotate 22.5 degrees, undo. |
| Aim and hold trigger on cube | Grab and move at its original ray distance; release to drop. |
| Point index finger and pinch thumb/index | Hand-mode equivalent of trigger selection/grab. |
| Select unobstructed floor within five meters of scene origin on X/Z | Move the tracking origin to that floor point at the next frame boundary. |
| Thumbstick X, or Z / X | Snap turn 30 degrees, rearming after the stick returns to center. |
| O | Hide/show the violet pillar occluding button 4 from the initial viewpoint. |
| R | Reset cube position and rotation; undoable. |
| Q / Ctrl+C | Exit. |

The right located controller/hand is preferred; the left is the fallback.
Controllers bind Simple and Oculus Touch profiles through OpenXR actions.
Hand mode requests `XR_EXT_hand_tracking`, displays its 26 joints per hand and
derives a ray from the index proximal joint toward the index tip. Pinch uses
2.5 cm press / 4 cm release thresholds. This is a deliberately simple finger
gesture, not a platform gaze-and-pinch interaction. Unsupported hand tracking
fails clearly; it never silently substitutes demo input.

`c/sample-support/input.cpp` contains the shared native OpenXR code used by
both languages. It borrows OVXR's instance/session/reference space, attaches
one action set, and owns only action spaces/sets or hand trackers. It never
polls session events or begins/ends an XR frame. Read input at predicted display
time, then map poses **after begin_frame** once a renderable bind has resolved
the initial anchor. This release latches anchor updates at bind: navigation is
queued after submission and tracked markers are hidden for the transition frame
so old input mapping is never displayed against the new eye mapping.
With a stable anchor, eyes, pointers and joint markers agree. Check focus and location validity
before using a pose. Input loss, a different selected hand, or a session pause
cancels a grab; regaining input while held requires release and a fresh press.

Panels are scene geometry composed with the cube and room in the projection
image. The cube, panel, buttons and violet occluder share nearest-hit analytical
picking, so the occluder blocks button clicks as well as rendering. O exposes the
undo button; undoing O restores the occluder. These are illustrative colliders,
not general USD scene picking. The depth submission policy is AUTO and the report
records its effective enablement after frames have been submitted. Scene
occlusion works even when a runtime declines depth submission.

OVXR also accepts application-owned OpenXR composition layers at submit.
This sample covers scene panel layout and occlusion, not compositor layers or
real-world passthrough occlusion. State synchronization is one in-process model
shared by desktop/XR controls, not a distributed multiuser protocol. Teleport
places the tracking origin, retaining the user's physical offset in that space.

Demo rays exercise move, rotate, a blocked click, dragging and floor navigation.
Reports separate synthetic mode from active controller/hand frame counts and
always leave `headset_verified: false`. Use `--no-foveation` for a full-resolution
baseline. `--frames N` counts submitted frames and fails if stopped early.
The diagnostic mirror is Windows-only and performs a synchronous CPU copy
every third frame; omit it for timing measurements or Linux.

Build the C++ samples first. Set `OVXR_SAMPLE_SUPPORT_LIBRARY` to that library's absolute path; it supplies OpenXR input and the optional window, while Python owns scene state and the render loop.

## Visible pointing

A magenta beam follows the same origin/direction used for input and ends at the
nearest sample collider (or the ray's range limit). The white tip marks the hit;
the beam gets thicker while select/pinch is held. Invalid or lost tracking hides
the ray. `--input demo` uses synthetic rays from a visible off-center origin.
These markers explain the interaction; they are not a separate picking system.

Mint green identifies hands/controllers; cyan buttons have white markings and a
light ivory backing panel that contrasts with their cast shadows. The violet
occluder is distinct from the amber cube and magenta ray.

Both demo and hands modes draw 26 small joint spheres, 24 connecting cylinders
and a rounded palm per hand. Cylinders join valid joint positions; the palm is
a flattened sphere oriented and sized from the wrist and index/little knuckles.
Missing joints hide their attached segments; missing or degenerate palm points
hide the palm. No joint orientation or external hand mesh is required.

Demo mode uses illustrative finger proportions and an animated thumb/index
pinch. The right index tip is the pointing ray's origin. Hands mode instead uses
valid tracked OpenXR positions. Synthetic hands do not increment tracked-hand
evidence and do not prove headset support.

## Adapting this sample

**Goal:** Create a spatial XR workbench

Use the canonical skills: [spatial-interaction](../../../skills/spatial-interaction/SKILL.md), [openxr-input-and-extensions](../../../skills/openxr-input-and-extensions/SKILL.md), [tracking-anchor-and-scale](../../../skills/tracking-anchor-and-scale/SKILL.md).
See the [prompt context (FYI)](#prompt-context-fyi) and [validation coverage](../../../skills/COVERAGE.md).
The matched SDK contracts take precedence over example implementation choices.

Build the customer's own small spatial tool application with editable scene
objects, world-space controls and one model for desktop and XR actions. Choose
controllers, hands or an explicitly labeled synthetic demo according to the
request and runtime capabilities.

### Read first

Read [customer project setup](../../README.md#shared-project-setup-and-ownership), then:

- [Python loop](main.py),
  [C++ loop](../../c/spatial-workbench/main.cpp), and
  [CMake project](../../c/spatial-workbench/CMakeLists.txt).
- [Python model](../common/workbench_model.py),
  [C++ model](../../c/common/workbench_model.h), and
  [scene](../../assets/spatial-workbench.usda).
- [Native OpenXR input](../../c/sample-support/input.cpp),
  [bridge interface](../../c/common/sample_support.h), and
  [Python bridge](../common/sample_support.py).
- [Behavior and controls](README.md).

### Define the customer's workbench

Choose editable prims, tools, panel placement, input mode and navigation bounds.
The stock controls move/rotate a cube, undo, toggle an occluder, teleport and snap
turn. Replace those commands with the requested tools while keeping one command
model and history shared by desktop keys and XR selections.

The sample assumes a Y-up, meter-sized room and analytical colliders for its
known objects. Adapt transforms, collider geometry and movement bounds together
for the customer's scene. Those colliders are not a general USD picking API.
State shared by two input paths in one process is not distributed multiuser
synchronization.

### Create and own input correctly

OVXR owns the OpenXR instance/session/reference space and frame lifecycle.
Obtain borrowed handles through `get_openxr_handles` /
`ovxr_get_openxr_handles` once available. Resolve entry points through the
returned `xr_get_instance_proc_addr`; do not create a competing session or call
wait/begin/end-frame or session event polling yourself.

Reuse the native helper or implement its public OpenXR calls in the customer's
input layer:

- Controller mode creates aim-pose, select and navigation actions, suggests
  Simple/Touch bindings, attaches the action set once per session and owns its
  action spaces. Use active/focus/location flags before accepting a pose.
- Hand mode requests `XR_EXT_hand_tracking` in the OVXR descriptor before
  creation, then owns hand trackers and reads valid joint positions. The sample
  uses 26 joints per hand, an index-finger ray and thumb/index pinch hysteresis.
  Position validity does not imply a valid orientation.
- The sample's pinch thresholds are 2.5 cm to press and 4 cm to release. When
  measuring mapped scene positions, convert physical thresholds to stage units
  and account for a scaled anchor where applicable.
- Hand mode is not AVP's platform gaze-and-pinch interaction. Missing hand
  support must produce an explicit unsupported result, not an unlabeled
  controller or synthetic fallback.

Destroy owned input children before their borrowed session; recreate them after
any context replacement. Python can use the provided native bridge instead of
duplicating OpenXR ABI structs.

### Keep pointers and eyes in the same space

1. Begin the OVXR frame and ensure the initial anchor mapping is ready. During
   startup, allow the first renderable bind to initialize it before mapping input.
2. Locate input relative to the returned `xr_space`, using the plan's predicted
   display time. Map valid poses with `transform_openxr_pose` /
   `ovxr_transform_openxr_pose` after begin. Use the SDK helper rather than
   adding guessed scene offsets.
3. Prefer one located hand/controller consistently and reset interaction on
   source changes. Loss of focus/tracking cancels grabs; reacquisition while
   still pressed requires release and a new press.
4. Resolve the nearest ray hit across selectable targets, panel backgrounds and
   occluders. A visually occluded button must not receive input through the
   blocking object. Begin a grab at its original ray distance to avoid a jump.
5. Dispatch commands into one model, update rendered transforms from it, bind
   and submit. The reference uses PRIVATE ordinals and
   `update_prim_transforms` for every live scene write.
6. Queue navigation after submission so it applies at the next boundary. The
   reference hides tracked markers/skips input for the anchor-transition frame;
   do not display an old input mapping against newly anchored eyes.
7. Snap turn once per threshold crossing and rearm after the stick centers.
   Teleport moves the tracking origin and retains physical room-scale offset.
   Use the [anchor adaptation guide](../anchors-and-teleport/README.md#adapting-this-sample) for exact
   standing-point placement.

### Compose panels and synchronize tools

Author panels/buttons as USD scene geometry. Update collision bounds with panel
layout. Request depth submission with AUTO if useful and report its effective
state after submission; geometric depth ordering does not depend on runtime
accepting a depth layer.

This scene-geometry path does not establish real-world passthrough occlusion.
For independent compositor quads, see the [composition-layer adaptation guide](../composition-layers/README.md#adapting-this-sample).
Use the
[PiP adaptation guide](../frame-capture/README.md#adapting-this-sample) for a captured-image panel, preserving
its resolution limit and APPLICATION ordinal requirement when combining it with
this loop.

Add an optional desktop mirror and keys that dispatch the same commands.
Maintain revision/history in the model so undo produces the same state from
either input path. Keep synthetic demo input and hardware-active counters
distinct in the report.

### Validate and hand off

First run the deterministic demo and model checks: move, rotate, undo, grab
without jumping, an occluded selection, navigation and cleanup. Include focus/
tracking loss, source switching and held-input reacquisition. The sample demo
exercises at least four commands, a blocked click and a navigation command;
a custom demo should exercise the customer's corresponding tools.

Then check the selected actual input mode. Zero active controller/hand frames
only proves polling/setup, not interaction. Verify stereo panel depth, pointer
alignment and physical gestures on the target headset when available. Use the
official OpenXR client with CloudXR and record unsupported capabilities clearly.

Return the customer app and model, scene/prim/collider mapping, controls,
native-helper build/run instructions, model/emulator results and pending
hardware checks.

### App type and preview

Read the [per-sample compatibility matrix](../../COMPATIBILITY.md) and
[gallery](../../gallery/README.md) before choosing display/session modes.
The shipped sample uses stereo VR; a mono mirror/feed does not make it a mono XR
app. Do not claim AR, passthrough, hand tracking or physical headset validation
from emulator captures. Preserve these distinctions in the customer's README.

Keep the visible beam tied to the same ray and nearest-hit colliders as input;
hide it on tracking loss. Reuse [pointer visuals](../common/pointer_visuals.py)
for the shared math. Demo ray origins should be off-center so the beam is visible.
Use distinct colors for input, controls and scene geometry: the reference has
mint hands/controllers, a magenta ray with a white hit marker, cyan controls,
a violet occluder and an amber cube. Preserve the shapes and button markings
as cues alongside color.
Workbench's hands use 26 joint spheres, 24 connecting cylinders and a rounded
palm. Both demo and tracked input share the same hand-visual helper. Orient unit
Z cylinders between valid joint pairs; derive the ellipsoid palm frame from
wrist 1 and index/little knuckles 7/22, using positions only. Missing endpoints
hide a segment; missing/degenerate palm points hide the palm. Check that joints,
connecting cylinders and the palm stay aligned as the hand moves, and that
invalid joints hide the affected segments or palm.
Tracked pinch uses OpenXR thumb tip 5, index proximal 7 and index tip 10. Synthetic joints are never
tracked-hand evidence. Preview motion using genuine emulator frame sequences.

## Prompt context (FYI)

<details>
<summary>Request history and reconstructed prompt</summary>

References to a recipe in the recorded prompt now correspond to the
[adaptation guidance above](#adapting-this-sample).

### Provenance

Recovered request excerpt (2026-10-01):

> could spatial workbench show fake hands/fingers using spheres?

This is historical context, not a complete generation transcript or a claim
that this request alone produced the current code. The implementation evolved
through follow-up requests, review and test fixes.

### Reconstruction prompt

Written on 2026-10-06 to describe the current example; it is not the original
verbatim generation prompt. Language coverage: Python and C++.
Use the canonical skills: [spatial-interaction](../../../skills/spatial-interaction/SKILL.md), [openxr-input-and-extensions](../../../skills/openxr-input-and-extensions/SKILL.md), [tracking-anchor-and-scale](../../../skills/tracking-anchor-and-scale/SKILL.md), plus the language's project-setup skill.

```text
Create an application demonstrating spatial-workbench using the matched OVXR, OVRTX and OVStage SDKs
provided with this request. Read the listed skills and the linked sample recipe.
Build a small shared command model for XR and desktop controls with undo/redo, grabbing, teleport/reset and occluded scene buttons. Draw the same pointing beam used for picking and articulated synthetic hands for deterministic demo input; label that input synthetic.
Use the recipe's bundled scene and controls as the baseline, keeping all shared
helpers and asset references coherent. Do not change the SDK APIs or weaken the
tests to accommodate the result. Provide build/run commands and report actual
validation results separately from unrun hardware checks.
```

Read the [spatial-workbench adaptation guide](#adapting-this-sample) for source, snippet regions
and the precise feature contract. The adaptation guide links the checked-in implementation;
using it is reference-assisted adaptation, not a blind skill-generation evaluation.
For exact bytes or independent generation checks, follow
[reproduction and evaluation](../../VALIDATION.md#source-reproduction-and-generation-evaluation). No independent generation pass is
claimed by this prompt record.

</details>
