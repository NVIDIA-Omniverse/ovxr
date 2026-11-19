# Foveation (Python)

**App type:** stereo VR (opaque). Mono and AR are not configured by this sample. See the [compatibility matrix](../../COMPATIBILITY.md).

[Preview this sample in the gallery](../../gallery/README.md#foveation).

See [Adapting this sample](#adapting-this-sample) for guidance on using the
canonical skills in your application.

Compare three foveation policies in the same room with a fine line grid. Run each
mode in a separate session; these settings configure the initial render products.

After [environment setup](../../README.md#running-samples), run from this sample directory:
```powershell
python main.py --mode off --frames 300
python main.py --mode local --multiplier 0.2 --inset 0.1 --frames 300
python main.py --mode runtime --frames 300
```

| Mode | Local rendering | Runtime PQW policy | What it demonstrates |
| --- | --- | --- | --- |
| `off` | Disabled | Disabled | Full-resolution baseline. |
| `local` (default) | Warped | Disabled | Application-controlled warp, with unwarped headset submission. |
| `runtime` | Auto | Auto | Runtime-provided geometry when valid; full-resolution fallback otherwise. |

`--multiplier` and `--inset` accept values in (0, 1] and apply only to local
mode. A smaller multiplier reduces the fallback warped image dimensions;
the inset controls the central full-resolution region. Runtime PQW metadata
takes precedence over those fallback inputs, so the sample rejects tuning
arguments in other modes.

Local mode defaults to an intentionally strong **0.2 multiplier / 0.1 inset**:
a small full-resolution center surrounded by visibly reduced peripheral detail.
These are demonstration settings, not a recommended headset quality preset.
Use `--multiplier 0.5 --inset 0.4` for the previous, milder comparison.

Look at the grid near the center and at the edges while holding a similar head
pose between runs. The room landmarks help align the comparison. Output reports
effective local rendering and whether runtime foveated submission is enabled.
It queries **after submission**, because AUTO can resolve when views are first
located. Extension enablement alone is not evidence of valid runtime PQW.
A disabled local result in runtime mode is a supported fallback.

The sample also reports the selected raw render-product readback dimensions.
These are not necessarily the headset swapchain dimensions; a smaller warped
image may still be submitted at full size after unwarping. Readback is enabled
for this diagnostic, so this is an image-quality/policy example, **not a GPU
performance benchmark**. It does not configure eye-tracking actions.

The `foveation-policy` and `resolved-foveation` snippets in `main.py` show `ContextDesc` settings and `get_device_info()` / `acquire_frame()` queries.
Ctrl+C stops an unlimited session; `--frames N` counts submitted frames.

## Adapting this sample

**Goal:** Create a foveation comparison application

Use the canonical skills: [foveation-and-display](../../../skills/foveation-and-display/SKILL.md).
See the [prompt context (FYI)](#prompt-context-fyi) and [validation coverage](../../../skills/COVERAGE.md).
The matched SDK contracts take precedence over example implementation choices.

Build a customer-controlled comparison of full-resolution rendering, local
foveation and runtime-provided piecewise quadratic warp (PQW). Preserve the
distinction between requested policy, resolved rendering and submitted images.

### Read first

Read [project setup](../../README.md#shared-project-setup-and-ownership), then:

- [Python main](main.py) and
  [C++ main](../../c/foveation/main.cpp), regions
  `foveation-policy` and `resolved-foveation`.
- [Python guide](README.md),
  [C++ guide](../../c/foveation/README.md), and
  [CMake project](../../c/foveation/CMakeLists.txt).
- [Grid scene](../../assets/foveation-grid.usda) and
  [OVXR descriptor/features](../../../include/ovxr/ovxr.h).

### Implement the modes

Use one context per configured comparison run, with PRIVATE ordinals unless the
customer also needs direct OVStage edits. Expose a mode selector with these
descriptor policies (enum names below use Python spelling; C has matching
`OVXR_` enum constants):

| Mode | local_foveation_mode | runtime_foveated_submission_policy |
| --- | --- | --- |
| off | DISABLED | DISABLED |
| local | WARPED | DISABLED |
| runtime | AUTO | AUTO |

For local mode, expose `foveation_warped_resolution_multiplier` and
`foveation_warped_inset_size`; validate finite values in (0, 1]. The sample
defaults are deliberately strong: 0.2 and 0.1, giving a small sharp center and
more obvious peripheral detail loss. These are visual demonstration settings;
0.5 and 0.4 restore the previous, milder comparison. Reject local tuning flags
in other modes instead of pretending the runtime applies them.

Runtime PQW takes precedence over local tuning. AUTO permits an unfoveated
fallback when the runtime cannot provide the feature. Do not silently rename
that fallback "runtime foveation enabled". These modes do not establish
eye-tracked or gaze-driven foveation.

### Show what actually happened

After a submitted frame, query `get_device_info().features` /
`ovxr_get_device_info`. Report
`local_foveated_rendering_enabled` and `foveated_submission_enabled` along with
the requested mode. Resolve policies after frame setup; a descriptor print at
startup is insufficient.

Enable frame readback with `readback_foveation_unwarp=false` to inspect the
rendered dimensions. `acquire_frame` is best-effort; report width/height only
when present and keep the capture sequence so stale metadata is recognizable.
Do not dereference or copy the device pointer merely to print dimensions.

For image comparisons, distinguish raw rendered eye output, unwarped readback
and the runtime's compositor view. `readback_foveation_unwarp` controls readback
only. `foveation_unwarp` and runtime submission negotiation serve a different
purpose; preserve the source's settings and the SDK contract.

### Adapt the scene and controls

Use the customer's scene, optionally adding a fine grid or high-frequency detail
near the center and edges. Retain valid units and a start anchor. Keep geometry,
viewpoint, rendering settings and run length stable across modes so comparisons
have a useful baseline.

A smaller render product is not a measured frame-rate improvement. If performance
is requested, collect timings after shader warmup with equal workloads and
describe the measurement setup; the sample's dimensions alone are not a benchmark.
Disable diagnostic copying/windows for timing comparisons.

### Validate and hand off

Run all three modes for a finite submitted-frame count. Record effective flags,
rendered dimensions and whether runtime mode fell back. Compare center and
peripheral detail with the off baseline. Invalid tuning values and incompatible
flags should fail before session creation.

Do not require identical dimensions across different headsets, runtimes or SDK
releases. Return the customer's comparison app, mode controls, repeatable run
commands, observed policy table and any untested runtime capability.

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
Use the canonical skills: [foveation-and-display](../../../skills/foveation-and-display/SKILL.md), plus the language's project-setup skill.

```text
Create an application demonstrating foveation using the matched OVXR, OVRTX and OVStage SDKs
provided with this request. Read the listed skills and the linked sample recipe.
Compare off, local and runtime-provided foveation, report requested versus negotiated support, and show a visibly different peripheral region without claiming unsupported runtime modes.
Use the recipe's bundled scene and controls as the baseline, keeping all shared
helpers and asset references coherent. Do not change the SDK APIs or weaken the
tests to accommodate the result. Provide build/run commands and report actual
validation results separately from unrun hardware checks.
```

Read the [foveation adaptation guide](#adapting-this-sample) for source, snippet regions
and the precise feature contract. The adaptation guide links the checked-in implementation;
using it is reference-assisted adaptation, not a blind skill-generation evaluation.
For exact bytes or independent generation checks, follow
[reproduction and evaluation](../../VALIDATION.md#source-reproduction-and-generation-evaluation). No independent generation pass is
claimed by this prompt record.

</details>
