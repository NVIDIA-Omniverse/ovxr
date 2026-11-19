// Copyright (c) 2026, NVIDIA CORPORATION. All rights reserved.
#include "../common/interactive_sample.h"
#include "../common/workbench_model.h"
#include "../common/pointer_visuals.h"
#include <cctype>
#include <fstream>
#include <map>

using workbench::Vec;
namespace
{
sample::Matrix mapped(ovxr_context_t* context, const SamplePose& source, bool positionOnly = false)
{
    ovxr_pose_t pose{};
    pose.position = {source.position[0],source.position[1],source.position[2]};
    pose.orientation = positionOnly ? ovxr_quaternion_t{0,0,0,1} :
        ovxr_quaternion_t{source.orientation[0],source.orientation[1],source.orientation[2],source.orientation[3]};
    sample::Matrix matrix{};
    sample::check(ovxr_transform_openxr_pose(context,&pose,matrix.data()), "transform_openxr_pose");
    return matrix;
}
Vec position(const sample::Matrix& m) { return {m[12],m[13],m[14]}; }
void publish(ovxr_context_t* context, const std::map<std::string,sample::Matrix>& updates)
{
    std::vector<ovxr_prim_transform_t> prims;
    for (const auto& [name,matrix] : updates)
    {
        ovxr_prim_transform_t prim{};
        prim.prim_path = name.c_str();
        std::copy(matrix.begin(),matrix.end(),prim.matrix);
        prims.push_back(prim);
    }
    sample::check(ovxr_update_prim_transforms(context,prims.data(),prims.size(),0), "update_prim_transforms");
}
}
int main(int argc, char** argv)
{
    try
    {
        sample::Options options;
        sample::Configuration config;
        config.scene = "spatial-workbench.usda";
        std::string mode = "controllers";
        std::filesystem::path report;
        for (int i = 1; i < argc; ++i)
        {
            const std::string arg = argv[i];
            if (arg == "--help" || arg == "-h")
            {
                sample::message("Options: --frames N, --no-foveation, --input controllers|hands|demo, --mirror (Windows), --report PATH.json");
                return 0;
            }
            if (arg == "--no-foveation") options.noFoveation = true;
            else if (arg == "--mirror") config.readback = config.readbackUnwarp = true;
            else if (arg == "--frames" && i+1 < argc) options.frames = sample::nonnegative(argv[++i]);
            else if (arg == "--input" && i+1 < argc) mode = argv[++i];
            else if (arg == "--report" && i+1 < argc) report = argv[++i];
            else throw std::runtime_error("Unknown option or missing value: " + arg);
        }
        if (mode != "controllers" && mode != "hands" && mode != "demo") throw std::runtime_error("Invalid --input mode");
        constexpr char handExtension[] = "XR_EXT_hand_tracking";
        const ovx_string_t extension{handExtension,sizeof(handExtension)-1};
        if (mode == "hands") { config.extensions = &extension; config.extensionCount = 1; }
        sample::message("Panel / keys: 1 left, 2 right, 3 rotate, 4 undo; R reset, O occluder, Z/X turn, Q quit.");
        sample::message("Select / pinch cube to drag; select floor to teleport. Demo input is synthetic.");
        sample::Application app;
        app.open(argv[0],"ovxr.example.spatial-workbench",options,false,config);
        // Input children and the mirror are destroyed before the borrowed session.
        sample::Input input;
        sample::Mirror mirror;
        if (config.readback) mirror.open("OVXR workbench");
        sample::Keyboard keyboard;
        sample::Progress progress{options.frames};
        workbench::Model model;
        bool pinch[2]{}, turnArmed = false;
        int selected = -1, revision = -1, controllerFrames = 0, handFrames = 0, navigation = 0;
        Vec anchor{0,0,3};
        double yaw = 0;
        bool anchorTransition = false;
        while (progress.active())
        {
            if (!mirror.update(app.context,false)) break;
            char key = mirror.key();
            if (!key) key = keyboard.read();
            key = char(std::tolower(static_cast<unsigned char>(key)));
            if (key == 'q') break;
            model.command(key);
            ovxr_frame_plan_t plan{};
            auto result = ovxr_begin_frame(app.context,&plan);
            if (result == OVXR_NOT_READY)
            {
                model.pointer({}, {}, false,false); turnArmed = false;
                std::this_thread::sleep_for(std::chrono::milliseconds(10)); continue;
            }
            sample::check(result,"begin_frame");
            sample::FrameGuard guard{app.context,&plan};
            std::map<std::string,sample::Matrix> updates;
            ovxr_anchor_transform_t currentAnchor{};
            const auto anchorResult = ovxr_get_anchor_transform(app.context,&currentAnchor);
            if (anchorResult != OVXR_NOT_READY) sample::check(anchorResult,"get_anchor_transform");
            const bool ready = anchorResult == OVXR_SUCCESS;
            std::optional<ovxr_anchor_transform_t> nextAnchor;
            workbench::Pointer pointer;
            if (mode == "demo")
            {
                pointer = workbench::demoPointer(progress.submitted);
                for (int side=0; side<2; ++side)
                {
                    const auto joints = sample::demoHand(side ? pointer.origin : Vec{-.35,1.35,2.1},
                                                        side ? pointer.direction : Vec{0,.1,-1},pointer.down,side==0);
                    std::map<int,Vec> points;
                    for (int joint=0; joint<26; ++joint) points[joint] = joints[joint];
                    sample::handTransforms(updates,side ? "Right" : "Left",points);
                }
            }
            else
            {
                SampleInput state{};
                // The first renderable bind resolves the anchor. Navigation
                // also latches at bind; hide input for that transition frame.
                if (input.initialize(app.context,mode == "hands") && ready && !anchorTransition)
                    state = input.poll(plan.timing.predicted_display_time_ns);
                int identity = -1;
                // With a stable anchor, map located input after begin_frame.
                for (int side = 0; side < 2; ++side)
                {
                    const std::string name = side ? "Right" : "Left";
                    if (mode == "controllers")
                    {
                        const auto& controller = state.controllers[side];
                        auto matrix = sample::transform(0,-100,0);
                        if (controller.active && state.focused)
                        {
                            matrix = mapped(app.context,controller.aim);
                            pointer = {position(matrix),{-matrix[8],-matrix[9],-matrix[10]},true,
                                       controller.select != 0,controller.stick[0]};
                            identity = side;
                        }
                        updates["/World/Controller"+name] = matrix;
                    }
                    else
                    {
                        const auto& hand = state.hands[side];
                        std::map<int,Vec> points;
                        for (int joint = 0; joint < 26; ++joint)
                        {
                            if (hand.active && state.focused && hand.valid[joint])
                            {
                                points[joint] = position(mapped(app.context,hand.joints[joint],true));
                            }
                        }
                        sample::handTransforms(updates,name,points);
                        if (points.count(5) && points.count(7) && points.count(10))
                        {
                            const auto ray = workbench::normalize(workbench::add(points[10],workbench::scale(points[7],-1)));
                            const double distance = workbench::length(workbench::add(points[5],workbench::scale(points[10],-1)));
                            pinch[side] = distance < (pinch[side] ? .04 : .025);
                            if (workbench::length(ray) > .5) { pointer = {points[10],ray,true,pinch[side],0}; identity = side; }
                        }
                        else pinch[side] = false;
                    }
                }
                if (identity != selected)
                {
                    model.pointer({}, {}, false,false); turnArmed = false; selected = identity;
                }
                if (pointer.active) { controllerFrames += mode == "controllers"; handFrames += mode == "hands"; }
            }
            const auto hit = model.pointer(pointer.origin,pointer.direction,pointer.active,pointer.down);
            int turn = key == 'z' ? -1 : key == 'x' ? 1 : 0;
            if (std::abs(pointer.stick) < .3) turnArmed = pointer.active;
            else if (pointer.active && turnArmed && std::abs(pointer.stick) > .7)
            { turn = pointer.stick > 0 ? 1 : -1; turnArmed = false; }
            if (ready && (hit.teleport || turn))
            {
                if (hit.teleport) anchor = {(*hit.teleport)[0],0,(*hit.teleport)[2]};
                yaw -= turn * 3.141592653589793/6;
                const auto matrix = sample::transform(anchor[0],anchor[1],anchor[2],yaw);
                ovxr_anchor_transform_t transform{};
                std::copy(matrix.begin(),matrix.end(),transform.matrix);
                nextAnchor = transform;
            }
            updates["/World/AnimatedCube"] = sample::transform(model.position[0],model.position[1],model.position[2],model.yaw);
            updates["/World/Occluder"] = sample::transform(.66,model.occluder ? 1.1 : -100,1.55);
            const auto cursor = hit.cursor.value_or(Vec{0,-100,0});
            updates["/World/PointerBeam"] = sample::pointerBeam(pointer.origin,cursor,pointer.active && hit.cursor.has_value(),pointer.down);
            const auto tip = pointer.active && hit.cursor ? workbench::add(cursor,workbench::scale(pointer.direction,-.018)) : cursor;
            updates["/World/PointerTip"] = sample::transform(tip[0],tip[1],tip[2]);
            publish(app.context,updates);
            result = ovxr_bind_frame(app.context,&plan,sample::kDeltaTime,0,nullptr);
            if (result == OVXR_NOT_READY) continue;
            sample::check(result,"bind_frame");
            ovxr_frame_result_t frame{};
            sample::check(ovxr_submit_frame(app.context,&plan,nullptr,&frame),"submit_frame");
            progress.record(frame);
            anchorTransition = false;
            if (nextAnchor)
            {
                // Queue after this frame; the next bind latches the new origin.
                sample::check(ovxr_set_managed_anchor_transform(app.context,&*nextAnchor,0),"set_managed_anchor_transform");
                anchorTransition = true;
                ++navigation;
            }
            if (model.revision != revision)
            {
                revision = model.revision;
                const auto title = "OVXR " + mode + ": revision " + std::to_string(revision) +
                    ", cube x=" + std::to_string(model.position[0]) + " | 1/2 move 3 rotate 4 undo O occluder";
                sample::message(title); mirror.title(title);
            }
            if (!mirror.update(app.context,progress.submitted%3 == 0)) break;
        }
        if (options.frames && progress.submitted < options.frames) throw std::runtime_error("Session stopped before frame target");
        if (config.readback && !mirror.presented) throw std::runtime_error("Mirror received no fresh frames");
        ovxr_device_info_t info{};
        if (progress.submitted) sample::check(ovxr_get_device_info(app.context,&info),"get_device_info");
        input.close();
        app.close();
        if (!report.empty())
        {
            report = std::filesystem::absolute(report);
            std::filesystem::create_directories(report.parent_path());
            std::ofstream file(report);
            file.exceptions(std::ios::badbit | std::ios::failbit);
            file << "{\"input_mode\":\"" << mode << "\",\"submitted_frames\":" << progress.submitted
                 << ",\"controller_active_frames\":" << controllerFrames << ",\"hand_active_frames\":" << handFrames
                 << ",\"revision\":" << model.revision << ",\"commands\":" << model.commands
                 << ",\"occlusion_blocks\":" << model.blocked << ",\"navigation_commands\":" << navigation
                 << ",\"mirror_frames\":" << mirror.presented
                 << ",\"depth_submission_enabled\":" << (info.features.depth_submission_enabled ? "true" : "false")
                 << ",\"headset_verified\":false}\n";
            file.close();
            sample::message("Report: " + report.string());
        }
        sample::message("spatial-workbench: submitted " + std::to_string(progress.submitted) + " frames; clean shutdown.");
        return 0;
    }
    catch (const std::exception& error) { sample::message(std::string("Sample failed: ") + error.what()); return 1; }
}
