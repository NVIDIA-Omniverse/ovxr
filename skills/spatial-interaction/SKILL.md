---
name: spatial-interaction
description: Build application-owned XR picking, dragging, navigation and undoable tools shared with desktop input. Use for OVXR workbenches and inspectors after the SDK input and anchor contracts are selected.
license: LicenseRef-NvidiaProprietary
---

# Spatial interaction

Read [openxr-input-and-extensions](../openxr-input-and-extensions/SKILL.md) and
[tracking-anchor-and-scale](../tracking-anchor-and-scale/SKILL.md) for borrowed
handles, predicted-time input and the latched coordinate transform. Application
tool policy belongs in the application, not new public OVXR entrypoints.

Use the [workbench adaptation guide](../../examples/python/spatial-workbench/README.md#adapting-this-sample) for
scene-geometry controls, hand/beam visuals and navigation; use the
[inspector adaptation guide](../../examples/python/scene-inspector/README.md#adapting-this-sample) for a shared object
model and OVUI property editing. [Scene-occluded UI](../scene-occluded-ui/SKILL.md)
supplies the pixel compositor only when the application needs it.

- Route XR and desktop commands into one model and one publication owner.
  Treat each drag as one undoable operation; cancellation restores its initial
  value. Keep selection and history independent from rendering resources.
- Use nearest visible hits in consistent coordinates for buttons, objects and
  teleport targets. Retain a drag's grab offset and owner until release.
- Cancel on tracking/focus loss or input-source changes, then require release
  before another press. A held trigger must not activate a newly exposed target.
- Draw the same ray used for picking; hide invalid input visuals. Label scripted
  rays and synthetic hand joints as demo data, never hardware validation.
- Preserve the requested scope. These examples are small tools, not a general
  scene editor, arbitrary-mesh picker or a complete headset interaction system.

Exercise click/drag/cancel/undo/reset through the input path, inspect both eyes,
and keep visual checkpoints inside settled input intervals. See the linked
recipes' prompts and [coverage and limits](../COVERAGE.md).
