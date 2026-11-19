# Runtime lifecycle (C++)

**App type:** stereo VR (opaque). Mono and AR are not configured by this sample. See the [compatibility matrix](../../COMPATIBILITY.md).

[Preview this sample in the gallery](../../gallery/README.md#runtime-lifecycle).

See [Adapting this sample](#adapting-this-sample) for guidance on using the
canonical skills in your application.

Keep the renderer and populated application scene alive while recreating the
OVXR context. Exercise setup, a bounded retry policy, and desktop mirroring
before trying a physical client disconnect.

After the [CMake build](../../README.md#running-samples), run from the executable directory
(PowerShell; on Linux use `./runtime-lifecycle`):
```powershell
.\runtime-lifecycle.exe --mirror
.\runtime-lifecycle.exe --mirror --reconnect-at 40 --frames 100 --report "$PWD\runtime-cpp.json"
```

Use your **official OpenXR client** with the existing CloudXR runtime setup.
Follow [CloudXR setup and headset validation](../../VALIDATION.md); a test-runtime
recreation is separate from a network disconnect/reconnect test.

| Control | Effect |
| --- | --- |
| R | Destroy the current OVXR context, then create a fresh one with the same renderer, stage and settings. |
| Q / Ctrl+C | Exit and release resources. |
| `--reconnect-at N` | Exercise one context recreation after N submitted frames. |
| `--retry-after N` | Recreate after N seconds without a submitted frame; default 10. |
| `--max-restarts N` | Limit consecutive recreation attempts without a submitted frame; default 3. A submitted frame resets the budget. Zero disables retries. |
| `--mirror` | Show a Windows window with fresh, unwarped left-eye readback. |
| `--report PATH.json` | Save submitted-frame, successful restart, attempt and mirror-frame counts after a successful run. |

`ERROR_OPENXR` triggers recreation. `NOT_READY` and `TIMEOUT` keep waiting;
absence of submissions beyond `--retry-after` triggers recreation. Failed OpenXR creations retry every half
second within the same budget; quit input remains active. Successful frame submission
refills the budget, so later user-requested reconnects or independent transient
failures do not consume a lifetime allowance. The report retains total attempts. This heuristic cannot distinguish a
disconnected client from an unfocused or idle headset. Increase the retry window
for long pauses. Errors outside OpenXR fail the sample. The timeout is checked
between API calls; it cannot interrupt a runtime blocked inside an API.
`--frames N` is a validation target, so quitting early returns failure.

OVXR owns session events and the frame loop. The sample destroys its context,
retains the application scene and renderer, and creates a new context. Never
carry borrowed frame views or OpenXR handles across that boundary. A real
application must also destroy its action spaces/hand trackers before replacing
the context and recreate them afterward.

The mirror copies the borrowed CUDA frame into owned CPU memory every third
submitted frame, before advancing XR. Its sequence counter resets with a new
context. The last copied image remains visible while reconnecting. This
synchronous diagnostic path adds overhead and is not a performance benchmark.
The window supports Windows only; omit `--mirror` on Linux. The JSON field
`headset_verified` remains false; record physical results separately.

The CMake target links the small `ovxr-sample-support` library and copies it beside the executable.

## Adapting this sample

The [shared adaptation guide](../../python/runtime-lifecycle/README.md#adapting-this-sample)
covers application design, API ownership and validation for both implementations.
Use this README's C++ build/run instructions with that guidance and the
[shared project setup](../../README.md#shared-project-setup-and-ownership).

## Prompt context (FYI)

The [Python README](../../python/runtime-lifecycle/README.md#prompt-context-fyi)
records the shared request history and reconstructed prompt for both implementations.
These notes are informational; use the checked-in source for exact reproduction.
