# Live scene — C++

**App type:** stereo VR (opaque). Mono and AR are not configured by this sample. See the [compatibility matrix](../../COMPATIBILITY.md).

[Preview this sample in the gallery](../../gallery/README.md#live-scene).

See [Adapting this sample](#adapting-this-sample) for guidance on using the
canonical skills in your application.

Animate a cube and publish its transform together with XR camera state using
the C APIs. See the [Python sample's explanation](../../python/live-scene)
for the shared scene, ordinal contract and expected output.

After the [CMake build](../../README.md#running-samples), run from the executable directory
(PowerShell; on Linux use `./live-scene`):

```powershell
.\live-scene.exe --frames 180
```

See the [setup guide](../../README.md) for environment and build instructions.
To build with matching installed C SDKs, configure this directory or the parent
`c/` project:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="<ovxr>;<ovrtx>;<ovstage>"
cmake --build build --config Release
```

Run the resulting `live-scene` executable (`.exe` on Windows); multi-configuration
generators place it under `build/Release/`. CMake stages its runtime and the
bundled scene beside it. Preserve `../common/`, `../cmake/` and `../../assets/`
when copying the source project. The executable accepts `--help`, `--frames N`
and `--no-foveation`.

`TransformWriter` demonstrates canonical OVStage CPU transport: one 16-lane
double matrix per prim with `OVSTAGE_SEMANTIC_MATRIX`. `FrameGuard` cancels any
outstanding frame on retry or exception. `Application` keeps renderer/stage
ownership and partial-initialization cleanup out of the feature loop.

## Adapting this sample

The [shared adaptation guide](../../python/live-scene/README.md#adapting-this-sample)
covers application design, API ownership and validation for both implementations.
Use this README's C++ build/run instructions with that guidance and the
[shared project setup](../../README.md#shared-project-setup-and-ownership).

## Prompt context (FYI)

The [Python README](../../python/live-scene/README.md#prompt-context-fyi)
records the shared request history and reconstructed prompt for both implementations.
These notes are informational; use the checked-in source for exact reproduction.
