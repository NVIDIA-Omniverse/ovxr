---
name: frame-readback
description: Consume OVXR CUDA frame captures without violating borrowed buffer lifetime. Use for acquire-frame, expired pointers, capture correlation or readback domains.
license: LicenseRef-NvidiaProprietary
metadata:
  author: "mbickleyNV <129252267+mbickleyNV@users.noreply.github.com>"
  version: 0.1.0
  tags: [ovxr, openxr, sdk-integration]
  domain: graphics
---

# Frame Readback

## Requirements

Requires a matched distributed OVXR, OVRTX and OVStage SDK and a supported OpenXR runtime. No Kit or implementation-source checkout is required.

## Instructions

Enable readback for an actual configured product and acquire the latest capture with `Context.acquire_frame`. No capture returns None; readback is best effort and does not change the frame result.

Metadata is copied into the Python Frame. Its device pointer remains borrowed and read-only. OVXR triple-buffers capture: N expires before capture N+3 can begin, or on context destruction. Completion and mapping retirement run on the render thread during frame begin, submission, and cancellation. Acquisition only reads the latest published view; it never polls CUDA, calls OVRTX, or waits for capture cleanup. If frame calls stop, pending resources remain until the next frame call or context destruction. Disabling capture still allows pending resources to retire without publishing the disabled capture. While a copy is pending, new captures are skipped. The Python guard conservatively expires the view at entry to the second subsequent `render_frame` or `submit_frame` call even if that call produces no capture. An overlapping in-flight call counts as the first; observing capture N+2 through acquisition also expires it. Expired device-pointer access raises `FrameExpiredError`; validity is diagnostic and cannot reserve the buffer.

The guard does not pin a retrieved integer pointer. Serialize context capture/destruction with the application's source-reading lease and finish CUDA reads before allowing expiry. Copy into owned staging when a downstream consumer needs longer lifetime; release the source lease before slow transport work. Never free or modify OVXR's source buffer.

Record product, format, extent, pitch, GPU identity, sequence and predicted display time. Do not infer the capture includes an application overlay or has the same domain as submitted headset images.

Read [foveation-and-display](../foveation-and-display/SKILL.md) for independent capture unwarp. Desktop resizing, cadence, reconnect UI and transport are application policies.

## Examples

For an asynchronous encoder, copy the capture into owned GPU storage while the source lease is held. Release that lease before network delivery so slow consumers cannot retain the borrowed source.


## Examples and validation

Use [frame-capture](../../examples/python/frame-capture/README.md#adapting-this-sample), [async-capture](../../examples/python/async-capture/README.md#adapting-this-sample) for runnable source and prompt records.
See [coverage and remaining gaps](../COVERAGE.md#frame-readback) for the automated checks associated with this guidance.
