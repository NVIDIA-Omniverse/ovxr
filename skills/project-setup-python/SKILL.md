---
name: project-setup-python
description: Configure distributed OVXR Python bindings and their native dependencies. Use for Python SDK setup, loader failures, or package capability checks.
license: LicenseRef-NvidiaProprietary
metadata:
  author: "mbickleyNV <129252267+mbickleyNV@users.noreply.github.com>"
  version: 0.1.0
  tags: [ovxr, openxr, sdk-integration]
  domain: graphics
---

# Project Setup Python

## Requirements

Requires a matched distributed OVXR, OVRTX and OVStage SDK and a supported OpenXR runtime. No Kit or implementation-source checkout is required.

## Instructions

Use a matched OVXR, OVRTX and OVStage package set. Record platform, archive/build identity and loaded module paths; an unchanged OVXR version string does not establish ABI compatibility. The authoritative public README specifies a bindings-only Python wheel: supply a matching native OVXR runtime and its OVRTX dependencies separately. OVRTX and OVStage must remain matched. Artifact-specific packaging observations do not replace that documented deployment contract.

Select an OpenXR runtime with process-local `XR_RUNTIME_JSON`. For OVXR loading, set `ovxr.OVXR_LIBRARY_PATH_HINT` to a shared-library file or containing directory for an intentional override; an unusable hint fails without fallback. Otherwise discovery checks the package-local bin, owning deploy bin and plugin directories, then the linked build plugin directory. Default discovery does not search PATH or LD_LIBRARY_PATH. Development-only system and current-directory searches require their separate documented opt-ins.

On Windows, a split deployment must put its trusted bin directory on the process native PATH for by-name payload dependencies; this is separate from Python shim discovery. On Linux OVXR preloads matching OVRTX with RTLD_GLOBAL. Follow OVRTX's own hint/candidate policy or reuse its loaded library so both bindings share one OVRTX instance. Do not change machine-wide loader or OpenXR settings.

OVXR preloads its borrowed runtime dependencies. Use the package's documented loader configuration; preserve its ABI and exported interfaces. Renderer-global settings, when needed, must be applied before renderer construction.

Validate in separate gates: Python import, matching native exports, stage population, runtime/session readiness, then a submitted image. Import success does not prove plugins, shaders, graphics support or a connected client.

For lifecycle read [application-flow](../application-flow/SKILL.md); for readiness and evidence read [validation](../validation/SKILL.md).

## Examples

For a supplied Python SDK, install its matching platform wheel in an isolated environment. Record package versions and imported module locations, then separate import success from native-load and first-frame results.
