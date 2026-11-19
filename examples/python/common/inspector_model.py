# Copyright (c) 2026, NVIDIA CORPORATION. All rights reserved.
"""Scene inspector state and analytic picking, independent of OVUI/OpenXR/GPU."""
from dataclasses import asdict, dataclass, replace
import math

from panel_model import box_hit, panel_hit


PANEL_CENTER = (0, 1.55, 0)
PANEL_SIZE = (1.8, 1.125)
RESOLUTION = (1024, 640)
LIMITS = {"x": (-1.8, 1.8), "y": (0.5, 1.9), "z": (-0.7, 1.0),
          "yaw": (-90.0, 90.0), "scale": (0.5, 1.6)}
LABELS = {"/World/BluePart": "Blue part", "/World/AmberPart": "Amber part"}


@dataclass(frozen=True)
class Transform:
    x: float
    y: float = 0.65
    z: float = 0.45
    yaw: float = 0.0  # Degrees, about world Y.
    scale: float = 1.0

    def matrix(self):
        c, s = math.cos(math.radians(self.yaw)) * self.scale, math.sin(math.radians(self.yaw)) * self.scale
        return (c, 0, -s, 0, 0, self.scale, 0, 0, s, 0, c, 0, self.x, self.y, self.z, 1)


class SceneModel:
    """One authoritative model for XR, OVUI and desktop commands.

    A gesture keeps its original transform until release. Focus/tracking loss
    rolls it back. History stores committed transforms, never programmatic UI refreshes.
    """
    def __init__(self):
        self.defaults = {path: Transform(x) for path, x in zip(LABELS, (-1.0, 1.0))}
        self.objects = dict(self.defaults)
        self.selected = None
        self.revision = 0
        self.source = "Ready"
        self.pending = None
        self.undo_stack = []
        self.redo_stack = []
        self.events = {key: 0 for key in ("scene_selections", "panel_selections", "desktop_commands",
                                          "edits", "commits", "cancels", "undo", "redo", "reset")}

    def changed(self, source):
        self.revision += 1
        self.source = source

    def select(self, path, source):
        if path is not None and path not in self.objects:
            raise KeyError(path)
        if path == self.selected:
            return
        self.finish()
        self.selected = path
        if source in ("scene", "panel"):
            self.events[source + "_selections"] += 1
        self.changed(source)

    def edit(self, field, value, source="panel"):
        if self.selected is None:
            return
        if not math.isfinite(value):
            raise ValueError("Transforms must be finite")
        low, high = LIMITS[field]
        before = self.objects[self.selected]
        after = replace(before, **{field: max(low, min(high, float(value)))})
        if after == before:
            return
        if self.pending is None:
            self.pending = (self.selected, before)
        self.objects[self.selected] = after
        self.events["edits"] += 1
        self.changed(source)

    def finish(self, cancel=False):
        if self.pending is None:
            return
        path, before = self.pending
        self.pending = None
        after = self.objects[path]
        if before == after:
            return
        if cancel:
            self.objects[path] = before
            self.events["cancels"] += 1
            self.changed("cancelled drag")
        else:
            self.undo_stack.append((path, before, after))
            self.redo_stack.clear()
            self.events["commits"] += 1
            self.changed("committed edit")

    def undo(self):
        self.finish()
        if self.undo_stack:
            path, before, after = self.undo_stack.pop()
            self.objects[path] = before
            self.redo_stack.append((path, before, after))
            self.selected = path
            self.events["undo"] += 1
            self.changed("undo")

    def redo(self):
        self.finish()
        if self.redo_stack:
            path, before, after = self.redo_stack.pop()
            self.objects[path] = after
            self.undo_stack.append((path, before, after))
            self.selected = path
            self.events["redo"] += 1
            self.changed("redo")

    def reset(self):
        self.finish()
        if self.selected is not None and self.objects[self.selected] != self.defaults[self.selected]:
            self.pending = (self.selected, self.objects[self.selected])
            self.objects[self.selected] = self.defaults[self.selected]
            self.events["reset"] += 1
            self.changed("reset")
            self.finish()

    def key(self, key):
        """Desktop edits go through exactly the same commands as the panel."""
        if key not in ("1", "2", "a", "d", "u", "y", "r"):
            return
        self.events["desktop_commands"] += 1
        if key in ("1", "2"):
            self.select(tuple(self.objects)[int(key) - 1], "desktop")
        elif key in ("a", "d") and self.selected:
            self.finish()
            self.edit("x", self.objects[self.selected].x + (0.1 if key == "d" else -0.1), "desktop")
            self.finish()
        elif key in ("u", "y", "r"):
            {"u": self.undo, "y": self.redo, "r": self.reset}[key]()

    def snapshot(self):
        return dict(selected=self.selected, objects={path: asdict(value) for path, value in self.objects.items()},
                    revision=self.revision, undo_depth=len(self.undo_stack), redo_depth=len(self.redo_stack))


