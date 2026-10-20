# minimal-xr (C)

Load a USD stage and run a VR session — the C twin of
[the Python minimal-xr example](../../python/minimal-xr/).

ovxr does not own scene data or the renderer — it borrows both, owns the OpenXR
session, and drives the per-frame render and submit. The scene lives in an
application-owned ovstage attached to the renderer in borrow mode (OVXR requires
one): the example populates it with `ovstage_population_open_usd_from_file()`,
commits the population by advancing the stage's write floor, and passes the
stage to ovxr through `ovxr_desc_t::ovstage`.

## Prerequisites

- Windows x86_64 or Linux x86_64 for OVXR 0.1. Linux aarch64 packages are coming soon.
- Internet access for the first configure, or extracted SDKs for an offline build.
- Internet access for the default S3 scene, or a local scene selected with `--usd`.
- An OpenXR runtime installed and active (e.g. CloudXR, SteamVR, Monado) with a
  headset or simulator connected.
- CMake 3.19+, a C++17 compiler, and a Vulkan-capable NVIDIA GPU.

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
with paths to the extracted SDKs.

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

Useful flags:

```
minimal-xr --frames 300                # stop after 300 submitted frames
minimal-xr --no-foveation              # disable local foveation
minimal-xr --usd path/to/scene.usda    # your own stage
minimal-xr --up-axis y --meters-per-unit 0.01   # fallbacks for stages without
                                                # upAxis/metersPerUnit metadata
```

Stage scale and up-axis resolve from the attached ovstage's metadata
(`scene_info_policy = AUTO`); the `--up-axis` / `--meters-per-unit` flags are
descriptor fallbacks for stages that omit the metadata.

## Walkthrough

- `create-renderer` — create the ovrtx renderer (BORROW attach mode, Vulkan)
- `add-usd` — create + attach the ovstage, populate it from USD at a fresh
  ordinal, advance the write floor to commit
- `create-context` — describe and create the ovxr context (stereo products
  authored by ovxr, tracking anchored at `/Render/Camera`)
- `probe` — report the OpenXR runtime, system, and recommended swapchain
- `frame-loop` — `ovxr_render_frame()` with a bounded NOT_READY retry loop
