# Minimal ovxr Example

The smallest useful ovxr Python example: load a USD stage and run a VR or AR
session against a connected headset or simulator.

1. Create an ovrtx `Renderer` and load a USD stage into it
2. Create an ovxr `Context` that borrows the renderer and authors its own stereo
   render products
3. Probe the OpenXR runtime, then drive `render_frame` until stopped

ovxr does not own scene data or the renderer — it borrows both, owns the OpenXR
session, and drives the per-frame render and submit. The scene lives in an
application-owned ovstage attached to the renderer in borrow mode (OVXR requires
one): the example populates it with `ovstage.population.open_usd()`, commits the
population by advancing the stage's write floor, and passes the stage to ovxr
through `ContextDesc.ovstage`.

## Prerequisites

- Python 3.10-3.13
- [uv](https://docs.astral.sh/uv/)
- NVIDIA RTX-capable GPU and a supported NVIDIA driver
- An installed OpenXR runtime, with a headset or simulator connected
- Internet access for the default S3 scene, or a local scene selected with `--usd`
- Unsandboxed runtime execution

## Running

```bash
uv run main.py                      # VR session, runs until Ctrl-C
uv run main.py --session-mode ar --blend-mode alpha
uv run main.py --frames 300         # stop after 300 submitted frames
uv run main.py --no-foveation       # submit full-size views
uv run main.py --usd /path/to/scene.usda --up-axis y --meters-per-unit 0.01
```

## What to expect

On startup the first run compiles and caches shaders, which takes a while. Once
the session is running the example prints the runtime and system names plus the
recommended swapchain size, then a progress line every 90 submitted frames.

`render_frame` returning `NOT_READY` is normal and not an error: the runtime is
not asking for frames yet, typically because the session is still starting or the
headset is not being worn. The loop simply retries.

## Notes

- **Stage up axis and scale.** ovxr maps OpenXR's +Y-up, metres-based tracking
  poses into stage space using the stage's `upAxis` and `metersPerUnit`. Under
  `SceneInfoPolicy.AUTO` (what this example passes) both are read from the
  attached ovstage's metadata when the scene declares them, so well-formed stages
  need no flags. `--up-axis` and `--meters-per-unit` are the descriptor fallback
  for stages that omit the metadata; get a fallback wrong and the session still
  runs but the view is rotated and mis-scaled.

  The flag defaults match the default scene. To check your own, read the metadata
  block at the top of the `.usda`:

  ```
  (
      metersPerUnit = 1
      upAxis = "Z"
  )
  ```

  Note USD's own defaults differ from this scene's: `upAxis` defaults to `Y` and
  `metersPerUnit` to `0.01`, so a stage that omits them wants `--up-axis y
  --meters-per-unit 0.01`.

- **Tracking anchor.** `TRACKING_ANCHOR` names the prim whose transform anchors
  the tracking space, so the headset starts where that camera sits in the stage.
  Change it to match your scene.
- **Extra OpenXR extensions.** ovxr enables only the extensions it needs itself.
  If your application resolves its own entry points (hand tracking, CloudXR
  action or data-channel functions) against ovxr's instance, declare them via
  `ContextDesc.extra_openxr_extensions` or `xrGetInstanceProcAddr` will fail with
  `XR_ERROR_FUNCTION_UNSUPPORTED`.
- **Shutdown order.** The ovxr context borrows the renderer, so destroy the
  context before detaching the stage and destroying the renderer.
