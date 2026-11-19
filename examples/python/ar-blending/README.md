# AR blending

Two opaque objects float against a transparent background: an amber sphere and
a dark cube. The cube makes it clear that dark RGB and transparent alpha are
different. This is a **stereo AR** sample, with foveation disabled.

After [setting up the SDK environment](../../README.md#running-samples), run from
this directory:

```sh
python main.py --blend alpha --frames 180 --report ar.json
python main.py --blend auto --frames 180
python main.py --blend opaque --frames 180
```

`--blend` accepts `alpha` (default), `auto`, `opaque` and `additive`. The selected
OpenXR runtime must support the requested mode. `--frames 0` runs until Ctrl+C.
The optional report path is relative to the launching directory.

The app requests an AR session and reports the **resolved submission blend mode**.
The scene has no floor or background geometry, and its dome lights the objects
without appearing in primary rays. OVXR authors the AR compositing settings on
its generated RenderProducts; the sample adds no global renderer overrides.

After up to 30 submitted frames, it copies view 0's RGBA readback once and counts
fully transparent and fully opaque pixels, excluding row padding. Both counts
should be nonzero for the default view. Antialiased edges may have partial alpha.
This measurement describes renderer output. Even an opaque environment blend
can receive an image with alpha: the blend mode controls how it is presented.

On a supporting headset/client, alpha blending should reveal the environment
around the objects while the dark cube stays opaque. A desktop RGB preview
cannot prove this. Validate Quest Pro/AVP with the official CloudXR OpenXR client
separately. This sample does not implement real-world depth occlusion, spatial
mapping, camera access or vendor-specific passthrough layers.

[C++ version](../../c/ar-blending/README.md) ·
[Recreation adaptation guide](#adapting-this-sample) ·
[Compatibility](../../COMPATIBILITY.md)

## Adapting this sample

**Goal:** Create an AR blending example

Use the canonical skills: [session-modes-and-passthrough](../../../skills/session-modes-and-passthrough/SKILL.md).
See the [prompt context (FYI)](#prompt-context-fyi) and [validation coverage](../../../skills/COVERAGE.md).
The matched SDK contracts take precedence over example implementation choices.

Read [project setup](../../README.md#shared-project-setup-and-ownership), then the
[Python loop](main.py), [C++ loop](../../c/ar-blending/main.cpp),
[scene](../../assets/ar-objects.usda) and [run guide](README.md).
The `ar-blending` snippets show the session settings and observed output.

Adapt the customer's objects and placement while preserving empty background
rays and at least one dark opaque object. Keep the dome's
`visibleInPrimaryRay = false`: lighting an object should not fill the background.
Preserve opaque materials; do not confuse black RGB with zero alpha.

Request `SessionMode.AR` and the customer's environment blend mode separately.
This sample defaults to stereo and ALPHA_BLEND, with foveation disabled. OVXR
authors its AR renderer settings on generated RenderProducts. Do not add global
renderer overrides to hide a broken or unsupported SDK path. Use matched SDKs.

Report the resolved `frame.environment_blend_mode` after a submitted frame;
AUTO is a request, not proof of alpha blending. Copy RGBA readback synchronously
before advancing the context, honoring pitch and CUDA context as in the shared
frame-copy helper. Count alpha over actual pixels, excluding row padding.

Check that the default scene has both fully transparent background and opaque
foreground after warmup. Compare requested and resolved blend modes, and check
an unsupported request fails clearly. Preserve partial-alpha antialiased edges.
Renderer readback and submitted headset presentation are distinct evidence:
verify the environment is visible around objects and the dark object remains
opaque on the customer's actual headset/client before claiming passthrough.

The app does not implement real-world occlusion, spatial mapping or vendor
passthrough layers. Use the official OpenXR client for CloudXR. Deliver the small
app, scene, launch command, report and observed limits; retain unverified hardware
checks. See [compatibility](../../COMPATIBILITY.md).

## Prompt context (FYI)

<details>
<summary>Request history and reconstructed prompt</summary>

References to a recipe in the recorded prompt now correspond to the
[adaptation guidance above](#adapting-this-sample).

### Provenance

Recovered request excerpt (2026-10-02):

> ok add  those 3 samples you proposed for both languages

This is historical context, not a complete generation transcript or a claim
that this request alone produced the current code. The implementation evolved
through follow-up requests, review and test fixes.

### Reconstruction prompt

Written on 2026-10-06 to describe the current example; it is not the original
verbatim generation prompt. Language coverage: Python and C++.
Use the canonical skills: [session-modes-and-passthrough](../../../skills/session-modes-and-passthrough/SKILL.md), plus the language's project-setup skill.

```text
Create an application demonstrating ar-blending using the matched OVXR, OVRTX and OVStage SDKs
provided with this request. Read the listed skills and the linked sample recipe.
Request alpha blending and report the resolved environment mode. Demonstrate transparent background and opaque visible geometry, inspect captured alpha, and avoid claiming camera passthrough or real-world occlusion from emulator output.
Use the recipe's bundled scene and controls as the baseline, keeping all shared
helpers and asset references coherent. Do not change the SDK APIs or weaken the
tests to accommodate the result. Provide build/run commands and report actual
validation results separately from unrun hardware checks.
```

Read the [ar-blending adaptation guide](#adapting-this-sample) for source, snippet regions
and the precise feature contract. The adaptation guide links the checked-in implementation;
using it is reference-assisted adaptation, not a blind skill-generation evaluation.
For exact bytes or independent generation checks, follow
[reproduction and evaluation](../../VALIDATION.md#source-reproduction-and-generation-evaluation). No independent generation pass is
claimed by this prompt record.

</details>
