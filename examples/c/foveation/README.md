# Foveation (C++)

**App type:** stereo VR (opaque). Mono and AR are not configured by this sample. See the [compatibility matrix](../../COMPATIBILITY.md).

[Preview this sample in the gallery](../../gallery/README.md#foveation).

See [Adapting this sample](#adapting-this-sample) for guidance on using the
canonical skills in your application.

Compare three foveation policies in the same room with a fine line grid. Run each
mode in a separate session; these settings configure the initial render products.

After the [CMake build](../../README.md#running-samples), run from the executable directory
(PowerShell; on Linux use `./foveation`):
```powershell
.\foveation.exe --mode off --frames 300
.\foveation.exe --mode local --multiplier 0.2 --inset 0.1 --frames 300
.\foveation.exe --mode runtime --frames 300
```

| Mode | Local rendering | Runtime PQW policy | What it demonstrates |
| --- | --- | --- | --- |
| `off` | Disabled | Disabled | Full-resolution baseline. |
| `local` (default) | Warped | Disabled | Application-controlled warp, with unwarped headset submission. |
| `runtime` | Auto | Auto | Runtime-provided geometry when valid; full-resolution fallback otherwise. |

`--multiplier` and `--inset` accept values in (0, 1] and apply only to local
mode. A smaller multiplier reduces the fallback warped image dimensions;
the inset controls the central full-resolution region. Runtime PQW metadata
takes precedence over those fallback inputs, so the sample rejects tuning
arguments in other modes.

Local mode defaults to an intentionally strong **0.2 multiplier / 0.1 inset**:
a small full-resolution center surrounded by visibly reduced peripheral detail.
These are demonstration settings, not a recommended headset quality preset.
Use `--multiplier 0.5 --inset 0.4` for the previous, milder comparison.

Look at the grid near the center and at the edges while holding a similar head
pose between runs. The room landmarks help align the comparison. Output reports
effective local rendering and whether runtime foveated submission is enabled.
It queries **after submission**, because AUTO can resolve when views are first
located. Extension enablement alone is not evidence of valid runtime PQW.
A disabled local result in runtime mode is a supported fallback.

The sample also reports the selected raw render-product readback dimensions.
These are not necessarily the headset swapchain dimensions; a smaller warped
image may still be submitted at full size after unwarping. Readback is enabled
for this diagnostic, so this is an image-quality/policy example, **not a GPU
performance benchmark**. It does not configure eye-tracking actions.

The `foveation-policy` and `resolved-foveation` snippets in `main.cpp` show the C API settings and `ovxr_get_device_info` / `ovxr_acquire_frame` queries.
Ctrl+C stops an unlimited session; `--frames N` counts submitted frames.

## Adapting this sample

The [shared adaptation guide](../../python/foveation/README.md#adapting-this-sample)
covers application design, API ownership and validation for both implementations.
Use this README's C++ build/run instructions with that guidance and the
[shared project setup](../../README.md#shared-project-setup-and-ownership).

## Prompt context (FYI)

The [Python README](../../python/foveation/README.md#prompt-context-fyi)
records the shared request history and reconstructed prompt for both implementations.
These notes are informational; use the checked-in source for exact reproduction.
