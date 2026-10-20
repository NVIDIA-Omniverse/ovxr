---
name: application-flow
description: Integrate the OVXR lifecycle with an application-owned OVRTX renderer and OVStage. Use for a minimal XR render program or lifecycle design.
license: LicenseRef-NvidiaProprietary
metadata:
  author: "mbickleyNV <129252267+mbickleyNV@users.noreply.github.com>"
  version: 0.1.0
  tags: [ovxr, openxr, sdk-integration]
  domain: graphics
---

# Application Flow

## Requirements

Requires a matched distributed OVXR, OVRTX and OVStage SDK and a supported OpenXR runtime. No Kit or implementation-source checkout is required.

## Instructions

The application creates and owns the renderer and stage. Follow the public README quick start: construct `RendererConfig(read_gpu_transforms=True, keep_system_alive=True)` before the renderer. Create the stage under OVStage's default `NONDECREASING` ordinal admission policy, or select `NONDECREASING` or `FLOOR_ONLY` first; OVXR 0.1 rejects `STRICTLY_INCREASING` (see [stage-publication](../stage-publication/SKILL.md)). Attach that stage to the renderer, populate and publish content, create OVXR over both borrowed objects, drive one frame path, then destroy OVXR before the renderer and stage.

Choose the context's ordinal mode before rendering: [stage-publication](../stage-publication/SKILL.md) distinguishes a sole writer from an embedding app that co-writes the stage. Use [frame-loop](../frame-loop/SKILL.md) for convenience or split scheduling. Keep one OpenXR frame owner. Default mode lets OVXR step the renderer; an SDK's documented application-stepped opt-in changes step ownership as described by frame-loop.

Runtime/service hosting is separate from rendering; read [runtime-adapters](../runtime-adapters/SKILL.md) for CloudXR. A missing client/system is a readiness state, not evidence of broken scene population.

Application navigation, selection, gestures, UI and desktop delivery belong to the embedding app. This lifecycle supplies their SDK boundaries, not a complete viewer shell. For shutdown, call `ovxr_request_exit()` or `Context.request_exit()` to cooperatively wake an in-flight frame. This may overlap frame calls but must not overlap destroy. The owner thread pumps session events, ends the session at STOPPING, cancels live plans and destroys the context. Quiesce consumers and application GPU reads and retain borrowed owners until teardown finishes.

After OVRTX initialization and before context creation, route shipped-library diagnostics into the host with the process-global `ovxr_set_log_callback` API. Retain the callback and user data for their registered lifetime; copy messages if retained beyond a callback. Callbacks can run on any thread, so enqueue host work rather than making reentrant context calls. Follow the public header's replacement, severity and lifetime rules.

Use `ovxr_probe_openxr()` or `Context.probe_openxr()` to report runtime, system, view and graphics discovery separately from scene loading. Python carries the result code in the probe so partial discovery can be logged. A successful probe does not prove submitted pixels.

## Examples

For a minimal stereo host, populate the borrowed stage, attach it to the renderer, create one OVXR context and drive one frame path. Quiesce consumers and destroy OVXR before its borrowed owners.
