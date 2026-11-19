# Copyright (c) 2026, NVIDIA CORPORATION. All rights reserved.
"""Sample-owned color/depth composition submitted through OVXR additional_layers.

The native adapter reads (and unmaps) the borrowed bound step, and uploads owned
images to its own XR swapchains. Host staging keeps the example independent of
CUDA/Vulkan external-memory extensions and works across different GPUs. These
synchronous transfers are illustrative, not a low-latency headset implementation.
"""
import ctypes as c
from types import SimpleNamespace

import numpy as np
import warp as wp
from ovrtx import INVALID_STEP_RESULT_HANDLE, StepResultHandle

from sample_support import Image, Pose, check, load

NEAR_Z, FAR_Z = 0.01, 10000.0


class LocatedView(c.Structure):
    _fields_ = [("pose", Pose), ("fov", c.c_float * 4)]


def resize_depth(depth, height, width):
    """The same nearest pixel-center sampling used for unfoveated XRDepth."""
    rows = np.minimum(((np.arange(height) + 0.5) * depth.shape[0] / height).astype(int), depth.shape[0] - 1)
    cols = np.minimum(((np.arange(width) + 0.5) * depth.shape[1] / width).astype(int), depth.shape[1] - 1)
    return np.ascontiguousarray(depth[rows[:, None], cols[None, :]])


class ProjectionLayer:
    """Destroy before the borrowed context/renderer and keep alive through submit."""

    def __init__(self, renderer, context, compose, device):
        self.renderer, self.context, self.compose = renderer, context, compose
        self.device = device
        self.library = load()
        signatures = {
            "sample_projection_create": (c.c_void_p, [c.c_uint64] * 9 + [c.c_uint32]),
            "sample_projection_set_views": (c.c_int, [c.c_void_p, c.POINTER(LocatedView), c.c_uint32]),
            "sample_projection_read": (
                c.c_int, [c.c_void_p, StepResultHandle, c.c_char_p, c.POINTER(Image), c.POINTER(Image)]
            ),
            "sample_projection_upload": (c.c_int, [c.c_void_p, c.c_uint32, c.c_void_p, c.c_void_p,
                                                    c.c_uint32, c.c_uint32, c.c_float, c.c_float]),
            "sample_projection_layer": (c.c_void_p, [c.c_void_p]),
            "sample_projection_destroy": (None, [c.c_void_p]),
        }
        for name, (result, args) in signatures.items():
            function = getattr(self.library, name)
            function.restype, function.argtypes = result, args
        self.handle = None
        self.targets = {}
        self.latest = None

    def layers(self, plan):
        if plan.step_result == INVALID_STEP_RESULT_HANDLE:
            return []  # Valid no-render plans still go through OVXR submit.
        if not self.handle:
            xr, vk = self.context.get_openxr_handles(), self.context.get_vulkan_handles()
            if xr is None or vk is None:
                raise RuntimeError("Bound frame has no live XR/Vulkan session")
            self.handle = self.library.sample_projection_create(
                c.cast(self.renderer._handle, c.c_void_p).value,
                xr.xr_instance, xr.xr_session, xr.xr_space, xr.xr_get_instance_proc_addr,
                vk.vk_instance, vk.vk_physical_device, vk.vk_device, vk.vk_queue, vk.queue_family_index)
            check(self.library, self.handle)
        rendered = self.context.get_frame_projection_views(plan)
        if len(rendered) != 2:
            raise RuntimeError("This sample requires two rendered eye views")
        located = (LocatedView * 2)(*(LocatedView(
            Pose((c.c_float * 4)(*view.orientation), (c.c_float * 3)(*view.position)),
            (c.c_float * 4)(*view.fov_angles)) for view in rendered))
        check(self.library, self.library.sample_projection_set_views(self.handle, located, len(located)) == 0)
        products = plan.render_products
        if products.num_render_products != 2:
            raise RuntimeError("This sample requires unfoveated stereo eye products")
        for index in range(2):
            color, depth = Image(), Image()
            product = products.render_products[index]
            name = c.string_at(product.ptr, product.length)
            check(self.library, self.library.sample_projection_read(
                self.handle, plan.step_result, name, c.byref(color), c.byref(depth)) == 0)
            rgba = np.frombuffer(c.string_at(color.rgba, color.pitch * color.height), np.uint8).reshape(color.height, color.width, 4)
            z = np.frombuffer(c.string_at(depth.rgba, depth.pitch * depth.height), np.float32).reshape(depth.height, depth.width)
            z = resize_depth(z, color.height, color.width)
            key = index, color.width, color.height
            if key not in self.targets:
                self.targets[key] = (wp.empty(rgba.shape, dtype=wp.uint8, device=self.device),
                                     wp.empty(z.shape, dtype=float, device=self.device))
            image, depth_image = self.targets[key]
            image.assign(rgba)
            depth_image.assign(z)
            pose = located[index].pose
            view = SimpleNamespace(
                view_index=index, view_count=2, cuda_device_id=self.device.ordinal,
                width=color.width, height=color.height, color_cuda_ptr=image.ptr, depth_cuda_ptr=depth_image.ptr,
                eye_pose=SimpleNamespace(position=SimpleNamespace(**dict(zip("xyz", pose.position))),
                                         orientation=SimpleNamespace(**dict(zip("xyzw", pose.orientation)))),
                fov_angles=tuple(located[index].fov), near_z=NEAR_Z, far_z=FAR_Z,
                predicted_display_time_ns=plan.timing.predicted_display_time_ns)
            self.compose(view)  # App-owned writable copies, never renderer outputs.
            rgba, z = image.numpy(), depth_image.numpy()
            check(self.library, self.library.sample_projection_upload(self.handle, index, rgba.ctypes.data,
                z.ctypes.data, color.width, color.height, NEAR_Z, FAR_Z) == 0)
            if index == 0:
                self.latest = rgba
        layer = self.library.sample_projection_layer(self.handle)
        check(self.library, layer)
        return [c.c_void_p(layer)]

    def update_mirror(self, mirror, capture=True):
        image = self.latest if capture else None
        result = self.library.sample_mirror_present(mirror.handle, image.ctypes.data if image is not None else None,
            image.shape[1] if image is not None else 0, image.shape[0] if image is not None else 0,
            image.strides[0] if image is not None else 0)
        check(self.library, result >= 0)
        if result and image is not None:
            mirror.presented += 1
        return bool(result)

    def close(self):
        if self.handle:
            self.library.sample_projection_destroy(self.handle)
            self.handle = None
        self.targets.clear()
        self.latest = None
