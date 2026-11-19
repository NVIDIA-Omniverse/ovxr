# Independent OpenXR composition layers

This Python example appends an alpha-blended quad after OVXR's scene projection.
Choose a head-fixed diagnostic panel or one fixed in the borrowed OpenXR tracking
space. The latter stays in place as the head moves; this small sample does not
add navigation or convert an arbitrary stage anchor into that space.

This is an overlay: foreground scene geometry does not hide its pixels. Use
[OVUI panel](../ovui-panel/README.md) for scene-occluded UI instead. The colored
regions are a diagnostic texture, not interactive OVUI widgets.

## Run

Prepare matched OVXR, OVRTX and OVStage bindings/native SDKs as described in the
[example setup](../../README.md). Build the sibling sample-support library and
set `OVXR_SAMPLE_SUPPORT_LIBRARY` to it. No new public OVXR API is required.
With that environment and a running OpenXR runtime:

```text
python main.py --placement head --frames 100
python main.py --placement world --frames 100 --verify-upload --report output/sample.json
```

The application requires Vulkan and an additional layer slot beyond the scene.
It uses the runtime's RGBA8 UNORM swapchain format; unsupported formats or image
usage fail explicitly. Verification additionally requires transfer-source usage.
The finite run fails if it ends before its requested submissions. `Q` quits an
interactive run.

## Ownership and evidence

The [quad adapter](../common/quad_layer.py) uses the
[native graphics helper](../../c/sample-support/projection.cpp) to create its
swapchain, upload RGBA, finish queue work and release the image before submit.
The live layer and its resources outlive `submit_frame`; app-owned swapchains
and the head-relative space are destroyed before the borrowed session. The
original scene remains the primary projection.

`--verify-upload` copies pixels back from the actual acquired Vulkan image and
checks every byte before releasing it. It adds synchronous work and is intended
for validation, not a headset performance configuration. A report also saves
`overlay-readback.npy` for inspection. This checks image contents and submission,
not the runtime's final blending or headset pixels.

Private CI runs both placements for 100 submissions and independently checks
the GPU-readback color/alpha regions, then saves a PNG artifact. It also inspects
the actual submitted quad: head/world space, pose, extent, stereo visibility and
alpha flags, while requiring the original scene layer to remain. The isolated
composition test runtime captures all submitted projection/quad layers from
their actual swapchain images and produces stereo goldens. Four references
cover both placements and eyes; the [C++ counterpart](../../c/composition-layers/README.md)
compares against those same references. Metadata must confirm composed output
and both layers, so a scene-only capture fails independently of its golden.
The fixed test head is offset horizontally to distinguish the two placements.
This reference compositor does not model lens distortion or late-stage
reprojection. On a real runtime, check the transparent border, partial-alpha
strip, head/world anchoring and both eyes. Physical placement and headset
blending remain hardware checks.

Read the [composition skill](../../../skills/composition-integration/SKILL.md),
[adaptation guide](#adapting-this-sample), [prompt context (FYI)](#prompt-context-fyi)
and [coverage map](../../../skills/COVERAGE.md).

The native adapter caches an owned copy of the uploaded quad pixels and reuses
the last released swapchain image while the contents and dimensions are unchanged.
Both languages use this adapter. Pixel changes trigger another acquire/upload/release;
`--verify-upload` deliberately repeats the upload/readback every renderable frame
so the composed-capture probes retain their release timing. This follows the
[OpenXR swapchain image lifetime](https://registry.khronos.org/OpenXR/specs/1.1-khr/html/xrspec.html#xrReleaseSwapchainImage).

## Adapting this sample

**Goal:** Append an independent overlay

Use [composition-integration](../../../skills/composition-integration/SKILL.md),
[frame-loop](../../../skills/frame-loop/SKILL.md) and the language's setup skill.
The [Python sample](README.md) and
[C++ sample](../../c/composition-layers/README.md) show the
`append-quad` frame loop; [quad_layer.py](../common/quad_layer.py)
contains `quad-upload`. The native adapter keeps OpenXR/Vulkan ABI structures
in C++ and shares graphics-resource code with the projection example.

Create the customer's texture and placement policy. Acquire/wait an app-owned
swapchain image, upload and synchronize, release it, and pass the live quad via
`additional_layers`. Preserve the original scene layer. Use VIEW space for a
head-relative panel and an appropriate reference space/pose for world placement.
Match straight versus premultiplied alpha to the layer flags. Keep all child
resources alive through submission and destroy them before their borrowed owners.

No-render plans submit with no added layer. A world-fixed overlay still does not
depth-test against the scene; choose [scene-occluded UI](../../../skills/scene-occluded-ui/SKILL.md)
when pixels and pointer input must be blocked by foreground geometry.

The diagnostic texture uses no toolkit. Add OVUI controls only when requested,
then implement input routing explicitly; there are no automatic quad pointer
events. Check actual swapchain pixels, frame counts and failure cleanup before
device checks. Compare both composed eyes against shared references, retaining
readback tests. Do not label scene-only runtime captures as final overlay output.
See the [prompt context (FYI)](#prompt-context-fyi) and
[validation coverage](../../../skills/COVERAGE.md#composition-integration).

## Prompt context (FYI)

<details>
<summary>Request history and reconstructed prompt</summary>

References to a recipe in the recorded prompt now correspond to the
[adaptation guidance above](#adapting-this-sample).

### Provenance

This sample was added in response to the 2026-10-06 request:

> can we have samples and tests for each skill though?

The context was the missing independent-layer example for the merged composition
skill. The implementation brief below makes that scope explicit; it is not a
verbatim historical prompt or evidence of independent regeneration.

The 2026-10-07 follow-up requested a C++ counterpart and a compositor capture
path including all submitted layers, followed by stereo goldens while retaining
texture readback tests. Those are included in the reconstruction brief below.

### Reconstruction prompt

Read [composition-integration](../../../skills/composition-integration/SKILL.md),
[frame-loop](../../../skills/frame-loop/SKILL.md),
[Python setup](../../../skills/project-setup-python/SKILL.md),
[C++ setup](../../../skills/project-setup-c/SKILL.md) and the
[adaptation guide](#adapting-this-sample).

```text
Build matching Python and C++ OVXR examples that append an alpha-blended OpenXR quad
after the scene. Support head-fixed and tracking-space-fixed placement. Use a
diagnostic RGBA panel, keep the original scene projection and do not claim scene
occlusion. Reuse the sample's C++ graphics adapter without extending public OVXR
APIs. Own the swapchain and optional VIEW space, finish/release uploads before
submit and destroy them before their borrowed session. Handle no-render plans,
failures and a bounded submitted-frame count. Add optional actual Vulkan image
readback to verify uploads, unit tests for ownership/error paths, and runtime
tests for both placements. Capture all submitted projection/quad layers inside
the test runtime and compare both eyes against shared references. Reject
projection-only captures as overlay evidence. Keep GPU readback tests alongside
the goldens. Keep physical headset blending validation separate.
```

Use the [reproduction and evaluation protocol](../../VALIDATION.md#source-reproduction-and-generation-evaluation) when comparing a new
generation with the checked-in sample. No fresh independent generation has been
evaluated by this record.

</details>
