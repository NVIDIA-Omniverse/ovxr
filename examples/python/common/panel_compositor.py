# Copyright (c) 2026, NVIDIA CORPORATION. All rights reserved.
"""Per-eye GPU panel projection with reverse-Z scene occlusion, using Warp."""
import math

import numpy as np
import warp as wp


# [snippet:panel-depth-composite]
@wp.kernel
def composite_panel(color: wp.array3d(dtype=wp.uint8), depth: wp.array2d(dtype=float),
                    ui: wp.array3d(dtype=wp.uint8), eye: wp.vec3, orientation: wp.quat,
                    world_from_tracking: wp.mat44, tangents: wp.vec4, center: wp.vec3, size: wp.vec2,
                    near_z: float, far_z: float, counts: wp.array(dtype=wp.int64)):
    y, x = wp.tid()
    u = (float(x) + 0.5) / float(depth.shape[1])
    v = (float(y) + 0.5) / float(depth.shape[0])
    ray = wp.vec3(wp.lerp(tangents[0], tangents[1], u), wp.lerp(tangents[2], tangents[3], v), -1.0)
    origin = wp.transform_point(world_from_tracking, eye)
    direction = wp.transform_vector(world_from_tracking, wp.quat_rotate(orientation, ray))
    if direction[2] < -0.000001:
        distance = (center[2] - origin[2]) / direction[2]
        if distance >= near_z and distance <= far_z:
            point = origin + distance * direction
            s = (point[0] - center[0]) / size[0] + 0.5
            t = 0.5 - (point[1] - center[1]) / size[1]
            if s >= 0.0 and s < 1.0 and t >= 0.0 and t < 1.0:
                panel_z = (near_z * far_z / distance - near_z) / (far_z - near_z)
                wp.atomic_add(counts, 0, wp.int64(1))
                if panel_z > depth[y, x]:
                    tx = wp.min(int(s * float(ui.shape[1])), ui.shape[1] - 1)
                    ty = wp.min(int(t * float(ui.shape[0])), ui.shape[0] - 1)
                    # An opaque canvas gives one unambiguous depth at each pixel.
                    # OVUI antialiases text into its opaque background beforehand.
                    for channel in range(3):
                        color[y, x, channel] = ui[ty, tx, channel]
                    color[y, x, 3] = wp.uint8(255)
                    depth[y, x] = panel_z
                    wp.atomic_add(counts, 1, wp.int64(1))
                else:
                    wp.atomic_add(counts, 2, wp.int64(1))
# [/snippet:panel-depth-composite]


class PanelCompositor:
    def __init__(self, surface, center=(0.0, 1.4, 0.0), size=(1.2, 0.75)):
        self.surface = surface
        self.center, self.size = center, size
        self.world_from_tracking = None
        self.targets = {}
        self.views = set()
        self.calls = 0

    def set_tracking_transform(self, matrix):
        self.world_from_tracking = wp.mat44(np.array(matrix).reshape(4, 4).T.tolist())

    # [snippet:process-xr-eye]
    def __call__(self, view):
        if self.world_from_tracking is None:
            raise RuntimeError("Cannot compose an XR eye before its anchor transform is resolved")
        device = wp.get_device(f"cuda:{view.cuda_device_id}")
        color = wp.array(ptr=view.color_cuda_ptr, shape=(view.height, view.width, 4), dtype=wp.uint8,
                         device=device, copy=False)
        depth = wp.array(ptr=view.depth_cuda_ptr, shape=(view.height, view.width), dtype=float,
                         device=device, copy=False)
        p, q = view.eye_pose.position, view.eye_pose.orientation
        try:
            if view.cuda_device_id not in self.targets:
                image = (self.surface.image if device == self.surface.device else
                         wp.empty(self.surface.image.shape, dtype=wp.uint8, device=device))
                self.targets[view.cuda_device_id] = image, wp.zeros(3, dtype=wp.int64, device=device)
            image, counts = self.targets[view.cuda_device_id]
            if device != self.surface.device:
                # tick() finished exporting the owned canvas. Warp also handles
                # strided atlas regions when copying between devices.
                wp.copy(image, self.surface.image)
            wp.launch(composite_panel, dim=(view.height, view.width), inputs=[
                color, depth, image, wp.vec3(p.x, p.y, p.z), wp.quat(q.x, q.y, q.z, q.w),
                self.world_from_tracking, wp.vec4(*(math.tan(a) for a in view.fov_angles)),
                wp.vec3(*self.center), wp.vec2(*self.size), view.near_z, view.far_z, counts,
            ], device=device)
            self.views.add(view.view_index)
            self.calls += 1
        finally:
            # Finish using the app-owned eye images even when a launch fails.
            wp.synchronize_device(device)
    # [/snippet:process-xr-eye]

    def report(self):
        totals = sum((counts.numpy() for _, counts in self.targets.values()), np.zeros(3, dtype=np.int64))
        tested, visible, occluded = [int(value) for value in totals]
        return dict(eye_indices=sorted(self.views), processed_views=self.calls,
                    tested_pixels=tested, visible_pixels=visible, occluded_pixels=occluded)
