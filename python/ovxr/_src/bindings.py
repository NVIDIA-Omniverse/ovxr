# Copyright (c) 2026, NVIDIA CORPORATION. All rights reserved.
#
# NVIDIA CORPORATION and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA CORPORATION is strictly prohibited.

"""ctypes binding for the ovxr C API (libovxr-dynamic.so).

ovxr bridges the OVRTX renderer to OpenXR. This module mirrors ``ovxr/ovxr.h``;
struct field order and types are byte-exact with that header.

Symbol-sharing contract: a consumer drives ONE ``ovrtx.Renderer`` (the ``ovrtx``
wheel, which loads ``libovrtx-dynamic.so`` via ctypes). ovxr must use the SAME
``libovrtx`` instance so the renderer pointer it borrows is valid. We therefore
PRELOAD the wheel's ``libovrtx-dynamic.so`` with ``RTLD_GLOBAL`` *before* loading
``libovxr-dynamic.so``; the shim's ``NEEDED libovrtx-dynamic.so`` (same SONAME)
then binds to that instance, and the wheel's own lazy load dedups onto it.

OVStage follows the same ownership rule: its wheel loader initializes the
process's OVStage runtime before ovxr loads. Besides ensuring that ovxr binds to
the consumer's OVStage instance, this lets OVStage retain responsibility for its
own trusted dependency directories on Windows.

The conventions here follow the sibling ``ovrtx`` package
(``ovrtx/_src/bindings.py``): C-style names for the raw ctypes structs
(``ovxr_desc_t`` …), :class:`enum.IntEnum` for the C enums, a low-level
:class:`Bindings` wrapper exposing one method per C entry point, and a lazily
loaded :class:`_LibraryLoader` singleton honoring :data:`OVXR_LIBRARY_PATH_HINT`.
"""
from __future__ import annotations

import ctypes
import operator
import os
import sys
from ctypes import (
    CFUNCTYPE,
    POINTER,
    Structure,
    c_bool,
    c_char_p,
    c_double,
    c_float,
    c_int32,
    c_int64,
    c_size_t,
    c_uint32,
    c_uint64,
    c_void_p,
)
from enum import IntEnum
from pathlib import Path
from threading import Lock
from typing import List, Optional

# ovxr's frame plan embeds ovrtx struct types (ovxr.h itself `#include`s
# <ovrtx/ovrtx.h>). Import them from the ovrtx package rather than redefining,
# so the two bindings can never disagree on the shared ABI.
from ovrtx._src.bindings import (
    OVRTX_LOADER_LIB_NAME,
    ovrtx_render_product_set_t,
)

OVXR_LOADER_LIB_NAME: str = "ovxr-dynamic.dll" if sys.platform.startswith("win") else "libovxr-dynamic.so"
"""Basename of the dlopen-able ovxr shared library."""


# ---------------------------------------------------------------------------
# Enums (mirror ovxr.h; IntEnum to match the ovrtx binding convention)
# ---------------------------------------------------------------------------
class Result(IntEnum):
    """``ovxr_result_t`` — return code of the ovxr C entry points."""

    SUCCESS = 0
    TIMEOUT = 1
    NOT_READY = 2
    ERROR = -1
    ERROR_OPENXR = -2
    ERROR_OVRTX = -3
    ERROR_DEVICE_MISMATCH = -4

    @classmethod
    def _missing_(cls, value):
        """Preserve result codes added by a newer native payload."""
        if not isinstance(value, int):
            return None

        unknown = int.__new__(cls, value)
        unknown._name_ = f"UNKNOWN_{value}"
        unknown._value_ = value
        return unknown


def result_name(code: int) -> str:
    """Return the ``OVXR_*`` name for an ``ovxr_result_t`` code."""
    return f"OVXR_{Result(code).name}"


class StageOrdinalMode(IntEnum):
    """Selects whether OVXR or its embedding application owns Stage ordinals."""

    PRIVATE = 0
    APPLICATION = 1


def check_ordinal(value: int) -> int:
    """Bound a caller-supplied stage ordinal to the ``c_uint64`` field behind it.

    ctypes never raises on an out-of-range value — declared argtypes wrap mod
    ``2**64`` — so this is the only place a negative or over-wide ordinal can
    fail instead of silently coercing. ``operator.index`` (not ``int()``) so
    floats and other truncating types are rejected rather than truncated.
    """
    ordinal = operator.index(value)
    if ordinal < 0 or ordinal > 0xFFFFFFFFFFFFFFFF:
        raise ValueError(f"stage ordinal out of uint64 range: {value!r}")
    return ordinal


class FeaturePolicy(IntEnum):
    """``ovxr_feature_policy_t``.

    ``AUTO`` uses the feature when the runtime advertises it and silently skips
    it otherwise; ``DISABLED`` never uses it; ``REQUIRED`` fails session creation
    when the runtime does not advertise it.
    """

    AUTO = 0
    DISABLED = 1
    REQUIRED = 2


class DisplayMode(IntEnum):
    """``ovxr_display_mode_t``."""

    AUTO = 0
    STEREO = 1
    QUAD_VIEW = 2
    MONO = 3


class SessionMode(IntEnum):
    """``ovxr_session_mode_t``."""

    AUTO = 0
    VR = 1
    AR = 2


class EnvironmentBlendMode(IntEnum):
    """``ovxr_environment_blend_mode_t``."""

    AUTO = 0
    OPAQUE = 1
    ALPHA_BLEND = 2
    ADDITIVE = 3


class LocalFoveationMode(IntEnum):
    """Local render-product foveation mode (``ovxr_local_foveation_mode_t``)."""

    AUTO = 0
    DISABLED = 1
    WARPED = 2


class GraphicsApi(IntEnum):
    """``ovxr_graphics_api_t`` — the graphics API OVXR uses for the OpenXR session."""

    AUTO = 0
    VULKAN = 1
    D3D12 = 2


class StageUpAxis(IntEnum):
    """``ovxr_stage_up_axis_t`` — maps OpenXR's +Y-up tracking poses into the stage.

    ``Y`` uses OpenXR's +Y-up poses unchanged; ``Z`` applies a +90-degree
    about-X basis change so poses land in a Z-up stage.
    """

    Y = 0
    Z = 1

    @classmethod
    def _missing_(cls, value):
        """Preserve up-axis values added by a newer native payload."""
        if not isinstance(value, int):
            return None

        unknown = int.__new__(cls, value)
        unknown._name_ = f"UNKNOWN_{value}"
        unknown._value_ = value
        return unknown


class SceneInfoPolicy(IntEnum):
    """``ovxr_stage_scene_info_policy_t`` — where scene scale and up-axis resolve from.

    ``AUTO`` prefers OVStage metadata when available, then the descriptor, then
    defaults; ``FROM_DESC`` uses the descriptor/defaults (OVStage still required);
    ``FROM_OVSTAGE`` makes missing OVStage metadata an error.
    """

    AUTO = 0
    FROM_DESC = 1
    FROM_OVSTAGE = 2


class SceneInfoSource(IntEnum):
    """``ovxr_scene_info_source_t`` — origin of a resolved scene-info field."""

    DEFAULT = 0
    DESCRIPTOR = 1
    OVSTAGE = 2

    @classmethod
    def _missing_(cls, value):
        """Preserve sources added by a newer native payload."""
        if not isinstance(value, int):
            return None

        unknown = int.__new__(cls, value)
        unknown._name_ = f"UNKNOWN_{value}"
        unknown._value_ = value
        return unknown

