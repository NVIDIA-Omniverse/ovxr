# Scene inspector (Python)

**App type:** stereo VR (opaque). Mono and AR are not configured by this sample. Python only; opaque UI, scene depth, no foveation. See the [compatibility matrix](../../COMPATIBILITY.md).

[Preview this sample in the gallery](../../gallery/README.md#scene-inspector).

Select either part with an XR ray or the OVUI object list. Edit its XYZ position,
Y-axis rotation and uniform scale, then undo, redo or reset it. A green cage marks
the selected part; a small sphere marks the ray's scene hit. The object list,
property sliders, desktop shortcuts and scene publication share one model.

The 1024 × 640 OVUI canvas occupies a 1.8 × 1.125 m panel. Both eyes and the
desktop mirror receive the same depth-composited UI. Moving a part in front of
the panel hides its pixels and blocks pointer input to the covered controls.

Create your own version with [the scene inspector skill](#adapting-this-sample).

## Run

Use the [OVUI panel environment and native build](../ovui-panel/README.md#requirements-and-launch):
Windows, Python 3.12, OVUI 0.2.0, Warp 1.15.0, NVIDIA RTX GPUs, matched
OVRTX/OVStage SDKs and an OVXR payload with `plan.step_result` and `additional_layers`. The inspector
reuses that sample's requirements file and venv; no additional packages or native
bridge code are needed. The sample automatically requires depth and disables
local/runtime foveation.


For an extracted SDK, follow
[project setup](../../README.md#shared-project-setup-and-ownership), keep the sibling
`common/` and `assets/` folders, and run `python main.py --input controllers --mirror`.

Use `--input hands` only when the client/runtime exposes `XR_EXT_hand_tracking`.
This mode uses an index-finger ray and thumb/index pinch, not AVP gaze-and-pinch.
Controller aim or finger direction uses OVXR's resolved scene-anchor conversion.

| Input | Action |
| --- | --- |
| Point at a part and select/pinch | Select it and populate its properties. |
| Select an object-list button | Select the same scene object. |
| Drag a slider | Preview an edit; release commits one undo step. |
| Undo / Redo / Reset | Traverse shared history / restore the selected part's initial transform. Reset is undoable. |
| Click empty space | Clear selection. The panel blocks selection of objects behind it. |
| Desktop 1 / 2 | Select blue / amber. |
| Desktop A / D | Move the selected part 0.1 m along X. |
| Desktop U / Y / R | Undo / redo / reset. |
| Q or Ctrl+C | Exit. `--frames N` limits submitted frames. |

Desktop keys work in the focused mirror or launching console. Keyboard edits
cancel an active XR drag first. Losing tracking/focus, switching pointer source,
or hiding a dragged control behind a part rolls the gesture back. Release before
pressing again. Drag capture continues beyond the canvas edge while its plane
remains in front of the pointer and unobstructed.

## Structure and scope

- [Controls](controls.py) uses atomic OVUI widgets and stable value models.
  Programmatic refresh suppresses callbacks, so selecting an object or undoing
  an edit cannot create another edit command.
- [Application model and picking](../common/inspector_model.py) owns transforms,
  selection, gesture transactions and history. The analytic boxes account for
  yaw and uniform scale. Decorative outline bars and the hit marker are not
  pick targets; the floor is not selectable.
- [Frame loop](main.py) publishes the model through `update_prim_transforms`
  before bind. Each drawable cage bar receives its composed world transform;
  the live transform API does not promise subtree animation from a parent write.
- The existing [OVUI surface](../common/ovui_panel.py),
  [GPU compositor](../common/panel_compositor.py) and
  [tracked pointer](../common/xr_pointer.py) supply the shared bridge.

This is an in-memory inspector for two known scene objects, not a general USD
stage browser. It does not watch external USD edits, save a layer, synchronize
multiple processes, or provide arbitrary-mesh picking. Replace the scene adapter
and colliders for customer geometry. Property ranges keep the supplied scene
usable. The panel is opaque, world-aligned and unfoveated; the shared bridge uses
synchronous GPU copies and is not a latency benchmark.

## Validation

`--input demo --frames 100 --report result.json --evidence output/` sends
synthetic rays through the actual OVUI mouse-event path. It selects both objects,
drags height and yaw, switches selection through the list, and exercises undo,
reset and redo. Before/after canvas images accompany the interaction checkpoints.
With a report enabled, each checkpoint reads back the two objects and twelve cage
bars from OVStage **after submission** and compares them with the model. Native
OVUI value models are also checked against the selected object.


The report retains `headset_verified: false`.

Quest Pro testing is pending. Check selection/outline agreement, all five sliders,
undo/redo/reset, desktop/XR synchronization, readable stereo text, occluded input,
and tracking/focus loss during a drag with the official OpenXR CloudXR client.
Record those results using the [headset checklist](../../VALIDATION.md).

## Visible pointing

A magenta beam follows the same origin/direction used for input and ends at the
nearest sample collider (or the ray's range limit). The white tip marks the hit;
the beam gets thicker while select/pinch is held. Invalid or lost tracking hides
the ray. `--input demo` uses synthetic rays from a visible off-center origin.
These markers explain the interaction; they are not a separate picking system.

## Adapting this sample

**Goal:** Build an OVUI scene inspector

Use the canonical skills: [scene-occluded-ui](../../../skills/scene-occluded-ui/SKILL.md), [spatial-interaction](../../../skills/spatial-interaction/SKILL.md).
See the [prompt context (FYI)](#prompt-context-fyi) and [validation coverage](../../../skills/COVERAGE.md).
The matched SDK contracts take precedence over example implementation choices.

Create the customer's inspector with one application model shared by XR input,
OVUI controls, desktop commands and scene publication. Start with the Python
sample and its existing GPU bridge. Keep the customer's object paths, scene,
units, properties and layout choices; ask only for missing decisions that change
the implementation materially.

### Read and reuse

- [Project setup](../../README.md#shared-project-setup-and-ownership) and
  [inspector launch, controls and limits](README.md).
- [OVUI panel adaptation guide](../ovui-panel/README.md#adapting-this-sample) for the surface, per-eye color/depth
  projection composition, foveation restrictions, GPU ownership and teardown contracts.
- [Inspector widgets](controls.py), snippet
  `inspector-widgets`, and [application loop](main.py),
  snippet `inspector-frame-loop`.
- [Command model, transformed picking and pointer router](../common/inspector_model.py),
  [controller/hand conversion](../common/xr_pointer.py) and
  [example scene](../../assets/scene-inspector.usda).

### Adapt the inspector

Use atomic OVUI buttons, labels and value models for the property editor. Keep
widget/value-model identities stable. Replace `SceneModel.defaults`, object
labels and property limits together with the USD scene adapter. For a larger
scene, replace the fixed list with a hierarchy model; do not claim that these
two buttons implement a general stage browser.

Treat the application model as the source of truth. Route selection, XR property
edits, desktop shortcuts, undo, redo and reset through the same commands. Refresh
OVUI from model revisions and suppress callbacks during refresh. Scene publication
reads that same state before bind; it must not read stale widget values instead.
The sample uses private OVXR ordinals. Direct OVStage writes or additional
writers require a deliberate switch to one application-owned ordinal ledger;
consult [live scene](../live-scene/README.md#adapting-this-sample) before combining those patterns.

Keep one transform snapshot at gesture start. Preview edits while held; release
commits one history entry. Tracking/focus loss, source changes and newly blocking
geometry cancel and restore the snapshot. Require release before rearming. A
selection change must resolve the old gesture before targeting another object.
Undo/redo select the object affected by the command, so its properties are visible.
Reset restores the selected object's initial transform and is itself undoable.

Compare the nearest object hit with the panel-plane hit. A nearer part blocks
panel input; the visible panel blocks objects behind it. Preserve panel drag
capture outside its rectangle, but cancel if the ray no longer faces the plane
or scene geometry blocks it. A held scene click must not become a new panel drag
or select a second object as the pointer moves. Use the same scene coordinates,
dimensions and transforms for drawing and hit testing. The sample's size-0.6
boxes support Y rotation and uniform scale; arbitrary customer geometry needs
matching collision/picking, not a bounding box presented as exact mesh picking.

`update_prim_transforms` takes **world** matrices. Compose each drawable's world
pose explicitly, including selection decorations; do not assume writing an
authored parent animates all its descendants. Keep decorative pointer/outline
geometry out of object picking. Use a separate adapter if external USD edits,
layer persistence or distributed state are requested; none is implemented by
the in-memory sample.

### Verify observable behavior

Exercise drag history, transformed picking, source/focus loss, occlusion,
pointer capture and shared desktop commands in the adapted application. Check
normal startup, non-rendering startup, failed bind and resource cleanup when
changing the loop.

Render the real OVUI surface and inspect it before interaction. Use mouse events
derived from synthetic XR rays to select and drag; capture before/after images
and inspect the results. Do not substitute direct callback calls for UI evidence.
Compare OVUI value models with application state. Read the rendered world
transforms back from the sealed stage after submission, as `verify_scene` does,
and compare every matrix element, including selection decorations. Perform
these readbacks after the eye compositions.

Use the demo for deterministic emulator checks and keep synthetic input separate
from tracked hardware counts. Inspect both eyes and the mirror for correct
selection, transformed geometry and depth ordering. On Quest Pro/AVP with the
official OpenXR CloudXR client, separately verify actual input, readable stereo
UI, focus loss and every property control. Joint-based hands are not AVP
gaze-and-pinch. Leave physical checks pending until a tester supplies evidence.

Deliver the adapted application, required scene/helpers, reproducible launch,
test results, evidence paths and remaining limits. Preserve the customer's
commit/push preferences. The skill and its linked sources work with any coding
assistant able to read the OVXR tree; no particular LLM service is required.

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

Recovered request excerpt (2026-09-29):

> ok propose how we could create samples to show more XR features

This is historical context, not a complete generation transcript or a claim
that this request alone produced the current code. The implementation evolved
through follow-up requests, review and test fixes.

### Reconstruction prompt

Written on 2026-10-06 to describe the current example; it is not the original
verbatim generation prompt. Language coverage: Python.
Use the canonical skills: [scene-occluded-ui](../../../skills/scene-occluded-ui/SKILL.md), [spatial-interaction](../../../skills/spatial-interaction/SKILL.md), plus the language's project-setup skill.

```text
Create an application demonstrating scene-inspector using the matched OVXR, OVRTX and OVStage SDKs
provided with this request. Read the listed skills and the linked sample recipe.
Select scene objects and edit their transforms through real OVUI controls. Share selection and undo/redo with desktop commands, make each drag one undoable transaction, and block selection or UI input behind nearer geometry.
Use the recipe's bundled scene and controls as the baseline, keeping all shared
helpers and asset references coherent. Do not change the SDK APIs or weaken the
tests to accommodate the result. Provide build/run commands and report actual
validation results separately from unrun hardware checks.
```

Read the [scene-inspector adaptation guide](#adapting-this-sample) for source, snippet regions
and the precise feature contract. The adaptation guide links the checked-in implementation;
using it is reference-assisted adaptation, not a blind skill-generation evaluation.
For exact bytes or independent generation checks, follow
[reproduction and evaluation](../../VALIDATION.md#source-reproduction-and-generation-evaluation). No independent generation pass is
claimed by this prompt record.

</details>
