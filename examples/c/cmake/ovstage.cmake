# SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: LicenseRef-NvidiaProprietary
#
# NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
# property and proprietary rights in and to this material, related
# documentation and any modifications thereto. Any use, reproduction,
# disclosure or distribution of this material and related documentation
# without an express license agreement from NVIDIA CORPORATION or
# its affiliates is strictly prohibited.

# ovstage.cmake - Fetch and configure the ovstage library as its own package
#
# ovstage ships an independent, self-contained C++ package (headers + import lib
# + shared library + CMake config) parallel to ovrtx. This module fetches that
# package and exposes both the dynamic ovstage::ovstage and the static
# ovstage::ovstage_static imported targets via find_package.
#
# Usage:
#   list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../cmake")
#   include(ovrtx)
#   include(ovstage)
#   ovrtx_fetch()
#   ovstage_fetch()
#
#   add_executable(myapp main.cpp)
#   # Static loaders for both packages (the sample's path): each package is
#   # exposed as a single side-by-side link (ovrtx/ and ovstage/) and main.cpp
#   # hands each loader its own binary package root.
#   target_link_libraries(myapp PRIVATE ovrtx::ovrtx_static ovstage::ovstage_static)
#   # Dynamic ovstage is also supported: link ovstage::ovstage instead and
#   # ovstage_setup_runtime replicates the dll + schemas/plugins beside the exe.
#   ovrtx_setup_runtime(myapp)
#   ovstage_setup_runtime(myapp)
#
# Attach mode requires ONE shared omni.fabric/USD runtime; see the
# ovstage_setup_runtime() doc below for how that single-runtime guarantee is met.

# Capture this file's directory at parse time (before macro expansion);
# CMAKE_CURRENT_LIST_DIR inside a macro would refer to the caller's directory.
set(_OVSTAGE_CMAKE_DIR "${CMAKE_CURRENT_LIST_DIR}")

# ovstage_setup_runtime detects the ovrtx linking model via helpers defined in
# ovrtx.cmake (ovstage is always used in attach mode alongside ovrtx). Pull them
# in if a project included ovstage without ovrtx first.
if(NOT COMMAND _ovrtx_target_uses_static_loader)
    include(ovrtx OPTIONAL)
endif()

# Detect whether TARGET links the static ovstage loader (ovstage::ovstage_static).
# The examples link it directly before calling ovstage_setup_runtime, so it shows
# up in LINK_LIBRARIES. When present we deploy ovstage as a single side-by-side
# ovstage/ link (mirroring ovrtx model #1) instead of the dynamic copy/junction
# path. Mirrors _ovrtx_target_uses_static_loader.
function(_ovstage_target_uses_static_loader TARGET_NAME OUT_VAR)
    get_target_property(_libs ${TARGET_NAME} LINK_LIBRARIES)
    if(_libs AND "ovstage::ovstage_static" IN_LIST _libs)
        set(${OUT_VAR} TRUE PARENT_SCOPE)
    else()
        set(${OUT_VAR} FALSE PARENT_SCOPE)
    endif()
endfunction()

# Fetch the ovstage package
macro(ovstage_fetch)
    find_package(ovstage QUIET)

    if (ovstage_FOUND)
        message(STATUS "found ovstage at: ${ovstage_DIR}")
    else()
        include("${_OVSTAGE_CMAKE_DIR}/ovxr.cmake")
        ovxr_fetch_dependency(ovstage)
    endif()
endmacro()

