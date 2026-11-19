# Asynchronous capture consumer (C++)

Same [ownership pattern, counters and limits](../../python/async-capture/README.md)
as Python: copy on the XR thread, one pending owned image, and a slow worker.

Build with matched SDKs using the [C++ build instructions](../../README.md#running-samples).
You can build all samples from `c/` or this directory alone. Run from the
executable's directory (use `./async-capture` on Linux):

```powershell
.\async-capture.exe --frames 180 --consumer-ms 200 --report capture.json
.\async-capture.exe --frames 180 --consumer-ms 0
```

The consumer joins before XR teardown, including error paths. Read its counters
only after joining. The pending image is replaced by newer work; the active
image is immutable until processing completes. CMake links the platform thread
library. CUDA readback uses the installed driver and needs no CUDA toolkit.
Keep the staged `assets/` beside the executable.

[Recreation adaptation guide](../../python/async-capture/README.md#adapting-this-sample)

## Adapting this sample

The [shared adaptation guide](../../python/async-capture/README.md#adapting-this-sample)
covers application design, API ownership and validation for both implementations.
Use this README's C++ build/run instructions with that guidance and the
[shared project setup](../../README.md#shared-project-setup-and-ownership).

## Prompt context (FYI)

The [Python README](../../python/async-capture/README.md#prompt-context-fyi)
records the shared request history and reconstructed prompt for both implementations.
These notes are informational; use the checked-in source for exact reproduction.
