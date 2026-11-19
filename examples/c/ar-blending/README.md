# AR blending (C++)

Same [behavior, controls and AR limits](../../python/ar-blending/README.md) as the
Python sample: stereo AR, transparent background, opaque objects and alpha counts.

Build with matched SDKs using the [C++ build instructions](../../README.md#running-samples).
You can build all samples from `c/` or this directory alone. Run from the
executable's directory (use `./ar-blending` on Linux):

```powershell
.\ar-blending.exe --blend alpha --frames 180 --report ar.json
.\ar-blending.exe --blend auto --frames 180
.\ar-blending.exe --blend opaque --frames 180
```

No CUDA toolkit is needed; the copied readback uses the installed NVIDIA driver.
Keep the staged `assets/` beside the executable. An unsupported blend mode fails
with runtime diagnostics. Headset passthrough must be checked on the actual client.

[Recreation adaptation guide](../../python/ar-blending/README.md#adapting-this-sample)

## Adapting this sample

The [shared adaptation guide](../../python/ar-blending/README.md#adapting-this-sample)
covers application design, API ownership and validation for both implementations.
Use this README's C++ build/run instructions with that guidance and the
[shared project setup](../../README.md#shared-project-setup-and-ownership).

## Prompt context (FYI)

The [Python README](../../python/ar-blending/README.md#prompt-context-fyi)
records the shared request history and reconstructed prompt for both implementations.
These notes are informational; use the checked-in source for exact reproduction.
