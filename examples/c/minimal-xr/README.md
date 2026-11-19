# minimal-xr (C)

**App type:** stereo VR by default. AR/blend options are runtime-dependent and not headset-validated; mono is not exposed. See the [compatibility matrix](../../COMPATIBILITY.md).

[Preview this sample in the gallery](../../gallery/README.md#minimal-xr).

See [Adapting this sample](#adapting-this-sample) for guidance on using the
canonical skills in your application.

Load a USD stage and run a VR or AR session — the C twin of
[the Python minimal-xr example](../../python/minimal-xr).

ovxr does not own scene data or the renderer — it borrows both, owns the OpenXR
session, and drives the per-frame render and submit. The scene lives in an
application-owned ovstage attached to the renderer in borrow mode (OVXR requires
one): the example uses `ovstage_population_open_usd_from_file_with_desc()` to
populate rendering data and the `upAxis`/`metersPerUnit` stage metadata,
commits the population by advancing the stage's write floor, and passes the
stage to ovxr through `ovxr_desc_t::ovstage`. At startup it prints the scene's
resolved up axis and scale and where each came from (`OVStage` when the scene
declares them, otherwise the `--up-axis`/`--meters-per-unit` fallback).

## Prerequisites

- Windows x86_64 or Linux x86_64 for OVXR. Linux aarch64 packages are coming soon.
- Internet access for the first configure, or extracted SDKs for an offline build.
- Robot-OVRTX is the network default, with the bundled room as an offline fallback.
- An OpenXR runtime installed, active and started (e.g. CloudXR, SteamVR,
  Monado). Without a usable runtime the example exits with status 1 before
  creating the renderer or loading the scene. A headset or streaming client
  can connect before or after the example starts; it loads the scene
  meanwhile and waits for the session.
- CMake 3.19+, a C++17 compiler, libcurl 7.85+ development files, and a Vulkan-capable NVIDIA GPU.

## Build

From the root of the OVXR source distribution:

```powershell
cmake -S examples/c -B build
cmake --build build --config Release
```

CMake downloads the pinned OVXR, OVRTX and OVStage SDKs and verifies their SHA256
checksums. OVXR reads its SDK pin from `ovxr-release.json`; source ZIPs fetch
that receipt from the matching public release tag. For an offline build, add
`-DCMAKE_PREFIX_PATH="<ovxr-package>;<ovrtx-package>;<ovstage-package>"`
with paths to the extracted SDKs. The network preflight links `CURL::libcurl`;
install libcurl separately (for example through a vcpkg toolchain on Windows),
or pass its package prefix through `CMAKE_PREFIX_PATH`. Static libcurl builds
need their TLS/compression dependencies; shared builds need their runtime
libraries available to the executable.

The OVRTX SDK download alone is about 1.24 GB on Windows or 1.34 GB on Linux.
Allow additional disk space for the extracted SDKs. CMake caches downloaded
dependencies in `examples/c/_deps` for subsequent builds.

## Run

No environment setup is needed: the build anchors the whole runtime beside the
executable (ovrtx's package DLLs and runtime directories, `ovstage.dll`
delay-loaded into that shared USD/fabric runtime, and ovxr's shim + payload +
OpenXR loader copied alongside). Just run it:

```powershell
# Visual Studio or Ninja Multi-Config (Release):
.\build\minimal-xr\Release\minimal-xr.exe
# Ninja (single-config):
.\build\minimal-xr\minimal-xr.exe
```

On Linux the build sets rpaths onto the package `bin/` directories instead
of copying. With a single-config generator, run:

```bash
./build/minimal-xr/minimal-xr
```

Startup messages identify availability checks, renderer creation, scene loading,
XR setup and first-frame preparation. Shader and ray-tracing pipeline compilation
can take several minutes on first use, including during the first render call.
Messages are flushed before each blocking phase. The first actual submitted frame
reports its wait time and confirms startup is complete.

Useful flags:

```
minimal-xr --frames 300                # stop after 300 submitted frames
minimal-xr --wait-timeout 120          # exit 1 after 120 s without frames
minimal-xr --no-foveation              # disable local foveation
minimal-xr --session-mode ar --blend-mode alpha  # request AR with alpha blending
minimal-xr --usd path/to/scene.usda    # explicit scene, automatic fallback disabled
minimal-xr --fallback-usd path/to/fallback.usda  # override the fallback
minimal-xr --offline                   # use the fallback without probing the robot
minimal-xr --up-axis y --meters-per-unit 0.01   # fallbacks for stages without
                                                # upAxis/metersPerUnit metadata
```

`--session-mode` accepts `vr` (default), `ar`, or `auto`. `--blend-mode` accepts
`auto` (default), `opaque`, `alpha`, or `additive`. These are the same options as
the Python sample; the selected runtime must support the requested modes.

Stage scale and up-axis resolve from the attached ovstage's metadata
(`scene_info_policy = AUTO`); the `--up-axis` / `--meters-per-unit` flags are
descriptor fallbacks for stages that omit the metadata.

## Default scene

Without `--usd`, the sample tries Robot-OVRTX. Bounded GET probes cover its
root layer, GR1T2 robot payload and dome HDR; opening the root alone does not
establish that the robot and lighting are available. The probes check access,
not the contents of every transitive dependency. OVStage uses its normal
resolver and cache for actual scene population.

A failed probe or failed robot population selects `assets/minimal-xr.usda`
beside the executable, or `--fallback-usd PATH_OR_URL` when provided. The sample
logs the selected fallback and why it was needed. A failed population's partial
stage is destroyed before a fresh fallback stage is created and attached.
A fallback that also fails is reported as an error.

`--usd PATH_OR_URL` always means exactly that scene, without automatic fallback.
For example, `--usd assets/minimal-xr.usda` selects the basic room explicitly.
`--offline` selects the fallback without robot probes; it cannot accompany
`--usd`. CI uses `--offline` to cover the real bundled fallback deterministically.

CMake stages the shared `examples/assets` directory beside every executable
configuration, including asset-only edits. The default fallback is located from
the executable, even through `PATH` or an unrelated working directory. Keep
`assets/` beside it when relocating, and retain `common/`, `cmake/` and `assets/`
when copying the source project. Online robot loads and explicit scenes do not
require the bundle; missing assets selected with `--offline` fail before renderer
initialization.

The fallback is a texture-free, Y-up, meter-scale room with a floor-level
`/Render/Camera` at `(0, 0, 3)` looking along -Z. Materials use built-in
`OmniPBR.mdl`. The robot's authored Z-up metadata wins over descriptor defaults
under AUTO.

The robot, including an explicit `--usd` of its URL, uses the managed floor
start pose: 1 m from the robot, facing it. The bundled fallback or
another scene uses `/Render/Camera`; change `kTrackingAnchor` for scenes without
that prim. Automatic floor alignment requires `STAGE` or `LOCAL_FLOOR`;
with only `LOCAL`, calibrate the runtime start pose before relying on alignment.
The robot's AR passthrough dome and matte ground are properties of the robot
scene; the bundled room does not provide them.

## Walkthrough

- `check-runtime` — `ovxr_check_openxr_runtime()` before any expensive
  startup; an unavailable runtime exits, no headset or client yet continues
  so the scene loads while the device connects
- `create-renderer` — create the ovrtx renderer (BORROW attach mode, Vulkan)
- `add-usd` — create + attach the ovstage, populate it from USD at a fresh
  ordinal, advance the write floor to commit
- `create-context` — describe and create the ovxr context (stereo products
  authored by ovxr). The default scene starts the user on its floor facing the
  robot through the managed anchor; other scenes anchor at `/Render/Camera`
- `probe` — report the OpenXR runtime, system, and recommended swapchain
- `frame-loop` — `ovxr_render_frame()`, waiting through NOT_READY (no headset
  or client yet, session starting or idle) with a rate-limited notice; waits
  until Ctrl-C, or `--wait-timeout` seconds (default 60 with `--frames`)

## Adapting this sample

The [shared adaptation guide](../../python/minimal-xr/README.md#adapting-this-sample)
covers application design, API ownership and validation for both implementations.
Use this README's C++ build/run instructions with that guidance and the
[shared project setup](../../README.md#shared-project-setup-and-ownership).

## Prompt context (FYI)

The [Python README](../../python/minimal-xr/README.md#prompt-context-fyi)
records the shared request history and reconstructed prompt for both implementations.
These notes are informational; use the checked-in source for exact reproduction.
