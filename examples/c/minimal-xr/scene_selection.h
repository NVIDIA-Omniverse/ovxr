// SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: LicenseRef-NvidiaProprietary
#pragma once

#include <stdexcept>
#include <string>

namespace ovxr_sample
{
inline constexpr const char* kRobotAssets[] = {
    "https://omniverse-content-production.s3.us-west-2.amazonaws.com/Samples/Robot-OVRTX/robot-ovrtx.usda",
    "https://omniverse-content-production.s3-us-west-2.amazonaws.com/Assets/Isaac/5.1/Isaac/Robots/"
    "FourierIntelligence/GR-1/GR1T2_fourier_hand_6dof/GR1T2_fourier_hand_6dof.usd",
    "https://omniverse-content-production.s3.us-west-2.amazonaws.com/Environments/2023_1/DomeLights/"
    "Indoor/ZetoCGcom_ExhibitionHall_Interior1.hdr",
};

struct SceneSelection
{
    std::string path;
    std::string fallbackReason;
    bool canFallback = false;
};

inline bool usesRobotStartAnchor(const SceneSelection& scene)
{
    return scene.path == kRobotAssets[0];
}

// An explicit scene never probes or substitutes the default scene. Resolve the
// bundled path lazily: an online run does not require the offline assets.
template <class Probe, class Fallback>
SceneSelection selectScene(const std::string& explicitUsd, bool offline, Probe probe, Fallback fallback)
{
    if (!explicitUsd.empty())
        return { explicitUsd, {}, false };
    std::string reason = "offline mode requested";
    if (!offline)
    {
        reason.clear();
        for (const char* asset : kRobotAssets)
        {
            std::string detail;
            if (!probe(asset, detail))
            {
                reason = std::string("cannot read ") + asset + ": " + detail;
                break;
            }
        }
        if (reason.empty())
            return { kRobotAssets[0], {}, true };
    }
    return { fallback(), reason, false };
}

// The loader must use a fresh stage for every attempt. A failed asynchronous
// population can already have written part of the previous scene.
template <class Load, class Fallback>
SceneSelection loadScene(SceneSelection scene, Load load, Fallback fallback)
{
    std::string detail;
    if (load(scene.path, detail))
        return scene;
    if (!scene.canFallback)
        throw std::runtime_error("cannot load " + scene.path + ": " + detail);
    scene = { fallback(), "cannot load Robot-OVRTX: " + detail, false };
    if (!load(scene.path, detail))
        throw std::runtime_error(scene.fallbackReason + "; cannot load fallback " + scene.path + ": " + detail);
    return scene;
}
} // namespace ovxr_sample
