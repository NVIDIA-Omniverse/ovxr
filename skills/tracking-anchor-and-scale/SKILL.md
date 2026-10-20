---
name: tracking-anchor-and-scale
description: Use managed or prim-sourced OVXR tracking anchors and convert OpenXR poses to stage space. Use for scale, up-axis, placement or anchor-latching errors.
license: LicenseRef-NvidiaProprietary
metadata:
  author: "mbickleyNV <129252267+mbickleyNV@users.noreply.github.com>"
  version: 0.1.0
  tags: [ovxr, openxr, sdk-integration]
  domain: graphics
---

# Tracking Anchor And Scale

## Requirements

Requires a matched distributed OVXR, OVRTX and OVStage SDK and a supported OpenXR runtime. No Kit or implementation-source checkout is required.

## Instructions

Read resolved scene information rather than assuming centimetres or descriptor defaults. OpenXR poses are in metres and Y-up. OVXR maps them through unit conversion, basis conversion and the active anchor, in row-vector order `P_scene = P * U * B * A`.

The anchor is prim-sourced. An omitted or unresolved initial path uses the managed runtime Anchor, created at the first renderable bind and seeded from the first eye camera's pre-existing pose (identity for runtime-authored cameras). An existing configured anchor with an invalid transform fails initialization rather than silently falling back. A live prim selection validates readability and rejects OVXR-driven eye cameras. If its prim later becomes unreadable, the last committed transform survives.

Managed-transform and clear calls publish through the selected ordinal mode. The active anchor changes at the frame boundary; getter state is distinct from pending intent. The public README specifies an immutable snapshot at begin or render; no-render boundaries also latch it. Getter publication follows successful complete camera writes. Clear resets the managed anchor to identity; restoring a user's spawn is an application operation.

Matrices use finite row-major row-vector values with translation at 12..14. Reject shear, reflection, perspective and nonpositive/nonuniform scale. Default floor alignment preserves translation and heading while discarding scale/pitch/roll. Full-transform anchors require stage-space Xforms; a Z-up Camera's extra basis rotation is unsuitable.

For an application anchor, compute and seal its derived world matrix before the boundary: OVXR reads the committed world-space value, not the local transform. Use the documented `update_prim_transforms` operation or an application-owned sealed OVStage transaction for moving scene prims. `set_anchor_prim` writes nothing and takes no ordinal; managed-transform and clear operations seal their writes.

Locate valid input poses in the returned reference space and convert through `transform_openxr_pose` using the latched mapping. Do not apply units, basis or navigation twice. Teleport acceptance, snap-turn thresholds and gestures remain application policy.

## Examples

For a Z-up scene with a nonidentity spawn transform, resolve scene units and convert tracking poses once through the frame-latched mapping. Verify translation, heading and scale before adding application navigation.