def object_hit(origin, direction, transform):
    """Intersect a size-0.6 cube under yaw + uniform scale; preserve ray parameter."""
    c, s = math.cos(math.radians(transform.yaw)), math.sin(math.radians(transform.yaw))

    def inverse(vector):
        x, y, z = vector
        return ((c * x - s * z) / transform.scale, y / transform.scale,
                (s * x + c * z) / transform.scale)

    local_origin = inverse((origin[0] - transform.x, origin[1] - transform.y, origin[2] - transform.z))
    return box_hit(local_origin, inverse(direction), (0, 0, 0))


def selection_transforms(model):
    """Publish each cage bar in world space, as required by update_prim_transforms.

    The live world-transform API does not promise to animate an authored USD
    subtree from its parent's update. Explicitly compose each drawable's pose.
    """
    parent = (model.objects[model.selected] if model.selected else Transform(0, y=-100, z=0)).matrix()
    result = {}
    for axis, name in enumerate("XYZ"):
        other = [i for i in range(3) if i != axis]
        for a in range(2):
            for b in range(2):
                offset = [0.0, 0.0, 0.0]
                offset[other[0]], offset[other[1]] = (2 * a - 1) * 0.32, (2 * b - 1) * 0.32
                matrix = list(parent)
                for row in range(3):
                    for column in range(3):
                        matrix[row * 4 + column] *= 0.66 if row == axis else 0.012
                for column in range(3):
                    matrix[12 + column] += sum(offset[row] * parent[row * 4 + column] for row in range(3))
                result[f"/World/Selection/{name}{a}{b}"] = matrix
    return result


@dataclass
class PointerRoute:
    pixels: tuple | None = None
    down: bool = False
    cancel: bool = False
    cursor: tuple | None = None


class PointerRouter:
    """Nearest visible object/panel wins; source loss requires a release to rearm."""
    def __init__(self):
        self.identity = None
        self.armed = self.held = self.captured = False

    def route(self, model, origin, direction, down, active, identity):
        if not active or identity != self.identity:
            cancel = self.held or self.captured
            self.identity = identity if active else None
            self.armed = active and not down
            self.held = self.captured = False
            if not active or down or cancel:
                return PointerRoute(cancel=cancel)
            # Forward a fresh source's idle hover so the UI adapter also sees
            # its release before a possible press on the following frame.
        if not down:
            self.armed = True
        nearest = None
        for path, value in model.objects.items():
            distance = object_hit(origin, direction, value)
            if distance is not None and (nearest is None or distance < nearest[0]):
                nearest = (distance, path)
        panel = panel_hit(origin, direction, center=PANEL_CENTER, size=PANEL_SIZE,
                          resolution=RESOLUTION, captured=self.captured)
        blocked = panel is not None and nearest is not None and nearest[0] <= panel[0]
        if self.captured and (panel is None or blocked):
            self.captured = False
            self.armed = not down
            self.held = down
            return PointerRoute(cancel=True)
        if blocked:
            panel = None
        pressed = down and self.armed
        if pressed and not self.held:
            if panel is not None:
                self.captured = True
            else:
                model.select(nearest[1] if nearest else None, "scene")
        # A scene press cannot become a panel drag just by moving across it.
        pixels = panel[1] if panel and (not down or self.captured) else None
        cursor = None
        if nearest and panel is None:
            cursor = tuple(origin[i] + max(0, nearest[0] - 0.005) * direction[i] for i in range(3))
        self.held = down
        if not down:
            self.captured = False
        return PointerRoute(pixels=pixels, down=pressed and pixels is not None, cursor=cursor)
