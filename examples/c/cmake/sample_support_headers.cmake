# Copyright (c) 2026, NVIDIA CORPORATION. All rights reserved.
# Shared discovery for aggregate and standalone support-dependent examples.
include_guard(GLOBAL)
# Maintainer builds can opt into staged headers; public builds use installed SDKs
# or the explicit SAMPLE_*_INCLUDE_DIR cache entries.
if(OVXR_SAMPLE_DEPENDENCY_ROOT)
    set(_ovxr_openxr_hint "${OVXR_SAMPLE_DEPENDENCY_ROOT}/openxr.release/include")
    set(_ovxr_vulkan_hint "${OVXR_SAMPLE_DEPENDENCY_ROOT}/vulkan-headers/include")
endif()
find_path(SAMPLE_OPENXR_INCLUDE_DIR openxr/openxr.h HINTS "${_ovxr_openxr_hint}")
find_path(SAMPLE_VULKAN_INCLUDE_DIR vulkan/vulkan.h
    HINTS "${_ovxr_vulkan_hint}"
          "$ENV{VULKAN_SDK}/Include" "$ENV{VULKAN_SDK}/include")
