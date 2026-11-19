// Copyright (c) 2026, NVIDIA CORPORATION. All rights reserved.
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <optional>
#include <string>
#include <vector>

namespace workbench
{
using Vec = std::array<double, 3>;
inline Vec add(Vec a, Vec b) { return { a[0]+b[0], a[1]+b[1], a[2]+b[2] }; }
inline Vec scale(Vec a, double k) { return { a[0]*k, a[1]*k, a[2]*k }; }
inline double length(Vec a) { return std::sqrt(a[0]*a[0]+a[1]*a[1]+a[2]*a[2]); }
inline Vec normalize(Vec a) { const double n = length(a); return n > 1e-8 ? scale(a, 1/n) : Vec{}; }
inline std::optional<double> boxHit(Vec o, Vec d, Vec center, Vec half)
{
    double entry = 0, exit = std::numeric_limits<double>::infinity();
    for (int i = 0; i < 3; ++i)
    {
        if (std::abs(d[i]) < 1e-8)
        {
            if (o[i] < center[i]-half[i] || o[i] > center[i]+half[i]) return {};
            continue;
        }
        const double a = (center[i]-half[i]-o[i])/d[i], b = (center[i]+half[i]-o[i])/d[i];
        entry = std::max(entry, std::min(a,b)); exit = std::min(exit, std::max(a,b));
        if (entry > exit) return {};
    }
    return entry;
}
struct Model
{
    Vec position{0,1.3,0}, dragOffset{};
    double yaw = 0;
    bool occluder = true, previousActive = false, previousDown = false;
    int revision = 0, commands = 0, blocked = 0;
    struct Snapshot { Vec position; double yaw; bool occluder; };
    std::vector<Snapshot> history;
    std::optional<double> dragDistance;
    void remember()
    {
        history.push_back({position,yaw,occluder});
        if (history.size() > 64) history.erase(history.begin());
    }
    bool command(char key)
    {
        if (!key || std::string("1234ro").find(key) == std::string::npos) return false;
        if (key == '4')
        {
            if (history.empty()) return false;
            const auto old = history.back(); history.pop_back();
            position = old.position; yaw = old.yaw; occluder = old.occluder;
        }
        else
        {
            remember();
            if (key == '1' || key == '2') position[0] = std::clamp(position[0]+(key == '1' ? -.25 : .25), -3.0, 3.0);
            else if (key == '3') yaw += 3.141592653589793/8;
            else if (key == 'r') { position = {0,1.3,0}; yaw = 0; }
            else occluder = !occluder;
        }
        ++revision; ++commands; dragDistance.reset();
        return true;
    }
    std::pair<double, std::string> hit(Vec origin, Vec direction) const
    {
        std::pair<double, std::string> nearest{std::numeric_limits<double>::infinity(), "none"};
        auto accept = [&](double distance, const std::string& name) {
            if (distance < nearest.first) nearest = {distance,name};
        };
        auto box = [&](const std::string& name, Vec center, Vec half, Vec o, Vec d) {
            if (auto t = boxHit(o,d,center,half)) accept(*t,name);
        };
        const double c = std::cos(yaw), s = std::sin(yaw);
        auto local = [&](Vec v) -> Vec { return {c*v[0]-s*v[2],v[1],s*v[0]+c*v[2]}; };
        box("cube", {0,0,0}, {.3,.3,.3}, local(add(origin,scale(position,-1))), local(direction));
        for (int i = 0; i < 4; ++i) box(std::to_string(i+1), {-.9+.6*i,.9,1.04}, {.225,.175,.04}, origin,direction);
        box("panel", {0,.9,.96}, {1.2,.325,.02}, origin,direction);
        if (occluder) box("occluder", {.66,1.1,1.55}, {.07,.3,.07}, origin,direction);
        if (direction[1] < -1e-8)
        {
            const double t = -origin[1]/direction[1];
            const auto p = add(origin,scale(direction,t));
            if (t >= 0 && std::abs(p[0]) <= 5 && std::abs(p[2]) <= 5) accept(t,"floor");
        }
        if (nearest.second == "none") nearest.first = 5;
        return nearest;
    }
    struct HitResult { std::optional<Vec> cursor, teleport; };
    HitResult pointer(Vec origin, Vec direction, bool active, bool down)
    {
        const bool pressed = active && previousActive && down && !previousDown;
        previousActive = active; previousDown = down;
        if (!active) { dragDistance.reset(); return {}; }
        const auto [distance,target] = hit(origin,direction);
        const auto point = add(origin,scale(direction,distance));
        HitResult result{point,{}};
        if (pressed)
        {
            if (target.size() == 1 && target[0] >= '1' && target[0] <= '4') command(target[0]);
            else if (target == "cube") { remember(); dragDistance = distance; dragOffset = add(position,scale(point,-1)); }
            else if (target == "floor") result.teleport = point;
            else if (target == "occluder") ++blocked;
        }
        if (dragDistance)
        {
            if (!down) dragDistance.reset();
            else
            {
                auto p = add(add(origin,scale(direction,*dragDistance)),dragOffset);
                p = {std::clamp(p[0],-3.0,3.0),std::clamp(p[1],.3,3.0),std::clamp(p[2],-2.0,2.0)};
                if (p != position) { position = p; ++revision; }
            }
        }
        return result;
    }
};
struct Pointer { Vec origin{0,0,0}, direction{0,0,-1}; bool active = false, down = false; double stick = 0; };
inline Pointer demoPointer(int frame)
{
    // Synthetic world-space rays exercise the real hit-test/command path.
    const int phase = (frame/15)%5;
    const Vec targets[] = {{-.9,.9,1.04},{.3,.9,1.04},{.9,.9,1.04},{0,1.3,0},{-2,0,2}};
    auto target = targets[phase];
    if (phase == 3 && frame%15 >= 5) target[0] += std::min(frame%15-5,4)*.05;
    const Vec origin{.3,1.4,2.1};
    return {origin,normalize(add(target,scale(origin,-1))),true,frame%15 >= 5 && frame%15 < 10,0};
}
}