class FrameFormat(IntEnum):
    """``ovxr_frame_format_t`` — pixel format of an :func:`Bindings.acquire_frame` view."""

    RGBA8_UNORM = 0

    @classmethod
    def _missing_(cls, value):
        """Preserve formats added by a newer native payload."""
        if not isinstance(value, int):
            return None

        unknown = int.__new__(cls, value)
        unknown._name_ = f"UNKNOWN_{value}"
        unknown._value_ = value
        return unknown


class Setting(IntEnum):
    """``ovxr_setting_int64_t`` — int64-valued setting keys (order matches ovxr.h)."""

    DISPLAY_MODE = 0
    LOCAL_FOVEATION_MODE = 1
    RUNTIME_FOVEATED_SUBMISSION_POLICY = 2
    DEPTH_SUBMISSION_POLICY = 4
    SESSION_MODE = 5
    ENVIRONMENT_BLEND_MODE = 6
    AR_BLEND_TRANSLUCENCY_AS_OPACITY = 7
    GRAPHICS_API = 8
    FLOOR_ALIGNED_ANCHOR = 9
    HOST_WAIT_RENDER_OUTPUT = 10


class SettingKeyType(IntEnum):
    """``ovxr_setting_key_type_t``."""

    INT64 = 0


# ---------------------------------------------------------------------------
# Structs — byte-exact with ovxr.h / ovx/types.h
# ---------------------------------------------------------------------------
class ovx_string_t(Structure):
    """``ovx_string_t { const char* ptr; size_t length; }``."""

    _fields_ = [("ptr", c_char_p), ("length", c_size_t)]

    @classmethod
    def make(cls, s: str | None) -> "ovx_string_t":
        """Build an ``ovx_string_t`` from a Python string, keeping the bytes alive."""
        if s is None:
            return cls(None, 0)
        b = s.encode("utf-8")
        inst = cls(b, len(b))
        inst._keepalive = b  # prevent GC of the bytes backing ptr
        return inst


class _SettingValue(ctypes.Union):
    _fields_ = [("int_value", c_int64)]


class _SettingKey(ctypes.Union):
    _fields_ = [("int64_key", c_int32)]  # ovxr_setting_int64_t (enum = int)


class ovxr_setting_entry_t(Structure):
    """``ovxr_setting_entry_t { key_type; union key; union value; }``."""

    _fields_ = [
        ("key_type", c_int32),  # ovxr_setting_key_type_t
        ("key", _SettingKey),
        ("value", _SettingValue),
    ]

    @classmethod
    def int_entry(cls, key: int, value: int) -> "ovxr_setting_entry_t":
        """Build an int64 setting entry for setting key :class:`Setting` ``key``."""
        return cls(int(SettingKeyType.INT64), _SettingKey(int(key)), _SettingValue(int(value)))


class ovxr_settings_t(Structure):
    """``ovxr_settings_t { const ovxr_setting_entry_t* entries; size_t entry_count; }``."""

    _fields_ = [("entries", POINTER(ovxr_setting_entry_t)), ("entry_count", c_size_t)]


class ovxr_desc_t(Structure):
    """``ovxr_desc_t`` — mirror ovxr.h field order exactly."""

    _fields_ = [
        ("ovrtx_renderer", c_void_p),            # borrowed ovrtx_renderer_t*
        ("ovstage", c_void_p),                   # borrowed ovstage_instance_t*
        ("application_name", c_char_p),
        ("runtime_prim_root", c_char_p),
        ("settings", POINTER(ovxr_settings_t)),
        ("max_frames_in_flight", c_uint32),
        ("max_render_gpus", c_uint32),
        ("enable_debug_labels", c_bool),
        ("render_products", c_void_p),           # const ovx_string_t*
        ("render_product_count", c_size_t),
        ("camera_paths", c_void_p),              # const ovx_string_t*
        ("camera_path_count", c_size_t),
        ("meters_per_unit", c_float),
        ("create_render_products", c_bool),
        ("render_vars", c_void_p),               # const ovx_string_t*
        ("render_var_count", c_size_t),
        ("source_render_product", ovx_string_t),  # by value
        ("render_settings", ovx_string_t),        # by value
        ("foveation_unwarp", c_bool),
        ("foveation_unwarped_width", c_int32),
        ("foveation_unwarped_height", c_int32),
        ("foveation_warped_resolution_multiplier", c_float),
        ("foveation_warped_inset_size", c_float),
        ("tracking_anchor_path", ovx_string_t),  # by value
        ("up_axis", c_int32),                    # ovxr_stage_up_axis_t
        # Extra OpenXR instance extensions the application needs enabled.
        ("extra_openxr_extensions", c_void_p),   # const ovx_string_t*
        ("extra_openxr_extension_count", c_size_t),
        # Eye-camera near/far clip planes, in meters.
        ("depth_near_z", c_float),
        ("depth_far_z", c_float),
        ("scene_info_policy", c_int32),          # ovxr_stage_scene_info_policy_t
        # Optional configured render-product color capture.
        ("enable_frame_readback", c_bool),
        ("readback_render_product_index", c_uint32),
        ("readback_foveation_unwarp", c_bool),
        ("ordinal_mode", c_int32),  # ovxr_stage_ordinal_mode_t
        ("application_steps_renderer", c_bool),
    ]


class ovxr_scene_info_t(Structure):
    """``ovxr_scene_info_t`` — resolved scene scale and up-axis."""

    _fields_ = [
        ("meters_per_unit", c_float),
        ("meters_per_unit_source", c_int32),  # ovxr_scene_info_source_t
        ("up_axis", c_int32),  # ovxr_stage_up_axis_t
        ("up_axis_source", c_int32),  # ovxr_scene_info_source_t
    ]


class ovxr_frame_view_t(Structure):
    """``ovxr_frame_view_t`` — output of :meth:`Bindings.acquire_frame`.

    ``device_ptr`` is a read-only (``const void*``) CUDA device pointer to a
    tightly packed (``pitch_bytes == width*4``) ``FrameFormat.RGBA8_UNORM`` image
    on ``cuda_device_id``, owned by ovxr; do not write to or free it. The
    double-buffered pointer remains valid until capture of ``sequence + 2``
    begins. ``sequence`` is a 1-based monotonic capture counter.
    ``predicted_display_time_ns`` is the producing frame's OpenXR predicted
    display time, or 0 when the frame had no OpenXR timing. ctypes inserts the
    same padding the C ABI does.
    """

    _fields_ = [
        ("device_ptr", c_void_p),
        ("width", c_uint32),
        ("height", c_uint32),
        ("pitch_bytes", c_uint32),
        ("cuda_device_id", c_int32),
        ("format", c_int32),  # ovxr_frame_format_t (enum = int)
        ("sequence", c_uint64),
        ("predicted_display_time_ns", c_int64),
    ]


class ovxr_gpu_identity_t(Structure):
    """``ovxr_gpu_identity_t``."""

    _fields_ = [
        ("has_device_luid", c_bool),
        ("device_luid", ctypes.c_uint8 * 8),
        ("device_node_mask", c_uint32),
        ("device_uuid", ctypes.c_uint8 * 16),
        ("driver_uuid", ctypes.c_uint8 * 16),
        ("cuda_device_id", c_int32),
    ]


