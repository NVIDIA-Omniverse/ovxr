# Migrating from OVXR 0.1 to 0.2

OVXR 0.2 uses OVRTX 0.7 and OVStage 0.4, replacing the OVRTX 0.6 / OVStage 0.3
dependencies of OVXR 0.1. See the [changelog](../CHANGELOG.md) for the complete
release notes and the [README](../README.md) for integration contracts.

## Install matching SDKs

Update OVXR, OVRTX, and OVStage together. Release packages record exact build
versions in their dependency metadata; the source tree's development ranges
are not a substitute for that record. Rebuild C/C++ applications against the
matching headers and libraries, and install matching Python bindings and
native payloads. Do not copy a new header or Python package onto a 0.1 binary
installation. Public structures and required symbols have evolved; the
pre-release ABI identifier alone does not establish compatibility.

## Update source types and scene loading

| Area | OVXR 0.2 requirement |
| --- | --- |
| OVRTX results | Keep `ovrtx_step_result_handle_t` throughout frame plans and submissions. Use `OVRTX_INVALID_STEP_RESULT_HANDLE` for an absent result. Python also checks handle kinds; a raw integer or a different handle type is not a substitute. |
| OVStage ordinals | Use signed `ovstage_ordinal_t` and the matching OVStage bounds constants. Zero still selects OVXR's private ledger; application mode requires a nonzero ordinal above the write floor. Negative nonzero ordinals are valid. |
| Scene population | Replace removed `PopulationDomain.ALL` usage with population descriptions. Include rendering data and the `upAxis` and `metersPerUnit` stage metadata needed by OVXR. The C++ and Python minimal-xr samples show this setup. |
| Frame-plan limit | Set `max_frames_in_flight` to zero for the default of one, or an explicit value from 1 through 64. Larger values are rejected at creation. |

Use the sibling SDK migration guides for their other API changes. OVXR still
requires `NONDECREASING` or `FLOOR_ONLY` OVStage ordinal admission;
`STRICTLY_INCREASING` support did not land in 0.2.

## Adapt application-stepped frame loops

The default OVXR-stepped path still waits for the next OpenXR frame inside
`begin_frame()`. With `application_steps_renderer=True`, however, it now starts
or polls an internal wait and returns `NOT_READY` while the wait is pending.
Treat this as normal scheduling: service application work and poll again.
Only a successful begin yields an owning plan to bind, submit, or cancel.

If the application steps auxiliary sensors between XR frames:

1. Use application-owned ordinals and serialize stage publication with renderer
   reads. Keep the eye products in every step's product set to preserve history.
2. Before each step without a renderable bound XR frame, publish signed `int32`
   `omni:sensor:frameLimit = 0` on the eye **cameras**, not the RenderProducts.
   Do this for pending polls, no-render frames, and steps after cancellation.
3. A renderable bind publishes the eye poses and `frameLimit = -1`. Update and
   step OVRTX at `plan.render_ordinal` over `plan.render_products`, then pass
   the application-owned result to submission and complete/destroy it afterward.

Polling, submission, and cancellation never publish a pause for the application.
A camera cut or expired scene-transform history may require clearing OVRTX
history before resuming. See [auxiliary sensors between XR frames](../README.md#auxiliary-sensors-between-xr-frames).

## Check startup, outputs, and teardown

- Use `check_openxr_runtime()` before renderer startup to distinguish an unusable
  runtime from a supported device that has not connected yet. `DisplayMode.AUTO`
  supports stereo headsets and mono handheld clients. Call `warm_up()` before
  the display loop in the default OVXR-stepped path; it is unavailable in
  application-stepped mode (`OVXR_ERROR`).
- Vulkan submission requires CUDA external-memory interop and exportable timeline
  semaphores; the CPU-blocking fallback is gone. See [Vulkan requirements](../README.md#vulkan-requirements).
- Register OpenXR event observers instead of competing with OVXR's `xrPollEvent`.
  Callbacks borrow native event data and cannot re-enter the same context.
- Resize supplied products through `update_render_product_resolutions()` so
  OVXR invalidates its cached geometry. Supplied foveation must satisfy the
  descriptor contract. OVXR leaves its eco-mode and de-moire overrides authored
  on supplied products after context destruction; restore them before reuse
  outside XR if needed.
- Readback returns the last completed color capture and may repeat a sequence
  while newer work is pending. It excludes application composition layers and
  does not provide a depth capture. Preserve the documented borrowed-pointer lifetime.
- Unmap borrowed outputs before submit, cancel, or context destruction. Destroying
  a context invalidates borrowed views but does not dispose retained owning frame
  handles: cancel those plans before unloading OVXR. Complete and destroy
  application-owned step results separately.
- The shared telemetry opt-out is now
  `NVIDIA_OMNIVERSE_PRODUCT_TELEMETRY_DISABLED=1`; the old
  `OVX_PHONE_HOME_OPT_OUT=1` remains accepted but is deprecated.
