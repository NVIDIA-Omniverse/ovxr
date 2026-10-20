# OVXR library skills

These documentation-only skills teach integration with distributed OVXR SDK artifacts. Install a matched SDK and use its documented exports. No Kit implementation, test-source or reference-viewer checkout is required. A missing SDK capability is a supported feature boundary; do not infer that it exists from an older build.

| Task | Skill |
| --- | --- |
| Configure distributed OVXR Python bindings and their native dependencies | [project-setup-python](project-setup-python/SKILL.md) |
| Configure a C or C++ application against the OVXR native package | [project-setup-c](project-setup-c/SKILL.md) |
| Integrate the OVXR lifecycle with an application-owned OVRTX renderer and OVStage | [application-flow](application-flow/SKILL.md) |
| Choose OVXR context descriptor, runtime, scene metadata and lifecycle settings | [context-creation](context-creation/SKILL.md) |
| Coordinate OVXR with application or simulation writes to the borrowed OVStage | [stage-publication](stage-publication/SKILL.md) |
| Drive OVXR convenience or begin, bind and submit frame scheduling | [frame-loop](frame-loop/SKILL.md) |
| Consume OVXR CUDA frame captures without violating borrowed buffer lifetime | [frame-readback](frame-readback/SKILL.md) |
| Use managed or prim-sourced OVXR tracking anchors and convert OpenXR poses to stage space | [tracking-anchor-and-scale](tracking-anchor-and-scale/SKILL.md) |
| Integrate OpenXR actions, hands or extensions through OVXR borrowed handles | [openxr-input-and-extensions](openxr-input-and-extensions/SKILL.md) |
| Select OVXR view configuration, foveation policy and capture domain | [foveation-and-display](foveation-and-display/SKILL.md) |
| Configure OVXR VR/AR session and environment blending | [session-modes-and-passthrough](session-modes-and-passthrough/SKILL.md) |
| Identify supported OVXR seams for application composition and GPU resources | [composition-integration](composition-integration/SKILL.md) |
| Validate OVXR SDK contracts separately from runtime and application proof | [validation](validation/SKILL.md) |
| Integrate a separately supplied OpenXR runtime with OVXR | [runtime-adapters](runtime-adapters/SKILL.md) |

## Authority

The parent public README is the source of truth for packaging, runtime behavior and integration semantics. Use installed headers and bindings for exact signatures that it does not describe; do not override README contracts with implementation observations.

## Ownership

OVXR owns OpenXR lifecycle, frame coordination, primary submission and capture. The caller owns the borrowed renderer/stage, publication coordination when co-writing, application input resources and longer-lived image consumers. Runtime vendors own service and extension contracts. Viewer applications own navigation/tool policy, UI/compositors, simulation orchestration and product validation.


These skills follow the NVIDIA license supplied with the SDK. See the parent LICENSE for the applicable terms.
