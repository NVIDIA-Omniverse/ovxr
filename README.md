# ovxr

`ovxr` bridges the [ovrtx](https://github.com/NVIDIA-Omniverse/ovrtx) Omniverse RTX renderer to OpenXR. It
drives a borrowed `ovrtx.Renderer` to render and submit head-tracked mono,
stereo, or four-view foveated-inset frames, with optional CUDA RGBA8 capture of
one configured render product.

It ships as:
- a **C API** — `include/ovxr/ovxr.h` (built into `libovxr-dynamic.so`)
- a **Python package** — `python/ovxr/`, a ctypes binding over that C API,
  exposed the same way the sibling `ovrtx` package exposes its bindings.

The Python wheel includes the bindings, OVXR native libraries and OpenXR loader.
It depends on the matching OVRTX and OVStage wheels. An OpenXR runtime must be
installed separately.

OVXR 0.1 provides Windows x86_64 and Linux x86_64 packages.
Linux aarch64 packages are coming soon.

## Data Collection

OVXR collects anonymous initialization data by default to help improve OVXR. Collection is optional and can be disabled without affecting OVXR functionality. To opt out, set the environment variable `OVX_PHONE_HOME_OPT_OUT=1` before launching your application.

More information is available at [Data Collection & Use](https://github.com/NVIDIA-Omniverse/ovxr/blob/main/TELEMETRY.md).

## Python quick start

```python
import ovrtx
import ovstage
import ovxr

# 1. Create the OVRTX renderer and the OVStage scene (ovxr borrows both).
renderer = ovrtx.Renderer(ovrtx.RendererConfig(read_gpu_transforms=True, keep_system_alive=True))
stage = ovstage.Stage("my-xr-app")
renderer.attach_ovstage(stage)
# ... populate the stage (ovstage.population.open_usd) and commit the write floor ...

# 2. Create the ovxr context against that renderer and stage.
ctx = ovxr.Context(ovxr.ContextDesc(
    ovrtx_renderer=renderer,
    ovstage=stage,
    application_name="my-xr-app",
    display_mode=ovxr.DisplayMode.STEREO,
    session_mode=ovxr.SessionMode.VR,
    create_render_products=True,
    enable_frame_readback=True,
    readback_render_product_index=0,
))

# 3. Render + submit each frame and optionally consume the latest color capture.
while running:
    ctx.render_frame(delta_time_seconds)
    frame = ctx.acquire_frame()
    if frame is not None:
        consume_cuda_frame(frame.device_ptr, frame.width, frame.height, frame.cuda_device_id)
        synchronize_cuda_work()  # finish before two more frames can be captured
ctx.destroy()
```

### OVStage ordinal admission policy

OVStage applies one ordinal admission policy per process, fixed by the first
process reference. OVXR 0.1 supports `NONDECREASING`, the default, and
`FLOOR_ONLY`. It does not support `STRICTLY_INCREASING`, because OVXR writes
some attributes more than once at a single ordinal; `ovxr.Context` creation
(`ovxr_create` in C) fails when the stage's process selected that policy.
Support is planned for OVXR 0.2.

Keep the default, or select `NONDECREASING` before anything else in the process
creates or initializes OVStage:

```python
stage = ovstage.Stage("my-xr-app", config=ovstage.StageConfig(
    runtime_default_ordinal_admission_policy=ovstage.OrdinalAdmissionPolicy.NONDECREASING,
))
```

In C, pass `ovstage_config_entry_runtime_default_ordinal_admission_policy(OVSTAGE_ORDINAL_ADMISSION_NONDECREASING)`
to `ovstage_initialize` before the first `ovstage_create_instance`.

### Frame readback ownership and lifetime

`Context.acquire_frame()` returns an `ovxr.Frame` on success, or `None` when no
capture is available. The wrapper copies `width`, `height`, `pitch_bytes`,
`cuda_device_id`, `format`, `sequence`, and `predicted_display_time_ns`, so that
metadata remains safe to inspect even after the frame expires.

`Frame.device_ptr` is different: it is a borrowed, read-only CUDA device pointer
owned by OVXR. Do not write to or free it. OVXR double-buffers captures, so the
pointer for sequence `N` expires when capture `N + 2` can begin, and every
pointer expires when its `Context` is destroyed. The Python context enforces a
safe, conservative boundary: the second subsequent `render_frame()` or
`submit_frame()` call expires the wrapper at call entry, even if that call later
does not produce a capture. If acquisition overlaps a capture-capable call, the
in-flight call counts as the first one. Observing sequence `N + 2` through
another `acquire_frame()` also expires it.

Check `frame.is_valid` for diagnostics and retrieve the pointer through
`frame.device_ptr`; expired access raises `ovxr.FrameExpiredError`. Validity can
change concurrently, and Python cannot revoke an integer pointer that was
already retrieved. Consumers must therefore finish and synchronize CUDA work
before allowing two more capture-capable calls to begin, and should not retain
the integer separately from its `Frame`.

## Application composition layers

`ovxr_submit_frame` takes an optional `ovxr_submit_desc_t` whose
`additional_layers` and `additional_layer_count` (Python:
`submit_frame(plan, additional_layers=...)`) append native OpenXR projection,
quad, or supported extension layers after the OVRTX scene. The application owns
the layer data, swapchains, and GPU work; OVXR owns frame submission. No-render
frames omit all layers, and CUDA readback remains scene-only.

Zero-initialize `ovxr_submit_desc_t`; a null descriptor or a zero
additional-layer count submits only the scene. The changed `ovxr_submit_frame`
signature changes the pre-release API without changing the dylib ABI version:
rebuild callers and use matching headers, Python bindings, loader, and runtime
binaries. The candidate ABI has not shipped and may change before GA.

`ovxr_get_vulkan_handles` / `Context.get_vulkan_handles()` exposes the borrowed
session Vulkan binding and enabled external-memory/semaphore handle types.
Python returns an `ovxr.VulkanHandles` value with integer handles and masks,
or `None` before the session is created.
Applications can import images from their own same-GPU Vulkan device, then copy
into application-owned OpenXR swapchains. Serialize borrowed queue access with
OVXR, query format/usage support, and destroy app resources before the context.
Use matching updated runtime and loader packages for these APIs.

## Runtime anchors

OVXR tracks one anchor prim. `set_anchor_prim(path)` switches to a prim whose
latest committed OVStage world transform is read at every frame boundary. Use
this for a moving vehicle, rig, platform, or camera.

The anchor is always a prim. `ContextDesc.tracking_anchor_path` selects the
initial application prim. When it is omitted or cannot be resolved, OVXR follows
its own managed prim `<runtime_prim_root>/Anchor` (default `/OVXR/Anchor`),
which it creates in the first renderable `bind_frame()` -- inside the same
OVStage transaction as the runtime RenderProducts, or on its own when the
context does not author them. That prim starts at the
first eye camera's pre-existing pose for backward compatibility, which is
identity for runtime-authored cameras. `set_managed_anchor_transform()`
publishes its world transform through the same live OVStage channels as camera
updates and selects it, placing or teleporting the user without configuring a
separate anchor prim. Like every OVXR writer it takes a `stage_ordinal`
(`None`/0 = OVXR's private ledger) and seals it; that seal is what makes the new
value visible to the next frame boundary. Runtime prim selection is strict, and an OVXR-driven eye
camera cannot be selected because that would create a transform feedback loop.

`set_anchor_prim` writes nothing and therefore takes no ordinal. It requires
the prim to be readable when it is called, so a mistyped path fails immediately
rather than at the next frame. That validation read -- and the per-frame anchor
read -- see SEALED OVStage state only: a prim you moved at an ordinal you have
not sealed is not visible to the next `bind_frame()`, which keeps the last
committed anchor. Move it with `set_managed_anchor_transform(...,
stage_ordinal=...)` or `update_prim_transforms(..., stage_ordinal=...)` and let
OVXR's seal publish it, or seal your own ordinal before that boundary.
The managed prim may be selected before the first bind has created it: the
selection is queued and the bind authors the prim. If the prim later
becomes unreadable (deleted, or resolved as an OVXR-driven eye camera), frames
continue on the last committed anchor, a warning is logged once for that path,
and following resumes when the prim is readable again. `get_anchor_prim()`
returns the prim currently driving the anchor.
`ContextDesc.tracking_anchor_path` applies the same pointer, embedded-NUL,
absolute-path, and driven-eye-camera validation before its first OVStage read.
`clear_anchor(stage_ordinal=...)` resets the managed `/OVXR/Anchor` prim to
identity and selects it, so the tracking origin sits at the stage origin. Managed-transform, clear,
and set-prim requests are ordered; the last one before a frame boundary wins.
`get_anchor_prim()` is never empty.

Floor alignment is enabled by default, matching the legacy Kit XR OpenXR path.
OVXR preserves the anchor's translation and heading about the resolved stage up
axis while discarding pitch, roll, and authored scale. Camera prims use local -Z
(USD camera forward); stage-space Xforms use the stage-space forward axis. OVXR
selects that convention from stable prim metadata when the anchor is selected
and retains it while the same prim animates, so crossing an orientation
threshold cannot snap the heading.

Set `ContextDesc.floor_aligned_anchor=False` to apply the prim's complete
validated rotation and positive uniform scale verbatim. The anchor `A` is then
composed after the OpenXR-to-stage basis change `B` (see the mapping below), so
the prim must be authored as a stage-space transform: the managed prim and
application Xform rigs qualify, but a Camera prim in a Z-up stage would pitch
the view 90 degrees. Keep floor alignment enabled for Camera anchors.

If `tracking_anchor_path` is missing or its transform cannot be read from
OVStage at creation, or a pre-existing eye camera cannot be used to place the
managed prim, OVXR logs a warning and continues with the managed prim (at
identity in the last case). An existing configured anchor whose transform is
non-finite, non-affine, non-uniformly scaled, sheared, or reflected fails anchor
initialization rather than silently selecting the managed prim.

```python
# Follow a moving scene prim.
ctx.set_anchor_prim("/World/MovingRig")

# Selection becomes active only at a completed frame boundary.
result, _, _ = ctx.render_frame(delta_time_seconds)
assert result == ovxr.Result.SUCCESS

active_anchor = ctx.get_anchor_transform()
assert ctx.get_anchor_prim() == "/World/MovingRig"
```

### Placing and teleporting the user

The anchor is always prim-sourced. Publish a solved transform to the default
managed prim with `set_managed_anchor_transform()`, or author an
application-owned anchor prim through OVStage; the next frame boundary picks it
up. OVXR does not decide what "teleport" means for your application. It exposes
the mapping and every input needed to solve for the anchor:

For an application-owned anchor, finish the OVStage hierarchy computation
before the boundary so its committed `omni:fabric:worldMatrix` is current; OVXR
samples that derived world-space value rather than a local transform.

- `get_anchor_transform()`: the active anchor `A`.
- `get_scene_info()`: `meters_per_unit` (defines `U`) and `up_axis` (defines
  `B`, identity for Y-up or a +90-degree rotation about X for Z-up).
- `get_openxr_handles()`: `xr_space` to locate the head or a controller against,
  and `xr_reference_space_type`. For `STAGE` and `LOCAL_FLOOR`, tracking
  `y = 0` is the physical floor; for `LOCAL` the origin is the runtime's start
  pose and no floor is implied.

Given a tracked pose `P` in meters and a desired scene pose `X` in stage units,
build `M = P * U * B` as a rigid matrix (scale the translation by `U`, leave the
rotation unit-length) and solve `A = M^-1 * X`. Which `P` and `X` to use is the
application's choice. A typical room-scale teleport flattens the head pose to
its standing point first (floor projection with heading only) and pairs it with
a floor-level target, so the user keeps their tracked height and head pitch. A
seated or cockpit setup may instead match the exact eye point. Recentering is
the same solve with `X` at the scene origin. With floor alignment enabled the
resolved anchor keeps only translation and heading regardless.

```python
ctx.set_managed_anchor_transform(anchor_matrix)
```

Raw and resolved anchor matrices are row-major, use row vectors, and store
translation in elements 12..14 in stage units. Raw transforms may contain
translation, arbitrary rotation, and positive uniform scale. Non-finite,
perspective, sheared, reflected, zero-scale, and non-uniformly scaled matrices
fail with an explicit error before the floor-alignment policy is applied.

If `P` is an OpenXR pose in meters, the mapping is:

```text
P_scene = P * U * B * A
```

`U` converts meters to stage units, `B` converts OpenXR +Y-up into the configured
stage basis, and `A` is the active absolute anchor. OVXR resolves one immutable
anchor snapshot at `begin_frame`/`render_frame`; that snapshot drives both eyes
and is published by `get_anchor_transform()` only after the complete camera
write succeeds. A no-render OpenXR boundary also latches it. Changing the
anchor prim does not recreate the context, OpenXR session/reference space,
swapchains, or renderer attachment, and tracking remains valid.

### Direct OpenXR input poses

After `ovxr_begin_frame()`, obtain the shared handles with
`ovxr_get_openxr_handles()`. Locate controller, hand, gaze, or other action spaces
relative to the returned `xr_space`, at the frame plan's predicted display time.
Copy the position and orientation into an `ovxr_pose_t` and pass it to
`ovxr_transform_openxr_pose()` after `ovxr_begin_frame()`. Python callers
pass the pose's position and orientation to `Context.transform_openxr_pose()`.
The helper reads the same frame-latched mapping used for the eyes and removes
uniform scale from the output orientation while retaining it on positions and
IPD. It is the preferred mapping for application-owned controllers, hands,
rays, and spatial UI. Check `XrSpaceLocation::locationFlags` before mapping: an
unlocated space commonly reports an all-zero orientation, which the helper
rejects as a non-unit quaternion.

OVXR remains the OpenXR session owner: external users may locate spaces and
resolve entry points, but must not wait/begin/end frames, change session state,
or destroy shared handles. Enable any input extension names through
`extra_openxr_extensions` before context creation. Serialize anchor and frame-loop
calls using one context; `Context.acquire_frame()` is the only overlapping
context operation, and `Context.destroy()` must not overlap any call.

## Application render products in the bound frame

OVXR owns the frame's single OVRTX step: `bind_frame()` publishes the frame's
Stage writes, seals the ordinal and steps the renderer. Applications extend
that frame in two ways, neither of which steps the renderer or ends the OpenXR
frame themselves (composition layers are covered above):

- **Render your own products in OVXR's step.** `ctx.set_frame_render_products(
  ["/Render/PictureInPicture"])` (C: `ovxr_set_frame_render_products()`) adds
  application RenderProducts after the eye products in every bound step until
  replaced; an empty list clears it. One step keeps their sensor history and
  simulation clock aligned with the eyes -- a separate application step would
  discard the history of every product not in its set. Call it between frames.
  OVXR authors nothing on these prims; a product whose output you need every
  frame should author `omni:rtx:rt:ecoMode:enabled = false` itself.
- **Read their outputs before submit.** After a renderable `bind_frame()`,
  `plan.step_result` borrows the bound OVRTX step result (the step has already
  completed). Use the `ovrtx` package's `fetch_results()` /
  `map_render_var_output()` / `unmap_render_var_output()` on it, unmap before
  `submit_frame()` or `cancel_frame()`, and never destroy it.

`render_frame()` and the implicit-bind `begin_frame() -> submit_frame()` loop
are unchanged and scene-only.

Applications that must run the OVRTX step themselves can opt in at creation
with `ContextDesc(application_steps_renderer=True)` (C:
`ovxr_desc_t::application_steps_renderer`). `bind_frame()` then publishes and
seals as usual but stops after reading the floor into `plan.render_ordinal`;
the application calls the `ovrtx` update and step at that ordinal over
`plan.render_products` and passes the result to `submit_frame(plan,
step_result=...)`, waiting for and destroying it afterwards. This mode needs an
explicit `bind_frame()` (`render_frame()` and the implicit bind are rejected),
and a nonzero `step_result` is rejected outside it.

## Display selection

`DisplayMode.AUTO` prefers OpenXR primary stereo and falls back to primary mono
when stereo is unavailable. `DisplayMode.MONO` and `DisplayMode.STEREO` select
those configurations explicitly. `DisplayMode.QUAD_VIEW` selects the four-view
foveated-inset configuration in outer-left, outer-right, inset-left,
inset-right order; OpenXR probing or frame setup returns `OVXR_NOT_READY` when
an explicitly selected configuration is unavailable. Supply one render
product/camera for mono, two for stereo, or four for quad view. With `AUTO`, a
stereo-ordered pair is accepted and only its first entry is used after a mono
fallback. When OVXR authors quad-view products, it uses each view's recommended
OpenXR resolution. Renderer-local warp is opt-in for quad view: `AUTO` resolves
to disabled, while explicitly selecting `WARPED` authors independent foveation
settings and resolutions for all four products. When local foveation is
disabled, OVXR neutralizes any foveation inherited from `source_render_product`.

## Multi-GPU assignment

OVXR does not expose a multi-GPU assignment setting. For stereo and quad view,
OVXR clears inherited `deviceIds` on products cloned from `source_render_product`
so OVRTX applies its normal assignment policy. Prepared products retain their
authored `deviceIds`. For mono, OVXR authors the OpenXR composition GPU's
process-visible CUDA ordinal onto the render product. That GPU must be visible
to CUDA and active in OVRTX; otherwise frame setup reports
`OVXR_ERROR_DEVICE_MISMATCH` rather than rendering mono on another GPU.

OVXR does not enable or disable OVRTX multi-GPU rendering or change the active
GPU set established when the OVRTX renderer is created. The mono assignment is
authored once before the first render. `Context.get_device_info()` reports the
placement observed from rendered outputs.

## Locating `libovxr-dynamic.so`

The supported production search is deterministic. The Python package checks, in
order:

1. `ovxr.OVXR_LIBRARY_PATH_HINT`, when set to either the shared-library file or
   a directory containing it. Use this for an intentional runtime override. An
   unusable explicit hint is an error; the loader does not silently fall back.
2. The package-local `ovxr/bin` directory.
3. The owning deploy tree's `bin` and `bin/plugins/rtx` directories.
4. The linked build-tree `plugins/rtx` directory used by the OVXR SDK build.

The Python discovery above does not search `PATH` or `LD_LIBRARY_PATH` by
default. For local development only, set `OVXR_ALLOW_SYSTEM_LIBRARY_SEARCH=1`
to append `LD_LIBRARY_PATH` (Linux) and `PATH` after every trusted location
above. `true`, `yes`, and `on` are also accepted case-insensitively.

This policy governs the ovxr shim only. The matching `libovrtx-dynamic.so` that
OVXR preloads is resolved by following the `ovrtx` wheel's own discovery
(`ovrtx.OVRTX_LIBRARY_PATH_HINT`, then `ovrtx.ovrtx_loader_candidate_dirs`),
or by reusing the library the wheel has already loaded. Applying a stricter
policy to the preload would not make the process safer: the wheel loads that
library regardless, and a mismatch would bind ovxr to a second ovrtx instance.

The current directory has a separate development opt-in,
`OVXR_ALLOW_CWD_LIBRARY_SEARCH=1`, accepts the same truthy values, and is
likewise searched only after trusted package/deploy locations. Loader failures
list every trusted location checked and state whether either development search
was enabled.

On Windows, the native payload's remaining by-name dependencies follow the
native loader's standard search order independently of this Python discovery
policy. A split deployment must therefore keep its trusted `bin` directory on
the native `PATH`.

OVXR preloads the matching `libovrtx-dynamic.so` with `RTLD_GLOBAL` on Linux so
ovxr binds the *same* ovrtx instance the `ovrtx` wheel already loaded.

For the low-level ctypes surface (`Bindings`, `load`, the `ovxr_*_t` structs),
import from `ovxr._src.bindings`.

## License

NVIDIA Proprietary. See [LICENSE](LICENSE).
