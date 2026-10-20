---
name: frame-loop
description: Drive OVXR convenience or begin, bind and submit frame scheduling. Use for frame-plan ownership, no-render states, timing or cancellation.
license: LicenseRef-NvidiaProprietary
metadata:
  author: "mbickleyNV <129252267+mbickleyNV@users.noreply.github.com>"
  version: 0.1.0
  tags: [ovxr, openxr, sdk-integration]
  domain: graphics
---

# Frame Loop

## Requirements

Requires a matched distributed OVXR, OVRTX and OVStage SDK and a supported OpenXR runtime. No Kit or implementation-source checkout is required.

## Instructions

OVXR provides default OVXR-stepped frames, additional composition layers and an opt-in application-stepped mode. Match the installed header and bindings for the selected SDK.

In default OVXR-stepped mode, use one path per iteration: `render_frame`, or `begin_frame, then bind_frame, then submit_frame`. A sole stage writer may omit explicit bind and let submit bind. Co-writers bind inside their publication rendezvous with an application-owned ordinal. Bind updates and steps the renderer; OVXR owns and destroys that result.

Python `render_frame` returns result, frame result and ordinal-sealed report; `begin_frame` returns result and plan; `bind_frame` returns result and seal report; `submit_frame(plan, additional_layers=...)` returns result and frame result. C uses `ovxr_submit_frame(ctx, plan, submit_desc, out)`. Keep the [application layers](../composition-integration/SKILL.md) and their resources alive through submit.

In SDKs exposing `ovxr_desc_t::application_steps_renderer`, explicitly opt in before context creation. Bind seals stage writes and records the committed render ordinal but stops before renderer update/step. The application runs `ovrtx_update_from_stage` and `ovrtx_step_with_stage` at the plan render ordinal over the plan render products under its publication lock, then supplies the result through the submit descriptor's step_result. The application waits for and destroys that result after submit returns; OVXR does not destroy it. Convenience render and implicit bind are rejected in this mode. In default mode the caller-supplied step_result must be zero.

The same release API provides `ovxr_set_frame_render_products`: application products join the eyes in one step. In default mode the plan step_result is borrowed between successful bind and submit or cancel; unmap all application output mappings before that consuming call and never destroy OVXR's result. Application-stepped mode leaves that plan result zero because the app owns its result. Confirm equivalent Python exports rather than inventing keyword arguments.

Use runtime-suggested delta timing (zero selects it). A no-render boundary completes without rendering and latches the anchor snapshot described by the public README. NOT_READY and TIMEOUT are readiness states; use bounded recovery and inspect diagnostics.

Validation rejection leaves a live plan. Once submission accepts ownership, every return path consumes it and clears plan handles. A finally block cancels only a still-live plan. Bind errors require the seal reconciliation in [stage-publication](../stage-publication/SKILL.md), independently of plan disposal.

The public README specifies an immutable anchor snapshot at begin or render, shared by both eyes; getter publication follows the complete camera write. Read [input timing](../openxr-input-and-extensions/SKILL.md). Use `Context.request_exit()` or `ovxr_request_exit()` to wake an in-flight frame for owner-thread shutdown; destruction remains exclusive.

## Examples

For default-mode UI layers, explicitly bind inside the publication rendezvous and submit the live layers with the plan. For a matched application-stepped SDK, opt in, explicitly bind, step once at the plan ordinal, submit the app-owned result, then wait and destroy it.
