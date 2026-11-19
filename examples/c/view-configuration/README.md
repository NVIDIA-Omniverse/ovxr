# View configuration (C++)

Same [behavior, controls and runtime limits](../../python/view-configuration/README.md)
as Python: mono/stereo/AUTO selection, resolved view count and observed view-0 size.

Build with matched SDKs using the [C++ build instructions](../../README.md#running-samples).
You can build all samples from `c/` or this directory alone. Run from the
executable's directory (use `./view-configuration` on Linux):

```powershell
.\view-configuration.exe --display auto --frames 180 --report views.json
.\view-configuration.exe --display stereo --frames 180
.\view-configuration.exe --display mono --frames 180
```

The runtime must expose the selected configuration. Restart to change modes.
Keep the staged `assets/` beside the executable. This is opaque VR with foveation
disabled, not a tablet or passthrough client.

[Recreation adaptation guide](../../python/view-configuration/README.md#adapting-this-sample)

## Adapting this sample

The [shared adaptation guide](../../python/view-configuration/README.md#adapting-this-sample)
covers application design, API ownership and validation for both implementations.
Use this README's C++ build/run instructions with that guidance and the
[shared project setup](../../README.md#shared-project-setup-and-ownership).

## Prompt context (FYI)

The [Python README](../../python/view-configuration/README.md#prompt-context-fyi)
records the shared request history and reconstructed prompt for both implementations.
These notes are informational; use the checked-in source for exact reproduction.
