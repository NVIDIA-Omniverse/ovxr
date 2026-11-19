# Copyright (c) 2026, NVIDIA CORPORATION. All rights reserved.
"""Synchronous CUDA-to-host copy and a dependency-free BMP writer."""

import ctypes as c
import os
from pathlib import Path
import struct

import ovxr


class FrameCopy:
    """Load a small CUDA Driver API subset from the installed NVIDIA driver."""

    def __init__(self):
        self.driver = c.WinDLL("nvcuda.dll") if os.name == "nt" else c.CDLL("libcuda.so.1")
        self.init = self._function("cuInit", [c.c_uint])
        self.pointer_attribute = self._function("cuPointerGetAttribute", [c.c_void_p, c.c_int, c.c_uint64])
        self.push = self._function("cuCtxPushCurrent_v2", [c.c_void_p])
        self.pop = self._function("cuCtxPopCurrent_v2", [c.POINTER(c.c_void_p)])
        self.copy_to_host = self._function("cuMemcpyDtoH_v2", [c.c_void_p, c.c_uint64, c.c_size_t])
        self.check(self.init(0), "cuInit")

    def _function(self, name, arguments):
        function = getattr(self.driver, name)
        function.argtypes = arguments
        function.restype = c.c_int
        return function

    @staticmethod
    def check(result, operation):
        if result:
            raise RuntimeError(f"{operation}: CUDA driver error {result}")

    def copy(self, frame):
        if (frame.format != ovxr.FrameFormat.RGBA8_UNORM or not frame.width or not frame.height
                or frame.pitch_bytes < frame.width * 4):
            raise ValueError("Invalid RGBA8 readback layout")
        pointer = frame.device_ptr  # The Python wrapper rejects an expired view.
        if not pointer:
            raise ValueError("Null readback pointer")
        context = c.c_void_p()
        # CU_POINTER_ATTRIBUTE_CONTEXT = 1. This context is also borrowed from
        # OVXR; no frame call or context destruction may overlap the copy.
        self.check(self.pointer_attribute(c.byref(context), 1, pointer), "cuPointerGetAttribute")
        if not context.value:
            raise RuntimeError("Readback pointer has no CUDA context")
        rgba = (c.c_ubyte * (frame.pitch_bytes * frame.height))()
        self.check(self.push(context), "cuCtxPushCurrent")
        try:
            # A synchronous copy completes before the next XR frame can begin.
            self.check(self.copy_to_host(rgba, pointer, len(rgba)), "cuMemcpyDtoH")
        finally:
            previous = c.c_void_p()
            self.check(self.pop(c.byref(previous)), "cuCtxPopCurrent")
        return bytes(rgba)


def save_bmp(path, frame, rgba):
    size = frame.width * frame.height * 4
    if (not 0 < frame.width <= 0x7fffffff or not 0 < frame.height <= 0x7fffffff or size > 0xffffffff - 54
            or frame.pitch_bytes < frame.width * 4 or len(rgba) != frame.pitch_bytes * frame.height):
        raise ValueError("Image is too large or has an invalid BMP layout")
    # BI_RGB, 32-bit BGRX; negative height preserves top-down row order.
    with Path(path).open("wb") as output:
        output.write(struct.pack("<2sIHHI", b"BM", size + 54, 0, 0, 54))
        output.write(struct.pack("<IiiHHIIiiII", 40, frame.width, -frame.height, 1, 32, 0, size, 2835, 2835, 0, 0))
        for y in range(frame.height):
            row = rgba[y * frame.pitch_bytes:y * frame.pitch_bytes + frame.width * 4]
            bgra = bytearray(len(row))
            bgra[0::4], bgra[1::4], bgra[2::4] = row[2::4], row[1::4], row[0::4]
            output.write(bgra)
