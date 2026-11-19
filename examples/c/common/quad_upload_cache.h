// Copyright (c) 2026, NVIDIA CORPORATION. All rights reserved.
#pragma once
#include <cstdint>
#include <cstring>
#include <vector>

namespace sample
{
// Track the pixels actually released to XR, not a borrowed caller buffer.
// Record only after a successful upload. Readback probes always upload again.
class QuadUploadCache
{
    uint32_t width = 0, height = 0;
    std::vector<uint8_t> pixels;
public:
    bool needsUpload(const void* rgba, uint32_t w, uint32_t h, bool readback) const
    {
        const auto bytes = size_t(w) * h * 4;
        return readback || width != w || height != h || pixels.size() != bytes ||
            std::memcmp(pixels.data(), rgba, bytes) != 0;
    }
    void record(const void* rgba, uint32_t w, uint32_t h)
    {
        const auto* source = static_cast<const uint8_t*>(rgba);
        pixels.assign(source, source + size_t(w) * h * 4);
        width = w;
        height = h;
    }
};
} // namespace sample
