---
name: scene-occluded-ui
description: Build world-space OVUI panels whose pixels and pointer input respect scene depth in an OVXR application. Use for spatial panels and property inspectors that must be hidden by nearer geometry.
license: LicenseRef-NvidiaProprietary
---

# Scene-occluded UI

Use [composition-integration](../composition-integration/SKILL.md) for the SDK
submission contract and [openxr-input-and-extensions](../openxr-input-and-extensions/SKILL.md)
for predicted-time input. Placement and occlusion are separate choices: an
OpenXR layer can be world-fixed without depth-testing against the rendered scene.
For a HUD or an independent layer, start with composition-integration. For UI
hidden by scene objects, use this application-owned compositor.

Choose the smallest recipe: [one OVUI panel](../../examples/python/ovui-panel/README.md#adapting-this-sample),
[two draggable panels](../../examples/python/spatial-ui/README.md#adapting-this-sample), or
[scene inspector](../../examples/python/scene-inspector/README.md#adapting-this-sample).
The recipes link runnable code, dependencies, prompts and validation.

- Require the matched SDK's borrowed completed color/depth result and application
  projection submission capabilities. If unavailable, report that boundary;
  do not silently replace occlusion with overlays or invent an SDK callback.
- After bind, copy renderer-owned color/depth into application-owned storage.
  Finish reads and unmap before submit/cancel; never modify or destroy the
  borrowed result. Use each eye's pose, FOV and consistent depth convention.
- Depth-test panel pixels and nearest-hit pointer input against the scene.
  Adding a depth extension to an independent quad is not proof of this behavior.
  Retain pointer capture, cancellation on focus/tracking loss and release-to-rearm.
- Submit the complete composed scene through the sample's replacement projection
  path; keep its layer descriptors and swapchains alive through submission.
  Destroy app resources before their borrowed session and renderer.
- Keep the current limitations visible: Python OVUI adapter, unfoveated stereo,
  simple panel planes/picking, synchronous CPU staging between GPUs. General
  surfaces, AR and headset latency require separate implementation and evidence.

Validate both eyes with visible and hidden panel regions, actual widget events,
blocked input and interrupted drags. Use [spatial-interaction](../spatial-interaction/SKILL.md)
when adding object editing or shared undo. See [coverage and limits](../COVERAGE.md).
