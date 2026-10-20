---
name: stage-publication
description: Coordinate OVXR with application or simulation writes to the borrowed OVStage. Use for ordinal modes, bind-frame publication, or seal-then-fail recovery.
license: LicenseRef-NvidiaProprietary
metadata:
  author: "mbickleyNV <129252267+mbickleyNV@users.noreply.github.com>"
  version: 0.1.0
  tags: [ovxr, openxr, sdk-integration]
  domain: graphics
---

# Stage Publication

## Requirements

Requires a matched distributed OVXR, OVRTX and OVStage SDK and a supported OpenXR runtime. No Kit or implementation-source checkout is required.

## Instructions

Choose the `PRIVATE` value of `StageOrdinalMode` only while OVXR is the sole ongoing stage writer. Choose `APPLICATION` when the app or simulation co-writes: supply a nonzero application-owned ordinal for every stage-writing OVXR call, including teardown. The mode is fixed for the context lifetime.

An application publication coordinator owns allocation and serialization. A supplied ordinal must be above the floor, exclusively available for OVXR to finish, with every lower ordinal already resolved. Independent writers choosing floor-plus-one can collide or publish partial batches.

Keep OVStage's default `NONDECREASING` ordinal admission policy, or select `NONDECREASING` or `FLOOR_ONLY` before anything else in the process creates or initializes OVStage; the first process reference fixes it. OVXR 0.1 writes some attributes more than once at one ordinal (first-bind eye-camera state, failure undo and same-ordinal retry), which `STRICTLY_INCREASING` rejects, so context creation fails under that policy. Support is planned for OVXR 0.2.

`begin_frame` supplies predicted timing and, under the public README contract, resolves the frame anchor snapshot from committed state. Publish navigation and derived anchor world transforms before that boundary. Hold the application's publication rendezvous across eligible scene writes and `bind_frame`; bind writes camera/runtime state, seals the ordinal and, in default OVXR-stepped mode, renders. A documented application-stepped opt-in stops after publication; frame-loop defines the caller's update/step and result ownership. It returns a seal report even on failure. Mark a sealed ordinal resolved and allocate a fresh one; cancellation does not undo publication.

On bind NOT_READY the plan remains active and the ordinal unsealed. Cancel, begin a new plan, and retry with the still-owned ordinal; rebinding the old plan cannot refresh its located views.

Do not infer abandonment removes unpublished writes. Use OVStage's supported rollback and read-visibility rules when recovering unpublished work; never invent an abandonment operation. Destruction consumes the context even when overlay publication fails; its result describes the stage release outcome.

## Examples

If bind seals an ordinal and then rendering fails, record that ordinal as resolved and allocate a fresh one. Canceling the plan does not roll back already published stage writes.
