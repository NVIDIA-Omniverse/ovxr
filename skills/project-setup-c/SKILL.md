---
name: project-setup-c
description: Configure a C or C++ application against the OVXR native package. Use for CMake setup, linking, or platform runtime deployment.
license: LicenseRef-NvidiaProprietary
metadata:
  author: "mbickleyNV <129252267+mbickleyNV@users.noreply.github.com>"
  version: 0.1.0
  tags: [ovxr, openxr, sdk-integration]
  domain: graphics
---

# Project Setup C

## Requirements

Requires a matched distributed OVXR, OVRTX and OVStage SDK and a supported OpenXR runtime. No Kit or implementation-source checkout is required.

## Instructions

Install compatible OVXR, OVRTX and OVStage packages side by side and expose their prefixes through `CMAKE_PREFIX_PATH`. Use the distributed CMake package configuration; it resolves the sibling dependencies rather than requiring application guesses about loader libraries.

Link the package targets `ovxr::ovxr`, `ovrtx::ovrtx` and `ovstage::ovstage`, and apply the matching runtime setup helpers. Windows requires the shim, implementation payload and OpenXR loader alongside the executable. Linux runtime lookup follows the package's rpath configuration.

The current helper has no published fallback download pin. Supply an approved complete package; do not invent a public release URL or ship a lone shared library without plugins and shaders.

Include the matched public header instead of reproducing ABI structs. Read [context-creation](../context-creation/SKILL.md) and [stage-publication](../stage-publication/SKILL.md) before embedding OVXR in another engine. Report native loading and first-frame results separately.

## Examples

For a native headset application, configure the supplied SDK prefixes, link package targets, and deploy their matched runtime files. Confirm native loading before diagnosing session readiness.
