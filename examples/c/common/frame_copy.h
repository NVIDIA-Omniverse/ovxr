// Copyright (c) 2026, NVIDIA CORPORATION. All rights reserved.
#pragma once
#include "xr_sample.h"
#include <fstream>
#include <limits>
#include <vector>
#ifdef _WIN32
#    ifndef NOMINMAX
#        define NOMINMAX
#    endif
#    include <windows.h>
#    define SAMPLE_CUDA_CALL __stdcall
#else
#    include <dlfcn.h>
#    define SAMPLE_CUDA_CALL
#endif

namespace sample
{
// A small dynamically loaded CUDA Driver API subset avoids a CUDA toolkit build
// dependency. The NVIDIA display driver supplies nvcuda.dll / libcuda.so.1.
class FrameCopy
{
#ifdef _WIN32
    HMODULE library = LoadLibraryW(L"nvcuda.dll");
#else
    void* library = dlopen("libcuda.so.1", RTLD_NOW | RTLD_LOCAL);
#endif
    template <class T> T symbol(const char* name)
    {
        if (!library) throw std::runtime_error("Cannot load the NVIDIA CUDA driver");
#ifdef _WIN32
        auto address = GetProcAddress(library, name);
#else
        auto address = dlsym(library, name);
#endif
        if (!address) throw std::runtime_error(std::string("Missing CUDA driver function: ") + name);
        return reinterpret_cast<T>(address);
    }
    static void checkCuda(int result, const char* operation)
    {
        if (result) throw std::runtime_error(std::string(operation) + ": CUDA driver error " + std::to_string(result));
    }

public:
    FrameCopy() = default;
    FrameCopy(const FrameCopy&) = delete;
    FrameCopy& operator=(const FrameCopy&) = delete;
    ~FrameCopy()
    {
#ifdef _WIN32
        if (library) FreeLibrary(library);
#else
        if (library) dlclose(library);
#endif
    }
    std::vector<uint8_t> copy(const ovxr_frame_view_t& frame)
    {
        if (frame.format != OVXR_FRAME_FORMAT_RGBA8_UNORM || !frame.device_ptr || !frame.width || !frame.height ||
            uint64_t(frame.pitch_bytes) < uint64_t(frame.width) * 4 ||
            uint64_t(frame.pitch_bytes) * frame.height > std::numeric_limits<size_t>::max())
            throw std::runtime_error("Invalid RGBA8 readback layout");
        const auto init = symbol<int(SAMPLE_CUDA_CALL*)(unsigned int)>("cuInit");
        const auto pointerAttribute = symbol<int(SAMPLE_CUDA_CALL*)(void*, int, uint64_t)>("cuPointerGetAttribute");
        const auto push = symbol<int(SAMPLE_CUDA_CALL*)(void*)>("cuCtxPushCurrent_v2");
        const auto pop = symbol<int(SAMPLE_CUDA_CALL*)(void**)>("cuCtxPopCurrent_v2");
        const auto copyToHost = symbol<int(SAMPLE_CUDA_CALL*)(void*, uint64_t, size_t)>("cuMemcpyDtoH_v2");
        checkCuda(init(0), "cuInit");
        void* context = nullptr;
        // CU_POINTER_ATTRIBUTE_CONTEXT = 1. Borrow the allocation's context too;
        // both stay alive because this thread has not advanced/destroyed OVXR.
        const auto pointer = reinterpret_cast<uint64_t>(frame.device_ptr);
        checkCuda(pointerAttribute(&context, 1, pointer), "cuPointerGetAttribute");
        if (!context) throw std::runtime_error("Readback pointer has no CUDA context");
        std::vector<uint8_t> rgba(size_t(frame.pitch_bytes) * frame.height);
        checkCuda(push(context), "cuCtxPushCurrent");
        // Synchronous copy finishes before we pop or permit another XR frame.
        const auto copied = copyToHost(rgba.data(), pointer, rgba.size());
        void* previous = nullptr;
        const auto popped = pop(&previous); // Restore the caller even if the copy failed.
        checkCuda(copied, "cuMemcpyDtoH");
        checkCuda(popped, "cuCtxPopCurrent");
        return rgba;
    }
};

inline void saveBmp(const std::filesystem::path& path, const ovxr_frame_view_t& frame,
                    const std::vector<uint8_t>& rgba)
{
    const uint64_t size = uint64_t(frame.width) * frame.height * 4;
    if (!frame.width || !frame.height || frame.width > INT32_MAX || frame.height > INT32_MAX ||
        size > UINT32_MAX - 54 || uint64_t(frame.pitch_bytes) < uint64_t(frame.width) * 4 ||
        rgba.size() != uint64_t(frame.pitch_bytes) * frame.height)
        throw std::runtime_error("Image is too large or has an invalid BMP layout");
    std::ofstream file(path, std::ios::binary);
    file.exceptions(std::ios::failbit | std::ios::badbit);
    const auto little = [&file](uint32_t value, int count) {
        for (int i = 0; i < count; ++i) file.put(char((value >> (8 * i)) & 255));
    };
    file.write("BM", 2);
    little(uint32_t(size) + 54, 4); little(0, 4); little(54, 4);
    little(40, 4); little(frame.width, 4); little(uint32_t(-int32_t(frame.height)), 4);
    little(1, 2); little(32, 2); little(0, 4); little(uint32_t(size), 4);
    little(2835, 4); little(2835, 4); little(0, 4); little(0, 4);
    // Negative height selects top-down rows. BMP BI_RGB uses B,G,R,unused.
    std::vector<char> row(size_t(frame.width) * 4);
    for (uint32_t y = 0; y < frame.height; ++y)
    {
        const auto* source = rgba.data() + size_t(y) * frame.pitch_bytes;
        for (uint32_t x = 0; x < frame.width; ++x)
        {
            row[x * 4] = char(source[x * 4 + 2]);
            row[x * 4 + 1] = char(source[x * 4 + 1]);
            row[x * 4 + 2] = char(source[x * 4]);
            row[x * 4 + 3] = 0;
        }
        file.write(row.data(), std::streamsize(row.size()));
    }
    file.close();
}
} // namespace sample
#undef SAMPLE_CUDA_CALL
