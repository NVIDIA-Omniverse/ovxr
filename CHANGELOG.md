# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/) and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

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

[0.1.0]: https://github.com/NVIDIA-Omniverse/ovxr/releases/tag/v0.1.0
