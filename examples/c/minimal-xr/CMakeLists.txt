# SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: LicenseRef-NvidiaProprietary

# Standalone entry point: allow configuring this example directly, without the
# parent examples/ project.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.19)
    project(minimal-xr LANGUAGES CXX)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../cmake")
    include(ovrtx)
    include(ovstage)
    include(ovxr)
    ovrtx_fetch()
    ovstage_fetch()
    ovxr_fetch()
endif()

add_executable(minimal-xr main.cpp)

# The example drives all three C APIs directly: ovstage for the scene, ovrtx
# for the renderer, ovxr for the OpenXR session. Dynamic targets throughout:
# ovrtx's runtime model #2 anchors the shared USD/fabric runtime beside the
# exe, ovstage delay-loads into it, and ovxr's shim loads its implementation
# payload from the same directory at ovxr_create() time.
target_link_libraries(minimal-xr PRIVATE ovxr::ovxr ovrtx::ovrtx ovstage::ovstage)

ovrtx_setup_runtime(minimal-xr)
ovstage_setup_runtime(minimal-xr)
ovxr_setup_runtime(minimal-xr)
