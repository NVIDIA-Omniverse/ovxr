// Copyright (c) 2026, NVIDIA CORPORATION. All rights reserved.
#pragma once
#include "xr_sample.h"
#include <ovx/path_dictionary/path_dictionary.h>
#include <dlpack/dlpack.h>
namespace sample
{
struct TransformWriter
{
    ovstage_instance_t* stage;
    path_dictionary_instance_t* paths;
    ovx_primpath_list_t list = OVX_INVALID_PRIMPATH_LIST;
    ovstage_query_handle_t query = OVSTAGE_INVALID_QUERY_HANDLE;

    explicit TransformWriter(ovstage_instance_t* source) : stage(source), paths(ovstage_get_path_dictionary(source)) {}
    void open()
    {
        if (!paths)
            throw std::runtime_error("Stage has no path dictionary");
        constexpr char path[] = "/World/AnimatedCube";
        ovx_string_t name{ path, sizeof(path) - 1 };
        auto result = path_dictionary_create_path_list_from_strings(paths, &name, 1, &list);
        if (result.status != OVX_API_SUCCESS)
        {
            const auto error = sample::string(result.error);
            path_dictionary_release_error(paths, result.error);
            throw std::runtime_error("create_path_list: " + error);
        }
        sample::checkStage(ovstage_query_from_path_list(stage, list, &query), "query_from_path_list");
    }
    ~TransformWriter()
    {
        try
        {
            if (query != OVSTAGE_INVALID_QUERY_HANDLE)
                sample::wait(stage, ovstage_release_query(stage, query));
        }
        catch (const std::exception& error) { sample::message(error.what()); }
        if (list != OVX_INVALID_PRIMPATH_LIST)
            path_dictionary_release_path_list_reference(paths, list);
    }
    void write(sample::Matrix& matrix, ovstage_ordinal_t ordinal)
    {
        // Canonical CPU transport: one 16-lane double matrix per prim.
        int64_t shape[] = { 1 };
        DLTensor tensor{};
        tensor.data = matrix.data();
        tensor.device = { kDLCPU, 0 };
        tensor.ndim = 1;
        tensor.dtype = { kDLFloat, 64, 16 };
        tensor.shape = shape;
        ovstage_write_data_t data{};
        data.tensors = &tensor;
        data.tensor_count = 1;
        data.is_array = false;
        data.semantic = OVSTAGE_SEMANTIC_MATRIX;
        constexpr char attribute[] = "omni:xform";
        ovx_token_or_string_t name{};
        name.string = { attribute, sizeof(attribute) - 1 };
        sample::wait(stage, ovstage_write_attribute(stage, query, name, ordinal, data, OVSTAGE_PRIM_MODE_UPSERT));
    }
};
}