class ovxr_runtime_features_t(Structure):
    """``ovxr_runtime_features_t`` — runtime-negotiated feature state."""

    _fields_ = [
        ("display_mode", c_int32),            # ovxr_display_mode_t
        ("graphics_api", c_int32),            # ovxr_graphics_api_t
        ("session_mode", c_int32),            # ovxr_session_mode_t
        ("environment_blend_mode", c_int32),  # ovxr_environment_blend_mode_t
        ("local_foveation_mode", c_int32),    # ovxr_local_foveation_mode_t
        ("ar_blend_enabled", c_bool),
        ("environment_blend_opaque_supported", c_bool),
        ("environment_blend_alpha_supported", c_bool),
        ("environment_blend_additive_supported", c_bool),
        ("depth_submission_enabled", c_bool),
        ("foveated_submission_enabled", c_bool),
        ("local_foveated_rendering_enabled", c_bool),
    ]


class ovxr_device_info_t(Structure):
    """``ovxr_device_info_t`` — output of :meth:`Bindings.get_device_info`."""

    _fields_ = [
        ("openxr_composition_gpu", ovxr_gpu_identity_t),
        ("primary_ovrtx_render_gpu", ovxr_gpu_identity_t),
        ("same_gpu_fast_path", c_bool),
        ("cross_gpu_required", c_bool),
        ("cuda_peer_transfer_available", c_bool),
        ("stereo_split_enabled", c_bool),
        ("render_gpu_count", c_uint32),
        ("features", ovxr_runtime_features_t),
    ]

    @property
    def topology_ready(self) -> bool:
        """Whether composition and render GPU topology has been resolved."""
        return (
            self.openxr_composition_gpu.cuda_device_id >= 0
            and self.primary_ovrtx_render_gpu.cuda_device_id >= 0
        )


class ovxr_vulkan_handles_t(Structure):
    """Borrowed session graphics binding; serialize queue use with OVXR."""

    _fields_ = [
        ("vk_instance", c_uint64),
        ("vk_physical_device", c_uint64),
        ("vk_device", c_uint64),
        ("vk_queue", c_uint64),
        ("queue_family_index", c_uint32),
        ("queue_index", c_uint32),
        ("external_memory_handle_types", c_uint32),
        ("external_semaphore_handle_types", c_uint32),
    ]


class ovxr_openxr_handles_t(Structure):
    """``ovxr_openxr_handles_t`` — output of :meth:`Bindings.get_openxr_handles`.

    Raw handle/function-pointer bits for the session ovxr owns, so a separate
    consumer can drive input on the SAME OpenXR session.
    """

    _fields_ = [
        ("xr_instance", c_uint64),
        ("xr_session", c_uint64),
        ("xr_space", c_uint64),
        ("xr_get_instance_proc_addr", c_uint64),
        ("xr_reference_space_type", c_int32),
    ]


class ReferenceSpaceType(IntEnum):
    """Values ovxr may use when creating its shared OpenXR reference space."""

    VIEW = 1
    LOCAL = 2
    STAGE = 3
    LOCAL_FLOOR = 1000426000


class ovxr_frame_timing_t(Structure):
    """``ovxr_frame_timing_t``."""

    _fields_ = [
        ("frame_index", c_uint64),
        ("predicted_display_time_ns", c_int64),
        ("suggested_delta_time_seconds", c_double),
        ("should_render", c_bool),
    ]


class ovxr_frame_plan_t(Structure):
    """``ovxr_frame_plan_t`` — output of :meth:`Bindings.begin_frame`.

    ``frame`` is an opaque ``ovxr_frame_t*`` owned by ovxr; ``render_products``
    (OVXR's eye products followed by the application products selected with
    :meth:`Bindings.set_frame_render_products`) remains valid until
    :meth:`Bindings.submit_frame` / :meth:`Bindings.cancel_frame`.
    ``step_result`` is the borrowed ``ovrtx_step_result_handle_t`` of the bound
    step: zero until a renderable bind succeeds, usable with the ``ovrtx``
    package's ``fetch_results`` / ``map_render_var_output`` until the plan is
    consumed, and never to be destroyed by the application. ``render_ordinal``
    is the committed OVStage ordinal the frame renders at; in
    application-stepped mode (``ContextDesc.application_steps_renderer``) it is
    the ordinal to pass to the ``ovrtx`` update and step.
    """

    _fields_ = [
        ("frame", c_void_p),  # ovxr_frame_t*
        ("timing", ovxr_frame_timing_t),
        ("render_products", ovrtx_render_product_set_t),
        ("step_result", c_uint64),  # ovrtx_step_result_handle_t, borrowed
        ("render_ordinal", c_uint64),  # ovstage_ordinal_t
    ]


class ovxr_submit_desc_t(Structure):
    """``ovxr_submit_desc_t`` — optional inputs to :meth:`Bindings.submit_frame`.

    ``step_result`` is the application's ``ovrtx_step_with_stage`` result and is
    only accepted in application-stepped mode (must be 0 otherwise).
    ``additional_layers`` holds borrowed native ``XrCompositionLayerBaseHeader*``
    values appended after the OVXR scene layer; zero-initialized means scene only.
    """

    _fields_ = [
        ("step_result", c_uint64),  # ovrtx_step_result_handle_t, application-owned
        ("additional_layers", POINTER(c_void_p)),  # const XrCompositionLayerBaseHeader* const*
        ("additional_layer_count", c_uint32),
    ]


class ovxr_frame_result_t(Structure):
    """``ovxr_frame_result_t`` — per-``render_frame``/``submit_frame`` outcome."""

    _fields_ = [
        ("frame_index", c_uint64),
        ("submitted", c_bool),
        ("used_cross_gpu_transfer", c_bool),
        ("source_gpu_count", c_uint32),
        ("environment_blend_mode", c_int32),
        ("openxr_submit_message", ctypes.c_char * 256),
    ]


class ovxr_camera_transform_t(Structure):
    """``ovxr_camera_transform_t`` — row-major 4x4; translation is ``matrix[12..14]``."""

    _fields_ = [
        ("camera_path", c_char_p),
        ("matrix", c_double * 16),
    ]


class ovxr_prim_transform_t(Structure):
    """``ovxr_prim_transform_t`` — row-major 4x4 WORLD transform for any prim."""

    _fields_ = [
        ("prim_path", c_char_p),
        ("matrix", c_double * 16),
    ]


class ovxr_anchor_transform_t(Structure):
    """Resolved absolute scene-space anchor transform."""

    _fields_ = [
        ("matrix", c_double * 16),
    ]


class ovxr_quaternion_t(Structure):
    _fields_ = [("x", c_float), ("y", c_float), ("z", c_float), ("w", c_float)]


class ovxr_vector3_t(Structure):
    _fields_ = [("x", c_float), ("y", c_float), ("z", c_float)]


class ovxr_pose_t(Structure):
    """``ovxr_pose_t`` — an orientation and position in OpenXR tracking space."""

    _fields_ = [
        ("orientation", ovxr_quaternion_t),
        ("position", ovxr_vector3_t),
    ]


class ovxr_render_product_resolution_t(Structure):
    """``ovxr_render_product_resolution_t``."""

    _fields_ = [
        ("render_product_path", c_char_p),
        ("width", c_int32),
        ("height", c_int32),
    ]


