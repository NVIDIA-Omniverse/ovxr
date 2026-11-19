# Sample app types

These describe the samples **as shipped**, in both languages where available.
Stereo/mono describes the application's OpenXR view configuration. VR/AR describes
its environment blend/session mode; those are separate choices.

| Sample | Display | VR | AR / passthrough | Main constraints |
| --- | --- | --- | --- | --- |
| [Minimal XR](python/minimal-xr/README.md) | Stereo | Default | Exposes `--session-mode ar` and `--blend-mode alpha\|additive\|auto`; runtime-dependent and not headset-validated | Start here to experiment with session/blend configuration. |
| [Live scene](python/live-scene/README.md) | Stereo | Opaque | Not configured | Scene animation and application-owned ordinals. |
| [Anchors and teleport](python/anchors-and-teleport/README.md) | Stereo | Opaque | Not configured | Artificial locomotion in a virtual room; not real-world anchoring. |
| [Foveation](python/foveation/README.md) | Stereo | Opaque | Not configured | Runtime foveation requires runtime support. |
| [Frame capture / PiP](python/frame-capture/README.md) | Stereo XR; mono PiP image | Opaque | Not configured | Separate scene camera by default; optional left/right eye capture. Desktop PiP requires Windows. |
| [Runtime lifecycle](python/runtime-lifecycle/README.md) | Stereo | Opaque | Not configured | Context restart and optional Windows mirror; network recovery is a separate headset check. |
| [Spatial workbench](python/spatial-workbench/README.md) | Stereo | Opaque | Not configured | Controller aim/select or `XR_EXT_hand_tracking`; synthetic hands in demo mode. |
| [OVUI panel](python/ovui-panel/README.md) | Stereo | Opaque | Not configured | Python, opaque panels, scene depth, no foveation. Copies the canvas when OVUI and XR use different GPUs. |
| [Scene inspector](python/scene-inspector/README.md) | Stereo | Opaque | Not configured | Same OVUI limits; controller/hand/demo pointing. |
| [Spatial UI](python/spatial-ui/README.md) | Stereo | Opaque | Not configured | Same OVUI limits; world-facing panels, header drag and nearest-panel input. |
| [Composition layers](python/composition-layers/README.md) / [C++](c/composition-layers/README.md) | Stereo, quad visible to both eyes | Opaque scene plus alpha overlay | Not configured | Independent head/tracking-space quad, no scene depth test. Shared composed stereo goldens plus actual Vulkan readback; physical headset blending/anchoring still need device checks. |
| [AR blending](python/ar-blending/README.md) | Stereo | Not configured | Default AR/alpha; supports explicit opaque/additive and AUTO negotiation | Transparent background and opaque objects; actual passthrough requires runtime/client support. |
| [View configuration](python/view-configuration/README.md) | Mono, stereo or AUTO | Opaque | Not configured | Runtime must expose the selected view configuration. Reports actual view-0 size; no tablet client. |
| [Asynchronous capture](python/async-capture/README.md) | Stereo XR; mono captured eye | Opaque | Not configured | Owned CPU copy, one pending frame and a slow worker; no encoder/network stack. |

The view-configuration sample exposes a mono OpenXR app mode when the runtime
supports it. A mono PiP image or desktop mirror does not change a stereo session.
When adapting other samples, check runtime support, view counts, eye-index
assumptions and any per-eye compositor code.

For AR, the runtime must offer a suitable session and environment blend mode.
Changing a flag does not add camera passthrough, real-world depth occlusion or
spatial mapping to these samples. The room/floor backgrounds and opaque OVUI
compositor need deliberate adaptation for alpha blending.

The recorded checks use the Windows OpenXR emulator. Quest Pro and Apple Vision
Pro checks with the official CloudXR OpenXR client are pending. In particular,
hand tracking requires the runtime/client to expose the extension and valid
joints; these samples do not implement AVP eye-gaze selection. Controller/hand
beams visualize the application's actual hit-test ray. Demo rays and sphere
hands are synthetic input, not evidence of a tracked device.

[Gallery](gallery/README.md) · [Run instructions](README.md) · [Validation](VALIDATION.md)
