// Copyright (c) 2026, NVIDIA CORPORATION. All rights reserved.
#include "../common/xr_sample.h"
#include <ovx/path_dictionary/path_dictionary.h>
#include <dlpack/dlpack.h>

namespace
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
} // namespace

int main(int argc, char** argv)
{
    try
    {
        const auto options = sample::options(argc, argv);
        if (options.help)
            return 0;
        sample::Application app;
        app.open(argv[0], "ovxr.example.live-scene", options, true);
        sample::Progress progress{ options.frames };
        {
            TransformWriter writer(app.stage);
            writer.open();
            auto ordinal = sample::nextOrdinal(app.stage);
            bool pending = false;
            while (progress.active())
            {
                ovxr_frame_plan_t plan{};
                auto result = ovxr_begin_frame(app.context, &plan);
                if (result == OVXR_NOT_READY)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    continue;
                }
                sample::check(result, "begin_frame");
                sample::FrameGuard guard{ app.context, &plan };
                // [snippet:publish-live-scene]
                // This single thread owns the publication rendezvous. All
                // co-writers would need to share this ledger and synchronization.
                if (!pending)
                {
                    const double t = progress.submitted * sample::kDeltaTime;
                    auto matrix = sample::transform(std::sin(t), 1.3, 0, t);
                    writer.write(matrix, ordinal);
                    pending = true;
                }
                bool sealed = false;
                result = ovxr_bind_frame(app.context, &plan, sample::kDeltaTime, ordinal, &sealed);
                // Sealing can precede a render failure: never reuse that ordinal.
                if (sealed)
                {
                    ordinal = sample::nextOrdinal(app.stage);
                    pending = false;
                }
                // The guard cancels this plan. Retry a NEW plan with the same
                // unconsumed ordinal and the already queued application write.
                if (result == OVXR_NOT_READY)
                    continue;
                sample::check(result, "bind_frame");
                ovxr_frame_result_t frame{};
                sample::check(ovxr_submit_frame(app.context, &plan, nullptr, &frame), "submit_frame");
                progress.record(frame);
                // [/snippet:publish-live-scene]
            }
        }
        app.close();
        sample::message("live-scene: submitted " + std::to_string(progress.submitted) + " frames; clean shutdown.");
        return 0;
    }
    catch (const std::exception& error)
    {
        sample::message(std::string("Sample failed: ") + error.what());
        return 1;
    }
}