class ovxr_openxr_probe_result_t(Structure):
    """``ovxr_openxr_probe_result_t``."""

    _fields_ = [
        ("instance_created", c_bool),
        ("system_found", c_bool),
        ("view_config_enumerated", c_bool),
        ("vulkan_enable_available", c_bool),
        ("vulkan_enable2_available", c_bool),
        ("vulkan_requirements_queried", c_bool),
        ("d3d12_enable_available", c_bool),
        ("d3d12_requirements_queried", c_bool),
        ("composition_gpu_queried", c_bool),
        ("session_created", c_bool),
        ("view_count", c_uint32),
        ("recommended_swapchain_width", c_uint32),
        ("recommended_swapchain_height", c_uint32),
        ("recommended_swapchain_sample_count", c_uint32),
        ("max_swapchain_width", c_uint32),
        ("max_swapchain_height", c_uint32),
        ("recommended_swapchain_size_uniform", c_bool),
        ("environment_blend_opaque_supported", c_bool),
        ("environment_blend_alpha_supported", c_bool),
        ("environment_blend_additive_supported", c_bool),
        ("last_xr_result", c_int32),
        ("environment_blend_mode", c_int32),
        ("system_id", c_uint64),
        ("runtime_version", c_uint64),
        ("min_vulkan_api_version_supported", c_uint64),
        ("max_vulkan_api_version_supported", c_uint64),
        ("d3d12_min_feature_level", c_uint32),
        ("graphics_api", c_int32),               # ovxr_graphics_api_t
        ("composition_gpu", ovxr_gpu_identity_t),
        ("runtime_name", ctypes.c_char * 128),
        ("system_name", ctypes.c_char * 256),
        ("vulkan_physical_device_name", ctypes.c_char * 256),
        ("d3d12_adapter_name", ctypes.c_char * 256),
        ("message", ctypes.c_char * 256),
    ]


class ovxr_openxr_frame_result_t(Structure):
    """``ovxr_openxr_frame_result_t``."""

    _fields_ = [
        ("session_created", c_bool),
        ("session_running", c_bool),
        ("spaces_created", c_bool),
        ("swapchains_created", c_bool),
        ("frame_waited", c_bool),
        ("frame_begun", c_bool),
        ("views_located", c_bool),
        ("swapchains_cleared", c_bool),
        ("frame_ended", c_bool),
        ("submitted_projection_layer", c_bool),
        ("should_render", c_bool),
        ("view_count", c_uint32),
        ("swapchain_count", c_uint32),
        ("swapchain_width", c_uint32),
        ("swapchain_height", c_uint32),
        ("session_state", c_int32),
        ("last_xr_result", c_int32),
        ("swapchain_format", c_int64),
        ("predicted_display_time", c_int64),
        ("environment_blend_mode", c_int32),
        ("message", ctypes.c_char * 256),
    ]


# ---------------------------------------------------------------------------
# Library loading
# ---------------------------------------------------------------------------
OVXR_LIBRARY_PATH_HINT: Optional[str] = None
"""Optional directory or file path to ``libovxr-dynamic.so``.

When set, it takes precedence over :func:`ovxr_loader_candidate_dirs` in the
lazy ``import ovxr`` / :class:`~ovxr.Context` path. Mirrors
``ovrtx.OVRTX_LIBRARY_PATH_HINT``.
"""

_ALLOW_CWD_LIBRARY_SEARCH_ENV = "OVXR_ALLOW_CWD_LIBRARY_SEARCH"
_ALLOW_SYSTEM_LIBRARY_SEARCH_ENV = "OVXR_ALLOW_SYSTEM_LIBRARY_SEARCH"
_TRUTHY_OPT_IN_VALUES = frozenset(("1", "true", "yes", "on"))


def _opt_in_enabled(name: str) -> bool:
    """Return whether a native-library development opt-in is explicitly enabled."""
    value = os.environ.get(name)
    return value is not None and value.strip().lower() in _TRUTHY_OPT_IN_VALUES


def _resolve_library_hint(hint: str, library_name: str, setting_name: str) -> str:
    """Resolve an explicit library hint, failing closed when it is unusable."""
    path = Path(hint)
    try:
        if path.is_file():
            return os.path.abspath(path)
        if path.is_dir():
            candidate = path / library_name
            if candidate.is_file():
                return os.path.abspath(candidate)
            reason = f"directory does not contain {library_name}"
        else:
            reason = "path does not exist or is not a regular file or directory"
    except OSError as error:
        raise FileNotFoundError(
            f"{setting_name} is set to {hint!r}, but it could not be inspected ({error}); refusing to search "
            "fallback locations."
        ) from error
    raise FileNotFoundError(
        f"{setting_name} is set to {hint!r}, but the {reason}; refusing to search fallback locations."
    )


def _system_loader_candidate_dirs() -> List[Path]:
    """Return process environment paths only for the explicit development opt-in."""
    if not _opt_in_enabled(_ALLOW_SYSTEM_LIBRARY_SEARCH_ENV):
        return []

    candidates: List[Path] = []
    if not sys.platform.startswith("win"):
        if ld_paths := os.environ.get("LD_LIBRARY_PATH", ""):
            candidates.extend(Path(path) for path in ld_paths.split(os.pathsep) if path)
    if path_paths := os.environ.get("PATH", ""):
        candidates.extend(Path(path) for path in path_paths.split(os.pathsep) if path)
    return candidates


def _ovrtx_wheel_library_hint(ovrtx_package, ovrtx_bindings) -> Optional[str]:
    """Return the OVRTX hint the wheel's loader will honor.

    The wheel reads ``ovrtx._src.bindings.OVRTX_LIBRARY_PATH_HINT``; the public
    ``ovrtx.OVRTX_LIBRARY_PATH_HINT`` re-export is a by-value copy, so also honor
    it when only the public attribute was set.
    """
    return getattr(ovrtx_bindings, "OVRTX_LIBRARY_PATH_HINT", None) or getattr(
        ovrtx_package, "OVRTX_LIBRARY_PATH_HINT", None
    )


def _wheel_libovrtx_path() -> str:
    """Absolute path to the ``libovrtx-dynamic.so`` the ovrtx wheel loads.

    The preload exists only so the ovxr shim binds the SAME ovrtx instance the
    wheel's renderer uses, so this deliberately mirrors the wheel's own
    discovery rather than applying ovxr's trusted-first policy: refusing a
    location the wheel is about to load anyway would create a second instance,
    not a safer one. ovxr's own shim discovery (:func:`ovxr_loader_candidate_dirs`)
    is unaffected.

    Resolution order:

    1. The library the wheel's singleton loader has already loaded, if any.
    2. The wheel's own search: ``OVRTX_LIBRARY_PATH_HINT`` (a directory, as the
       wheel treats it), then its ``ovrtx_loader_candidate_dirs`` snapshot,
       filtered and symlink-resolved the way the wheel filters them.
    """
    import ovrtx  # the installed ov[ovrtx] wheel
    from ovrtx._src import bindings as ovrtx_bindings

    loader = getattr(ovrtx_bindings, "_ovrtx_loader", None)
    loaded = getattr(loader, "_lib", None)
    loaded_path = getattr(loaded, "_name", None)
    if loaded_path:
        return os.path.abspath(loaded_path)

    resolve_existing_dirs = getattr(ovrtx_bindings, "_resolve_existing_dirs", _resolve_existing_dirs)
    hint = _ovrtx_wheel_library_hint(ovrtx, ovrtx_bindings)
    search_dirs: List[Path] = resolve_existing_dirs([Path(hint)]) if hint else []
    snapshot = getattr(loader, "_lib_search_paths", None)
    if snapshot is None:
        snapshot = resolve_existing_dirs(ovrtx_bindings.ovrtx_loader_candidate_dirs())
    search_dirs.extend(snapshot)

    candidates = [directory / OVRTX_LOADER_LIB_NAME for directory in search_dirs]
    for candidate in candidates:
        try:
            if candidate.is_file():
                return str(candidate)
        except OSError:
            continue

    tried = "\n  ".join(str(candidate) for candidate in candidates) or "(none)"
    raise FileNotFoundError(
        f"{OVRTX_LOADER_LIB_NAME} was not found for the OVXR preload. The preload follows the ovrtx wheel's own "
        f"discovery (ovrtx.OVRTX_LIBRARY_PATH_HINT, then ovrtx.ovrtx_loader_candidate_dirs). Tried:\n  {tried}"
    )


