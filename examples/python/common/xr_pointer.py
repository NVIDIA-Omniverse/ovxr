# Copyright (c) 2026, NVIDIA CORPORATION. All rights reserved.
"""Choose one controller aim or joint-based hand pointer in scene coordinates."""
import math


def tracked_pointer(context, state, hands, pinch):
    candidates = []
    if state and state.focused:
        for side in range(2):
            if not hands:
                controller = state.controllers[side]
                if controller.active:
                    matrix = context.transform_openxr_pose(controller.aim.position, controller.aim.orientation)
                    candidates.append((tuple(matrix[12:15]), tuple(-v for v in matrix[8:11]),
                                       bool(controller.select), True, side))
            else:
                hand = state.hands[side]
                if hand.active and all(hand.valid[j] for j in (5, 7, 10)):
                    points = [context.transform_openxr_pose(hand.joints[j].position)[12:15] for j in (5, 7, 10)]
                    thumb, base, tip = points
                    pinch[side] = math.dist(thumb, tip) < (0.04 if pinch[side] else 0.025)
                    direction = tuple(tip[i] - base[i] for i in range(3))
                    if math.sqrt(sum(v*v for v in direction)) > 1e-5:
                        candidates.append((tip, direction, pinch[side], True, side))
                else:
                    pinch[side] = False
    else:
        pinch[:] = [False, False]
    return candidates[-1] if candidates else ((0, 0, 0), (0, 0, -1), False, False, None)
