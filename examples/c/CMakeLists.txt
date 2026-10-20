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
