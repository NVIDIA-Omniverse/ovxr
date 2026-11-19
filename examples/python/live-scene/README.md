# Live scene — Python

**App type:** stereo VR (opaque). Mono and AR are not configured by this sample. See the [compatibility matrix](../../COMPATIBILITY.md).

[Preview this sample in the gallery](../../gallery/README.md#live-scene).

See [Adapting this sample](#adapting-this-sample) for guidance on using the
canonical skills in your application.

An orange cube moves sideways and rotates in a small, meter-scale, Y-up room.
The viewer starts on the blue platform, looking toward the cube. The same scene
is used by the [C++ sample](../../c/live-scene).

After [environment setup](../../README.md#running-samples), run from this sample directory:

```powershell
python main.py
python main.py --frames 180 --no-foveation
```

See the [gallery](../../README.md) for environment setup and build instructions.
With matching SDKs already installed, run `python main.py` from this directory;
retain the sibling `common/` folder and `../../assets/`. No NumPy is needed.

## What the code explains

1. Populate and seal the scene before creating the XR context.
2. Select `StageOrdinalMode.APPLICATION`: the application owns the stage-wide
   ordinal ledger. This example has one writer thread; multiple writers must
   share a ledger and publication lock.
3. Begin an XR frame, write `omni:xform` at a fresh ordinal, then bind the frame
   at that **same** ordinal. Bind publishes the XR cameras, seals all those
   writes, and renders. The application does not step OVRTX separately.
4. Inspect the `sealed` result even on failure. A sealed ordinal is consumed.
   `NOT_READY` leaves the application update pending: cancel the old plan and
   retry a new plan with that same ordinal, without rewriting the update.
5. Submit the rendered frame. A `finally` block cancels any still-owned plan.
   Teardown obtains a fresh ordinal, destroys OVXR, detaches the renderer, and
   destroys the renderer and stage. A failed stage is disposed, never reused.

Animation advances by a fixed 1/90 second per submitted frame for reproducible
runs; it is not a wall-clock simulation. Transforms use row vectors with
translation in elements 12–14. The native CPU tensor stores one 16-lane double
matrix with `AttributeSemantic.MATRIX`.

Look for `live-scene: submitted N frames; clean shutdown.` after a finite run.

## Adapting this sample

**Goal:** Create an XR application with live scene edits

Use the canonical skills: [stage-publication](../../../skills/stage-publication/SKILL.md), [frame-loop](../../../skills/frame-loop/SKILL.md).
See the [prompt context (FYI)](#prompt-context-fyi) and [validation coverage](../../../skills/COVERAGE.md).
The matched SDK contracts take precedence over example implementation choices.

Use this recipe when the customer's simulation or tool writes directly to
OVStage while OVXR writes the eyes. Build a small editable or animated object
example in the customer's scene, with one coordinated publication boundary.

### Read first

Read [project setup and ownership](../../README.md#shared-project-setup-and-ownership), then:

- [Python loop](main.py) and
  [C++ loop](../../c/live-scene/main.cpp), especially
  `publish-live-scene`.
- [Python setup](../common/xr_sample.py),
  [C++ setup](../../c/common/xr_sample.h), and the
  [CMake project](../../c/live-scene/CMakeLists.txt).
- [Python guide](README.md) or
  [C++ guide](../../c/live-scene/README.md).

### Define the customer edit

Choose the target prim, attribute, animation or command, update rate and source
of time. The stock scene uses `/World/AnimatedCube`; author or query a real prim
in the customer's stage instead. A deterministic test may advance animation by
submitted frames; a simulation can use its own clock.

For the transform example, the public OVStage attribute is `omni:xform` with
MATRIX semantic. The source supplies one 16-lane float64 CPU DLTensor element,
`is_array=false`, in row-major row-vector form. Keep tensor data, shape and
backing storage alive until the asynchronous write finishes. Reuse the exact
SDK types instead of reconstructing tensor ABI layouts from memory.

### Implement the publication loop

1. Populate and seal initial scene data, then create OVXR in APPLICATION ordinal
   mode. Allocate an ordinal above the current attribute write floor.
2. Begin the frame. NOT_READY before a usable plan requires a delayed retry.
   Establish a guard/finally that cancels an outstanding plan on every exit.
3. If this ordinal has no pending application write, enqueue the customer's
   edit and wait for it. Keep a pending flag so an unsealed retry does not apply
   the same relative edit twice.
4. Call bind with the exact same positive ordinal:
   Python returns `result, sealed`; C uses the `out_stage_ordinal_sealed` parameter.
   Process `sealed` before handling the result. When true, allocate a new
   ordinal and clear pending state even if bind reported an error.
5. On an unsealed NOT_READY result, cancel the outstanding plan, retain the
   ordinal and pending write, and retry a new frame plan. Do not submit a plan
   whose bind did not succeed.
6. Submit a successfully bound plan, count actual submissions and release any
   remaining plan. Keep all stage writers on this ledger and publication
   rendezvous; multiple threads must synchronize before bind.
7. Release queries/path lists before their stage. Destroy OVXR using a fresh
   application ordinal, then detach/destroy the renderer and destroy the stage.

Do not seal application edits yourself before bind when the intent is to publish
them with the eyes. Do not mix PRIVATE helper calls with APPLICATION writes.
Reading an unsealed attribute through a typed value reader is not proof that it
is committed; inspect rendered or sealed state.

### Customer adaptation

Keep the loop visible in the generated entrypoint. Reuse setup/query wrappers,
then replace the sine-wave cube with the customer's control or data feed.
For other attributes, use their actual tensor type, semantic and array shape.

Moving a tracking anchor is a separate case: its world transform must be
committed and hierarchy computation complete before the boundary that reads it.
Do not promise that a newly written, unsealed anchor drives the same frame.
Use the [anchor adaptation guide](../anchors-and-teleport/README.md#adapting-this-sample) for that mapping.

### Validate and hand off

Run a finite session and inspect at least two frames with visibly different
object poses. Exercise or mock an unsealed bind retry, a sealed bind failure,
and a write failure: no double edit, reused sealed ordinal, or abandoned frame
plan should remain. Report the submitted count and cleanup outcome.

Return the customer's editable application, target prim/attribute mapping,
clock choice, build/run command and test evidence. Keep the shared ledger and
data-lifetime explanation in the application documentation.

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
Use the canonical skills: [stage-publication](../../../skills/stage-publication/SKILL.md), [frame-loop](../../../skills/frame-loop/SKILL.md), plus the language's project-setup skill.

```text
Create an application demonstrating live-scene using the matched OVXR, OVRTX and OVStage SDKs
provided with this request. Read the listed skills and the linked sample recipe.
Animate the supplied scene through direct OVStage edits. Coordinate scene writes and XR camera writes at one APPLICATION ordinal, including unsealed retries, seal reporting and cancellation.
Use the recipe's bundled scene and controls as the baseline, keeping all shared
helpers and asset references coherent. Do not change the SDK APIs or weaken the
tests to accommodate the result. Provide build/run commands and report actual
validation results separately from unrun hardware checks.
```

Read the [live-scene adaptation guide](#adapting-this-sample) for source, snippet regions
and the precise feature contract. The adaptation guide links the checked-in implementation;
using it is reference-assisted adaptation, not a blind skill-generation evaluation.
For exact bytes or independent generation checks, follow
[reproduction and evaluation](../../VALIDATION.md#source-reproduction-and-generation-evaluation). No independent generation pass is
claimed by this prompt record.

</details>