def _resolve_existing_dirs(paths) -> List[Path]:
    """Return lexical absolute paths for existing directories without following symlinks."""
    out: List[Path] = []
    for candidate in paths:
        try:
            if candidate.exists() and candidate.is_dir():
                out.append(Path(os.path.abspath(candidate)))
        except OSError:
            continue
    return out


def _trusted_ovxr_loader_candidate_dirs() -> List[Path]:
    """Return package-owned wheel, deploy, and build-tree loader directories."""
    package_dir = Path(__file__).parent.parent
    deploy_bin = package_dir.parent.parent / "bin"
    return [
        package_dir / "bin",
        deploy_bin,
        deploy_bin / "plugins" / "rtx",
        # Build-tree SDK layout: <build>/ovxr/python/ovxr plus the native
        # library under <build>/plugins/rtx.
        package_dir.parent.parent.parent / "plugins" / "rtx",
    ]


def _cwd_loader_candidate_dirs() -> List[Path]:
    """Return the current directory only for its separate explicit opt-in."""
    if _opt_in_enabled(_ALLOW_CWD_LIBRARY_SEARCH_ENV):
        try:
            return [Path.cwd()]
        except OSError:
            pass
    return []


def _loader_candidate_groups() -> tuple[List[Path], List[Path], List[Path]]:
    """Return trusted, current-directory, and system-path candidates separately."""
    return (
        _trusted_ovxr_loader_candidate_dirs(),
        _cwd_loader_candidate_dirs(),
        _system_loader_candidate_dirs(),
    )


def ovxr_loader_candidate_dirs() -> List[Path]:
    """Return ordered directories considered for :data:`OVXR_LOADER_LIB_NAME`.

    :data:`OVXR_LIBRARY_PATH_HINT` is handled separately and always has highest
    precedence. This list then contains, in order:

    1. ``<package_dir>/bin`` for a wheel with a bundled runtime.
    2. ``<deploy>/bin`` and ``<deploy>/bin/plugins/rtx``.
    3. ``<build>/plugins/rtx`` for the linked build-tree SDK layout.
    4. The current directory only when ``OVXR_ALLOW_CWD_LIBRARY_SEARCH=1``.
    5. ``LD_LIBRARY_PATH`` (Linux) and ``PATH`` only when the development-only
       ``OVXR_ALLOW_SYSTEM_LIBRARY_SEARCH=1`` opt-in is set.

    Trusted package/deploy paths always precede process-controlled locations.
    Raw ``Path(__file__)`` ancestry is intentional so build-tree symlinks retain
    the deploy layout rather than resolving back into the source tree.
    """
    trusted, current_directory, system = _loader_candidate_groups()
    return [*trusted, *current_directory, *system]


_REQUIRED_FUNCTIONS = (
    "ovxr_create",
    "ovxr_request_exit",
    "ovxr_destroy",
    "ovxr_get_device_info",
    "ovxr_get_scene_info",
    "ovxr_get_openxr_handles",
    "ovxr_get_vulkan_handles",
    "ovxr_begin_frame",
    "ovxr_submit_frame",
    "ovxr_cancel_frame",
    "ovxr_render_frame",
    "ovxr_bind_frame",
    "ovxr_acquire_frame",
    "ovxr_update_camera_transforms",
    "ovxr_update_prim_transforms",
    "ovxr_update_render_product_resolutions",
    "ovxr_probe_openxr",
    "ovxr_submit_openxr_blank_frame",
    "ovxr_get_last_error",
    "ovxr_set_anchor_prim",
    "ovxr_get_anchor_transform",
    "ovxr_transform_openxr_pose",
    "ovxr_get_anchor_prim",
    "ovxr_clear_anchor",
    "ovxr_set_managed_anchor_transform",
    "ovxr_library_telemetry_opt_out",
    "ovxr_set_frame_render_products",
)


