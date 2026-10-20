---
name: openxr-input-and-extensions
description: Integrate OpenXR actions, hands or extensions through OVXR borrowed handles. Use for extension enablement, native input timing or shared-session ownership.
license: LicenseRef-NvidiaProprietary
metadata:
  author: "mbickleyNV <129252267+mbickleyNV@users.noreply.github.com>"
  version: 0.1.0
  tags: [ovxr, openxr, sdk-integration]
  domain: graphics
---

# Openxr Input And Extensions

## Requirements

Requires a matched distributed OVXR, OVRTX and OVStage SDK and a supported OpenXR runtime. No Kit or implementation-source checkout is required.

## Instructions

Request application extensions in `ContextDesc.extra_openxr_extensions` before instance creation. Obtain `get_openxr_handles` once the shared session exists; retry NOT_READY without recreating the renderer. Resolve extension functions through the returned instance/proc-address contract and matching runtime headers.

OVXR owns instance, session, reference space and frame lifecycle. The app owns only actions, action spaces, trackers and extension resources it creates. Do not add a second wait/begin/end/session lifecycle or destroy borrowed handles. Serialize calls with the context owner and invalidate caches on replacement.

Preserve the returned reference-space type; LOCAL does not imply a physical floor. Locate actions/joints at the current plan's predicted time where available, and validate tracking/activity before converting poses. The public README directs callers to locate and convert poses after `begin_frame`, using the returned reference space and plan predicted display time. `transform_openxr_pose` uses the same frame-latched mapping as the eyes, removing uniform scale from orientation while retaining it on positions and IPD. Do not defer all same-plan input conversion until after bind or invent an extra frame of latency.

Follow the matching OpenXR action-set attachment rules and runtime capabilities. An already attached session may reject attaching a second set; a successful extension request is not evidence of usable controller/hand data.

For opaque channels, read [runtime-adapters](../runtime-adapters/SKILL.md). Snapshot normalization, gesture recognition, input-source arbitration and tool capture belong to the application.

## Examples

For hand tracking, request the runtime extension before context creation, obtain borrowed handles after readiness and validate active joints. Destroy app-created trackers before replacing their context.
