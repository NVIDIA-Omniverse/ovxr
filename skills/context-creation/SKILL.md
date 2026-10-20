---
name: context-creation
description: Choose OVXR context descriptor, runtime, scene metadata and lifecycle settings. Use for context creation failures or live-versus-recreate configuration.
license: LicenseRef-NvidiaProprietary
metadata:
  author: "mbickleyNV <129252267+mbickleyNV@users.noreply.github.com>"
  version: 0.1.0
  tags: [ovxr, openxr, sdk-integration]
  domain: graphics
---

# Context Creation

## Requirements

Requires a matched distributed OVXR, OVRTX and OVStage SDK and a supported OpenXR runtime. No Kit or implementation-source checkout is required.

## Instructions

Before the first successful create, choose the host's startup-reporting policy. OVXR sends one anonymous library-initialization report per process. To opt out, set `OVX_PHONE_HOME_OPT_OUT=1` at process launch, or call `ovxr_library_telemetry_opt_out()` / `ovxr.library_telemetry_opt_out()` before that first create. The programmatic opt-out loads the implementation payload early and brings its dependency graph forward; configure native loading before calling it. Calling it after create cannot retract an already sent report. Context recreation does not send another startup report. The public header also documents that opting out avoids reporter-worker shutdown waits.

Choose descriptor values from the installed SDK API documentation and exported bindings. The renderer and OVStage are required borrowed objects; the renderer must use that same attached stage. The stage's process must use OVStage's default `NONDECREASING` ordinal admission policy or `FLOOR_ONLY`. The first process reference fixes the policy, so select one explicitly before anything else creates or initializes OVStage. Creation fails when the process selected `STRICTLY_INCREASING`, which OVXR 0.1 does not support; support is planned for OVXR 0.2. Creation performs no stage writes. Runtime products, eye cameras and the managed anchor prim are authored at the first renderable bind. Anchor snapshot timing follows the public README boundary contract; prim creation and snapshot latching are separate concerns.

Resolve units/up-axis through the selected scene-info policy and inspect the resolved result. Request application OpenXR extensions before instance creation. Optional extensions may be skipped when unavailable; a descriptor request is not proof of usable native input.

Use documented live anchor, transform and product-resolution operations only within their contracts. Product topology, ordinal mode, graphics choice and other construction settings require context replacement when no live setter exists. Runtime backend/profile changes belong to that runtime's service lifetime.

Read [tracking-anchor-and-scale](../tracking-anchor-and-scale/SKILL.md), [foveation-and-display](../foveation-and-display/SKILL.md) and [session-modes-and-passthrough](../session-modes-and-passthrough/SKILL.md) for focused settings. Replacing a context invalidates borrowed handles and captured pointers; retain renderer/stage only where compatible.

## Examples

For a stage shared with simulation, create the context in application ordinal mode. Request required OpenXR extensions before creation, and retain requested settings independently of native context lifetime.