def _configure(lib: ctypes.CDLL) -> ctypes.CDLL:
    """Set ``argtypes``/``restype`` on every ovxr entry point the library exports."""
    missing = [name for name in _REQUIRED_FUNCTIONS if not hasattr(lib, name)]
    if missing:
        raise RuntimeError(f"OVXR library is missing required symbols: {', '.join(missing)}")

    lib.ovxr_create.argtypes = [POINTER(ovxr_desc_t), POINTER(c_void_p)]
    lib.ovxr_create.restype = c_int32
    lib.ovxr_request_exit.argtypes = [c_void_p]
    lib.ovxr_request_exit.restype = c_int32
    lib.ovxr_destroy.argtypes = [c_void_p, c_uint64]
    lib.ovxr_destroy.restype = c_int32
    lib.ovxr_probe_openxr.argtypes = [c_void_p, POINTER(ovxr_openxr_probe_result_t)]
    lib.ovxr_probe_openxr.restype = c_int32
    lib.ovxr_submit_openxr_blank_frame.argtypes = [c_void_p, POINTER(ovxr_openxr_frame_result_t)]
    lib.ovxr_submit_openxr_blank_frame.restype = c_int32
    lib.ovxr_get_last_error.argtypes = []
    lib.ovxr_get_last_error.restype = c_char_p

    lib.ovxr_get_device_info.argtypes = [c_void_p, POINTER(ovxr_device_info_t)]
    lib.ovxr_get_device_info.restype = c_int32
    lib.ovxr_get_scene_info.argtypes = [c_void_p, POINTER(ovxr_scene_info_t)]
    lib.ovxr_get_scene_info.restype = c_int32
    lib.ovxr_get_openxr_handles.argtypes = [c_void_p, POINTER(ovxr_openxr_handles_t)]
    lib.ovxr_get_openxr_handles.restype = c_int32
    lib.ovxr_set_frame_render_products.argtypes = [c_void_p, POINTER(ovx_string_t), c_size_t]
    lib.ovxr_set_frame_render_products.restype = c_int32
    lib.ovxr_begin_frame.argtypes = [c_void_p, POINTER(ovxr_frame_plan_t)]
    lib.ovxr_begin_frame.restype = c_int32
    lib.ovxr_submit_frame.argtypes = [
        c_void_p,
        POINTER(ovxr_frame_plan_t),
        POINTER(ovxr_submit_desc_t),
        POINTER(ovxr_frame_result_t),
    ]
    lib.ovxr_submit_frame.restype = c_int32
    lib.ovxr_get_vulkan_handles.argtypes = [c_void_p, POINTER(ovxr_vulkan_handles_t)]
    lib.ovxr_get_vulkan_handles.restype = c_int32
    lib.ovxr_cancel_frame.argtypes = [c_void_p, POINTER(ovxr_frame_plan_t)]
    lib.ovxr_cancel_frame.restype = None
    lib.ovxr_render_frame.argtypes = [c_void_p, c_double, c_uint64, POINTER(ovxr_frame_result_t), POINTER(c_bool)]
    lib.ovxr_render_frame.restype = c_int32
    lib.ovxr_bind_frame.argtypes = [c_void_p, POINTER(ovxr_frame_plan_t), c_double, c_uint64, POINTER(c_bool)]
    lib.ovxr_bind_frame.restype = c_int32
    lib.ovxr_acquire_frame.argtypes = [c_void_p, POINTER(ovxr_frame_view_t)]
    lib.ovxr_acquire_frame.restype = c_int32
    lib.ovxr_update_camera_transforms.argtypes = [c_void_p, POINTER(ovxr_camera_transform_t), c_size_t, c_uint64]
    lib.ovxr_update_camera_transforms.restype = c_int32
    lib.ovxr_update_prim_transforms.argtypes = [c_void_p, POINTER(ovxr_prim_transform_t), c_size_t, c_uint64]
    lib.ovxr_update_prim_transforms.restype = c_int32
    lib.ovxr_update_render_product_resolutions.argtypes = [
        c_void_p,
        POINTER(ovxr_render_product_resolution_t),
        c_size_t,
        c_uint64,
    ]
    lib.ovxr_update_render_product_resolutions.restype = c_int32
    lib.ovxr_set_anchor_prim.argtypes = [c_void_p, ovx_string_t]
    lib.ovxr_set_anchor_prim.restype = c_int32
    lib.ovxr_get_anchor_transform.argtypes = [c_void_p, POINTER(ovxr_anchor_transform_t)]
    lib.ovxr_get_anchor_transform.restype = c_int32
    lib.ovxr_transform_openxr_pose.argtypes = [
        c_void_p,
        POINTER(ovxr_pose_t),
        POINTER(c_double),
    ]
    lib.ovxr_transform_openxr_pose.restype = c_int32
    lib.ovxr_get_anchor_prim.argtypes = [c_void_p, c_char_p, c_size_t, POINTER(c_size_t)]
    lib.ovxr_get_anchor_prim.restype = c_int32
    lib.ovxr_clear_anchor.argtypes = [c_void_p, c_uint64]
    lib.ovxr_clear_anchor.restype = c_int32
    lib.ovxr_set_managed_anchor_transform.argtypes = [
        c_void_p,
        POINTER(ovxr_anchor_transform_t),
        c_uint64,
    ]
    lib.ovxr_set_managed_anchor_transform.restype = c_int32
    lib.ovxr_library_telemetry_opt_out.argtypes = []
    lib.ovxr_library_telemetry_opt_out.restype = None
    return lib


def _load_shared_global(path: str) -> ctypes.CDLL:
    """Load a shared library with process-global symbol visibility.

    On POSIX, ``RTLD_GLOBAL`` lets the shim's NEEDED ``libovrtx`` bind to this
    preloaded instance (the symbol-sharing contract in this module's docstring).
    On Windows, ctypes receives an explicit absolute path and uses its secure
    default load flags. OVRTX and OVStage are initialized first through their
    owning packages, so the native ovxr shim can load its colocated payload with
    a restricted search policy and bind its by-name imports to those trusted,
    already-loaded runtimes. Lexical absolute-path normalization deliberately
    avoids resolving symlinks so a linked shim retains its deployment-local
    payload anchor.
    """
    absolute_path = os.path.abspath(path)
    if os.name == "posix":
        return ctypes.CDLL(absolute_path, mode=ctypes.RTLD_GLOBAL)
    return ctypes.CDLL(absolute_path)


def _preload_ovstage() -> None:
    """Initialize OVStage through its owning package's trusted loader."""
    from ovstage._src import bindings as ovstage_bindings

    ovstage_bindings.load()


def load(ovxr_so_path: str) -> ctypes.CDLL:
    """Preload the borrowed OVRTX and OVStage runtimes, then load ``libovxr``.

    ``ovxr_so_path`` is an explicit path to the ovxr shared library (callers that
    do not have one can use :data:`OVXR_LIBRARY_PATH_HINT` + :class:`Context`).
    Returns the configured CDLL with all ``argtypes``/``restypes`` set.
    """
    # 1) Preload the wheel's libovrtx so the shim's libovrtx dependency binds to
    #    THIS instance (shared with the wheel) — see the symbol-sharing contract.
    _load_shared_global(_wheel_libovrtx_path())
    # 2) Let OVStage load itself and register its trusted dependency directories.
    #    The ovxr payload then binds to that already-loaded OVStage instance.
    _preload_ovstage()
    # 3) Load the ovxr shim.
    lib = _load_shared_global(ovxr_so_path)
    return _configure(lib)


def last_error(lib: ctypes.CDLL) -> str:
    """Return this thread's last ovxr error (or ``""``).

    Storage is thread-local; another thread's calls do not change it. The C
    pointer is decoded immediately because its lifetime ends at the next ovxr
    call on this thread. Safe to call without synchronization.
    """
    e = lib.ovxr_get_last_error()
    return e.decode("utf-8", "replace") if e else ""


