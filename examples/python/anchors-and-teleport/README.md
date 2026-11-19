# Anchors and teleport — Python

**App type:** stereo VR (opaque). Mono and AR are not configured by this sample. See the [compatibility matrix](../../COMPATIBILITY.md).

[Preview this sample in the gallery](../../gallery/README.md#anchors-and-teleport).

See [Adapting this sample](#adapting-this-sample) for guidance on using the
canonical skills in your application.

Explore how a physical tracking space maps into a Y-up room authored in meters.
The initial origin is the blue platform at `(0, 0, 3)`, looking along -Z.
The [C++ sample](../../c/anchors-and-teleport) exposes the same controls.

After [environment setup](../../README.md#running-samples), run from this sample directory:

```powershell
python main.py
python main.py --demo --demo-interval 90 --frames 360
```

See the [gallery](../../README.md) for environment setup and build instructions.
With matching SDKs already installed, run `python main.py` from this directory;
retain the sibling `common/` folder and `../../assets/`.

## Controls and APIs

Keep keyboard focus in the launching terminal; press keys without Enter.

| Key | Visible behavior | API |
| --- | --- | --- |
| `1` | Return to the blue start position and original heading | `set_managed_anchor_transform` |
| `2` | Teleport to the orange pad at `(3, 0, 1)`, turn 90 degrees | `set_managed_anchor_transform` |
| `3` | Follow a platform moving slowly sideways | `set_anchor_prim`, then `update_prim_transforms` |
| `R` | Put the tracking origin at the stage origin | `clear_anchor` |
| `Q` / Ctrl+C | Exit and release the XR session | normal teardown |

`--demo` cycles through `1`, `2`, `3`, `R` every `--demo-interval` submitted
frames (default 180). `--frames 0` runs until stopped; a positive value bounds
the run. `--no-foveation` disables both local and runtime foveation.

## What the code explains

- The anchor maps the **tracking origin**, not the exact head position. Physical
  head height and room-scale offsets remain part of the final view. `R` resets
  the application mapping; it does not ask the runtime to recenter tracking.
- Floor alignment preserves translation and heading about the scene's up axis.
  A STAGE/LOCAL_FLOOR reference space supplies a physical floor; LOCAL does not
  guarantee one. Unit scale and up axis come from scene metadata.
- Anchor selection takes effect at a frame boundary. Controls start after the
  first submitted frame has initialized the managed anchor.
- A selected prim is a **live** anchor. Publishing `/World/MovingRig` before
  the next frame makes its transform visible without selecting it again.
- After initial population, all writes go through OVXR with private ordinals.
  The [live-scene example](../live-scene) shows explicit application ownership
  when the application also writes directly to OVStage.

The movement is scripted at 1/90 second per submitted frame. This is an API
demonstration, not a controller locomotion or collision/navigation system.

## Adapting this sample

**Goal:** Create anchored XR navigation

Use the canonical skills: [tracking-anchor-and-scale](../../../skills/tracking-anchor-and-scale/SKILL.md), [openxr-input-and-extensions](../../../skills/openxr-input-and-extensions/SKILL.md).
See the [prompt context (FYI)](#prompt-context-fyi) and [validation coverage](../../../skills/COVERAGE.md).
The matched SDK contracts take precedence over example implementation choices.

Build a customer application that places the tracking origin, selects a teleport
destination, follows a moving scene prim and resets. Keep navigation policy in
the application; OVXR supplies the tracking-to-scene mapping.

### Read first

Read [project setup](../../README.md#shared-project-setup-and-ownership), then:

- [Python main](main.py) and
  [C++ main](../../c/anchors-and-teleport/main.cpp), regions
  `select-anchor` and `live-anchor`.
- [Python guide](README.md),
  [C++ guide](../../c/anchors-and-teleport/README.md), and
  [CMake project](../../c/anchors-and-teleport/CMakeLists.txt).
- The [anchor and pose mapping contract](../../../README.md#runtime-anchors) and
  [room asset](../../assets/xr-room.usda).

### Resolve the customer's navigation model

Identify scene units/up-axis, initial anchor, target pads or destinations and
the prim to follow. Decide whether a destination means the tracking origin or
the user's exact standing/head point. Default to the sample's tracking-origin
behavior and state that physical room-scale offsets remain.

The sample's transform helper assumes Y-up meters. For another scene, use its
resolved scene metadata and construct transforms in stage units. Do not reuse
the room's coordinates, pitch/heading convention or meter-sized movement
unchanged in a centimeter or Z-up scene.

### Implement the behavior

1. Load a scene containing the customer's anchor/rig prims. Use PRIVATE ordinals
   if all live writes go through OVXR helpers after population.
2. Wait for the initial renderable bind to resolve the anchor. The sample starts
   controls after its first submission. A NOT_READY anchor query during startup
   is not a usable mapping.
3. Implement origin placement with `set_managed_anchor_transform` /
   `ovxr_set_managed_anchor_transform`. Translation is in elements 12..14 of
   a row-major row-vector matrix. Validate the desired pose before publishing.
4. Implement live following with `set_anchor_prim` /
   `ovxr_set_anchor_prim` once when selected. Publish the rig's changing
   transform through `update_prim_transforms` /
   `ovxr_update_prim_transforms` before the next frame reads it.
   Do not reselect the same live path every frame.
5. Reset with `clear_anchor` / `ovxr_clear_anchor`. This selects the managed
   identity anchor; it does not request OpenXR hardware recentering.
6. Apply and observe changes at frame boundaries. Selection itself writes no
   stage data; a newly authored application anchor must be committed and have
   a current world matrix before selection/boundary reads.
7. Provide keys, buttons or application commands for these actions and a finite,
   repeatable demo sequence. Check results and return failure on API errors.

### Preserve mapping semantics

Floor alignment retains translation and heading while discarding authored tilt
and scale. Keep it for ordinary floor navigation and camera anchors. Full rig
rotation/positive uniform scale requires an intentional floor-alignment opt-out
and a compatible stage-space anchor; reject shear, reflection and nonuniform
scale. An OVXR-driven eye camera cannot itself be the anchor.

For exact standing-point placement, locate a tracked pose relative to OVXR's
returned reference space and use the documented row-vector mapping:
`P_scene = P * U * B * A`. Here U converts translation from meters into stage
units, B changes the up-axis basis, and A is the anchor. Build the rigid mapped
pose M from `P * U * B`, then solve `A = inverse(M) * X` for the desired scene
pose X. A floor teleport usually flattens the tracked pose to a standing point
with heading first. STAGE/LOCAL_FLOOR provide a floor; LOCAL does not guarantee it.

If the customer writes OVStage directly, switch the whole application to
APPLICATION ordinals using the [live scene adaptation guide](../live-scene/README.md#adapting-this-sample).
Commit anchor changes before the boundary that samples them, rather than
assuming same-frame unsealed anchor reads.

### Validate and hand off

Exercise start, teleport, follow and reset. Confirm that the rig continues moving
without reselection, the user's physical height/offset remains, and reset returns
the application mapping to identity. Test a missing prim path and an invalid
transform to confirm visible errors. Inspect scale and heading in the customer's
scene and label exact-head placement separately from origin placement.

Return the customer navigation code, configurable paths/destinations, chosen
coordinate convention, run command and observed results. Collision avoidance,
walkable-surface queries, controller gestures and locomotion comfort policy are
additional application work; use the
[workbench adaptation guide](../spatial-workbench/README.md#adapting-this-sample) if interaction is requested.

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

Recovered request excerpt (2026-09-29):

> ok propose how we could create samples to show more XR features

This is historical context, not a complete generation transcript or a claim
that this request alone produced the current code. The implementation evolved
through follow-up requests, review and test fixes.

### Reconstruction prompt

Written on 2026-10-06 to describe the current example; it is not the original
verbatim generation prompt. Language coverage: Python and C++.
Use the canonical skills: [tracking-anchor-and-scale](../../../skills/tracking-anchor-and-scale/SKILL.md), [openxr-input-and-extensions](../../../skills/openxr-input-and-extensions/SKILL.md), plus the language's project-setup skill.

```text
Create an application demonstrating anchors-and-teleport using the matched OVXR, OVRTX and OVStage SDKs
provided with this request. Read the listed skills and the linked sample recipe.
Demonstrate fixed placement, teleportation, following an authored rig and reset. Convert tracking poses with the resolved anchor and retain configurable scene units and start anchor.
Use the recipe's bundled scene and controls as the baseline, keeping all shared
helpers and asset references coherent. Do not change the SDK APIs or weaken the
tests to accommodate the result. Provide build/run commands and report actual
validation results separately from unrun hardware checks.
```

Read the [anchors-and-teleport adaptation guide](#adapting-this-sample) for source, snippet regions
and the precise feature contract. The adaptation guide links the checked-in implementation;
using it is reference-assisted adaptation, not a blind skill-generation evaluation.
For exact bytes or independent generation checks, follow
[reproduction and evaluation](../../VALIDATION.md#source-reproduction-and-generation-evaluation). No independent generation pass is
claimed by this prompt record.

</details>
