# OVXR skill coverage

<!-- Generated from the maintainer skill coverage catalog. -->

This map associates each skill with runnable examples and existing automated
checks. It is not a test run, a full prose specification audit or a claim of
fresh model-generation success. Maintainer CI runs the checks; private test
sources are not required to use the distributed skills.

All fourteen Python examples have runtime image scenarios (60 eye comparisons).
All eleven C++ entrypoints have compiled, smoke and runtime image checks:
44 eye comparisons reuse the corresponding Python references (104 total).
Composition layers includes stereo test-runtime composition plus independent
GPU texture readback and submitted-descriptor checks in both languages.
See the [sample validation guide](../examples/VALIDATION.md)
for source reproduction and independent generation evaluation.

Maintainer CI runs native/API and sample checks. Opt-in unit GPU tests and physical-device
checks must be reported separately when not executed. Platform, image and
hardware limits below are part of the evidence, not exceptions to hide.

## project-setup-python

Examples: [minimal-xr](../examples/python/minimal-xr/README.md#adapting-this-sample).

Runtime scenarios: `minimal`.

Associated checks:

- `test_python_api.py::test_built_native_library_loads_and_preloads_borrowed_runtimes` (maintainer checkout only)
- `test_python_api.py::test_windows_payload_dependencies_load_without_plugin_directory_on_path` (maintainer checkout only)

Limits: Native checks require the built SDK. This does not validate arbitrary package/version combinations.

## project-setup-c

Examples: [minimal-xr](../examples/python/minimal-xr/README.md#adapting-this-sample).

Runtime scenarios: `minimal`.

Associated checks:

- `test_cmake_package.py::test_dev_tree_cmake_package_configures_and_reports_version` (maintainer checkout only)
- `test_cpp_samples.py::test_packaged_cpp_samples` (maintainer checkout only)

Limits: The public CMake test configures stub dependencies; it is not a real SDK build. The sample CI separately compiles all eleven C++ entrypoints. CMake absence is reported as a skip.

## application-flow

Examples: [minimal-xr](../examples/python/minimal-xr/README.md#adapting-this-sample), [runtime-lifecycle](../examples/python/runtime-lifecycle/README.md#adapting-this-sample).

Runtime scenarios: `minimal`, `lifecycle`.

Associated checks:

- `test_python_api.py::test_context_lifecycle_and_high_level_acquire` (maintainer checkout only)
- `test_example_reconnect.py::test_context_replacement_preserves_renderer_and_stage` (maintainer checkout only)

Limits: Mock lifecycle checks and bounded runtime scenarios do not establish headset exit latency.

## context-creation

Examples: [minimal-xr](../examples/python/minimal-xr/README.md#adapting-this-sample), [runtime-lifecycle](../examples/python/runtime-lifecycle/README.md#adapting-this-sample).

Runtime scenarios: `minimal`, `lifecycle`.

Associated checks:

- `test_python_api.py::test_context_scene_info_marshals_resolved_values_for_each_policy` (maintainer checkout only)
- `test_python_api.py::test_library_telemetry_opt_out_reaches_the_c_symbol_from_both_entry_points` (maintainer checkout only)
- `test_python_api.py::test_check_openxr_runtime_decodes_status_without_a_context` (maintainer checkout only)

Limits: Only the scenario configurations are rendered, not every descriptor combination or deployment policy.

## stage-publication

Examples: [live-scene](../examples/python/live-scene/README.md#adapting-this-sample).

Runtime scenarios: `live-scene`.

Associated checks:

- `test_example_lifecycle.py::test_not_ready_retries_new_plan_with_same_ordinal_and_one_scene_write` (maintainer checkout only)
- `test_example_lifecycle.py::test_failed_bind_cancels_and_reconciles_sealed_ordinal` (maintainer checkout only)

Limits: Single-process sample publication is covered; arbitrary multi-writer simulation integrations are not.

## frame-loop

Examples: [minimal-xr](../examples/python/minimal-xr/README.md#adapting-this-sample), [live-scene](../examples/python/live-scene/README.md#adapting-this-sample).

Runtime scenarios: `minimal`, `live-scene`.

Associated checks:

- `test_example_lifecycle.py::test_consumed_failed_submission_is_not_cancelled_twice` (maintainer checkout only)
- `test_example_lifecycle.py::test_success_without_submission_does_not_reset_timeout` (maintainer checkout only)

Limits: The samples exercise OVXR-stepped frames. Application-stepped SDK variants need their own matched-version tests.

## frame-readback

Examples: [frame-capture](../examples/python/frame-capture/README.md#adapting-this-sample), [async-capture](../examples/python/async-capture/README.md#adapting-this-sample).

Runtime scenarios: `pip-camera`, `pip-eye`, `async-capture`.

Associated checks:

- `test_python_api.py::test_frame_pointer_expires_after_two_capture_capable_calls` (maintainer checkout only)
- `test_example_new_features.py::test_slow_consumer_keeps_latest_pending_owned_frame` (maintainer checkout only)

Limits: Readback ownership is checked; no claim of throughput or headset streaming latency.

## tracking-anchor-and-scale

Examples: [anchors-and-teleport](../examples/python/anchors-and-teleport/README.md#adapting-this-sample), [spatial-workbench](../examples/python/spatial-workbench/README.md#adapting-this-sample).

Runtime scenarios: `anchors`, `workbench`.

Associated checks:

- `test_python_api.py::test_context_anchor_marshals_prim_readback_and_openxr_pose` (maintainer checkout only)

Limits: Scripted anchors are covered. Physical tracking scale and comfort need device validation.

## openxr-input-and-extensions

Examples: [anchors-and-teleport](../examples/python/anchors-and-teleport/README.md#adapting-this-sample), [spatial-workbench](../examples/python/spatial-workbench/README.md#adapting-this-sample).

Runtime scenarios: `anchors`, `workbench`.

Associated checks:

- `test_python_api.py::test_desc_marshals_extensions_and_depth_planes` (maintainer checkout only)
- `test_example_workbench.py::test_reacquiring_tracking_while_pressed_requires_release` (maintainer checkout only)

Limits: Demo input is synthetic. Controller, joint tracking and gaze/pinch need their actual runtime and device.

## foveation-and-display

Examples: [foveation](../examples/python/foveation/README.md#adapting-this-sample), [view-configuration](../examples/python/view-configuration/README.md#adapting-this-sample).

Runtime scenarios: `foveation-off`, `foveation-local`, `foveation-runtime`, `view-configuration`.

Associated checks:

- `test_example_new_features.py::test_display_modes_report_actual_negotiation_including_auto_mono_fallback` (maintainer checkout only)

Limits: Runtime visuals use the pinned stereo profile; mono fallback has a behavioral test, not a mono golden here. No physical quad-view proof.

## session-modes-and-passthrough

Examples: [ar-blending](../examples/python/ar-blending/README.md#adapting-this-sample).

Runtime scenarios: `ar-blending`.

Associated checks:

- `test_example_new_features.py::test_alpha_statistics_ignore_padding_and_preserve_black_opaque_pixels` (maintainer checkout only)

Limits: AR checks cover submitted alpha and resolved blending, not physical camera passthrough or real-world depth.

## composition-integration

Examples: [composition-layers](../examples/python/composition-layers/README.md#adapting-this-sample), [ovui-panel](../examples/python/ovui-panel/README.md#adapting-this-sample).

Runtime scenarios: `ovui-panel`, `overlay-head`, `overlay-world`.

Associated checks:

- `test_python_api.py::test_submit_descriptor_forwards_layers_and_retains_them_through_native_call` (maintainer checkout only)
- `TestApiBoundary.cpp::[priority=mandatory] OVXR appends projection and quad layers and omits them on no-render` (maintainer checkout only)
- `test_samples.py::test_overlay_swapchain_and_submission` (maintainer checkout only)
- `test_example_quad.py::test_quad_rejects_a_corrupted_gpu_readback` (maintainer checkout only)
- `test_cpp_visual.py::test_cpp_sample_image` (maintainer checkout only)
- `test_cpp_visual.py::test_packaged_cpp_samples_render` (maintainer checkout only)

Limits: Python and C++ head/world scenarios compare stereo output from the isolated test runtime, composing every submitted projection/quad layer. Independent Vulkan readback and descriptor checks remain. Physical anchoring, lens distortion and headset reprojection need hardware validation. OVUI uses the separate scene-occluded replacement projection path.

## runtime-adapters

Examples: [runtime-lifecycle](../examples/python/runtime-lifecycle/README.md#adapting-this-sample).

Runtime scenarios: `lifecycle`.

Associated checks:

- `test_python_api.py::test_openxr_probe_decodes_strings` (maintainer checkout only)
- `test_example_reconnect.py::test_temporary_creation_failure_retries_and_preserves_application` (maintainer checkout only)

Limits: Probe/recreation uses the test runtime. CloudXR service/client compatibility and hardware reconnect remain manual.

## validation

Examples: [minimal-xr](../examples/python/minimal-xr/README.md#adapting-this-sample), [live-scene](../examples/python/live-scene/README.md#adapting-this-sample), [anchors-and-teleport](../examples/python/anchors-and-teleport/README.md#adapting-this-sample), [foveation](../examples/python/foveation/README.md#adapting-this-sample), [frame-capture](../examples/python/frame-capture/README.md#adapting-this-sample), [runtime-lifecycle](../examples/python/runtime-lifecycle/README.md#adapting-this-sample), [spatial-workbench](../examples/python/spatial-workbench/README.md#adapting-this-sample), [ovui-panel](../examples/python/ovui-panel/README.md#adapting-this-sample), [scene-inspector](../examples/python/scene-inspector/README.md#adapting-this-sample), [spatial-ui](../examples/python/spatial-ui/README.md#adapting-this-sample), [ar-blending](../examples/python/ar-blending/README.md#adapting-this-sample), [view-configuration](../examples/python/view-configuration/README.md#adapting-this-sample), [async-capture](../examples/python/async-capture/README.md#adapting-this-sample), [composition-layers](../examples/python/composition-layers/README.md#adapting-this-sample).

Runtime scenarios: `minimal`, `live-scene`, `anchors`, `foveation-off`, `foveation-local`, `foveation-runtime`, `pip-camera`, `pip-eye`, `lifecycle`, `workbench`, `ovui-panel`, `inspector`, `spatial-ui`, `ar-blending`, `view-configuration`, `async-capture`, `overlay-head`, `overlay-world`.

Associated checks:

- `test_example_image_validation.py::test_empty_clear_buffers_are_rejected` (maintainer checkout only)
- `test_samples.py::test_sample_image` (maintainer checkout only)

Limits: A nonuniform image is not proof of correct content. Approved-reference comparisons and visual review remain separate; no fresh-model generation evaluation is recorded.

## scene-occluded-ui

Examples: [ovui-panel](../examples/python/ovui-panel/README.md#adapting-this-sample), [scene-inspector](../examples/python/scene-inspector/README.md#adapting-this-sample), [spatial-ui](../examples/python/spatial-ui/README.md#adapting-this-sample).

Runtime scenarios: `ovui-panel`, `inspector`, `spatial-ui`.

Associated checks:

- `test_example_panel_gpu.py::test_stereo_depth_composition_preserves_foreground_and_writes_panel_depth` (maintainer checkout only)
- `test_example_spatial_ui.py::test_cube_blocks_input_even_after_the_near_panel_is_moved` (maintainer checkout only)

Limits: The unit compositor test is opt-in GPU coverage. Runtime scenarios cover fixed panel planes, simple picking and unfoveated stereo, not arbitrary meshes/rotations or headset latency.

## spatial-interaction

Examples: [spatial-workbench](../examples/python/spatial-workbench/README.md#adapting-this-sample), [scene-inspector](../examples/python/scene-inspector/README.md#adapting-this-sample), [spatial-ui](../examples/python/spatial-ui/README.md#adapting-this-sample).

Runtime scenarios: `workbench`, `inspector`, `spatial-ui`.

Associated checks:

- `test_example_workbench.py::test_grab_has_no_initial_jump_and_tracking_loss_cancels_drag` (maintainer checkout only)
- `test_example_inspector.py::test_drag_is_one_command_and_reset_is_undoable` (maintainer checkout only)

Limits: Scripted input and command behavior are covered; this is not a complete editor or proof of physical controller ergonomics.
