// Copyright (c) 2026, NVIDIA CORPORATION. All rights reserved.
#include "../common/sample_support.h"
#include <ovstage/ovstage.h>
#include <ovx/path_dictionary/path_dictionary.h>
#include <dlpack/dlpack.h>
#include <cmath>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

extern thread_local std::string sampleError;
namespace
{
std::string text(ovx_string_t value) { return value.ptr ? std::string(value.ptr, value.length) : std::string(); }
void check(ovstage_api_status_t status)
{
    if (status != OVSTAGE_OK) throw std::runtime_error(text(ovstage_get_last_error()));
}
struct Panel
{
    static constexpr uint32_t columns = 128, rows = 96;
    ovstage_instance_t* stage = nullptr;
    path_dictionary_instance_t* paths = nullptr;
    ovx_primpath_list_t list = OVX_INVALID_PRIMPATH_LIST;
    ovstage_query_handle_t query = OVSTAGE_INVALID_QUERY_HANDLE;
    uint32_t imageWidth = 0, imageHeight = 0;
    void wait(ovstage_enqueue_result_t operation)
    {
        check(operation.status);
        ovstage_op_wait_result_t result{};
        const auto status = ovstage_wait_op(stage, operation.op_index, OVSTAGE_TIMEOUT_INFINITE, &result);
        const bool failed = result.error_op_id_count != 0;
        const auto error = failed ? text(ovstage_get_last_op_error(stage, result.error_op_ids[0])) : std::string();
        const auto released = ovstage_release_op(stage, operation.op_index);
        check(status); check(released);
        if (failed) throw std::runtime_error("Panel write: " + error);
    }
    void initialize(ovstage_instance_t* source)
    {
        stage = source;
        paths = ovstage_get_path_dictionary(stage);
        if (!paths) throw std::runtime_error("Panel stage has no path dictionary");
        constexpr char path[] = "/World/PipPanel/Screen";
        const ovx_string_t name{path, sizeof(path)-1};
        const auto result = path_dictionary_create_path_list_from_strings(paths, &name, 1, &list);
        if (result.status != OVX_API_SUCCESS)
        {
            const auto error = text(result.error);
            path_dictionary_release_error(paths, result.error);
            throw std::runtime_error(error);
        }
        check(ovstage_query_from_path_list(stage, list, &query));
    }
    ~Panel()
    {
        if (query != OVSTAGE_INVALID_QUERY_HANDLE)
        {
            try { wait(ovstage_release_query(stage, query)); }
            catch (const std::exception& error) { sampleError = error.what(); }
        }
        if (list != OVX_INVALID_PRIMPATH_LIST) path_dictionary_release_path_list_reference(paths, list);
    }
    template<class T> void write(const char* attribute, std::vector<T>& values, uint8_t code, uint16_t lanes,
                                 ovstage_attribute_semantic_t semantic, int64_t ordinal)
    {
        int64_t shape[] = {int64_t(values.size()/lanes)};
        DLTensor tensor{};
        tensor.data = values.data(); tensor.device = {kDLCPU, 0}; tensor.ndim = 1;
        tensor.dtype = {code, uint8_t(sizeof(T)*8), lanes}; tensor.shape = shape;
        ovstage_write_data_t data{};
        data.tensors = &tensor; data.tensor_count = 1; data.is_array = true; data.semantic = semantic;
        ovx_token_or_string_t name{};
        name.string = {attribute, std::strlen(attribute)};
        // Waiting consumes the caller-owned tensor before its storage can move.
        wait(ovstage_write_attribute(stage, query, name, ordinal, data, OVSTAGE_PRIM_MODE_UPSERT));
    }
    void update(const uint8_t* rgba, uint32_t width, uint32_t height, uint32_t pitch, int64_t ordinal)
    {
        if (!rgba || !width || !height || uint64_t(width)*4 > pitch || !ordinal)
            throw std::runtime_error("Invalid panel frame or application ordinal");
        if (imageWidth != width || imageHeight != height)
        {
            std::vector<float> points;
            for (uint32_t y = 0; y <= rows; ++y)
                for (uint32_t x = 0; x <= columns; ++x)
                {
                    points.push_back((float(x)/columns-.5f)*.9f);
                    points.push_back((.5f-float(y)/rows)*.9f*float(height)/width);
                    points.push_back(0);
                }
            std::vector<int32_t> counts(columns*rows, 4), indices;
            for (uint32_t y = 0; y < rows; ++y)
                for (uint32_t x = 0; x < columns; ++x)
                {
                    const int32_t p = int32_t(y*(columns+1)+x);
                    indices.insert(indices.end(), {p, p+int32_t(columns)+1, p+int32_t(columns)+2, p+1});
                }
            write("points", points, kDLFloat, 3, OVSTAGE_SEMANTIC_POINT, ordinal);
            write("faceVertexCounts", counts, kDLInt, 1, OVSTAGE_SEMANTIC_NONE, ordinal);
            write("faceVertexIndices", indices, kDLInt, 1, OVSTAGE_SEMANTIC_NONE, ordinal);
            imageWidth = width; imageHeight = height;
        }
        // A small face-color mesh uses only public OVStage attributes. It is a
        // diagnostic preview, not a dynamic texture API or video-performance path.
        // Decode sRGB before feeding scene-linear emission to avoid double gamma.
        float linear[256];
        for (int i = 0; i < 256; ++i)
        {
            const float s = i/255.0f;
            linear[i] = s <= .04045f ? s/12.92f : std::pow((s+.055f)/1.055f, 2.4f);
        }
        std::vector<float> colors;
        colors.reserve(columns*rows*3);
        for (uint32_t y = 0; y < rows; ++y)
            for (uint32_t x = 0; x < columns; ++x)
            {
                const auto sx = uint32_t((uint64_t(2*x+1)*width)/(2*columns));
                const auto sy = uint32_t((uint64_t(2*y+1)*height)/(2*rows));
                const auto* pixel = rgba + size_t(sy)*pitch + size_t(sx)*4;
                for (int c = 0; c < 3; ++c) colors.push_back(linear[pixel[c]]);
            }
        write("primvars:displayColor", colors, kDLFloat, 3, OVSTAGE_SEMANTIC_COLOR, ordinal);
    }
};
}
extern "C" void* sample_panel_create(uint64_t stage)
{
    try
    {
        if (!stage) throw std::runtime_error("Panel requires a stage");
        auto panel = std::make_unique<Panel>();
        panel->initialize(reinterpret_cast<ovstage_instance_t*>(stage));
        return panel.release();
    }
    catch (const std::exception& error) { sampleError = error.what(); return nullptr; }
}
extern "C" int sample_panel_update(void* panel, const void* rgba, uint32_t width, uint32_t height,
                                    uint32_t pitch, int64_t ordinal)
{
    try
    {
        if (!panel) throw std::runtime_error("Panel is null");
        static_cast<Panel*>(panel)->update(static_cast<const uint8_t*>(rgba), width, height, pitch, ordinal);
        return 0;
    }
    catch (const std::exception& error) { sampleError = error.what(); return -1; }
}
extern "C" void sample_panel_destroy(void* panel) { delete static_cast<Panel*>(panel); }
