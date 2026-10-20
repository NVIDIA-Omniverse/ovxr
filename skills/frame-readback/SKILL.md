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

Metadata is copied into the Python Frame. Its device pointer remains borrowed and read-only. OVXR double-buffers capture: N expires before capture N+2 can begin, or on context destruction. The Python guard conservatively expires it at entry to the second subsequent `render_frame` or `submit_frame` call even if that call produces no capture. An overlapping in-flight call counts as the first; observing capture N+2 through acquisition also expires it. Expired device-pointer access raises `FrameExpiredError`; validity is diagnostic and cannot reserve the buffer.

The guard does not pin a retrieved integer pointer. Serialize context capture/destruction with the application's source-reading lease and finish CUDA reads before allowing expiry. Copy into owned staging when a downstream consumer needs longer lifetime; release the source lease before slow transport work. Never free or modify OVXR's source buffer.

Record product, format, extent, pitch, GPU identity, sequence and predicted display time. Do not infer the capture includes an application overlay or has the same domain as submitted headset images.

Read [foveation-and-display](../foveation-and-display/SKILL.md) for independent capture unwarp. Desktop resizing, cadence, reconnect UI and transport are application policies.

## Examples

For an asynchronous encoder, copy the capture into owned GPU storage while the source lease is held. Release that lease before network delivery so slow consumers cannot retain the borrowed source.
