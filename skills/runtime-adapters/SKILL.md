---
name: runtime-adapters
description: Integrate a separately supplied OpenXR runtime with OVXR. Use for runtime/service readiness or CloudXR opaque-channel handoff.
license: LicenseRef-NvidiaProprietary
metadata:
  author: "mbickleyNV <129252267+mbickleyNV@users.noreply.github.com>"
  version: 0.1.0
  tags: [ovxr, openxr, sdk-integration]
  domain: graphics
---

# Runtime Adapters

## Requirements

Requires a matched distributed OVXR, OVRTX and OVStage SDK and a supported OpenXR runtime. No Kit or implementation-source checkout is required.

## Instructions

OVXR is runtime-independent. Choose and start the runtime through its own distribution and documentation, then select its manifest process-locally. OVXR's probe/session/graphics state establishes whether rendering can proceed.

For CloudXR, read [the adapter boundary](references/cloudxr.md). Service hosting and vendor extension ABIs are supplied by CloudXR, not by OVXR. Use its matching SDK headers and client documentation; this directory does not redistribute that runtime.

OVXR exposes borrowed instance/session/reference-space handles for application extension consumers. Request their extensions at context creation, respect frame-thread serialization and destroy only application-owned resources.

Client choice, user connection flow, message semantics and reconnect state remain application concerns. A runtime-specific backend or port is not a generic OVXR protocol setting.

## Examples

For a CloudXR host awaiting its client, retain renderer state and report readiness. Verify the installed runtime listener and client profile independently from OVXR frame ownership.
