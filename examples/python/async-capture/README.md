# Asynchronous capture consumer

Render the room in **stereo opaque VR** while a deliberately slow CPU worker
consumes captured eye images. Foveation is disabled. The visible room stays
simple; the queue behavior is shown in the final counters.

After [setting up the SDK environment](../../README.md#running-samples), run from
this directory:

```sh
python main.py --frames 180 --consumer-ms 200 --report capture.json
python main.py --frames 180 --consumer-ms 0
```

`--consumer-ms` simulates work after each owned image reaches the worker. Compare
consumed and dropped counts across the two runs. Scheduling and rendering speed
affect these counts; a sufficiently slow consumer causes drops. `--frames 0`
runs until Ctrl+C. No desktop window, encoder or network service is required.

The render thread acquires a fresh view-0 capture and synchronously copies it
to owned CPU bytes **before advancing OVXR**. Only those bytes and copied scalar
metadata cross to the worker; no borrowed device pointer or OVXR context does.
The CUDA copy is still synchronous. The downstream consumer is asynchronous.

There is at most one active consumer frame and one pending frame. A new image
replaces a pending stale image, incrementing `dropped`, without waiting for the
slow consumer. After the producer finishes, shutdown interrupts the simulated
delay, drains the latest pending image and joins the worker. On normal completion,
`offered = consumed + dropped`, and `last_sequence` identifies the final consumed
capture. The checksum is the unsigned 32-bit sum of its copied bytes.

Replace the worker's checksum with customer processing after this ownership
pattern works. A real blocking encoder needs its own cancellation policy; the
sample's interruptible delay is not a watchdog. CPU copies, Python's GIL and
worker processing still have overhead; this is not zero-copy GPU streaming or
a performance benchmark. The pending queue remains bounded as the session runs.

[C++ version](../../c/async-capture/README.md) ·
[Recreation adaptation guide](#adapting-this-sample) ·
[Compatibility](../../COMPATIBILITY.md)

## Adapting this sample

**Goal:** Create an asynchronous capture consumer

Use the canonical skills: [frame-readback](../../../skills/frame-readback/SKILL.md).
See the [prompt context (FYI)](#prompt-context-fyi) and [validation coverage](../../../skills/COVERAGE.md).
The matched SDK contracts take precedence over example implementation choices.

Read [project setup](../../README.md#shared-project-setup-and-ownership), the
[Python loop](main.py) and
[consumer](consumer.py), the
[C++ loop](../../c/async-capture/main.cpp) and
[consumer](../../c/common/async_capture.h), and the
[run guide](README.md).
The `owned-async-capture` snippets mark the ownership boundary.

Keep every OVXR call and the source copy on the render thread. Accept only a
sequence newer than the last queued capture. Complete the synchronous CUDA copy
while the borrowed source is valid, before advancing or destroying the context.
Use the existing frame-copy helper to honor pitch and restore CUDA context.
Send only owned bytes and copied scalar metadata to the worker, never a Frame
object, borrowed device pointer, or context. Copying to CPU is deliberately
synchronous; only the downstream processing is asynchronous.

Keep one active frame and one pending frame. Replace an older pending frame
when newer data arrives, count the drop, and hold the queue lock only while
transferring ownership. The worker must release that lock while processing.
Adapt the checksum to the customer's consumer without silently adding an
encoder, network protocol or unbounded queue.

On shutdown, stop producing, finish active work and the latest pending image,
then join the worker before teardown. Forward worker errors to the owner. A real
blocking consumer needs its own cancellation contract; this sample only
interrupts its simulated delay. Read shared result counters after joining.

Validate with a slow consumer: XR continues submitting, pending capacity stays
one, stale captures are not recopied, and the latest queued sequence is consumed.
After normal draining, `offered = consumed + dropped`. Exercise copy failure,
consumer failure and shutdown while work is pending. Report actual counters,
not an assumed performance improvement. The sample is stereo opaque VR with
disabled foveation; see [compatibility](../../COMPATIBILITY.md).

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
Use the canonical skills: [frame-readback](../../../skills/frame-readback/SKILL.md), plus the language's project-setup skill.

```text
Create an application demonstrating async-capture using the matched OVXR, OVRTX and OVStage SDKs
provided with this request. Read the listed skills and the linked sample recipe.
Copy borrowed eye data into owned images before handing it to a bounded worker queue. Replace stale pending frames for a slow consumer, propagate worker errors, and join the worker before destroying borrowed owners.
Use the recipe's bundled scene and controls as the baseline, keeping all shared
helpers and asset references coherent. Do not change the SDK APIs or weaken the
tests to accommodate the result. Provide build/run commands and report actual
validation results separately from unrun hardware checks.
```

Read the [async-capture adaptation guide](#adapting-this-sample) for source, snippet regions
and the precise feature contract. The adaptation guide links the checked-in implementation;
using it is reference-assisted adaptation, not a blind skill-generation evaluation.
For exact bytes or independent generation checks, follow
[reproduction and evaluation](../../VALIDATION.md#source-reproduction-and-generation-evaluation). No independent generation pass is
claimed by this prompt record.

</details>
