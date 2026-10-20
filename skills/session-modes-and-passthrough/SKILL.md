---
name: session-modes-and-passthrough
description: Configure OVXR VR/AR session and environment blending. Use for passthrough, opaque backgrounds, blend negotiation or alpha submission.
license: LicenseRef-NvidiaProprietary
metadata:
  author: "mbickleyNV <129252267+mbickleyNV@users.noreply.github.com>"
  version: 0.1.0
  tags: [ovxr, openxr, sdk-integration]
  domain: graphics
---

# Session Modes And Passthrough

## Requirements

Requires a matched distributed OVXR, OVRTX and OVStage SDK and a supported OpenXR runtime. No Kit or implementation-source checkout is required.

## Instructions

Session mode selects the requested experience; environment blend mode specifies how rendered pixels combine with the environment. AR selection alone does not guarantee passthrough, and alpha-blend selection does not create transparent renderer pixels.

Inspect runtime support and the resolved blend mode. OPAQUE replaces the view; ALPHA_BLEND uses rendered alpha; ADDITIVE adds rendered light. AUTO is negotiation, not an acceptance result. Treat mode changes as context/runtime configuration unless the installed API documents a live operation.

Trace failures through renderer alpha, OVXR submission and runtime/client presentation separately. A desktop RGB preview with a black background cannot establish submitted alpha. Runtime-specific passthrough capability belongs to the runtime/client, not OVXR's enum.

Keep material policies, dome visibility, matte receivers and exposure separate from OpenXR session settings. OVXR does not supply a viewer settings panel or guarantee arbitrary live renderer attribute writes invalidate active eye products.

Use current OVRTX schemas for renderer-output settings and [stage-publication](../stage-publication/SKILL.md) for writes. Validate a submitted transparent-background fixture on the intended client before claiming passthrough support.

## Examples

For passthrough acceptance, use a transparent-background fixture plus an opaque dark object. Inspect submitted alpha on the intended client; a black desktop RGB window is insufficient evidence.
