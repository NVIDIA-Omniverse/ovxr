// Copyright (c) 2026, NVIDIA CORPORATION. All rights reserved.
#include "../common/sample_support.h"
#include <ovrtx/ovrtx.h>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

extern thread_local std::string sampleError;
namespace
{
std::string text(ovx_string_t s) { return s.ptr ? std::string(s.ptr, s.length) : std::string(); }
void check(ovrtx_api_status_t status)
{
    if (status != OVRTX_API_SUCCESS) throw std::runtime_error(text(ovrtx_get_last_error()));
}
struct Observer
{
    ovrtx_renderer_t* renderer; // Borrowed; caller destroys us before the renderer.
    std::vector<uint8_t> pixels;
};
struct Capture
{
    ovrtx_renderer_t* renderer;
    ovrtx_step_result_handle_t step{};
    ovrtx_render_var_output_map_handle_t map{};
    ~Capture()
    {
        if (map != OVRTX_INVALID_RENDER_VAR_OUTPUT_MAP_HANDLE) ovrtx_unmap_render_var_output(renderer, map, {});
        if (step != OVRTX_INVALID_STEP_RESULT_HANDLE) ovrtx_destroy_results(renderer, step);
    }
};
}
void* sample_observer_create(uint64_t renderer)
{
    try
    {
        if (!renderer) throw std::runtime_error("Observer requires a renderer");
        return new Observer{reinterpret_cast<ovrtx_renderer_t*>(renderer), {}};
    }
    catch (const std::exception& error) { sampleError = error.what(); return nullptr; }
}
int sample_observer_capture(void* handle, int64_t ordinal, SampleImage* image)
{
    try
    {
        if (!handle || !image || !ordinal) throw std::runtime_error("Invalid observer capture arguments");
        *image = {};
        auto& observer = *static_cast<Observer*>(handle);
        Capture capture{observer.renderer};
        constexpr char path[] = "/Render/PipObserver";
        const ovx_string_t product{path, sizeof(path)-1};
        // A separate mono RenderProduct, never an XR eye or the desktop window.
        check(ovrtx_step_with_stage(observer.renderer, {&product, 1}, 1.0/90.0, ordinal, &capture.step).status);
        ovrtx_render_product_set_outputs_t outputs{};
        check(ovrtx_fetch_results(observer.renderer, capture.step, {~uint64_t(0)}, &outputs).status);
        if (outputs.status != OVRTX_EVENT_COMPLETED) throw std::runtime_error(text(outputs.error_message));
        for (size_t p = 0; p < outputs.output_count; ++p)
        {
            const auto& output = outputs.outputs[p];
            if (!output.output_frame_count) continue;
            const auto& frame = output.output_frames[output.output_frame_count-1];
            for (size_t v = 0; v < frame.render_var_count; ++v)
            {
                const auto& var = frame.output_render_vars[v];
                if (text(var.source_name) != "LdrColor") continue;
                const ovrtx_map_output_description_t desc{OVRTX_MAP_DEVICE_TYPE_CPU, 0};
                ovrtx_render_var_output_t mapped{};
                check(ovrtx_map_render_var_output(observer.renderer, var.output_handle, &desc, {~uint64_t(0)}, &mapped).status);
                capture.map = mapped.map_handle;
                if (mapped.status != OVRTX_EVENT_COMPLETED || mapped.num_tensors != 1)
                    throw std::runtime_error("Observer LdrColor did not return one completed tensor");
                const auto* t = mapped.tensors[0].dl;
                if (!t || t->device.device_type != kDLCPU || t->ndim != 3 || t->shape[2] != 4 ||
                    t->dtype.code != kDLUInt || t->dtype.bits != 8 || t->dtype.lanes != 1 ||
                    t->shape[0] <= 0 || t->shape[0] > 8192 || t->shape[1] <= 0 || t->shape[1] > 8192 ||
                    (t->strides && (t->strides[1] != 4 || t->strides[2] != 1 || t->strides[0] < t->shape[1]*4)))
                    throw std::runtime_error("Observer expected a CPU RGBA8 image");
                image->width = uint32_t(t->shape[1]); image->height = uint32_t(t->shape[0]);
                image->pitch = image->width * 4;
                observer.pixels.resize(size_t(image->pitch) * image->height);
                const auto* source = static_cast<const uint8_t*>(t->data) + t->byte_offset;
                const auto stride = t->strides ? t->strides[0] : image->pitch;
                for (uint32_t row = 0; row < image->height; ++row)
                    std::memcpy(observer.pixels.data() + size_t(row)*image->pitch, source + row*stride, image->pitch);
                image->rgba = observer.pixels.data();
                return 0; // RAII unmaps/releases; only our CPU copy escapes.
            }
        }
        return 1; // RenderProduct has not produced a frame yet.
    }
    catch (const std::exception& error) { sampleError = error.what(); return -1; }
}
void sample_observer_destroy(void* observer) { delete static_cast<Observer*>(observer); }