class _LibraryLoader:
    """Lazily load a singleton libovxr (via :func:`load`) and wrap it in :class:`Bindings`."""

    def __init__(self):
        self._lib: Optional[ctypes.CDLL] = None
        self._load_lock = Lock()

    @property
    def is_loaded(self) -> bool:
        return self._lib is not None

    def _resolve_so(self) -> str:
        # OVXR_LIBRARY_PATH_HINT is re-exported from the public ``ovxr`` module.
        # Read that attribute dynamically so ``ovxr.OVXR_LIBRARY_PATH_HINT = ...``
        # updates an already-imported package as documented.
        public_module = sys.modules.get("ovxr")
        public_hint = getattr(public_module, "OVXR_LIBRARY_PATH_HINT", None) if public_module else None
        library_hint = public_hint or OVXR_LIBRARY_PATH_HINT
        if library_hint:
            return _resolve_library_hint(library_hint, OVXR_LOADER_LIB_NAME, "OVXR_LIBRARY_PATH_HINT")

        trusted_candidates, cwd_candidates, system_candidates = _loader_candidate_groups()
        search_paths = _resolve_existing_dirs([*trusted_candidates, *cwd_candidates, *system_candidates])
        for d in search_paths:
            cand = d / OVXR_LOADER_LIB_NAME
            if cand.is_file():
                return str(cand)

        searched = {os.path.abspath(directory) for directory in search_paths}

        def format_candidates(candidates: List[Path]) -> str:
            # Only existing directories were actually searched; say so per entry so a
            # reader does not chase a path the loader merely expected to be there.
            lines = []
            for directory in candidates:
                suffix = "" if os.path.abspath(directory) in searched else "  (directory missing, not searched)"
                lines.append(f"{directory / OVXR_LOADER_LIB_NAME}{suffix}")
            return "\n  ".join(lines) or "(none)"

        details = [
            f"Failed to find {OVXR_LOADER_LIB_NAME}.",
            "Set ovxr.OVXR_LIBRARY_PATH_HINT to an existing library file or containing directory.",
        ]
        details.extend(
            [
                "Trusted package/deploy locations (existing directories were searched):",
                f"  {format_candidates(trusted_candidates)}",
            ]
        )
        cwd_opt_in = os.environ.get(_ALLOW_CWD_LIBRARY_SEARCH_ENV)
        if _opt_in_enabled(_ALLOW_CWD_LIBRARY_SEARCH_ENV):
            details.extend(
                [
                    f"Current-directory search was ENABLED by {_ALLOW_CWD_LIBRARY_SEARCH_ENV}={cwd_opt_in!r}; checked:",
                    (
                        f"  {format_candidates(cwd_candidates)}"
                        if cwd_candidates
                        else "  (the current directory could not be determined)"
                    ),
                ]
            )
        elif cwd_opt_in is not None:
            details.append(
                f"Current-directory search is disabled because {_ALLOW_CWD_LIBRARY_SEARCH_ENV} has unrecognized value "
                f"{cwd_opt_in!r}; use one of {sorted(_TRUTHY_OPT_IN_VALUES)} to enable it."
            )
        if _opt_in_enabled(_ALLOW_SYSTEM_LIBRARY_SEARCH_ENV):
            system_opt_in = os.environ.get(_ALLOW_SYSTEM_LIBRARY_SEARCH_ENV)
            details.extend(
                [
                    f"Development system-path search was ENABLED by "
                    f"{_ALLOW_SYSTEM_LIBRARY_SEARCH_ENV}={system_opt_in!r}; checked:",
                    f"  {format_candidates(system_candidates)}",
                ]
            )
        elif (system_opt_in := os.environ.get(_ALLOW_SYSTEM_LIBRARY_SEARCH_ENV)) is not None:
            details.append(
                f"Development system-path search is disabled because {_ALLOW_SYSTEM_LIBRARY_SEARCH_ENV} has "
                f"unrecognized value {system_opt_in!r}; PATH and LD_LIBRARY_PATH were not searched. Use one of "
                f"{sorted(_TRUTHY_OPT_IN_VALUES)} to enable it."
            )
        else:
            details.append(
                "Development system-path search is disabled (default); PATH and LD_LIBRARY_PATH were not searched. "
                f"For local development only, set {_ALLOW_SYSTEM_LIBRARY_SEARCH_ENV}=1."
            )
        raise RuntimeError("\n".join(details))

    def get_bindings(self) -> "Bindings":
        """Load libovxr (once) and return a :class:`Bindings` wrapper."""
        if self._lib is None:
            with self._load_lock:
                if self._lib is None:
                    self._lib = load(self._resolve_so())
        return Bindings(self._lib)


