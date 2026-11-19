// Copyright (c) 2026, NVIDIA CORPORATION. All rights reserved.
#include "../common/sample_support.h"
#include <openxr/openxr.h>
#include <array>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

thread_local std::string sampleError;
extern "C" const char* sample_support_error() { return sampleError.c_str(); }

namespace
{
void check(XrResult result, const char* operation)
{
    if (XR_FAILED(result))
        throw std::runtime_error(std::string(operation) + ": XrResult " + std::to_string(result));
}
struct Input
{
    XrInstance instance;
    XrSession session;
    XrSpace base;
    PFN_xrGetInstanceProcAddr getProc;
    XrActionSet actionSet = XR_NULL_HANDLE;
    XrAction aim = XR_NULL_HANDLE, select = XR_NULL_HANDLE, stick = XR_NULL_HANDLE;
    XrPath users[2]{};
    XrSpace spaces[2]{};
    XrHandTrackerEXT hands[2]{};
    PFN_xrDestroySpace destroySpace = nullptr;
    PFN_xrDestroyActionSet destroySet = nullptr;
    PFN_xrDestroyHandTrackerEXT destroyHand = nullptr;
    PFN_xrLocateHandJointsEXT locateHand = nullptr;
    bool handMode = false;

    template <class T> T function(const char* name)
    {
        PFN_xrVoidFunction value = nullptr;
        check(getProc(instance, name, &value), name);
        if (!value) throw std::runtime_error(std::string("Unavailable OpenXR entry point: ") + name);
        return reinterpret_cast<T>(value);
    }
    ~Input()
    {
        // Destroy only our children, before the borrowed OVXR session/instance.
        for (auto hand : hands) if (hand && destroyHand) destroyHand(hand);
        for (auto space : spaces) if (space && destroySpace) destroySpace(space);
        if (actionSet && destroySet) destroySet(actionSet);
    }
    XrPath path(const char* text)
    {
        XrPath value{};
        check(function<PFN_xrStringToPath>("xrStringToPath")(instance, text, &value), "xrStringToPath");
        return value;
    }
    XrAction action(const char* name, XrActionType type)
    {
        XrActionCreateInfo info{ XR_TYPE_ACTION_CREATE_INFO };
        std::strcpy(info.actionName, name);
        std::strcpy(info.localizedActionName, name);
        info.actionType = type;
        info.countSubactionPaths = 2;
        info.subactionPaths = users;
        XrAction value{};
        check(function<PFN_xrCreateAction>("xrCreateAction")(actionSet, &info, &value), "xrCreateAction");
        return value;
    }
    void initialize(bool useHands)
    {
        destroySpace = function<PFN_xrDestroySpace>("xrDestroySpace");
        destroySet = function<PFN_xrDestroyActionSet>("xrDestroyActionSet");
        handMode = useHands;
        // An empty action set in hand mode still supplies focus gating through
        // xrSyncActions, without reading controller data alongside tracked hands.
        users[0] = path("/user/hand/left");
        users[1] = path("/user/hand/right");
        XrActionSetCreateInfo setInfo{ XR_TYPE_ACTION_SET_CREATE_INFO };
        std::strcpy(setInfo.actionSetName, "sample_workbench");
        std::strcpy(setInfo.localizedActionSetName, "Sample workbench");
        check(function<PFN_xrCreateActionSet>("xrCreateActionSet")(instance, &setInfo, &actionSet), "xrCreateActionSet");
        if (!useHands)
        {
            aim = action("aim", XR_ACTION_TYPE_POSE_INPUT);
            select = action("select", XR_ACTION_TYPE_BOOLEAN_INPUT);
            stick = action("navigate", XR_ACTION_TYPE_VECTOR2F_INPUT);
            for (bool touch : { false, true })
            {
                std::vector<XrActionSuggestedBinding> bindings;
                for (const char* side : { "left", "right" })
                {
                    const std::string prefix = std::string("/user/hand/") + side;
                    bindings.push_back({ aim, path((prefix + "/input/aim/pose").c_str()) });
                    bindings.push_back({ select, path((prefix + (touch ? "/input/trigger/value" : "/input/select/click")).c_str()) });
                    if (touch) bindings.push_back({ stick, path((prefix + "/input/thumbstick").c_str()) });
                }
                XrInteractionProfileSuggestedBinding suggestion{ XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING };
                suggestion.interactionProfile = path(touch ? "/interaction_profiles/oculus/touch_controller" :
                                                             "/interaction_profiles/khr/simple_controller");
                suggestion.countSuggestedBindings = uint32_t(bindings.size());
                suggestion.suggestedBindings = bindings.data();
                const auto result = function<PFN_xrSuggestInteractionProfileBindings>("xrSuggestInteractionProfileBindings")(
                    instance, &suggestion);
                // A runtime may not support Touch; Simple remains the baseline.
                if (!touch || (result != XR_ERROR_PATH_UNSUPPORTED && result != XR_ERROR_PATH_INVALID))
                    check(result, "xrSuggestInteractionProfileBindings");
            }
            for (int i = 0; i < 2; ++i)
            {
                XrActionSpaceCreateInfo info{ XR_TYPE_ACTION_SPACE_CREATE_INFO };
                info.action = aim;
                info.subactionPath = users[i];
                info.poseInActionSpace.orientation.w = 1;
                check(function<PFN_xrCreateActionSpace>("xrCreateActionSpace")(session, &info, &spaces[i]), "xrCreateActionSpace");
            }
        }
        else
        {
            destroyHand = function<PFN_xrDestroyHandTrackerEXT>("xrDestroyHandTrackerEXT");
            locateHand = function<PFN_xrLocateHandJointsEXT>("xrLocateHandJointsEXT");
            for (int i = 0; i < 2; ++i)
            {
                XrHandTrackerCreateInfoEXT info{ XR_TYPE_HAND_TRACKER_CREATE_INFO_EXT };
                info.hand = i ? XR_HAND_RIGHT_EXT : XR_HAND_LEFT_EXT;
                info.handJointSet = XR_HAND_JOINT_SET_DEFAULT_EXT;
                check(function<PFN_xrCreateHandTrackerEXT>("xrCreateHandTrackerEXT")(session, &info, &hands[i]),
                      "xrCreateHandTrackerEXT (requires XR_EXT_hand_tracking and runtime support)");
            }
        }
        XrSessionActionSetsAttachInfo attach{ XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO };
        attach.countActionSets = 1;
        attach.actionSets = &actionSet;
        // Exactly once for this session. OVXR owns frame/session/event driving.
        check(function<PFN_xrAttachSessionActionSets>("xrAttachSessionActionSets")(session, &attach), "xrAttachSessionActionSets");
    }
    static SamplePose pose(const XrPosef& value)
    {
        return { { value.orientation.x, value.orientation.y, value.orientation.z, value.orientation.w },
                 { value.position.x, value.position.y, value.position.z } };
    }
    void poll(XrTime time, SampleInput& out)
    {
        XrActiveActionSet active{ actionSet, XR_NULL_PATH };
        XrActionsSyncInfo sync{ XR_TYPE_ACTIONS_SYNC_INFO };
        sync.countActiveActionSets = 1;
        sync.activeActionSets = &active;
        const auto synced = function<PFN_xrSyncActions>("xrSyncActions")(session, &sync);
        if (synced == XR_SESSION_NOT_FOCUSED || synced == XR_ERROR_SESSION_NOT_RUNNING) return;
        check(synced, "xrSyncActions");
        out.focused = 1;
        for (int i = 0; i < 2; ++i)
        {
            if (handMode)
            {
                std::array<XrHandJointLocationEXT, XR_HAND_JOINT_COUNT_EXT> joints{};
                XrHandJointsLocateInfoEXT info{ XR_TYPE_HAND_JOINTS_LOCATE_INFO_EXT };
                info.baseSpace = base;
                info.time = time;
                XrHandJointLocationsEXT locations{ XR_TYPE_HAND_JOINT_LOCATIONS_EXT };
                locations.jointCount = uint32_t(joints.size());
                locations.jointLocations = joints.data();
                check(locateHand(hands[i], &info, &locations), "xrLocateHandJointsEXT");
                out.hands[i].active = locations.isActive;
                if (!locations.isActive) continue;
                for (size_t j = 0; j < joints.size(); ++j)
                {
                    out.hands[i].valid[j] = (joints[j].locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) != 0;
                    out.hands[i].joints[j] = pose(joints[j].pose);
                }
                continue;
            }
            XrActionStateGetInfo get{ XR_TYPE_ACTION_STATE_GET_INFO };
            get.subactionPath = users[i];
            get.action = aim;
            XrActionStatePose aiming{ XR_TYPE_ACTION_STATE_POSE };
            check(function<PFN_xrGetActionStatePose>("xrGetActionStatePose")(session, &get, &aiming), "xrGetActionStatePose");
            if (!aiming.isActive) continue;
            XrSpaceLocation location{ XR_TYPE_SPACE_LOCATION };
            check(function<PFN_xrLocateSpace>("xrLocateSpace")(spaces[i], base, time, &location), "xrLocateSpace");
            constexpr auto valid = XR_SPACE_LOCATION_ORIENTATION_VALID_BIT | XR_SPACE_LOCATION_POSITION_VALID_BIT;
            if ((location.locationFlags & valid) != valid) continue;
            auto& controller = out.controllers[i];
            controller.active = 1;
            controller.aim = pose(location.pose);
            get.action = select;
            XrActionStateBoolean selecting{ XR_TYPE_ACTION_STATE_BOOLEAN };
            check(function<PFN_xrGetActionStateBoolean>("xrGetActionStateBoolean")(session, &get, &selecting), "xrGetActionStateBoolean");
            controller.select = selecting.isActive && selecting.currentState;
            get.action = stick;
            XrActionStateVector2f moving{ XR_TYPE_ACTION_STATE_VECTOR2F };
            check(function<PFN_xrGetActionStateVector2f>("xrGetActionStateVector2f")(session, &get, &moving), "xrGetActionStateVector2f");
            if (moving.isActive) { controller.stick[0] = moving.currentState.x; controller.stick[1] = moving.currentState.y; }
        }
    }
};
}

extern "C" void* sample_input_create(uint64_t instance, uint64_t session, uint64_t space, uint64_t proc, uint32_t hands)
{
    try
    {
        auto input = std::make_unique<Input>();
        input->instance = reinterpret_cast<XrInstance>(instance);
        input->session = reinterpret_cast<XrSession>(session);
        input->base = reinterpret_cast<XrSpace>(space);
        std::memcpy(&input->getProc, &proc, sizeof(input->getProc));
        input->initialize(hands != 0);
        return input.release();
    }
    catch (const std::exception& error) { sampleError = error.what(); return nullptr; }
}
extern "C" int sample_input_poll(void* input, int64_t time, SampleInput* state)
{
    try
    {
        if (!input || !state || time <= 0) throw std::runtime_error("Invalid input poll arguments");
        *state = {};
        static_cast<Input*>(input)->poll(time, *state);
        return 0;
    }
    catch (const std::exception& error) { sampleError = error.what(); return -1; }
}
extern "C" void sample_input_destroy(void* input) { delete static_cast<Input*>(input); }
