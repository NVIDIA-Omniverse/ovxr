# Copyright (c) 2026, NVIDIA CORPORATION. All rights reserved.
"""Sample scene hit testing; these analytic colliders match the bundled scene."""
import math


def box_hit(origin, direction, center, half=0.3):
    low, high = 0.0, math.inf
    for o, d, c in zip(origin, direction, center):
        if abs(d) < 1e-8:
            if abs(o - c) > half:
                return None
        else:
            a, b = (c - half - o) / d, (c + half - o) / d
            low, high = max(low, min(a, b)), min(high, max(a, b))
    return low if high >= low else None


def panel_hit(origin, direction, *, center=(0, 1.4, 0), size=(1.2, 0.75),
              resolution=(768, 480), captured=False):
    """Front-facing world-Z plane hit as (ray distance, canvas pixels)."""
    if direction[2] >= -1e-8:
        return None
    distance = (center[2] - origin[2]) / direction[2]
    if distance <= 0:
        return None
    x = ((origin[0] + distance * direction[0] - center[0]) / size[0] + 0.5) * resolution[0]
    y = (0.5 - (origin[1] + distance * direction[1] - center[1]) / size[1]) * resolution[1]
    if captured or (0 <= x < resolution[0] and 0 <= y < resolution[1]):
        return distance, (x, y)
    return None


def panel_pointer(origin, direction, occluder, *, captured=False, resolution=(768, 480)):
    hit = panel_hit(origin, direction, captured=captured, resolution=resolution)
    if hit is None:
        return None
    distance, pixels = hit
    blocked = box_hit(origin, direction, occluder)
    if blocked is not None and blocked < distance:
        return None
    return pixels
