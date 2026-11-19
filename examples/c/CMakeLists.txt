# SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: LicenseRef-NvidiaProprietary

cmake_minimum_required(VERSION 3.19)
project(ovxr-examples LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Shared FetchContent directory so multiple examples download one package copy.
set(FETCHCONTENT_BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/_deps" CACHE PATH "Shared FetchContent directory")

# ovxr layers on the ovrtx and ovstage packages: fetch the siblings first so
# ovxrConfig.cmake's find_dependency(ovrtx)/find_dependency(ovstage) resolves.
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")
include(ovrtx)
include(ovstage)
include(ovxr)
ovrtx_fetch()
ovstage_fetch()
ovxr_fetch()

add_subdirectory(minimal-xr)
add_subdirectory(live-scene)
add_subdirectory(anchors-and-teleport)
add_subdirectory(foveation)
add_subdirectory(ar-blending)
add_subdirectory(view-configuration)
add_subdirectory(async-capture)

# Basic examples remain buildable with just the three public SDK packages.
# The four support-dependent examples additionally consume OpenXR/Vulkan headers.
include(sample_support_headers)
if(SAMPLE_OPENXR_INCLUDE_DIR AND SAMPLE_VULKAN_INCLUDE_DIR)
    add_subdirectory(sample-support)
    add_subdirectory(frame-capture)
    add_subdirectory(runtime-lifecycle)
    add_subdirectory(spatial-workbench)
    add_subdirectory(composition-layers)
else()
    message(WARNING "Skipping frame-capture, runtime-lifecycle, spatial-workbench and composition-layers: "
        "OpenXR (1.1.53+) or Vulkan headers not found. Set SAMPLE_OPENXR_INCLUDE_DIR and SAMPLE_VULKAN_INCLUDE_DIR.")
endif()
