# Copyright (c) 2026, NVIDIA CORPORATION. All rights reserved.
"""Application state and geometric hit testing, independent of XR and rendering."""

import math


BUTTON_X = (-0.9, -0.3, 0.3, 0.9)


def add(a, b):
    return tuple(x + y for x, y in zip(a, b))


def scale(a, factor):
    return tuple(x * factor for x in a)


def normalize(a):
    length = math.sqrt(sum(x * x for x in a))
    return scale(a, 1 / length) if length > 1e-8 else None


def box_hit(origin, direction, center, half):
    near, far = 0.0, float("inf")
    for o, d, c, h in zip(origin, direction, center, half):
        if abs(d) < 1e-8:
            if not c - h <= o <= c + h:
                return None
            continue
        a, b = (c - h - o) / d, (c + h - o) / d
        near, far = max(near, min(a, b)), min(far, max(a, b))
        if near > far:
            return None
    return near


class Model:
    def __init__(self):
        self.position = (0.0, 1.3, 0.0)
        self.yaw = 0.0
        self.occluder = True
        self.revision = 0
        self.history = []
        self.drag_distance = None
        self.previous_active = self.previous_down = False
        self.blocked = 0
        self.commands = 0

    def remember(self):
        self.history.append((self.position, self.yaw, self.occluder))
        self.history = self.history[-64:]

    def command(self, key):
        if key not in ("1", "2", "3", "4", "r", "o"):
            return False
        if key == "4":
            if not self.history:
                return False
            self.position, self.yaw, self.occluder = self.history.pop()
        else:
            self.remember()
            if key in "12":
                self.position = (max(-3, min(3, self.position[0] + (-0.25 if key == "1" else 0.25))),
                                 self.position[1], self.position[2])
            elif key == "3":
                self.yaw += math.pi / 8
            elif key == "r":
                self.position, self.yaw = (0.0, 1.3, 0.0), 0.0
            else:
                self.occluder = not self.occluder
        self.revision += 1
        self.commands += 1
        self.drag_distance = None
        return True

    def hit(self, origin, direction):
        hits = []
        def box(name, center, half, o=origin, d=direction):
            distance = box_hit(o, d, center, half)
            if distance is not None:
                hits.append((distance, name))
        c, s = math.cos(self.yaw), math.sin(self.yaw)
        def local(vector):
            x, y, z = vector
            return c * x - s * z, y, s * x + c * z
        box("cube", (0, 0, 0), (0.3, 0.3, 0.3),
            local(add(origin, scale(self.position, -1))), local(direction))
        for index, x in enumerate(BUTTON_X):
            box(str(index + 1), (x, 0.9, 1.04), (0.225, 0.175, 0.04))
        box("panel", (0, 0.9, 0.96), (1.2, 0.325, 0.02))
        if self.occluder:
            box("occluder", (0.66, 1.1, 1.55), (0.07, 0.3, 0.07))
        if direction[1] < -1e-8:
            distance = -origin[1] / direction[1]
            point = add(origin, scale(direction, distance))
            if distance >= 0 and abs(point[0]) <= 5 and abs(point[2]) <= 5:
                hits.append((distance, "floor"))
        return min(hits) if hits else (5.0, "none")

    def pointer(self, origin, direction, active, down):
        # Losing tracking/focus cancels a drag. Regaining it while held does not
        # synthesize a new click: a release and fresh press are required.
        pressed = active and self.previous_active and down and not self.previous_down
        self.previous_active, self.previous_down = active, down
        if not active:
            self.drag_distance = None
            return None, None
        distance, target = self.hit(origin, direction)
        point = add(origin, scale(direction, distance))
        teleport = None
        if pressed:
            if target in ("1", "2", "3", "4"):
                self.command(target)
            elif target == "cube":
                self.remember()
                self.drag_distance = distance
                self.drag_offset = add(self.position, scale(point, -1))
            elif target == "floor":
                teleport = point
            elif target == "occluder":
                self.blocked += 1
        if self.drag_distance is not None:
            if not down:
                self.drag_distance = None
            else:
                position = add(add(origin, scale(direction, self.drag_distance)), self.drag_offset)
                position = (max(-3, min(3, position[0])), max(0.3, min(3, position[1])),
                            max(-2, min(2, position[2])))
                if position != self.position:
                    self.position = position
                    self.revision += 1
        return point, teleport


def demo_pointer(frame):
    # Explicit synthetic input for repeatable emulator checks. It follows the
    # same hit-test/command path as controllers; it is never headset evidence.
    phase = (frame // 15) % 5
    target = ((-0.9, 0.9, 1.04), (0.3, 0.9, 1.04), (0.9, 0.9, 1.04),
              (0, 1.3, 0), (-2, 0, 2))[phase]
    if phase == 3 and frame % 15 >= 5:
        target = add(target, (min(frame % 15 - 5, 4) * 0.05, 0, 0))
    origin = (0.3, 1.4, 2.1)  # Simulated index tip below/right of the viewer.
    direction = normalize(add(target, scale(origin, -1)))
    return origin, direction, frame % 15 in range(5, 10)
