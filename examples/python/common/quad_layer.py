# Copyright (c) 2026, NVIDIA CORPORATION. All rights reserved.
"""Application-owned OpenXR quad; independent of scene color/depth composition."""
import ctypes as c

import numpy as np
from ovrtx import INVALID_STEP_RESULT_HANDLE

from sample_support import check, load


def overlay_pixels():
    """A diagnostic panel with transparent surround and distinct color/alpha regions."""
    pixels = np.zeros((128, 256, 4), dtype=np.uint8)
    pixels[8:120, 8:248] = (24, 32, 48, 192)
    pixels[24:72, 24:80] = (235, 72, 54, 255)
    pixels[24:72, 100:156] = (48, 210, 112, 255)
    pixels[24:72, 176:232] = (50, 120, 245, 255)
    pixels[88:104, 24:232] = (245, 190, 48, 128)
    return pixels


class QuadLayer:
    def __init__(self, renderer, context, placement="head", verify_upload=False):
        if placement not in ("head", "world"):
            raise ValueError("placement must be head or world")
        self.renderer, self.context = renderer, context
        self.head_locked = placement == "head"
        self.library = load()
        signatures = {
            "sample_projection_create": (c.c_void_p, [c.c_uint64] * 9 + [c.c_uint32]),
            "sample_quad_upload": (c.c_void_p, [c.c_void_p] * 3 + [c.c_uint32] * 3),
            "sample_projection_destroy": (None, [c.c_void_p]),
        }
        for name, (result, args) in signatures.items():
            fn = getattr(self.library, name)
            fn.restype, fn.argtypes = result, args
        self.handle = None
        self.closed = False
        self.pixels = overlay_pixels()
        self.readback = np.empty_like(self.pixels) if verify_upload else None
        self.verified_uploads = 0

    # [snippet:quad-upload]
    def layers(self, plan):
        if self.closed:
            raise RuntimeError("Quad layer is closed")
        if plan.step_result == INVALID_STEP_RESULT_HANDLE:
            return []  # OVXR still consumes the no-render plan normally.
        if not self.handle:
            xr, vk = self.context.get_openxr_handles(), self.context.get_vulkan_handles()
            if xr is None or vk is None:
                raise RuntimeError("A renderable frame needs live XR/Vulkan handles")
            self.handle = self.library.sample_projection_create(
                c.cast(self.renderer._handle, c.c_void_p).value,
                xr.xr_instance, xr.xr_session, xr.xr_space, xr.xr_get_instance_proc_addr,
                vk.vk_instance, vk.vk_physical_device, vk.vk_device, vk.vk_queue, vk.queue_family_index)
            check(self.library, self.handle)
        layer = self.library.sample_quad_upload(self.handle, self.pixels.ctypes.data,
            self.readback.ctypes.data if self.readback is not None else None,
            self.pixels.shape[1], self.pixels.shape[0], int(self.head_locked))
        check(self.library, layer)
        if self.readback is not None:
            if not np.array_equal(self.pixels, self.readback):
                raise RuntimeError("OpenXR swapchain pixels differ after Vulkan upload/readback")
            self.verified_uploads += 1
        return [c.c_void_p(layer)]
    # [/snippet:quad-upload]

    def close(self):
        if self.handle:
            self.library.sample_projection_destroy(self.handle)
            self.handle = None
        self.closed = True
