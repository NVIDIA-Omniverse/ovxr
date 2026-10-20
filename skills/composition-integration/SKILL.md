---
name: composition-integration
description: Identify supported OVXR seams for application composition and GPU resources. Use for application composition layers, borrowed Vulkan bindings or custom projection UI.
license: LicenseRef-NvidiaProprietary
metadata:
  author: "mbickleyNV <129252267+mbickleyNV@users.noreply.github.com>"
  version: 0.1.0
  tags: [ovxr, openxr, sdk-integration]
  domain: graphics
---

# Composition Integration

## Requirements

Requires a matched distributed OVXR, OVRTX and OVStage SDK and a supported OpenXR runtime. No Kit or implementation-source checkout is required.

## Instructions

The public README and header describe the composition APIs below. Match headers, Python bindings, loader and runtime binaries; the prerelease submit signature changed without changing the dylib ABI version.

OVXR owns the scene projection and final OpenXR frame submission. C callers use `ovxr_submit_frame(ctx, plan, submit_desc, out)`; Python uses `submit_frame(plan, additional_layers=...)`. A null or zero-initialized descriptor with zero additional-layer count submits only the scene. Application projection, quad or supported extension layers are appended after the scene in array order. The runtime's maxLayerCount includes OVXR's scene layer. No-render or invalid-view frames omit all layers; CUDA readback remains scene-only.

`ovxr_get_vulkan_handles` and Python `Context.get_vulkan_handles()` expose the borrowed session Vulkan binding and enabled external-memory/semaphore handle types. Python returns a VulkanHandles value or None before the session exists. The application may import images from its own same-GPU Vulkan device and copy into application-owned OpenXR swapchains. Query format/usage support and serialize borrowed queue access with OVXR.

The application owns layer structures, views, next chains, spaces, swapchains and graphics work. Keep submitted structures alive until submit returns. Create, acquire, wait, render and release application swapchain images and complete required graphics submission before submit. OVXR does not synchronize that GPU work or manage those images. Destroy app resources before the borrowed context.

For SDKs exposing `application_steps_renderer`, the C submit descriptor accepts an application-owned step result only in that opt-in mode. Its lifetime belongs to the application; [frame-loop](../frame-loop/SKILL.md) describes explicit bind and result ownership. Confirm the selected package's exported contract before relying on it.

Core additional quads lack per-fragment scene depth. A custom primary-depth compositor still needs an explicit primary-image/depth seam and matching per-eye projection, subimage, encoding, lifetime and vendor warped-domain contracts. Borrowed Vulkan handles alone do not establish that seam. Viewer skills own compositor algorithms, surface policy and integrated visual acceptance.

## Examples

For a spatial quad, obtain the shared OpenXR and Vulkan bindings, create an application swapchain, complete and release its rendered image, and pass the live layer to submit. Include the scene layer when checking the runtime layer limit.
