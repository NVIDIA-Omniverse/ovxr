# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/) and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.2.0] - 2026-11-17

### Summary

- OVXR 0.2 targets OVRTX 0.7 and OVStage 0.4 and remains a pre-release API;
  use matching SDK builds and follow the [migration guide](docs/MIGRATION.md).
- Highlights include handheld-only runtimes, renderer warm-up, OpenXR event
  observers, and application-stepped loops that keep auxiliary sensors running
  while waiting for the next XR frame.

### Fixed

- Context destruction safely drains OVXR-owned work while retaining unresolved
  frame handles for final disposal; see [teardown ownership](README.md#split-frame-ownership-during-teardown)
  before unloading the library.
- Handheld-only runtimes, including CloudXR serving an iPad, now work through
  automatic form-factor fallback while disconnected headsets remain a wait
  condition; see [display selection](README.md#display-selection).
- OpenXR instance creation retries with API 1.0 only when the runtime rejects
  1.1, avoiding duplicate diagnostics for unrelated failures such as a stopped
  runtime service.
- Windows C samples built with the dynamic SDK targets now stage OVStage's
  population plugin beside the executable, so `minimal-xr` and the other samples
  create their Stage instead of exiting with `ovstage_create_instance failed`.
  The executable's `plugins/` directory is now a real directory whose
  subdirectories link into the OVRTX SDK, and neither SDK is modified.

### Added

- `ovxr_check_openxr_runtime()` / `ovxr.check_openxr_runtime()` distinguish an
  unusable runtime from a disconnected device before renderer startup, enabling
  early failure or waiting; see [startup guidance](docs/MIGRATION.md#check-startup-outputs-and-teardown).
- C and Python OpenXR event observers expose events after internal handling
  without competing for the runtime queue; see [callback ownership and restrictions](README.md#observing-openxr-events).
- Python `LogSeverity` and `set_log_callback()` expose native OVXR logging
  with severity filtering and safe callback replacement, allowing applications
  to collect diagnostics; see [Python logging](README.md#python-logging).
- `replace_scene_layer` lets applications submit their own scene-plus-UI
  composition instead of appending layers over the original scene; see
  [composition integration](skills/composition-integration/SKILL.md).
- `ovxr_warm_up()` / `Context.warm_up()` avoid startup throttling by warming
  the renderer before displayed frames; unavailable in application-stepped mode,
  with usage in the [quick start](README.md#python-quick-start).
- Examples add a robot scene with offline fallback, floor-level startup,
  automatic display selection, composition layers, and interactive OVUI panels;
  see the [sample gallery](examples/gallery/README.md).

### Changed

- Application-stepped contexts no longer block in `begin_frame()` and must
  handle `NOT_READY` while waiting, allowing auxiliary sensors to step;
  see [migration guidance](docs/MIGRATION.md#adapt-application-stepped-frame-loops).
- OVRTX step-result handles are strongly typed in frame plans and submissions,
  requiring source updates in C/C++ and Python; see [type migration](docs/MIGRATION.md#update-source-types-and-scene-loading).
- OVStage ordinals are signed 64-bit values, with zero still reserved for
  OVXR's private ledger; migrate unsigned storage and sentinels using the
  [ordinal guidance](docs/MIGRATION.md#update-source-types-and-scene-loading).
- CUDA readback returns the latest completed capture without polling or
  waiting, so acquisition may repeat a sequence; see [readback ownership](README.md#frame-readback-ownership-and-lifetime)
  before retaining pointers.
- Application-provided foveated products now reject incompatible dimensions
  or warp settings rather than overwriting them; see the [descriptor contract](include/ovxr/ovxr.h)
  before supplying or resizing products.
- Startup reporting uses the shared `NVIDIA_OMNIVERSE_PRODUCT_TELEMETRY_DISABLED=1`
  opt-out; the deprecated `OVX_PHONE_HOME_OPT_OUT=1` remains accepted, and
  [telemetry documentation](TELEMETRY.md) describes the unchanged report data.
- Context creation rejects `max_frames_in_flight` above 64, while zero still
  selects one frame; see [frame-plan limits and ownership](README.md#split-frame-ownership-during-teardown)
  when configuring split-frame loops.
- De-moire is disabled on every XR RenderProduct, including supplied products,
  matching Kit XR; restore application settings before reuse outside XR as
  described in [migration guidance](docs/MIGRATION.md#check-startup-outputs-and-teardown).
- Vulkan submission requires exportable timeline semaphores and CUDA external
  memory interop, replacing the CPU-blocking fallback; check [Vulkan requirements](README.md#vulkan-requirements)
  when selecting a driver and runtime.
- Pending CUDA-to-Vulkan and CUDA-to-D3D12 handoff work is drained before
  resize or session teardown releases its resources, preventing cleanup from
  freeing images still in use.

### Limitations

- `STRICTLY_INCREASING` OVStage ordinal admission remains unsupported;
  use `NONDECREASING` or `FLOOR_ONLY`, with the support anticipated in the
  0.1 notes deferred to a future release.
- Packaged platforms remain Windows x86_64 and Linux x86_64, with Linux
  aarch64 packages not yet offered.
- Readback remains CUDA `RGBA8_UNORM` for one configured render product,
  excluding application composition layers and depth capture.

## [0.1.0] - 2026-10-19

### Summary

- Initial pre-release of OVXR (builds published as `0.1.0.<build>`). API,
  behavior, and packaging may change before GA.
  OVXR renders an application's OVRTX/OVStage scene to an OpenXR headset for
  VR or AR, handling the session, per-eye cameras, frame submission, and tracking.
  Targets OVRTX 0.6.0 and OVStage 0.3.0, installed alongside OVXR.

### Added

- C and Python APIs for mono, stereo, and quad views with Vulkan or Windows
  D3D12, depth submission, AR compositing, and local foveation with optional
  unwarping, plus runnable C and Python examples that load a USD stage.
- Frame control through a convenience render call or explicit begin, bind,
  submit, and cancel operations, with caller-owned OVStage ordinals for shared
  stages, private ordinals for sole writers, and cooperative session shutdown.
- Application composition layers and render products, borrowed Vulkan session
  handles, and an optional application-stepped rendering mode for integrating
  application graphics with OVXR's frame lifecycle.
- Prim-based tracking anchors, managed-anchor placement and reset, world-transform
  updates, and pose conversion for applications sharing OVXR's OpenXR handles.
- CUDA color-frame readback with capture metadata and Python wrappers that guard
  against expired frame access.
- Session/GPU diagnostics, an OVXR log callback, and anonymous library-startup
  reporting with C/Python opt-out APIs and the `OVX_PHONE_HOME_OPT_OUT=1`
  environment setting.

### Limitations

- Frame readback supports `RGBA8_UNORM` and one configured render product per
  context. Foveated captures remain warped unless readback unwarping is enabled.
- The OpenXR loader is included; an OpenXR runtime such as CloudXR, SteamVR,
  or Monado must be installed separately.
- OVXR requires the `NONDECREASING` (default) or `FLOOR_ONLY` OVStage ordinal
  admission policy. `STRICTLY_INCREASING` is not supported, and context
  creation fails when the stage's process selects it. Support is planned for
  OVXR 0.2.

[0.2.0]: https://github.com/NVIDIA-Omniverse/ovxr/compare/v0.1.0...v0.2.0
[0.1.0]: https://github.com/NVIDIA-Omniverse/ovxr/releases/tag/v0.1.0
