// Copyright (c) 2026, NVIDIA CORPORATION. All rights reserved.
#pragma once
#include "frame_copy.h"
#include "sample_support.h"

namespace sample
{
inline void supportCheck(bool success)
{
    if (!success) throw std::runtime_error(sample_support_error());
}
struct Input
{
    void* handle = nullptr;
    Input() = default;
    Input(const Input&) = delete;
    Input& operator=(const Input&) = delete;
    ~Input() { sample_input_destroy(handle); }
    bool initialize(ovxr_context_t* context, bool hands)
    {
        if (handle) return true;
        ovxr_openxr_handles_t handles{};
        const auto result = ovxr_get_openxr_handles(context, &handles);
        if (result == OVXR_NOT_READY) return false;
        check(result, "get_openxr_handles");
        handle = sample_input_create(handles.xr_instance, handles.xr_session, handles.xr_space,
                                     handles.xr_get_instance_proc_addr, hands);
        supportCheck(handle != nullptr);
        return true;
    }
    SampleInput poll(int64_t time)
    {
        SampleInput state{};
        supportCheck(sample_input_poll(handle, time, &state) == 0);
        return state;
    }
    void close() { sample_input_destroy(std::exchange(handle, nullptr)); }
};
struct Mirror
{
    void* handle = nullptr;
    FrameCopy copy;
    uint64_t sequence = 0;
    uint64_t presented = 0;
    Mirror() = default;
    Mirror(const Mirror&) = delete;
    Mirror& operator=(const Mirror&) = delete;
    ~Mirror() { sample_mirror_destroy(handle); }
    void open(const char* title)
    {
        handle = sample_mirror_create(title);
        supportCheck(handle != nullptr);
    }
    bool update(ovxr_context_t* context, bool capture)
    {
        if (!handle) return true;
        ovxr_frame_view_t view{};
        std::vector<uint8_t> rgba;
        if (capture)
        {
            const auto result = ovxr_acquire_frame(context, &view);
            if (result == OVXR_SUCCESS && view.sequence > sequence)
            {
                rgba = copy.copy(view); // Own the pixels before advancing XR.
                sequence = view.sequence;
                ++presented;
            }
            else if (result != OVXR_SUCCESS && result != OVXR_NOT_READY) check(result, "acquire_frame");
        }
        const auto result = sample_mirror_present(handle, rgba.empty() ? nullptr : rgba.data(),
                                                  view.width, view.height, view.pitch_bytes);
        supportCheck(result >= 0);
        return result != 0;
    }
    char key() { return handle ? char(sample_mirror_key(handle)) : 0; }
    void title(const std::string& value) { if (handle) sample_mirror_title(handle, value.c_str()); }
};
}
