# Runtime lifecycle (Python)

**App type:** stereo VR (opaque). Mono and AR are not configured by this sample. See the [compatibility matrix](../../COMPATIBILITY.md).

[Preview this sample in the gallery](../../gallery/README.md#runtime-lifecycle).

See [Adapting this sample](#adapting-this-sample) for guidance on using the
canonical skills in your application.

Keep the renderer and populated application scene alive while recreating the
OVXR context. Exercise setup, a bounded retry policy, and desktop mirroring
before trying a physical client disconnect.

After [environment setup](../../README.md#running-samples), run from this sample directory:
```powershell
python main.py --mirror
python main.py --mirror --reconnect-at 40 --frames 100 --report "$PWD\runtime-python.json"
```

Use your **official OpenXR client** with the existing CloudXR runtime setup.
Follow [CloudXR setup and headset validation](../../VALIDATION.md); a test-runtime
recreation is separate from a network disconnect/reconnect test.

| Control | Effect |
| --- | --- |
| R | Destroy the current OVXR context, then create a fresh one with the same renderer, stage and settings. |
| Q / Ctrl+C | Exit and release resources. |
| `--reconnect-at N` | Exercise one context recreation after N submitted frames. |
| `--retry-after N` | Recreate after N seconds without a submitted frame; default 10. |
| `--max-restarts N` | Limit consecutive recreation attempts without a submitted frame; default 3. A submitted frame resets the budget. Zero disables retries. |
| `--mirror` | Show a Windows window with fresh, unwarped left-eye readback. |
| `--report PATH.json` | Save submitted-frame, successful restart, attempt and mirror-frame counts after a successful run. |

`ERROR_OPENXR` triggers recreation. `NOT_READY` and `TIMEOUT` keep waiting;
absence of submissions beyond `--retry-after` triggers recreation. Failed OpenXR creations retry every half
second within the same budget; quit input remains active. Successful frame submission
refills the budget, so later user-requested reconnects or independent transient
failures do not consume a lifetime allowance. The report retains total attempts. This heuristic cannot distinguish a
disconnected client from an unfocused or idle headset. Increase the retry window
for long pauses. Errors outside OpenXR fail the sample. The timeout is checked
between API calls; it cannot interrupt a runtime blocked inside an API.
`--frames N` is a validation target, so quitting early returns failure.

OVXR owns session events and the frame loop. The sample destroys its context,
retains the application scene and renderer, and creates a new context. Never
carry borrowed frame views or OpenXR handles across that boundary. A real
application must also destroy its action spaces/hand trackers before replacing
the context and recreate them afterward.

The mirror copies the borrowed CUDA frame into owned CPU memory every third
submitted frame, before advancing XR. Its sequence counter resets with a new
context. The last copied image remains visible while reconnecting. This
synchronous diagnostic path adds overhead and is not a performance benchmark.
The window supports Windows only; omit `--mirror` on Linux. The JSON field
`headset_verified` remains false; record physical results separately.

Build the C++ samples first: this example uses the small `ovxr-sample-support` native library for its optional window. The application loop remains Python.

## Adapting this sample

**Goal:** Create a resilient XR session lifecycle

Use the canonical skills: [context-creation](../../../skills/context-creation/SKILL.md), [runtime-adapters](../../../skills/runtime-adapters/SKILL.md), [application-flow](../../../skills/application-flow/SKILL.md).
See the [prompt context (FYI)](#prompt-context-fyi) and [validation coverage](../../../skills/COVERAGE.md).
The matched SDK contracts take precedence over example implementation choices.

Build a customer application that reports its selected runtime, preserves its
application scene across XR context recreation, and recovers within an explicit
retry budget. Separate controlled recreation from evidence of real client or
network reconnect.

### Read first

Read [customer project setup](../../README.md#shared-project-setup-and-ownership), then:

- [Python main](main.py),
  [C++ main](../../c/runtime-lifecycle/main.cpp), and
  [CMake project](../../c/runtime-lifecycle/CMakeLists.txt).
- `application(reconnectable=True)` in the
  [Python helper](../common/xr_sample.py) and
  `Application` in the [C++ helper](../../c/common/xr_sample.h).
- [Mirror/input ownership wrappers](../../c/common/interactive_sample.h) and
  [Python bridge](../common/sample_support.py).
- [Runtime sample guide](README.md) and
  [official-client validation](../../VALIDATION.md).

### Resolve the customer environment

Use the already selected OpenXR runtime, or an explicit process-local
`XR_RUNTIME_JSON` manifest supplied by the customer. Record the runtime/system,
client, headset and host versions available. For CloudXR use the official
OpenXR client and the customer's connection/authentication setup. Do not replace
system registration, upgrade the runtime, or create a custom web client merely
to recreate this sample.

Choose the scene/anchor, retry interval, maximum restarts, a controlled restart
trigger and whether to display a mirror. The reference uses a 10-second
no-submission window, three restarts and optional R/Q keys. These are example
policy values, not transport-level reconnect guarantees.

### Implement recovery

1. Own one renderer and stage for the application lifetime. Populate once and
   retain application model state. Keep a factory/configuration for replacing
   only the OVXR context, using the same compatible descriptor options.
2. Use PRIVATE ordinals for the baseline. Context replacement must start its
   writes above the surviving stage's current floor. If combining direct stage
   edits, coordinate APPLICATION ordinals using the
   [live scene adaptation guide](../live-scene/README.md#adapting-this-sample).
3. Pump the optional Windows mirror, read commands and call `render_frame`.
   Count submissions; SUCCESS without submission does not reset a stall timer.
   Delay NOT_READY retries rather than busy-spinning.
4. Copy the diagnostic error immediately before another OVXR call can overwrite
   it. Classify ERROR_OPENXR and an expired no-submission window as restart
   candidates under the sample policy. Invalid arguments, missing assets,
   incompatible devices and other renderer errors should fail with their cause,
   not enter an unlimited restart loop.
5. Before replacement, cancel outstanding plans and destroy any application
   children of the old session, including action spaces/sets and hand trackers
   if the customer combined input with this recipe. Drop all borrowed XR handles
   and capture views.
6. Destroy the old context exactly once, clear its owner slot, then create the
   replacement. Retain the renderer, stage and customer model. If creation fails,
   cleanup must still safely release the surviving application resources.
7. Reset mirror capture-sequence filtering for the new context, whose sequence
   counter restarts. Reacquire input handles and rebuild session-owned input
   objects. Increment the restart count, use a short backoff and reset the retry
   window.
8. Stop with a useful error when the restart budget is exhausted. Normal quit
   destroys the latest context and then its borrowed renderer/stage in order.

A timer checked between API calls cannot recover a native call blocked inside
a runtime. Report that limitation instead of claiming a watchdog. A client
disconnect may resume the existing session without recreation; observe the
installed runtime's behavior rather than forcing every disconnect into one path.

### Customize diagnostics and persistence

Expose finite-frame operation and a report with submitted frames, context
restarts, copied mirror frames and unverified hardware status. Report effective
runtime information after frames start. Preserve customer settings and scene
edits across restarts.

OVXR runtime cameras/products may be reauthored; application state that must
persist belongs outside that runtime-owned subtree. If navigation should persist,
restore the customer's selected anchor intentionally when recreating the context
instead of assuming the default managed anchor is the user's last location.

The optional mirror uses synchronous copied readback and Win32. It is diagnostic
overhead; it does not prove in-headset stereo, tracking or reconnect quality.

### Validate and hand off

First run a controlled restart after frame 40 of a 100-submitted-frame session.
Verify exactly one replacement, unchanged application state, resumed fresh
mirror frames and clean final teardown. Exercise failed replacement and retry
budget exhaustion with appropriate mocks or available runtime conditions.

Then, when hardware is available, separately test official-client disconnect,
reconnect and runtime restart. Record elapsed pause, whether the same session
resumed or was recreated, and any failure. Do not label the controlled emulator
restart as a successful physical network test.

Return the customer application, retry/error policy, preserved-state behavior,
runtime selection instructions, run/report paths and validation evidence.

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
Use the canonical skills: [context-creation](../../../skills/context-creation/SKILL.md), [runtime-adapters](../../../skills/runtime-adapters/SKILL.md), [application-flow](../../../skills/application-flow/SKILL.md), plus the language's project-setup skill.

```text
Create an application demonstrating runtime-lifecycle using the matched OVXR, OVRTX and OVStage SDKs
provided with this request. Read the listed skills and the linked sample recipe.
Probe runtime readiness and recreate only the XR context within a bounded retry budget while preserving the renderer and stage. Keep quit responsive, count submitted frames, and distinguish recoverable runtime errors from other failures.
Use the recipe's bundled scene and controls as the baseline, keeping all shared
helpers and asset references coherent. Do not change the SDK APIs or weaken the
tests to accommodate the result. Provide build/run commands and report actual
validation results separately from unrun hardware checks.
```

Read the [runtime-lifecycle adaptation guide](#adapting-this-sample) for source, snippet regions
and the precise feature contract. The adaptation guide links the checked-in implementation;
using it is reference-assisted adaptation, not a blind skill-generation evaluation.
For exact bytes or independent generation checks, follow
[reproduction and evaluation](../../VALIDATION.md#source-reproduction-and-generation-evaluation). No independent generation pass is
claimed by this prompt record.

</details>
