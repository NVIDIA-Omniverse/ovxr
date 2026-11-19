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

find_package(CURL 7.85 REQUIRED)
add_executable(minimal-xr main.cpp)

# The example drives all three C APIs directly: ovstage for the scene, ovrtx
# for the renderer, ovxr for the OpenXR session. Dynamic targets throughout:
# ovrtx's runtime model #2 anchors the shared USD/fabric runtime beside the
# exe, ovstage delay-loads into it, and ovxr's shim loads its implementation
# payload from the same directory at ovxr_create() time.
target_link_libraries(minimal-xr PRIVATE ovxr::ovxr ovrtx::ovrtx ovstage::ovstage CURL::libcurl)

ovrtx_setup_runtime(minimal-xr)
ovstage_setup_runtime(minimal-xr)
ovxr_setup_runtime(minimal-xr)

# Keep the complete local scene beside every configuration of the executable.
# Refresh asset-only edits even when main.cpp does not need recompiling.
add_custom_target(minimal-xr-assets ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_LIST_DIR}/../../assets"
        "$<TARGET_FILE_DIR:minimal-xr>/assets"
    VERBATIM)
add_dependencies(minimal-xr minimal-xr-assets)