# Setup ovstage runtime dependencies for a target. Two models, selected from
# whether TARGET links ovstage::ovstage_static:
#
#  * STATIC loader (ovstage::ovstage_static) - the sample's path. Mirrors ovrtx
#    model #1: expose the package as a SINGLE link `ovstage/` next to the exe
#    (junction on Windows, symlink on Linux) -> the package bin/, which main.cpp
#    hands to ovstage_initialize() as the binary package root. The static loader
#    loads ovstage.dll from that link on the first ovstage call and self-locates
#    its own plugins/ + ovstage_usd_schemas/ under it, so there is NO dll copy, NO
#    /DELAYLOAD, and NO separate schemas/plugins junction. The dll still loads
#    lazily (after ovrtx_create_renderer loaded the shared usd_ms/tbb), so imports
#    dedupe by base name - one usd_ms, one tbb, one ovstage.
#
#  * DYNAMIC (ovstage::ovstage). ovstage ships an import lib (ovstage.lib) for
#    ovstage.dll and has no "binary root" config (ovstage_instance_desc_t carries
#    no package root). Unlike the static loader we cannot tell ovstage where its
#    package is - ovstage.dll self-locates its bundled carb plugins (omni.fabric /
#    usdrt.* / gpucompute / ...) RELATIVE TO THE DIRECTORY ovstage.dll IS LOADED
#    FROM, i.e. it expects a sibling plugins/ tree. Preserve that relative layout
#    when deploying the application.
#
# So (dynamic only) we copy ovstage.dll next to the exe (its module dir becomes the
# exe dir) and junction its data-only ovstage_usd_schemas/ beside it. The plugins/
# name is shared with ovrtx, so its treatment depends on the ovrtx model (detected
# via ovrtx::ovrtx_static on the target):
#   - Model #1: ovrtx's runtime stays under the single `ovrtx/` link, so <exe>/plugins
#     is free and we junction ovstage's own plugins/ closure there.
#   - Model #2: ovrtx replicates its plugins/ at the exe root, so <exe>/plugins is
#     an executable-owned root with linked subdirectories. ovstage.dll shares that
#     tree for the common Fabric/USD closure, but OVStage's population plugin
#     ships only in the ovstage SDK, so we copy it there without modifying either
#     source package.
#
# Single usd_ms (why delay-load): ovstage.dll STATICALLY imports ov_25.11usd_ms.dll
# and tbb12.dll (see `dumpbin /dependents ovstage.dll`), which the OS loader would
# resolve the instant ovstage.dll loads. A normal load-time import would load at
# process init - before ovrtx has loaded usd_ms, and while the junctioned plugins/
# is not on any DLL search path - and fail. We therefore DELAY-LOAD ovstage.dll: it
# loads on the first ovstage_* call, which in attach mode is always AFTER
# ovrtx_create_renderer has loaded the single usd_ms (and tbb12) from the ovrtx
# package. ovstage.dll's static imports then bind to those already-loaded modules by
# base name - one usd_ms, one tbb, one ovstage. (The static loader gets the same
# single-usd_ms guarantee for free: it loads ovstage.dll lazily on the first call.)
#
# NOTE: matched-train assumption (one usd_ms ABI shared by ovrtx + ovstage). The
# open item is that ovrtx (its package) and ovstage (junctioned here) each carry a
# carb plugin set; usd_ms/tbb dedupe by name, but a single Fabric/USD runtime in
# attach mode still needs on-hardware confirmation (Linux rpath path too).
#
# OVSTAGE_BINARY_DIR is exported by ovstageConfig.cmake (the package's bin/ dir).
function(ovstage_setup_runtime TARGET_NAME)
    if(NOT DEFINED OVSTAGE_BINARY_DIR)
        message(FATAL_ERROR "OVSTAGE_BINARY_DIR is not set; call ovstage_fetch() before ovstage_setup_runtime()")
    endif()

    # Static loader (ovstage::ovstage_static): a single side-by-side link to the
    # package bin, exactly like ovrtx model #1. main.cpp passes <exe>/ovstage to
    # ovstage_initialize(); the loader loads ovstage.dll and self-locates the rest,
    # so no dll copy / delay-load / schemas / plugins junction is needed here.
    _ovstage_target_uses_static_loader(${TARGET_NAME} _ovstage_uses_static)
    if(_ovstage_uses_static)
        if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
            _ovrtx_add_dir_junction(${TARGET_NAME}
                "$<TARGET_FILE_DIR:${TARGET_NAME}>/ovstage" "${OVSTAGE_BINARY_DIR}"
                "Junctioning the ovstage package bin beside the exe as ovstage/")
        else()
            # -sfnT replaces an existing symlink in place and refuses to descend
            # into a real directory named ovstage, keeping rebuilds idempotent.
            add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
                COMMAND ln -sfnT "${OVSTAGE_BINARY_DIR}" "$<TARGET_FILE_DIR:${TARGET_NAME}>/ovstage"
                COMMENT "Symlinking the ovstage package bin beside the exe as ovstage/"
            )
        endif()
        return()
    endif()

    # --- Dynamic ovstage::ovstage below ---
    # Under ovrtx model #2 ovstage shares the exe root's plugins/ and adds its
    # population plugin. Only under model #1 do we junction ovstage's own plugins/.
    _ovrtx_target_uses_static_loader(${TARGET_NAME} _ovstage_static_model)

    if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
        # /DELAYLOAD + delayimp are MSVC/clang-cl features; a non-MSVC Windows
        # toolchain (MinGW) would need its own lazy-load mechanism.
        if(MSVC)
            target_link_options(${TARGET_NAME} PRIVATE "/DELAYLOAD:ovstage.dll")
            target_link_libraries(${TARGET_NAME} PRIVATE delayimp)
        else()
            message(WARNING "ovstage_setup_runtime: non-MSVC Windows toolchain detected; "
                            "ovstage.dll is not delay-loaded and may load a second usd_ms at "
                            "process init. Use MSVC/clang-cl, or load ovstage lazily yourself.")
        endif()

        # Copy ovstage.dll next to the exe so its module directory is the exe dir.
        add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                # Newer SDKs import through a separate thin loader; retain it
                # alongside the implementation when staging the sample.
                "$<TARGET_FILE:ovstage::ovstage>"
                "${OVSTAGE_BINARY_DIR}/ovstage.dll"
                "$<TARGET_FILE_DIR:${TARGET_NAME}>"
            COMMENT "Copying ovstage.dll to build directory"
        )

        # Junction the data-only usd schemas always (unique name), and ovstage's own
        # plugins/ closure only under model #1. Model #2 shares ovrtx's plugins/,
        # which lacks the population plugin ovstage.dll loads from
        # <module dir>/plugins, so copy that one file into the exe-owned root.
        set(OVSTAGE_RUNTIME_DIRS ovstage_usd_schemas)
        if(_ovstage_static_model)
            list(APPEND OVSTAGE_RUNTIME_DIRS plugins)
        elseif(EXISTS "${OVSTAGE_BINARY_DIR}/plugins/ovstage.population.plugin.dll")
            # Older ovstage SDKs ship population as plugins/usdrt, which the
            # ovrtx plugins/ tree already provides.
            add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${OVSTAGE_BINARY_DIR}/plugins/ovstage.population.plugin.dll"
                    "$<TARGET_FILE_DIR:${TARGET_NAME}>/plugins/ovstage.population.plugin.dll"
                COMMENT "Copying the OVStage population plugin beside the shared runtime"
                VERBATIM
            )
        endif()
        foreach(DIR ${OVSTAGE_RUNTIME_DIRS})
            if(EXISTS "${OVSTAGE_BINARY_DIR}/${DIR}")
                _ovrtx_add_dir_junction(${TARGET_NAME}
                    "$<TARGET_FILE_DIR:${TARGET_NAME}>/${DIR}" "${OVSTAGE_BINARY_DIR}/${DIR}"
                    "Junctioning ovstage ${DIR} beside the exe")
            endif()
        endforeach()
    else()
        # Linux consumes ovstage in place: libovstage.so is self-describing
        # (RUNPATH=$ORIGIN:$ORIGIN/plugins set by patchelf at build time), so an
        # rpath entry pointing at the package bin/ lets the executable find
        # libovstage.so there, and the .so then resolves its own plugins/ closure
        # relative to $ORIGIN inside the package - no copy/junction needed. See the
        # function header for the single-Fabric runtime-validation note.
        set_property(TARGET ${TARGET_NAME} APPEND PROPERTY
            BUILD_RPATH "${OVSTAGE_BINARY_DIR}")
        set_property(TARGET ${TARGET_NAME} APPEND PROPERTY
            INSTALL_RPATH "${OVSTAGE_BINARY_DIR}")
    endif()
endfunction()
