// SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: LicenseRef-NvidiaProprietary
#pragma once

#include <filesystem>
#include <string>
#include <system_error>

#if defined(_WIN32)
#    if !defined(NOMINMAX)
#        define NOMINMAX
#    endif
#    include <windows.h>
#endif

namespace ovxr_sample
{
// Use the running executable, including when invoked through PATH or a symlink.
// argv[0] and the working directory need not identify the installed program.
inline std::filesystem::path executableDirectory()
{
#if defined(_WIN32)
    for (size_t capacity = 256; capacity <= 32768; capacity *= 2)
    {
        std::wstring filename(capacity, L'\0');
        const DWORD length = GetModuleFileNameW(nullptr, filename.data(), static_cast<DWORD>(filename.size()));
        if (length == 0)
            return {};
        if (length < filename.size())
        {
            filename.resize(length);
            return std::filesystem::path(filename).parent_path();
        }
    }
    return {};
#else
    std::error_code error;
    const auto filename = std::filesystem::read_symlink("/proc/self/exe", error);
    return error ? std::filesystem::path{} : filename.parent_path();
#endif
}
} // namespace ovxr_sample
