# ovxr Examples

This directory contains example projects demonstrating the ovxr OpenXR session
API. Each example is a self-contained project: the C example builds standalone
with CMake against the released ovxr package (paired with the ovrtx and ovstage
packages it layers on), and the Python example runs with
[uv](https://docs.astral.sh/uv/) against the released `ovxr` wheel. Per-example
READMEs carry the exact build and run commands.

The examples fail fast — any unexpected API failure prints and exits, so the
happy path reads straight through; a real application would propagate errors
instead.


## Example Projects

- **Minimal XR** — *"Populate an application-owned ovstage from USD, attach it
  to an ovrtx renderer, hand both to ovxr, and run a VR session: probe the
  OpenXR runtime, render and submit stereo frames sized to the runtime's
  recommended swapchain, and tear down cleanly."*
  Build & run in: [C →](c/minimal-xr/), [Python →](python/minimal-xr/)

## Snippets and skills

The example sources are the source of truth for code snippets referenced by
future ovxr skills. Snippet regions are marked `[snippet:name]` …
`[/snippet:name]`; keep the markers intact when editing example code.
