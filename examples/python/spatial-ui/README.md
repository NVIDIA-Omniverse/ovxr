# Spatial UI (Python)

**App type:** stereo VR (opaque). Mono and AR are not configured by this sample. Python only; opaque UI, scene depth, no foveation. See the [compatibility matrix](../../COMPATIBILITY.md).

[Preview this sample in the gallery](../../gallery/README.md#spatial-ui).

Two small OVUI panels, each with one button and a click counter. This sample
adds only **header dragging** and **multiple-panel depth/input ordering** to
the [single-panel example](../ovui-panel/README.md).

Panel A starts in front of part of panel B. Drag A aside to reach B's button.
The orange cube also blocks part of B. Only the nearest visible panel receives
pointer events, and hidden buttons cannot be clicked through another panel or
the cube.

Create your own version with [the spatial UI skill](#adapting-this-sample).

## Run

Use the existing [OVUI panel environment](../ovui-panel/README.md#requirements-and-launch):
Windows, Python 3.12, OVUI 0.2.0, Warp 1.15.0 and the matched native SDKs.
No new packages or native build are needed beyond that bridge. Depth is required
and local/runtime foveation is disabled automatically.


For an extracted SDK, follow [project setup](../../README.md#shared-project-setup-and-ownership)
and run `python main.py --input controllers --mirror`, preserving `common/` and
`assets/`. `--input hands` uses tracked finger direction and pinch when the
client/runtime exposes `XR_EXT_hand_tracking`; it is not AVP gaze-and-pinch.

- **Select/pinch a header and drag:** move its panel left/right and up/down.
- **Release:** leave it at the new position.
- **Select/pinch a button:** increment that panel's counter.
- **R:** reset panel positions. **Q:** quit, from the console or focused mirror.

Panels keep their size, orientation and distance from the start anchor. A lost
pointer, changed source, back-facing ray, or nearer occluder cancels an active
move and restores the starting position. Release before trying again. A held
button press cannot transfer to the other panel.

## Small implementation

[main.py](main.py) contains the two buttons and XR loop. The pointer logic is
in [panel_layout.py](panel_layout.py). The existing OVUI surface, tracked-pointer
adapter and compositor are reused; there is no inspector model or undo history.

Both panels occupy halves of one 1024 × 320 OVUI canvas. Borrowed CUDA views use
the **full canvas row stride**, even though each view is only 512 pixels wide.
Keep the canvas alive until both eye compositions finish. Each region is projected
as a 1.1 × 0.6875 m world-facing panel. Both compositor calls update the same
eye depth, so the nearer panel wins. Input uses the same panel positions and
sizes, then maps the visible hit back to its region's canvas coordinates.

The scene collider is the one known size-0.6 cube in the supplied USD. Arbitrary
scene picking, rotation, resizing, transparency, multi-pointer input and attached
menus are outside this sample. GPU copies/waits remain synchronous.

## Validate

The demo clicks A, attempts a hidden B click, drags A aside, clicks the exposed
B button, then attempts a cube-blocked click. Expected final counters: **A=1,
B=1**, with **one completed panel move**.


`--report file.json` and
`--evidence directory/` also work when launching the sample directly; use
`--input demo --frames 100` for the deterministic sequence.

Headset results remain pending:
on Quest Pro, check both panels' placement, header grabbing, visible/blocked
buttons, tracking loss and the mirror. Use the [headset checklist](../../VALIDATION.md).

## Visible pointing

A magenta beam follows the same origin/direction used for input and ends at the
nearest sample collider (or the ray's range limit). The white tip marks the hit;
the beam gets thicker while select/pinch is held. Invalid or lost tracking hides
the ray. `--input demo` uses synthetic rays from a visible off-center origin.
These markers explain the interaction; they are not a separate picking system.

## Adapting this sample

**Goal:** Build a small spatial UI sample

Use the canonical skills: [scene-occluded-ui](../../../skills/scene-occluded-ui/SKILL.md), [spatial-interaction](../../../skills/spatial-interaction/SKILL.md).
See the [prompt context (FYI)](#prompt-context-fyi) and [validation coverage](../../../skills/COVERAGE.md).
The matched SDK contracts take precedence over example implementation choices.

Demonstrate only two new ideas: moving a panel by its header and choosing the
nearest visible panel for rendering/input. Start with two panels and one button
per panel. Preserve the customer's explicit choices, but do not expand the
sample into an XR application, inspector, navigation system or tool suite.

### Read and reuse

- [Sample launch and limits](README.md).
- [Two buttons and frame loop](main.py), including
  the `two-panel-composition` snippet.
- [Panel picking and dragging](panel_layout.py),
  including `nearest-panel-input`, and [matching scene](../../assets/spatial-ui.usda).
- [OVUI panel adaptation guide](../ovui-panel/README.md#adapting-this-sample) for setup, GPU lifetime, stereo
  composition and teardown rules. Reuse its existing surface/compositor.

### Keep the adaptation focused

Change the labels, colors and starting panel positions to suit the customer.
Keep a single counter/button on each panel to make input ownership observable.
The reference uses translation in X/Y at fixed Z, orientation and dimensions.
Do not imply that it implements rotation, resizing or controller/wrist attachment.
Those can be separate demonstrations after this one works.

Build both panels in one OVUI canvas, rather than initializing two standalone
OVUI backends. Reuse `PanelSurface.horizontal_region` for borrowed GPU views:
the pointer starts at the region's column offset, while its row stride remains
the width of the **whole canvas**. Retain the owner through both eye compositions.
Changing row stride to the region width mixes the two panels' pixels.

Compose both regions into the same eye color/depth buffers using each eye's pose
and FOV. Preserve the existing reverse-Z comparison and update depth alongside
color. With opaque panels at distinct depths, nearest pixels must win in either
draw order. Require depth, disable foveation and keep one GPU as in the base skill.

For input, compare panel-plane distances and the scene occluder before injecting
OVUI mouse events. Convert the chosen panel's local pixels to its atlas offset.
Route clicks only to that panel. Capture a button press without letting it
transfer to another panel while held. A header grab keeps its initial hit offset
and moves the panel on its own fixed-depth plane. Restore its starting position
if tracking/source is lost, the ray turns away, or nearer geometry blocks the
grab; require release to rearm. Reset positions without introducing scene tools.

The analytic collider matches only the bundled cube. If the customer substitutes
geometry, provide matching picking and keep the rendering/picking coordinates
consistent. Do not promise general USD picking from this example.

### Verify and deliver

Inspect the rendered OVUI canvas before interaction; use actual mouse events
derived from rays, with before/after screenshots. Confirm that both buttons work
when visible and neither receives a click through a nearer panel or the cube.
Check header movement, offset preservation, cancellation and release-to-rearm.
Use distinct content on each panel to check atlas row stride, and reverse their
composition order to verify that the nearer panel still wins.

Run `--input demo --frames 100` on the available runtime to check A=1, B=1 and one
completed move, plus stereo captures before/after moving A. Synthetic input is
not headset evidence. Keep Quest Pro testing with the official OpenXR CloudXR
client separate, and report any input capability or platform restrictions.
Deliver the small sample, launch command, linked helpers/scene, validation results
and remaining limits. Follow the customer's local commit and push preferences.

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

> ok let's build this spatial UI sample then later the controller/wrist-attached menu

This is historical context, not a complete generation transcript or a claim
that this request alone produced the current code. The implementation evolved
through follow-up requests, review and test fixes.

### Reconstruction prompt

Written on 2026-10-06 to describe the current example; it is not the original
verbatim generation prompt. Language coverage: Python.
Use the canonical skills: [scene-occluded-ui](../../../skills/scene-occluded-ui/SKILL.md), [spatial-interaction](../../../skills/spatial-interaction/SKILL.md), plus the language's project-setup skill.

```text
Create an application demonstrating spatial-ui using the matched OVXR, OVRTX and OVStage SDKs
provided with this request. Read the listed skills and the linked sample recipe.
Keep the example small: two world panels, header dragging and nearest-visible-panel input. Preserve the grab offset and cancel interrupted gestures. Demonstrate overlapping panels, moving one aside and foreground scene occlusion.
Use the recipe's bundled scene and controls as the baseline, keeping all shared
helpers and asset references coherent. Do not change the SDK APIs or weaken the
tests to accommodate the result. Provide build/run commands and report actual
validation results separately from unrun hardware checks.
```

Read the [spatial-ui adaptation guide](#adapting-this-sample) for source, snippet regions
and the precise feature contract. The adaptation guide links the checked-in implementation;
using it is reference-assisted adaptation, not a blind skill-generation evaluation.
For exact bytes or independent generation checks, follow
[reproduction and evaluation](../../VALIDATION.md#source-reproduction-and-generation-evaluation). No independent generation pass is
claimed by this prompt record.

</details>
