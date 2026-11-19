# Copyright (c) 2026, NVIDIA CORPORATION. All rights reserved.
"""OVUI 0.2 offscreen surface and XR pointer adapter. No XR frame ownership here."""
import os
from types import SimpleNamespace

import warp as wp


class PanelSurface:
    """One opaque OVUI canvas exported to an owned GPU RGBA image.

    OVUI's tick/input entrypoints are currently private. Keep that versioned
    dependency in this adapter. Call only on the thread owning the XR loop.
    """

    def __init__(self, build, width=768, height=480):
        os.environ["OMNIUI_HEADLESS"] = "1"
        os.environ["OMNIUI_BACKEND"] = "vulkan"
        import omni.ui as ui
        from omni.ui import standalone
        from omni.ui.standalone import headless_frame

        self.ui, self.standalone, self.export = ui, standalone, headless_frame
        wp.init()
        self.width, self.height, self.device = width, height, None
        self.image = None
        self.window = None
        self.export_ready = False
        self.held = False
        self.armed = False
        self.captured = False
        self.identity = None
        try:
            standalone.init("OVUI XR panel", width=width, height=height, max_fps=None)
            self.window = ui.Window("OVUI XR panel", width=width, height=height,
                                    flags=ui.WINDOW_FLAGS_NO_TITLE_BAR | ui.WINDOW_FLAGS_NO_RESIZE |
                                    ui.WINDOW_FLAGS_NO_MOVE | ui.WINDOW_FLAGS_NO_SCROLLBAR)
            self.window.setPosition(0, 0)
            with self.window.frame:
                build(ui)
            standalone._tick_one_frame()
            if not headless_frame.init():
                raise RuntimeError("OVUI headless CUDA/Vulkan frame export is unavailable")
            # OVUI 0.2 makes the CUDA device matching its Vulkan UUID current.
            # Capture it immediately: Vulkan and CUDA ordinal zero may differ.
            self.device = wp.get_cuda_device()
            self.export_ready = True
            if headless_frame.extent() != (width, height) or headless_frame.format() != "rgba8":
                raise RuntimeError("Unexpected OVUI canvas extent or pixel format")
            self.image = wp.zeros((height, width, 4), dtype=wp.uint8, device=self.device)
        except BaseException:
            self.close()
            raise

    # [snippet:ovui-pointer-routing]
    def pointer(self, pixels, down=False, *, active=True, identity=0, scroll=0.0):
        """Use one pointer. Captured drags may continue beyond panel bounds.

        None means blocked, behind the panel, or untracked. Release away from
        widgets to cancel; a held trigger cannot click upon reacquisition.
        """
        native = self.ui._ui
        if not active or identity != self.identity or pixels is None:
            native._inject_mouse_move(-100000.0, -100000.0)
            native._inject_mouse_button(0, False)
            self.held = self.captured = self.armed = False
            self.identity = identity if active else None
            # An observed release from a new source may arm its next press.
            # Reacquiring while held still requires an explicit release.
            self.armed = bool(active and pixels is not None and not down)
            return
        if not down:
            self.armed = True
        x, y = pixels
        inside = 0 <= x < self.width and 0 <= y < self.height
        pressed = bool(down and self.armed and (inside or self.captured))
        native._inject_mouse_move(float(x), float(y))
        native._inject_mouse_button(0, pressed)
        if scroll and inside:
            native._inject_mouse_scroll(0.0, float(scroll))
        self.captured = pressed
        self.held = pressed
    # [/snippet:ovui-pointer-routing]

    # [snippet:ovui-gpu-export]
    def tick(self):
        # XR rendering may have changed the calling thread's CUDA context.
        with wp.ScopedDevice(self.device):
            self.standalone._tick_one_frame()
            if not self.export.wait_ready(10_000_000):
                raise RuntimeError("OVUI image producer wait failed")
            try:
                if not self.export.copy_to_linear(self.image.ptr, self.width * 4, cuda_stream_handle=0):
                    raise RuntimeError("OVUI GPU frame copy failed")
                wp.synchronize_device(self.device)
            finally:
                self.export.signal_consumed()
                wp.synchronize_device(self.device)
        return self.image
    # [/snippet:ovui-gpu-export]

    def horizontal_region(self, left, width):
        """A borrowed GPU view into this canvas; retain its owner until processing ends."""
        if left < 0 or width <= 0 or left + width > self.width:
            raise ValueError("Canvas region is outside the surface")
        view = wp.array(ptr=self.image.ptr + left * 4, shape=(self.height, width, 4),
                        strides=(self.width * 4, 4, 1), dtype=wp.uint8, device=self.device, copy=False)
        return SimpleNamespace(image=view, device=self.device, owner=self)

    def screenshot(self, path):
        from PIL import Image
        Image.fromarray(self.image.numpy()).save(path)

    def close(self):
        if self.export_ready:
            with wp.ScopedDevice(self.device):
                wp.synchronize_device(self.device)
                self.export.shutdown()
            self.export_ready = False
        if self.window is not None:
            self.window.destroy()
            self.window = None
        self.standalone.shutdown()
        self.image = None
