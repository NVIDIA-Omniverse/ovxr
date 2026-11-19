# Copyright (c) 2026, NVIDIA CORPORATION. All rights reserved.
"""Two fixed-size, world-facing panels: nearest hit and header dragging only."""
from panel_model import box_hit, panel_hit


RESOLUTION = (512, 320)
SIZE = (1.1, 0.6875)
INITIAL = ((-0.25, 1.55, 0.35), (0.25, 1.4, 0.0))
CUBE = (0.6, 1.25, 0.75)
HEADER = 60  # Includes the OVUI window's four-pixel top inset.
NO_INPUT = (None, False, None)


class Panels:
    def __init__(self):
        self.centers = list(INITIAL)
        self.identity = None
        self.armed = self.held = False
        self.capture = self.drag = None
        self.moves = self.cancels = 0

    def cancel(self):
        if self.drag is not None:
            index, original, _ = self.drag
            self.centers[index] = original
            self.cancels += 1
        self.capture = self.drag = None
        self.armed = self.held = False

    def reset(self):
        self.cancel()
        self.centers = list(INITIAL)

    def hit(self, origin, direction, index, captured=False):
        return panel_hit(origin, direction, center=self.centers[index], size=SIZE,
                         resolution=RESOLUTION, captured=captured)

    def obstructed(self, origin, direction, distance, skip=None):
        cube = box_hit(origin, direction, CUBE)
        if cube is not None and cube <= distance:
            return True
        for index in range(2):
            other = self.hit(origin, direction, index) if index != skip else None
            if other and other[0] < distance:
                return True
        return False

    # [snippet:nearest-panel-input]
    def pointer(self, origin, direction, down, active, identity):
        """Return atlas pixels, button state and focused panel, or no UI input."""
        if not active or identity != self.identity:
            self.cancel()
            self.identity = identity if active else None
            self.armed = active and not down
            if not active or down:
                return NO_INPUT
        if not down:
            self.armed = True
        if self.drag is not None:
            index, original, offset = self.drag
            hit = self.hit(origin, direction, index, captured=True)
            if hit is None or self.obstructed(origin, direction, hit[0], skip=index):
                self.cancel()
                return NO_INPUT
            if down:
                point = tuple(origin[i] + hit[0] * direction[i] for i in range(2))
                # Keep the simple demo within reach and above the floor. Depth,
                # orientation and dimensions stay fixed throughout the drag.
                self.centers[index] = (max(-1.3, min(1.3, point[0] - offset[0])),
                                       max(0.9, min(2.1, point[1] - offset[1])), original[2])
            else:
                self.moves += int(self.centers[index] != original)
                self.drag = None
            self.held = down
            return NO_INPUT

        hits = [(hit[0], index, hit[1]) for index in range(2)
                if (hit := self.hit(origin, direction, index)) is not None]
        nearest = min(hits) if hits else None
        if nearest and self.obstructed(origin, direction, nearest[0], skip=nearest[1]):
            nearest = None
        if self.capture is not None and (nearest is None or nearest[1] != self.capture):
            self.cancel()  # Never release a button over a different panel.
            return NO_INPUT
        pressed = down and self.armed
        if pressed and not self.held and nearest:
            distance, index, pixels = nearest
            if pixels[1] < HEADER:
                offset = tuple(origin[i] + distance * direction[i] - self.centers[index][i] for i in range(2))
                self.drag = (index, self.centers[index], offset)
                self.held = down
                return NO_INPUT
            self.capture = index
        self.held = down
        result = NO_INPUT
        if nearest and (not down or self.capture == nearest[1]):
            _, index, (x, y) = nearest
            result = ((index * RESOLUTION[0] + x, y), pressed, index)
        if not down:
            self.capture = None
        return result
    # [/snippet:nearest-panel-input]


def ray_to_pixel(index, pixels, centers=INITIAL):
    """Deterministic demo/test ray. Tracked input uses the same pointer() path."""
    x, y = pixels
    center = centers[index]
    target = (center[0] + (x / RESOLUTION[0] - 0.5) * SIZE[0],
              center[1] + (0.5 - y / RESOLUTION[1]) * SIZE[1], center[2])
    origin = (.3, 1.25, 2.5)
    return origin, tuple(target[i] - origin[i] for i in range(3))
