// Copyright (c) 2026, NVIDIA CORPORATION. All rights reserved.
#pragma once
#include "xr_sample.h"
#include "workbench_model.h"
#include <map>

namespace sample
{
using workbench::Vec;
inline Vec cross(Vec a, Vec b)
{
    return {a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0]};
}
inline Matrix pointerBeam(Vec origin, Vec endpoint, bool active, bool down)
{
    if (!active) return transform(0,-100,0);
    const auto delta = workbench::add(endpoint, workbench::scale(origin,-1));
    const auto direction = workbench::normalize(delta);
    const auto side = workbench::normalize(cross(direction, std::abs(direction[1]) < .95 ? Vec{0,1,0} : Vec{1,0,0}));
    const auto up = cross(side,direction);
    const auto center = workbench::scale(workbench::add(origin,endpoint),.5);
    const double radius = down ? .009 : .004;
    return {side[0]*radius,side[1]*radius,side[2]*radius,0,
            up[0]*radius,up[1]*radius,up[2]*radius,0,delta[0],delta[1],delta[2],0,
            center[0],center[1],center[2],1};
}
inline void handTransforms(std::map<std::string,Matrix>& updates, const std::string& name,
                           const std::map<int,Vec>& points)
{
    const auto prefix = "/World/Hand" + name;
    const auto hidden = transform(0,-100,0);
    for (int joint=0; joint<26; ++joint)
    {
        const auto p = points.find(joint);
        updates[prefix+"Joint"+std::to_string(joint)] = p == points.end() ? hidden :
            transform(p->second[0],p->second[1],p->second[2]);
    }
    static const std::vector<std::vector<int>> chains{{1,2,3,4,5},{1,6,7,8,9,10},{1,11,12,13,14,15},
                                              {1,16,17,18,19,20},{1,21,22,23,24,25}};
    int bone = 0;
    for (const auto& chain : chains)
        for (size_t joint=1; joint<chain.size(); ++joint)
        {
            auto matrix = hidden;
            const auto start = points.find(chain[joint-1]), end = points.find(chain[joint]);
            if (start != points.end() && end != points.end())
            {
                const auto delta = workbench::add(end->second,workbench::scale(start->second,-1));
                if (workbench::length(delta) > 1e-6)
                {
                    const auto forward = workbench::normalize(delta);
                    const auto side = workbench::normalize(cross(forward,std::abs(forward[1]) < .95 ? Vec{0,1,0} : Vec{1,0,0}));
                    const auto normal = cross(forward,side);
                    const auto center = workbench::scale(workbench::add(start->second,end->second),.5);
                    matrix = {side[0]*.009,side[1]*.009,side[2]*.009,0,
                              normal[0]*.009,normal[1]*.009,normal[2]*.009,0,
                              delta[0],delta[1],delta[2],0,center[0],center[1],center[2],1};
                }
            }
            updates[prefix+"Bone"+std::to_string(bone++)] = matrix;
        }
    // Joint positions define the palm; valid orientations are not required.
    auto palm = hidden;
    if (points.count(1) && points.count(7) && points.count(22))
    {
        const auto knuckles = workbench::scale(workbench::add(points.at(7),points.at(22)),.5);
        const auto delta = workbench::add(knuckles,workbench::scale(points.at(1),-1));
        const double length = workbench::length(delta);
        const auto forward = workbench::normalize(delta);
        auto across = workbench::add(points.at(22),workbench::scale(points.at(7),-1));
        const double along = across[0]*forward[0]+across[1]*forward[1]+across[2]*forward[2];
        across = workbench::add(across,workbench::scale(forward,-along));
        const double width = workbench::length(across);
        if (std::min(length,width) > 1e-6)
        {
            const auto side = workbench::normalize(across), normal = cross(forward,side);
            const auto center = workbench::scale(workbench::add(points.at(1),knuckles),.5);
            palm = {side[0]*width*.65,side[1]*width*.65,side[2]*width*.65,0,
                    normal[0]*width*.18,normal[1]*width*.18,normal[2]*width*.18,0,
                    forward[0]*length*.6,forward[1]*length*.6,forward[2]*length*.6,0,
                    center[0],center[1],center[2],1};
        }
    }
    updates[prefix+"Palm"] = palm;
}
inline std::array<Vec,26> demoHand(Vec tip, Vec direction, bool pinched, bool left)
{
    const auto forward = workbench::normalize(direction);
    const auto side = workbench::normalize(cross(forward,std::abs(forward[1]) < .95 ? Vec{0,1,0} : Vec{1,0,0}));
    const auto up = cross(side,forward);
    const auto point = [&](double x,double y,double z) {
        Vec result{};
        for (int i=0; i<3; ++i) result[i] = tip[i]+side[i]*x*(left ? -1 : 1)+up[i]*y+forward[i]*z;
        return result;
    };
    std::array<Vec,26> joints{};
    joints[0]=point(.034,0,-.135); joints[1]=point(.035,-.015,-.19);
    joints[2]=point(-.005,-.005,-.155); joints[3]=point(-.037,-.002,-.117);
    joints[4]=point(pinched ? -.03 : -.056,-.015,pinched ? -.045 : -.075);
    joints[5]=point(pinched ? -.012 : -.068,-.006,pinched ? -.008 : -.035);
    const double offsets[]{0,.027,.052,.075}, knuckles[]{-.10,-.10,-.105,-.115}, tips[]{0,.01,-.005,-.04};
    for (int finger=0; finger<4; ++finger)
    {
        const double k = knuckles[finger], t = tips[finger];
        const double z[]{-.155,k,k*.55+t*.45,k*.25+t*.75,t};
        for (int j=0; j<5; ++j) joints[6+finger*5+j]=point(offsets[finger]+.002*finger*j,-.004*finger,z[j]);
    }
    joints[10]=tip;
    return joints;
}
}
