---
name: validation
description: Validate OVXR SDK contracts separately from runtime and application proof. Use for ABI checks, loader tests, frame ownership or readiness evidence.
license: LicenseRef-NvidiaProprietary
metadata:
  author: "mbickleyNV <129252267+mbickleyNV@users.noreply.github.com>"
  version: 0.1.0
  tags: [ovxr, openxr, sdk-integration]
  domain: graphics
---

# Validation

## Requirements

Requires a matched distributed OVXR, OVRTX and OVStage SDK and a supported OpenXR runtime. No Kit or implementation-source checkout is required.

## Instructions

Match validation to the claim. Binding checks cover exported signatures, ABI shape, metadata, pointer expiry, ordinal-mode enforcement and loader selection. Mocked behavior does not prove a rendered frame. Library test sources are optional maintainer evidence, not an installation prerequisite.

Validate progressively: bindings/signatures, native dependency closure, attached stage population, runtime, session and graphics readiness, then successful submitted frames and the intended client pixels. Keep native-library-dependent skips separate from offline passes.

For frame changes cover each supported step-ownership mode, application layer resource lifetime and no-render layer omission, startup opt-out ordering, exit requests and diagnostic routing, plus no-render boundaries, live-plan rejection/cancellation, bind seal-then-fail and context teardown while consumers exist. For anchors include nonidentity spawn, Y/Z-up, units and scale; for capture include expiry and overlapping acquisition. Use the installed SDK API contract for native guarantees absent from wrapper mocks.

Inspect failures at their first boundary rather than treating an accepted setting or successful probe as visual proof. Report exact package/build identity and missing capabilities.

Complete viewer validation belongs downstream: controller/tool semantics, integrated UI/scene depth, decoded-frame correlation, performance budgets and physical headset ergonomics. The library tests establish SDK contracts, not those product outcomes.

## Examples

For a first integration check, report imports, native dependencies, populated scene, runtime readiness and submitted pixels separately. Mark missing headset or native-runtime gates untested.
