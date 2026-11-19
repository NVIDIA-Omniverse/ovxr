# View configuration

Render the same small room with a **mono, stereo or AUTO OpenXR view
configuration**, using opaque VR and disabled foveation. This sample focuses on
selection and reporting; it adds no UI or interaction tools.

After [setting up the SDK environment](../../README.md#running-samples), run from
this directory:

```sh
python main.py --display auto --frames 180 --report views.json
python main.py --display stereo --frames 180
python main.py --display mono --frames 180
```

AUTO prefers stereo and falls back to mono when the runtime exposes only mono.
It does not mean that every headset offers both. An unavailable explicit mode
fails with runtime diagnostics; the app does not silently switch your selection.
`--frames 0` runs until Ctrl+C. Restart the app to change view configuration.

The report distinguishes the requested configuration from the resolved view
count, recommended first-view dimensions, and whether recommended dimensions
are uniform across views. After submission it reports actual readback dimensions
for **view 0 only**. These dimensions are not a desktop-window size and do not
describe other views when the runtime uses unequal extents. The app inspects
readback metadata without transferring image pixels to the CPU.

A mono session is one OpenXR view, not a stereo application's desktop mirror.
This is not a tablet client or an AR example. The selected runtime must expose
the requested view configuration; physical headset checks remain separate from
emulator checks. Quad view is outside this sample.

[C++ version](../../c/view-configuration/README.md) ·
[Recreation adaptation guide](#adapting-this-sample) ·
[Compatibility](../../COMPATIBILITY.md)

## Adapting this sample

**Goal:** Create a view-configuration example

Use the canonical skills: [foveation-and-display](../../../skills/foveation-and-display/SKILL.md).
See the [prompt context (FYI)](#prompt-context-fyi) and [validation coverage](../../../skills/COVERAGE.md).
The matched SDK contracts take precedence over example implementation choices.

Read [project setup](../../README.md#shared-project-setup-and-ownership), the
[Python loop](main.py),
[C++ loop](../../c/view-configuration/main.cpp) and
[run guide](README.md).
Use the `view-configuration` snippets and the existing setup helpers.

Select MONO, STEREO or AUTO from the customer's choice at context creation.
AUTO prefers stereo and falls back to mono when needed; explicit unavailable
configurations must not silently change. Report runtime readiness/unsupported
diagnostics. Recreate the context to change topology instead of inventing a
live display-mode setter. This sample uses opaque VR with foveation disabled.

Let OVXR create the render products. Report the resolved view count and runtime
recommendation, including the uniform-size flag. After submission, acquire view
zero's readback metadata and record its actual width/height without copying
pixels. Keep runtime recommendation and rendered dimensions distinct. Do not
label the first view's dimensions as every view's dimensions on an asymmetric
runtime. Do not assume there is a second eye in mono mode.

Validate explicit stereo, explicit mono where supported, AUTO negotiation and
an unavailable configuration. A stereo-only runtime cannot establish mono
support; use a suitable runtime or report that check as pending. A desktop mirror
is not a mono XR session, and this example supplies no tablet client or AR path.

Deliver the adapted app, chosen scene, build/run commands and measured report.
Keep it focused on view configuration; quad-view topology and device-specific
compositing can be separate examples. See [compatibility](../../COMPATIBILITY.md).

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
Use the canonical skills: [foveation-and-display](../../../skills/foveation-and-display/SKILL.md), plus the language's project-setup skill.

```text
Create an application demonstrating view-configuration using the matched OVXR, OVRTX and OVStage SDKs
provided with this request. Read the listed skills and the linked sample recipe.
Select mono, stereo or AUTO, report the actual negotiated view count and recommendations separately from rendered dimensions, and handle an unsupported explicit request accurately.
Use the recipe's bundled scene and controls as the baseline, keeping all shared
helpers and asset references coherent. Do not change the SDK APIs or weaken the
tests to accommodate the result. Provide build/run commands and report actual
validation results separately from unrun hardware checks.
```

Read the [view-configuration adaptation guide](#adapting-this-sample) for source, snippet regions
and the precise feature contract. The adaptation guide links the checked-in implementation;
using it is reference-assisted adaptation, not a blind skill-generation evaluation.
For exact bytes or independent generation checks, follow
[reproduction and evaluation](../../VALIDATION.md#source-reproduction-and-generation-evaluation). No independent generation pass is
claimed by this prompt record.

</details>
