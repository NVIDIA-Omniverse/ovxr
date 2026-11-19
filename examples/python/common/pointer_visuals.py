# Copyright (c) 2026, NVIDIA CORPORATION. All rights reserved.
"""Small scene-geometry visualizations; input and hit testing remain with the app."""
import math


def normalize(value):
    length = math.sqrt(sum(v * v for v in value))
    return tuple(v / length for v in value) if length > 1e-8 else (0, 0, -1)


def cross(a, b):
    return (a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0])


def pose(point):
    return [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, *point, 1]


def beam_transforms(origin, endpoint, active=True, down=False):
    """A thin unit cube joins the actual ray origin and its nearest hit."""
    hidden = pose((0, -100, 0))
    if not active or endpoint is None:
        return {"/World/PointerBeam": hidden, "/World/PointerTip": hidden}
    delta = tuple(endpoint[i] - origin[i] for i in range(3))
    direction = normalize(delta)
    side = normalize(cross(direction, (0, 1, 0) if abs(direction[1]) < .95 else (1, 0, 0)))
    up = cross(side, direction)
    radius = .009 if down else .004
    center = tuple((origin[i] + endpoint[i]) * .5 for i in range(3))
    matrix = [*(v * radius for v in side), 0, *(v * radius for v in up), 0, *delta, 0, *center, 1]
    tip = tuple(endpoint[i] - direction[i] * .018 for i in range(3))
    return {"/World/PointerBeam": matrix, "/World/PointerTip": pose(tip)}


def ray_endpoint(origin, direction, distances):
    """Distances are ray parameters from the same colliders used for input."""
    length = math.sqrt(sum(v*v for v in direction))
    distance = min([5 / max(length, 1e-8)] + [d for d in distances if d is not None and d >= 0])
    return tuple(origin[i] + distance * direction[i] for i in range(3))


HAND_CHAINS = ((1, 2, 3, 4, 5), (1, 6, 7, 8, 9, 10), (1, 11, 12, 13, 14, 15),
               (1, 16, 17, 18, 19, 20), (1, 21, 22, 23, 24, 25))
HAND_BONES = tuple(pair for chain in HAND_CHAINS for pair in zip(chain, chain[1:]))


def hand_transforms(name, points):
    """Joint spheres, unit Z cylinders and an ellipsoid palm from valid scene positions."""
    prefix = f"/World/Hand{name}"
    hidden = pose((0, -100, 0))
    updates = {f"{prefix}Joint{i}": pose(points[i]) if i in points else hidden for i in range(26)}
    for bone, (start, end) in enumerate(HAND_BONES):
        matrix = hidden
        if start in points and end in points:
            delta = tuple(points[end][i] - points[start][i] for i in range(3))
            if math.dist(points[start], points[end]) > 1e-6:
                forward = normalize(delta)
                side = normalize(cross(forward, (0, 1, 0) if abs(forward[1]) < .95 else (1, 0, 0)))
                normal = cross(forward, side)
                center = tuple((points[start][i] + points[end][i]) * .5 for i in range(3))
                matrix = [*(v * .009 for v in side), 0, *(v * .009 for v in normal), 0,
                          *delta, 0, *center, 1]
        updates[f"{prefix}Bone{bone}"] = matrix
    # Derive the palm frame from positions: tracked joint orientations may be invalid.
    palm = hidden
    if all(j in points for j in (1, 7, 22)):
        knuckles = tuple((points[7][i] + points[22][i]) * .5 for i in range(3))
        length = math.dist(points[1], knuckles)
        forward = normalize(tuple(knuckles[i] - points[1][i] for i in range(3)))
        across = tuple(points[22][i] - points[7][i] for i in range(3))
        along = sum(across[i] * forward[i] for i in range(3))
        across = tuple(across[i] - along * forward[i] for i in range(3))
        width = math.sqrt(sum(v*v for v in across))
        if min(length, width) > 1e-6:
            side = normalize(across)
            normal = cross(forward, side)
            center = tuple((points[1][i] + knuckles[i]) * .5 for i in range(3))
            palm = [*(v * width * .65 for v in side), 0, *(v * width * .18 for v in normal), 0,
                    *(v * length * .6 for v in forward), 0, *center, 1]
    updates[f"{prefix}Palm"] = palm
    return updates


def demo_hand(tip, direction, pinched=False, left=False):
    """26 illustrative joint positions in OpenXR order, never reported as tracking."""
    forward = normalize(direction)
    side = normalize(cross(forward, (0, 1, 0) if abs(forward[1]) < .95 else (1, 0, 0)))
    up = cross(side, forward)
    sign = -1 if left else 1

    def point(x, y, z):
        return tuple(tip[i] + side[i]*x*sign + up[i]*y + forward[i]*z for i in range(3))

    joints = [point(.034, 0, -.135), point(.035, -.015, -.19),
              point(-.005, -.005, -.155), point(-.037, -.002, -.117),
              point(-.03 if pinched else -.056, -.015, -.045 if pinched else -.075),
              point(-.012 if pinched else -.068, -.006, -.008 if pinched else -.035)]
    for finger in range(4):
        x = (0, .027, .052, .075)[finger]
        knuckle = (-.10, -.10, -.105, -.115)[finger]
        end = (0, .01, -.005, -.04)[finger]
        for j, z in enumerate((-.155, knuckle, knuckle*.55 + end*.45, knuckle*.25 + end*.75, end)):
            joints.append(point(x + .002*finger*j, -.004*finger, z))
    joints[10] = tuple(tip)
    return joints
