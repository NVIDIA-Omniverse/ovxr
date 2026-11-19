# Spatial workbench (C++)

**App type:** stereo VR (opaque). Mono and AR are not configured by this sample. See the [compatibility matrix](../../COMPATIBILITY.md).

[Preview this sample in the gallery](../../gallery/README.md#spatial-workbench).

See [Adapting this sample](#adapting-this-sample) for guidance on using the
canonical skills in your application.

A shared application model drives an editable cube, four world-space buttons,
undo, and a movable tracking origin. Desktop keys and tracked input dispatch the
same commands. The mirror displays the rendered state and a revision in its title.

After the [CMake build](../../README.md#running-samples), run from the executable directory
(PowerShell; on Linux use `./spatial-workbench`):
```powershell
# Quest Pro: controller aim and trigger, thumbstick snap turn.
.\spatial-workbench.exe --input controllers --mirror
# AVP: requires XR_EXT_hand_tracking from the runtime/client.
.\spatial-workbench.exe --input hands --mirror
# Synthetic emulator sequence; never counts as headset proof.
.\spatial-workbench.exe --input demo --frames 100 --mirror --report "$PWD\workbench-cpp.json"
```

Use the [official-client setup and Quest Pro / AVP checklist](../../VALIDATION.md).
Both hardware targets are pending user validation.

| Input | Behavior |
| --- | --- |
| Buttons with 1 / 2 / 3 / 4 bars, or keys 1 / 2 / 3 / 4 | Move cube left / right, rotate 22.5 degrees, undo. |
| Aim and hold trigger on cube | Grab and move at its original ray distance; release to drop. |
| Point index finger and pinch thumb/index | Hand-mode equivalent of trigger selection/grab. |
| Select unobstructed floor within five meters of scene origin on X/Z | Move the tracking origin to that floor point at the next frame boundary. |
| Thumbstick X, or Z / X | Snap turn 30 degrees, rearming after the stick returns to center. |
| O | Hide/show the violet pillar occluding button 4 from the initial viewpoint. |
| R | Reset cube position and rotation; undoable. |
| Q / Ctrl+C | Exit. |

The right located controller/hand is preferred; the left is the fallback.
Controllers bind Simple and Oculus Touch profiles through OpenXR actions.
Hand mode requests `XR_EXT_hand_tracking`, displays its 26 joints per hand and
derives a ray from the index proximal joint toward the index tip. Pinch uses
2.5 cm press / 4 cm release thresholds. This is a deliberately simple finger
gesture, not a platform gaze-and-pinch interaction. Unsupported hand tracking
fails clearly; it never silently substitutes demo input.

`c/sample-support/input.cpp` contains the shared native OpenXR code used by
both languages. It borrows OVXR's instance/session/reference space, attaches
one action set, and owns only action spaces/sets or hand trackers. It never
polls session events or begins/ends an XR frame. Read input at predicted display
time, then map poses **after begin_frame** once a renderable bind has resolved
the initial anchor. This release latches anchor updates at bind: navigation is
queued after submission and tracked markers are hidden for the transition frame
so old input mapping is never displayed against the new eye mapping.
With a stable anchor, eyes, pointers and joint markers agree. Check focus and location validity
before using a pose. Input loss, a different selected hand, or a session pause
cancels a grab; regaining input while held requires release and a fresh press.

Panels are scene geometry composed with the cube and room in the projection
image. The cube, panel, buttons and violet occluder share nearest-hit analytical
picking, so the occluder blocks button clicks as well as rendering. O exposes the
undo button; undoing O restores the occluder. These are illustrative colliders,
not general USD scene picking. The depth submission policy is AUTO and the report
records its effective enablement after frames have been submitted. Scene
occlusion works even when a runtime declines depth submission.

OVXR also accepts application-owned OpenXR composition layers at submit.
This sample covers scene panel layout and occlusion, not compositor layers or
real-world passthrough occlusion. State synchronization is one in-process model
shared by desktop/XR controls, not a distributed multiuser protocol. Teleport
places the tracking origin, retaining the user's physical offset in that space.

Demo rays exercise move, rotate, a blocked click, dragging and floor navigation.
Reports separate synthetic mode from active controller/hand frame counts and
always leave `headset_verified: false`. Use `--no-foveation` for a full-resolution
baseline. `--frames N` counts submitted frames and fails if stopped early.
The diagnostic mirror is Windows-only and performs a synchronous CPU copy
every third frame; omit it for timing measurements or Linux.

Build with CMake as in the gallery. For a standalone copy, retain `../common`, `../sample-support`, `../cmake` and `../../assets`, and set `SAMPLE_OPENXR_INCLUDE_DIR` to your OpenXR SDK headers. A GPU-free `workbench-model` CTest checks interaction transitions.

## Visible pointing

A magenta beam follows the same origin/direction used for input and ends at the
nearest sample collider (or the ray's range limit). The white tip marks the hit;
the beam gets thicker while select/pinch is held. Invalid or lost tracking hides
the ray. `--input demo` uses synthetic rays from a visible off-center origin.
These markers explain the interaction; they are not a separate picking system.

Mint green identifies hands/controllers; cyan buttons have white markings and a
light ivory backing panel that contrasts with their cast shadows. The violet
occluder is distinct from the amber cube and magenta ray.

Both demo and hands modes draw 26 small joint spheres, 24 connecting cylinders
and a rounded palm per hand. Cylinders join valid joint positions; the palm is
a flattened sphere oriented and sized from the wrist and index/little knuckles.
Missing joints hide their attached segments; missing or degenerate palm points
hide the palm. No joint orientation or external hand mesh is required.

Demo mode uses illustrative finger proportions and an animated thumb/index
pinch. The right index tip is the pointing ray's origin. Hands mode instead uses
valid tracked OpenXR positions. Synthetic hands do not increment tracked-hand
evidence and do not prove headset support.

## Adapting this sample

The [shared adaptation guide](../../python/spatial-workbench/README.md#adapting-this-sample)
covers application design, API ownership and validation for both implementations.
Use this README's C++ build/run instructions with that guidance and the
[shared project setup](../../README.md#shared-project-setup-and-ownership).

## Prompt context (FYI)

The [Python README](../../python/spatial-workbench/README.md#prompt-context-fyi)
records the shared request history and reconstructed prompt for both implementations.
These notes are informational; use the checked-in source for exact reproduction.