class Bindings:
    """Low-level wrapper over a configured libovxr ``CDLL`` (one method per C entry point).

    Methods return ``(result, out_struct)`` tuples where the C call fills an out
    parameter, mirroring ``ovrtx._src.bindings.Bindings``. ``result`` is a plain
    int ``ovxr_result_t``; compare against :class:`Result` or format with
    :func:`result_name`. Use :meth:`get_last_error` after a failure.

    Threading follows ``ovxr.h``: externally serialize methods using the same
    context, except :meth:`acquire_frame`; :meth:`destroy` is exclusive. This
    wrapper deliberately adds no Python lock.
    """

    def __init__(self, lib: ctypes.CDLL):
        self._lib = lib

    @property
    def library_path(self) -> Path:
        return Path(self._lib._name)

    def create(self, desc: ovxr_desc_t):
        """``ovxr_create`` — returns ``(result, context_handle | None)``.

        May run on any thread while the borrowed renderer and OVStage are used
        exclusively by this call.
        """
        handle = c_void_p()
        rc = self._lib.ovxr_create(ctypes.byref(desc), ctypes.byref(handle))
        return rc, (handle if rc == Result.SUCCESS else None)

    def destroy(self, context: c_void_p, stage_ordinal: int = 0) -> int:
        """``ovxr_destroy``; exclusive with every call using ``context``.

        ``stage_ordinal`` is the OVStage ordinal the runtime overlay release is
        written at (0 = OVXR's private ledger; see ``ovxr.h`` "OVStage ordinals").
        """
        return self._lib.ovxr_destroy(context, c_uint64(check_ordinal(stage_ordinal)))

    def request_exit(self, context: c_void_p) -> int:
        """``ovxr_request_exit``; cooperative and safe alongside frame calls."""
        return self._lib.ovxr_request_exit(context)

    def get_device_info(self, context: c_void_p):
        """``ovxr_get_device_info`` — returns ``(result, ovxr_device_info_t)``.

        Serialize with non-acquire calls using ``context``.
        """
        out = ovxr_device_info_t()
        rc = self._lib.ovxr_get_device_info(context, ctypes.byref(out))
        return rc, out

    def get_scene_info(self, context: c_void_p):
        """``ovxr_get_scene_info`` — returns ``(result, ovxr_scene_info_t)``.

        Serialize with non-acquire calls using ``context``.
        """
        out = ovxr_scene_info_t()
        rc = self._lib.ovxr_get_scene_info(context, ctypes.byref(out))
        return rc, out

    def get_openxr_handles(self, context: c_void_p):
        """``ovxr_get_openxr_handles`` — returns ``(result, ovxr_openxr_handles_t)``.

        Serialize with non-acquire calls using ``context``.
        """
        out = ovxr_openxr_handles_t()
        rc = self._lib.ovxr_get_openxr_handles(context, ctypes.byref(out))
        return rc, out

    def set_frame_render_products(self, context: c_void_p, render_products) -> int:
        """``ovxr_set_frame_render_products`` from a sequence of prim-path strings.

        OVXR copies the paths, so the ctypes array built here need not outlive
        the call. Call between frames; serialize with non-acquire calls using
        ``context``.
        """
        paths = [ovx_string_t.make(path) for path in render_products]
        arr = (ovx_string_t * len(paths))(*paths) if paths else None
        return self._lib.ovxr_set_frame_render_products(context, arr, len(paths))

    def render_frame(self, context: c_void_p, delta_time_seconds: float, stage_ordinal: int = 0):
        """``ovxr_render_frame`` — returns ``(result, ovxr_frame_result_t, ordinal_sealed)``.

        The frame's OVStage writes land at ``stage_ordinal`` (0 = private ledger).
        ``ordinal_sealed`` says whether ``stage_ordinal`` was published; see
        :meth:`bind_frame` for why it must be read on failure.
        Serialize with non-acquire calls using ``context``.
        """
        out = ovxr_frame_result_t()
        sealed = c_bool(False)
        rc = self._lib.ovxr_render_frame(
            context,
            c_double(delta_time_seconds),
            c_uint64(check_ordinal(stage_ordinal)),
            ctypes.byref(out),
            ctypes.byref(sealed),
        )
        return rc, out, sealed.value

    def bind_frame(
        self, context: c_void_p, plan: ovxr_frame_plan_t, delta_time_seconds: float, stage_ordinal: int = 0
    ) -> "tuple[int, bool]":
        """Publish cached XR state at ``stage_ordinal``, seal it, and render it.

        Returns ``(result, ordinal_sealed)``. The ordinal is sealed *before* the
        renderer work, and the renderer steps can still fail afterwards, so a
        nonzero ``stage_ordinal`` that comes back with a failure may still have
        been published. Read ``ordinal_sealed`` on failure: when true, mark the
        ordinal resolved in your allocator and continue from a fresh one.
        """
        sealed = c_bool(False)
        rc = self._lib.ovxr_bind_frame(
            context,
            ctypes.byref(plan),
            c_double(delta_time_seconds),
            c_uint64(check_ordinal(stage_ordinal)),
            ctypes.byref(sealed),
        )
        return rc, sealed.value

    def get_vulkan_handles(self, context: c_void_p):
        """Return the borrowed Vulkan binding, or NOT_READY before session creation."""
        out = ovxr_vulkan_handles_t()
        rc = self._lib.ovxr_get_vulkan_handles(context, ctypes.byref(out))
        return rc, out

    def acquire_frame(self, context: c_void_p):
        """``ovxr_acquire_frame`` — returns ``(result, ovxr_frame_view_t)``.

        May overlap calls using ``context`` except :meth:`destroy`.
        """
        out = ovxr_frame_view_t()
        rc = self._lib.ovxr_acquire_frame(context, ctypes.byref(out))
        return rc, out

    def probe_openxr(self, context: c_void_p):
        """``ovxr_probe_openxr`` — returns ``(result, ovxr_openxr_probe_result_t)``.

        Serialize with non-acquire calls using ``context``.
        """
        out = ovxr_openxr_probe_result_t()
        rc = self._lib.ovxr_probe_openxr(context, ctypes.byref(out))
        return rc, out

    def submit_openxr_blank_frame(self, context: c_void_p):
        """Return ``(result, ovxr_openxr_frame_result_t)`` from the blank-frame helper.

        Serialize with non-acquire calls using ``context``.
        """
        out = ovxr_openxr_frame_result_t()
        rc = self._lib.ovxr_submit_openxr_blank_frame(context, ctypes.byref(out))
        return rc, out

    def begin_frame(self, context: c_void_p):
        """``ovxr_begin_frame`` — returns ``(result, ovxr_frame_plan_t)``.

        Serialize with non-acquire calls using ``context``.
        """
        out = ovxr_frame_plan_t()
        rc = self._lib.ovxr_begin_frame(context, ctypes.byref(out))
        return rc, out

    def submit_frame(self, context: c_void_p, plan: ovxr_frame_plan_t, submit_desc=None):
        """``ovxr_submit_frame`` — returns ``(result, ovxr_frame_result_t)``.

        ``submit_desc`` is an optional :class:`ovxr_submit_desc_t`; ``None``
        submits the scene only. Its layer array and layers must stay alive for
        the duration of this call. Serialize with non-acquire calls using ``context``.
        """
        out = ovxr_frame_result_t()
        desc = ctypes.byref(submit_desc) if submit_desc is not None else None
        rc = self._lib.ovxr_submit_frame(context, ctypes.byref(plan), desc, ctypes.byref(out))
        return rc, out

    def cancel_frame(self, context: c_void_p, plan: ovxr_frame_plan_t) -> None:
        """``ovxr_cancel_frame``; serialize with non-acquire calls using ``context``."""
        self._lib.ovxr_cancel_frame(context, ctypes.byref(plan))

    def update_camera_transforms(self, context: c_void_p, cameras, stage_ordinal: int = 0) -> int:
        """Update cameras from a ctypes array of :class:`ovxr_camera_transform_t`.

        The batch lands at ``stage_ordinal`` (0 = private ledger).
        Serialize with non-acquire calls using ``context``.
        """
        return self._lib.ovxr_update_camera_transforms(
            context, cameras, len(cameras), c_uint64(check_ordinal(stage_ordinal))
        )

    def update_prim_transforms(self, context: c_void_p, prims, stage_ordinal: int = 0) -> int:
        """``ovxr_update_prim_transforms`` — ``prims`` is a ctypes array of :class:`ovxr_prim_transform_t`.

        The batch lands at ``stage_ordinal`` (0 = private ledger).
        """
        return self._lib.ovxr_update_prim_transforms(context, prims, len(prims), c_uint64(check_ordinal(stage_ordinal)))

    def update_render_product_resolutions(self, context: c_void_p, render_products, stage_ordinal: int = 0) -> int:
        """Update resolutions from an array of :class:`ovxr_render_product_resolution_t`.

        The batch lands at ``stage_ordinal`` (0 = private ledger).
        Serialize with non-acquire calls using ``context``.
        """
        return self._lib.ovxr_update_render_product_resolutions(
            context, render_products, len(render_products), c_uint64(check_ordinal(stage_ordinal))
        )

    def set_anchor_prim(self, context: c_void_p, prim_path: str) -> int:
        """Queue a prim anchor; serialize with non-acquire calls using ``context``."""
        return self._lib.ovxr_set_anchor_prim(context, ovx_string_t.make(prim_path))

    def get_anchor_transform(self, context: c_void_p):
        """Return the active anchor; serialize with non-acquire calls using ``context``."""
        out = ovxr_anchor_transform_t()
        rc = self._lib.ovxr_get_anchor_transform(context, ctypes.byref(out))
        return rc, out

    def transform_openxr_pose(self, context: c_void_p, pose: ovxr_pose_t):
        """Map an OpenXR pose; serialize with non-acquire calls using ``context``."""
        out = (c_double * 16)()
        rc = self._lib.ovxr_transform_openxr_pose(context, ctypes.byref(pose), out)
        return rc, tuple(out)

    def clear_anchor(self, context: c_void_p, stage_ordinal: int = 0) -> int:
        """Queue a reset to the identity anchor; serialize with non-acquire calls using ``context``."""
        return self._lib.ovxr_clear_anchor(context, c_uint64(check_ordinal(stage_ordinal)))

    def set_managed_anchor_transform(
        self, context: c_void_p, transform: ovxr_anchor_transform_t, stage_ordinal: int = 0
    ) -> int:
        """Publish and select the managed anchor; serialize with non-acquire calls using ``context``."""
        return self._lib.ovxr_set_managed_anchor_transform(
            context, ctypes.byref(transform), c_uint64(check_ordinal(stage_ordinal))
        )

    def get_anchor_prim(self, context: c_void_p):
        """Return ``(result, prim_path)``; serialize with non-acquire calls using ``context``."""
        length = c_size_t(0)
        rc = self._lib.ovxr_get_anchor_prim(context, None, 0, ctypes.byref(length))
        if rc != Result.SUCCESS:
            return rc, ""
        buffer = ctypes.create_string_buffer(length.value + 1)
        rc = self._lib.ovxr_get_anchor_prim(context, buffer, len(buffer), ctypes.byref(length))
        if rc != Result.SUCCESS:
            return rc, ""
        return rc, buffer.raw[: length.value].decode("utf-8")

    def get_last_error(self) -> str:
        """Return this thread's last error; safe without synchronization."""
        return last_error(self._lib)

    def library_telemetry_opt_out(self) -> None:
        """Suppress the startup report; safe from any thread, with or without a context."""
        self._lib.ovxr_library_telemetry_opt_out()


_ovxr_loader = _LibraryLoader()


def library_telemetry_opt_out() -> None:
    """Suppress OVXR's anonymous library-startup report for this process.

    Call this before the first :class:`~ovxr.Context`: the report fires at most
    once per process, on the first create. It always loads libovxr and the
    borrowed ovrtx and ovstage runtimes like :func:`load`, raising the same
    errors, so ``OVX_PHONE_HOME_OPT_OUT=1`` at launch is the way to opt out
    without loading anything. See ``ovxr.h`` for the full contract.
    """
    _ovxr_loader.get_bindings().library_telemetry_opt_out()
