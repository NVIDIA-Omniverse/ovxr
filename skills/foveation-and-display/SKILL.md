---
name: foveation-and-display
description: Select OVXR view configuration, foveation policy and capture domain. Use for warped images, eye resolution, feature negotiation or multi-GPU topology.
license: LicenseRef-NvidiaProprietary
metadata:
  author: "mbickleyNV <129252267+mbickleyNV@users.noreply.github.com>"
  version: 0.1.0
  tags: [ovxr, openxr, sdk-integration]
  domain: graphics
---

# Foveation And Display

## Requirements

Requires a matched distributed OVXR, OVRTX and OVStage SDK and a supported OpenXR runtime. No Kit or implementation-source checkout is required.

## Instructions

Keep local rendering, headset submission and readback as separate contracts. Inspect current enums and device features; current local foveation uses AUTO, DISABLED and WARPED. Do not reuse historical INSET numeric values.

Display AUTO prefers stereo and falls back to mono. Explicit unavailable display configurations report NOT_READY. Supply one product/camera for mono, two for stereo or four for QUAD_VIEW, ordered outer-left, outer-right, inset-left, inset-right. AUTO accepts a stereo-ordered pair and uses its first entry after mono fallback.

For quad view, local foveation AUTO resolves to disabled; explicit WARPED configures all four products independently. Disabled local foveation neutralizes inherited source-product foveation. Other AUTO feature requests require inspecting the negotiated result. Record actual view count, per-eye extents, graphics/composition GPU and feature state. A NOT_READY topology query exposes features but unresolved device IDs; unknown values remain unknown.

`foveation_unwarp` influences authored submission outputs. `readback_foveation_unwarp` independently requests unwarped capture for runtime-created foveated products. Capture configuration cannot prove the headset domain or vendor mapping used by a custom compositor.

Use a fully explicit disabled baseline only when selected: disable local foveation and runtime foveated submission, and inspect actual output. Test warped operation independently. Do not disable a requested feature merely to obtain a desktop preview.

Runtime recommendations size authored eye products. Update existing products through the documented live resolution method; measure actual rendered and negotiated extents. Desktop sizing must not change headset products. Product topology and unsupported configuration changes require a new context.

OVXR exposes no multi-GPU assignment setting and does not change OVRTX's active GPU set. Cloned stereo/quad products clear inherited deviceIds so OVRTX applies its policy; prepared products retain authored assignments. Mono is assigned once before the first render to the composition GPU's process-visible CUDA ordinal. That GPU must be CUDA-visible and active in OVRTX, otherwise setup reports ERROR_DEVICE_MISMATCH. Device info reports placement observed from rendered outputs.

PQW/vendor warp definitions come from the runtime owner. Core OpenXR or an image's dimensions cannot establish its forward/inverse mapping. Read [composition-integration](../composition-integration/SKILL.md) before using these images for application UI.

## Examples

For a desktop companion to a warped headset session, request the supported capture domain independently. Measure actual output; an unwarped mirror does not prove rectilinear headset submission.
