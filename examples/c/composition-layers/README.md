# Independent OpenXR composition layers (C++)

Append an alpha-blended quad after the scene projection, either fixed to the
head or fixed in OpenXR tracking space. This is an overlay: scene geometry does
not occlude it. The diagnostic texture matches the
[Python example](../../python/composition-layers/README.md).

Build with the [C++ example setup](../../README.md) and the sample-support library:

```text
example.ovxr.composition-layers.exe --placement head --frames 100
example.ovxr.composition-layers.exe --placement world --frames 100 --verify-upload --report output/sample.json
```

The sample requires Vulkan and an additional OpenXR layer slot. It borrows the
session/device, owns its RGBA8 swapchain and optional VIEW space, completes GPU
uploads and releases the image before submission, and destroys those resources
before the session. `FrameGuard` cancels an unfinished frame on failure. The
original scene projection remains present; no public OVXR API is added.

`--verify-upload` reads the actual acquired Vulkan image back and checks every
byte. With `--report`, it also saves `overlay-readback.rgba` (256 × 128 RGBA8,
row-major) and a JSON submission report. This synchronous check is for validation,
not a headset latency measurement.

Private tests run both placements for 100 submissions. They retain independent
color/alpha readback and submitted-descriptor checks, and compare both composed
eyes against the same references as Python. The isolated test runtime composes
all submitted projection/quad layers using their real swapchain images, poses,
eye visibility and alpha flags. It does not model lens distortion, late-stage
reprojection or a physical headset display; those remain hardware checks.

See the [composition skill](../../../skills/composition-integration/SKILL.md),
[adaptation guide](../../python/composition-layers/README.md#adapting-this-sample) and
[prompt context (FYI)](#prompt-context-fyi).

The native adapter caches an owned copy of the uploaded quad pixels and reuses
the last released swapchain image while the contents and dimensions are unchanged.
Both languages use this adapter. Pixel changes trigger another acquire/upload/release;
`--verify-upload` deliberately repeats the upload/readback every renderable frame
so the composed-capture probes retain their release timing. This follows the
[OpenXR swapchain image lifetime](https://registry.khronos.org/OpenXR/specs/1.1-khr/html/xrspec.html#xrReleaseSwapchainImage).

## Adapting this sample

The [shared adaptation guide](../../python/composition-layers/README.md#adapting-this-sample)
covers application design, API ownership and validation for both implementations.
Use this README's C++ build/run instructions with that guidance and the
[shared project setup](../../README.md#shared-project-setup-and-ownership).

## Prompt context (FYI)

The [Python README](../../python/composition-layers/README.md#prompt-context-fyi)
records the shared request history and reconstructed prompt for both implementations.
These notes are informational; use the checked-in source for exact reproduction.
