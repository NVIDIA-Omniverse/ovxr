# Copyright (c) 2026, NVIDIA CORPORATION. All rights reserved.
#
# NVIDIA CORPORATION and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA CORPORATION is strictly prohibited.

"""Renderer-global carb settings for the kitless OVRTX runtime.

:func:`apply_renderer_settings` wraps the OVRTX ``ovrtx.settings.apply_settings``
extension. It lives in ovxr for now because the kitless OpenXR path is its only
consumer; it is, strictly, an OVRTX concern (it touches no OpenXR state) and
could later move to the ``ovrtx`` package.
"""
from __future__ import annotations

import ctypes
from ctypes import POINTER, Structure, c_char_p, c_int32, c_void_p

from .bindings import _load_shared_global, _wheel_libovrtx_path, ovx_string_t


class _OvrtxResult(Structure):
    """``ovrtx_result_t { ovrtx_api_status_t status; }`` (0 = OVRTX_API_SUCCESS)."""

    _fields_ = [("status", c_int32)]


class _OvrtxApplySettingsVtable(Structure):
    """``ovrtx_extension_apply_settings_t { ovrtx_result_t (*apply_settings)(ovx_string_t); }``."""

    _fields_ = [("apply_settings", ctypes.CFUNCTYPE(_OvrtxResult, ovx_string_t))]


def apply_renderer_settings(settings: str) -> None:
    """Apply Kit command-line-style carb settings to the OVRTX runtime.

    Wraps the ``ovrtx.settings.apply_settings`` extension, which accepts
    space-separated ``--/path/to/key=value``
    tokens. The extension is available immediately after libovrtx loads; values
    applied before the runtime is ready are cached and flushed automatically, so
    call this BEFORE creating the ``ovrtx.Renderer``.

    This is the only way to reach renderer-global carb settings on the kitless
    path (no Kit settings layer; a stage ``RenderSettings`` prim crashes the
    kitless hydra engine), e.g. ``/rtx/post/aa/op`` which gates the DLSS-RR
    denoiser in RealTimePathTracing mode.

    Args:
        settings: Space-separated ``--/path/to/key=value`` tokens.

    Raises:
        RuntimeError: If the extension is unavailable or rejects the settings.
    """
    lib = _load_shared_global(_wheel_libovrtx_path())
    lib.ovrtx_query_extension.argtypes = [c_char_p, POINTER(c_void_p)]
    lib.ovrtx_query_extension.restype = _OvrtxResult

    vtable_ptr = c_void_p()
    rc = lib.ovrtx_query_extension(b"ovrtx.settings.apply_settings", ctypes.byref(vtable_ptr))
    if rc.status != 0 or not vtable_ptr:
        raise RuntimeError("OVRTX apply_settings extension unavailable (ovrtx_query_extension failed)")

    vtable = ctypes.cast(vtable_ptr, POINTER(_OvrtxApplySettingsVtable)).contents
    rc = vtable.apply_settings(ovx_string_t.make(settings))
    if rc.status != 0:
        raise RuntimeError(f"OVRTX apply_settings rejected {settings!r}")
