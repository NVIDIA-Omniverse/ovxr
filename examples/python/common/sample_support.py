# Copyright (c) 2026, NVIDIA CORPORATION. All rights reserved.
"""Example-only input, observer camera, desktop window and scene panel bridge; not part of the OVXR API.

The OpenXR calls are readable in c/sample-support/input.cpp. The bridge avoids
duplicating OpenXR structure layouts in Python and owns only its child handles.
"""

import ctypes as c
import os
from types import SimpleNamespace

from frame_copy import FrameCopy


class Pose(c.Structure):
    _fields_ = [("orientation", c.c_float * 4), ("position", c.c_float * 3)]


class Controller(c.Structure):
    _fields_ = [("aim", Pose), ("active", c.c_uint32), ("select", c.c_uint32), ("stick", c.c_float * 2)]


class Hand(c.Structure):
    _fields_ = [("joints", Pose * 26), ("valid", c.c_uint32 * 26), ("active", c.c_uint32)]


class InputState(c.Structure):
    _fields_ = [("controllers", Controller * 2), ("hands", Hand * 2), ("focused", c.c_uint32)]


class Image(c.Structure):
    _fields_ = [("rgba", c.c_void_p), ("width", c.c_uint32), ("height", c.c_uint32), ("pitch", c.c_uint32)]


def load():
    path = os.environ.get("OVXR_SAMPLE_SUPPORT_LIBRARY")
    if not path:
        raise RuntimeError("Set OVXR_SAMPLE_SUPPORT_LIBRARY to the ovxr-sample-support library built with the C++ samples.")
    library = c.CDLL(path)
    signatures = {
        "sample_support_error": (c.c_char_p, []),
        "sample_input_create": (c.c_void_p, [c.c_uint64] * 4 + [c.c_uint32]),
        "sample_input_poll": (c.c_int, [c.c_void_p, c.c_int64, c.POINTER(InputState)]),
        "sample_input_destroy": (None, [c.c_void_p]),
        "sample_mirror_create": (c.c_void_p, [c.c_char_p]),
        "sample_pip_create": (c.c_void_p, [c.c_char_p]),
        "sample_mirror_present": (c.c_int, [c.c_void_p, c.c_void_p, c.c_uint32, c.c_uint32, c.c_uint32]),
        "sample_mirror_key": (c.c_int, [c.c_void_p]),
        "sample_mirror_title": (None, [c.c_void_p, c.c_char_p]),
        "sample_mirror_destroy": (None, [c.c_void_p]),
        "sample_panel_create": (c.c_void_p, [c.c_uint64]),
        "sample_panel_update": (c.c_int, [c.c_void_p, c.c_void_p, c.c_uint32, c.c_uint32, c.c_uint32, c.c_int64]),
        "sample_panel_destroy": (None, [c.c_void_p]),
        "sample_observer_create": (c.c_void_p, [c.c_uint64]),
        "sample_observer_capture": (c.c_int, [c.c_void_p, c.c_int64, c.POINTER(Image)]),
        "sample_observer_destroy": (None, [c.c_void_p]),
    }
    for name, (result, arguments) in signatures.items():
        function = getattr(library, name)
        function.restype, function.argtypes = result, arguments
    return library


def check(library, success):
    if not success:
        raise RuntimeError(library.sample_support_error().decode("utf-8", "replace"))


class Input:
    def __init__(self, handles, hands=False):
        self.library = load()
        self.handle = self.library.sample_input_create(handles.xr_instance, handles.xr_session, handles.xr_space,
                                                       handles.xr_get_instance_proc_addr, hands)
        check(self.library, self.handle)

    def poll(self, predicted_display_time_ns):
        state = InputState()
        check(self.library, self.library.sample_input_poll(self.handle, predicted_display_time_ns, c.byref(state)) == 0)
        return state

    def close(self):
        if self.handle:
            self.library.sample_input_destroy(self.handle)
            self.handle = None


class Mirror:
    def __init__(self, title):
        self.library = load()
        self.handle = self.library.sample_mirror_create(title.encode("utf-8"))
        check(self.library, self.handle)
        try:
            self.copy = FrameCopy()
        except BaseException:
            self.close()
            raise
        self.sequence = 0
        self.presented = 0

    def update(self, context, capture=True):
        # Copy on the frame thread before advancing XR. The native window owns
        # its CPU pixels; it never retains an OVXR CUDA view.
        view = context.acquire_frame() if capture else None
        rgba = None
        if view is not None and view.sequence > self.sequence:
            rgba = self.copy.copy(view)
            self.sequence = view.sequence
            self.presented += 1
        result = self.library.sample_mirror_present(self.handle, rgba, view.width if rgba else 0,
                                                   view.height if rgba else 0, view.pitch_bytes if rgba else 0)
        check(self.library, result >= 0)
        return bool(result)

    def key(self):
        value = self.library.sample_mirror_key(self.handle)
        return chr(value).lower() if value else ""

    def title(self, value):
        self.library.sample_mirror_title(self.handle, value.encode("utf-8"))

    def reset(self):
        self.sequence = 0  # A recreated OVXR context restarts its capture counter.

    def close(self):
        if self.handle:
            self.library.sample_mirror_destroy(self.handle)
            self.handle = None


class PipWindow:
    """Present an already-owned CPU copy, sharing it with saving and the panel."""

    def __init__(self, title):
        self.library = load()
        self.handle = self.library.sample_pip_create(title.encode("utf-8"))
        check(self.library, self.handle)
        self.presented = 0

    def present(self, rgba=None, width=0, height=0, pitch=0):
        result = self.library.sample_mirror_present(self.handle, rgba, width, height, pitch)
        check(self.library, result >= 0)
        if rgba is not None and result:
            self.presented += 1
        return bool(result)

    def key(self):
        value = self.library.sample_mirror_key(self.handle)
        return chr(value).lower() if value else ""

    def close(self):
        if self.handle:
            self.library.sample_mirror_destroy(self.handle)
            self.handle = None


class Observer:
    """Small mono scene camera; its owned CPU copy feeds both PiP consumers."""

    def __init__(self, renderer):
        self.library = load()
        self.handle = self.library.sample_observer_create(c.cast(renderer._handle, c.c_void_p).value)
        check(self.library, self.handle)
        self.sequence = 0

    def capture(self, ordinal):
        image = Image()
        result = self.library.sample_observer_capture(self.handle, ordinal, c.byref(image))
        check(self.library, result >= 0)
        if result:
            return None, None
        self.sequence += 1
        view = SimpleNamespace(width=image.width, height=image.height, pitch_bytes=image.pitch,
                               sequence=self.sequence)
        return view, c.string_at(image.rgba, image.pitch * image.height)

    def close(self):
        if self.handle:
            self.library.sample_observer_destroy(self.handle)
            self.handle = None


class PipPanel:
    """128x96 emissive mesh populated through the public OVStage C API."""

    def __init__(self, stage):
        self.library = load()
        # The pinned OVStage binding stores the borrowed instance in _inst,
        # also used by OVXR's ContextDesc handle handoff.
        self.handle = self.library.sample_panel_create(c.cast(stage._inst, c.c_void_p).value)
        check(self.library, self.handle)
        self.presented = 0

    def update(self, rgba, width, height, pitch, ordinal):
        check(self.library, self.library.sample_panel_update(self.handle, rgba, width, height, pitch, ordinal) == 0)
        self.presented += 1

    def close(self):
        if self.handle:
            self.library.sample_panel_destroy(self.handle)
            self.handle = None
