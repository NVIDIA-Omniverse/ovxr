# Anchors and teleport — C++

**App type:** stereo VR (opaque). Mono and AR are not configured by this sample. See the [compatibility matrix](../../COMPATIBILITY.md).

[Preview this sample in the gallery](../../gallery/README.md#anchors-and-teleport).

See [Adapting this sample](#adapting-this-sample) for guidance on using the
canonical skills in your application.

Move the tracking origin, teleport, and follow a live scene anchor using the
C APIs. The [Python sample's explanation](../../python/anchors-and-teleport)
documents the shared controls, scene, floor alignment and frame-boundary rules.

After the [CMake build](../../README.md#running-samples), run from the executable directory
(PowerShell; on Linux use `./anchors-and-teleport`):

```powershell
.\anchors-and-teleport.exe
.\anchors-and-teleport.exe --demo --demo-interval 90 --frames 360
```

Keyboard controls: `1` start, `2` teleport, `3` follow, `R` reset, `Q` exit.
Keep the launching terminal focused. Ctrl+C also exits. `--demo` runs the same
sequence without input; use `--no-foveation` to disable foveation.

See the [setup guide](../../README.md) for environment and build instructions.
To build with matching installed C SDKs:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="<ovxr>;<ovrtx>;<ovstage>"
cmake --build build --config Release
```

Run `anchors-and-teleport` (`.exe` on Windows) in the build directory, or
`build/Release/` for multi-configuration generators. CMake stages runtime
resources and assets beside it. Preserve `../common/`, `../cmake/` and
`../../assets/` when copying the source project.

`main.cpp` contains the anchor operations and frame loop. All writes after
population go through OVXR's private ordinal ledger. The shared helper owns
initialization, terminal restoration and orderly resource teardown.

## Adapting this sample

The [shared adaptation guide](../../python/anchors-and-teleport/README.md#adapting-this-sample)
covers application design, API ownership and validation for both implementations.
Use this README's C++ build/run instructions with that guidance and the
[shared project setup](../../README.md#shared-project-setup-and-ownership).

## Prompt context (FYI)

The [Python README](../../python/anchors-and-teleport/README.md#prompt-context-fyi)
records the shared request history and reconstructed prompt for both implementations.
These notes are informational; use the checked-in source for exact reproduction.
