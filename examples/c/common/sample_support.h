// Copyright (c) 2026, NVIDIA CORPORATION. All rights reserved.
#pragma once
#include <stdint.h>
#include <ovrtx/ovrtx_types.h>
#ifdef _WIN32
#    ifdef SAMPLE_SUPPORT_BUILD
#        define SAMPLE_API __declspec(dllexport)
#    else
#        define SAMPLE_API __declspec(dllimport)
#    endif
#else
#    define SAMPLE_API __attribute__((visibility("default")))
#endif

// Example-only C ABI shared by the C++ and Python workbenches. This is not an
// OVXR API. All calls, including mirror event processing, run on the frame thread.
extern "C"
{
struct SamplePose { float orientation[4]; float position[3]; };
struct SampleController
{
    SamplePose aim;
    uint32_t active;
    uint32_t select;
    float stick[2];
};
struct SampleHand
{
    SamplePose joints[26];
    uint32_t valid[26];
    uint32_t active;
};
struct SampleInput
{
    SampleController controllers[2];
    SampleHand hands[2];
    uint32_t focused;
};
SAMPLE_API const char* sample_support_error();
SAMPLE_API void* sample_input_create(uint64_t instance, uint64_t session, uint64_t space,
                                     uint64_t getProcAddress, uint32_t hands);
SAMPLE_API int sample_input_poll(void* input, int64_t displayTime, SampleInput* state);
SAMPLE_API void sample_input_destroy(void* input);
SAMPLE_API void* sample_mirror_create(const char* title);
// Compact, resizable, always-on-top desktop picture-in-picture window.
SAMPLE_API void* sample_pip_create(const char* title);
// Return 1 when open, 0 after window close, -1 on error. Pixels are copied.
SAMPLE_API int sample_mirror_present(void* mirror, const void* rgba, uint32_t width, uint32_t height, uint32_t pitch);
SAMPLE_API int sample_mirror_key(void* mirror);
SAMPLE_API void sample_mirror_title(void* mirror, const char* title);
SAMPLE_API void sample_mirror_destroy(void* mirror);
// Public OVStage-backed diagnostic panel. Call update at an application-owned
// ordinal and seal it through OVXR bind before rendering. Pixels are copied.
SAMPLE_API void* sample_panel_create(uint64_t stage);
SAMPLE_API int sample_panel_update(void* panel, const void* rgba, uint32_t width, uint32_t height,
                                  uint32_t pitch, int64_t ordinal);
SAMPLE_API void sample_panel_destroy(void* panel);
// Independent scene camera, using the same renderer/stage at a committed ordinal.
// Capture returns owned CPU storage valid until the next capture or destroy.
struct SampleImage { const void* rgba; uint32_t width, height, pitch; };
SAMPLE_API void* sample_observer_create(uint64_t renderer);
SAMPLE_API int sample_observer_capture(void* observer, int64_t ordinal, SampleImage* image);
SAMPLE_API void sample_observer_destroy(void* observer);
// Application-owned stereo projection layer. The renderer, XR session and
// Vulkan device/queue remain borrowed; destroy this helper before those owners.
// GPU transfers are deliberately synchronous via host staging in this example.
struct SampleProjectionView { SamplePose pose; float fov[4]; };
SAMPLE_API void* sample_projection_create(uint64_t renderer, uint64_t instance, uint64_t session,
    uint64_t space, uint64_t getProcAddress, uint64_t vkInstance, uint64_t physicalDevice,
    uint64_t device, uint64_t queue, uint32_t queueFamily);
// Use the exact cached views copied from ovxr_get_frame_projection_views().
SAMPLE_API int sample_projection_set_views(void* projection, const SampleProjectionView* views, uint32_t count);
// Copies outputs from a BORROWED step; mappings are released before returning.
SAMPLE_API int sample_projection_read(void* projection, ovrtx_step_result_handle_t step, const char* product,
    SampleImage* color, SampleImage* depth);
SAMPLE_API int sample_projection_upload(void* projection, uint32_t eye, const void* rgba,
    const void* depth, uint32_t width, uint32_t height, float nearZ, float farZ);
// Borrowed XrCompositionLayerBaseHeader*, valid until next set_views or destroy.
SAMPLE_API const void* sample_projection_layer(void* projection);
SAMPLE_API void sample_projection_destroy(void* projection);
// Independent alpha-blended quad, appended after the scene. No scene depth test.
// Use a separate helper instance from stereo projection composition. Optional
// readback receives width*height*4 bytes copied back from the actual XR image.
// Layer pointer remains borrowed until the next upload or helper destruction.
SAMPLE_API const void* sample_quad_upload(void* projection, const void* rgba, void* readback,
    uint32_t width, uint32_t height, uint32_t headLocked);
}
