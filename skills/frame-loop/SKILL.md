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

In SDKs exposing `ovxr_warm_up`, call it (Python `warm_up`) once after context creation and before the first frame. It renders ovRTX's slow first step (shader compilation, scene loading, initial accumulation) outside any OpenXR frame while submitting empty frames, so the runtime does not throttle the frames that follow. It takes the same stage ordinal and seal report as bind, returns NOT_READY when no renderable frame arrives within 30 seconds, and is unavailable in application-stepped mode.

Python `render_frame` returns result, frame result and ordinal-sealed report; `warm_up` returns result, warm-up result and ordinal-sealed report; `begin_frame` returns result and plan; `bind_frame` returns result and seal report; `submit_frame(plan, additional_layers=...)` returns result and frame result. C uses `ovxr_submit_frame(ctx, plan, submit_desc, out)`. Keep the [application layers](../composition-integration/SKILL.md) and their resources alive through submit.

In SDKs exposing `ovxr_desc_t::application_steps_renderer`, explicitly opt in before context creation. Bind seals stage writes and records the committed render ordinal but stops before renderer update/step. The application runs `ovrtx_update_from_stage` and `ovrtx_step_with_stage` at the plan render ordinal over the plan render products under its publication lock, then supplies the result through the submit descriptor's step_result. The application waits for and destroys that result after submit returns; OVXR does not destroy it. Convenience render and implicit bind are rejected in this mode. In default mode the caller-supplied step_result must be `OVRTX_INVALID_STEP_RESULT_HANDLE` in C/C++ or `ovrtx.INVALID_STEP_RESULT_HANDLE` in Python.

The same release API provides `ovxr_set_frame_render_products`: application products join the eyes in one step. In default mode the plan step_result is borrowed between successful bind and submit or cancel, or until context destruction if earlier; unmap all application output mappings before that consuming call or destruction and never destroy OVXR's result. The render_products array has the same lifetime. Application-stepped mode leaves that plan result at the matching typed invalid value because the app owns its result. Confirm equivalent Python exports rather than inventing keyword arguments.

Use runtime-suggested delta timing (zero selects it). A no-render boundary completes without rendering and latches the anchor snapshot described by the public README. NOT_READY and TIMEOUT are readiness states; use bounded recovery and inspect diagnostics.

Keep one owning plan from each successful begin until it is consumed; do not copy its owning frame handle or overwrite an unresolved plan. Validation rejection with a live context leaves the plan available for correction or cancellation. Once submission accepts ownership, every return path consumes it and clears plan handles. A finally block cancels only a still-live plan. Bind errors require the seal reconciliation in [stage-publication](../stage-publication/SKILL.md), independently of plan disposal.

Context destruction drains OVXR-owned frame work and discards the open OpenXR frame before Stage and session teardown. All borrowed plan views expire then, even if their fields remain nonzero, but a retained plan still needs disposal. After destruction, bind returns ERROR and leaves the plan intact; submit returns ERROR and consumes it; cancel consumes it without accessing the former context. Python callers can use `ctx.cancel_frame(plan)` after `ctx.destroy()`; C cancellation accepts a null context for a detached plan. Release every plan before unloading OVXR. Application-stepped results remain caller-owned and must be completed and released separately before their renderer or Stage is destroyed.

The public README specifies an immutable anchor snapshot at begin or render, shared by both eyes; getter publication follows the complete camera write. Read [input timing](../openxr-input-and-extensions/SKILL.md). Use `Context.request_exit()` or `ovxr_request_exit()` to wake an in-flight frame for owner-thread shutdown; destruction remains exclusive.

## Examples

For default-mode UI layers, explicitly bind inside the publication rendezvous and submit the live layers with the plan. For a matched application-stepped SDK, opt in, explicitly bind, step once at the plan ordinal, submit the app-owned result, then wait and destroy it.


## Examples and validation

Use [minimal-xr](../../examples/python/minimal-xr/README.md#adapting-this-sample), [live-scene](../../examples/python/live-scene/README.md#adapting-this-sample) for runnable source and prompt records.
See [coverage and remaining gaps](../COVERAGE.md#frame-loop) for the automated checks associated with this guidance.
