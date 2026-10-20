# SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: LicenseRef-NvidiaProprietary
#
# NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
# property and proprietary rights in and to this material, related
# documentation and any modifications thereto. Any use, reproduction,
# disclosure or distribution of this material and related documentation
# without an express license agreement from NVIDIA CORPORATION or
# its affiliates is strictly prohibited.

# ovxr.cmake - Locate the ovxr package (and its sibling ovrtx/ovstage packages)
#
# ovxr is a thin package layered on the ovrtx and ovstage packages: install the
# three side by side and put all three prefixes on CMAKE_PREFIX_PATH.
# ovxrConfig.cmake pulls the siblings in via find_dependency(), so a single
# find_package(ovxr) resolves the whole stack.
#
# Usage:
#   list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../cmake")
#   include(ovxr)
#   ovxr_fetch()
#
#   add_executable(myapp main.cpp)
#   target_link_libraries(myapp PRIVATE ovxr::ovxr ovrtx::ovrtx ovstage::ovstage)
#   ovxr_setup_runtime(myapp)

# Capture this file's directory at parse time (before macro expansion)
set(_OVXR_CMAKE_DIR "${CMAKE_CURRENT_LIST_DIR}")

# Locate an ovxr package, downloading the pinned release if none is found
macro(ovxr_fetch)
    find_package(ovxr QUIET)

    if (ovxr_FOUND)
        message(STATUS "found ovxr at: ${ovxr_DIR}")
    else()
        if(NOT EXISTS "${_OVXR_CMAKE_DIR}/../../../ovxr-build-info.json")
            message(FATAL_ERROR "Unpublished OVXR candidate: build it and supply matching SDKs through CMAKE_PREFIX_PATH.")
        endif()
        set(FETCHCONTENT_QUIET FALSE)

        # Override FetchContent's base directory to share large deps among examples.
        if(NOT DEFINED CACHE{FETCHCONTENT_BASE_DIR})
            set(FETCHCONTENT_BASE_DIR "${_OVXR_CMAKE_DIR}/../_deps" CACHE PATH "Shared FetchContent directory")
        endif()

        # Platform-specific package selection.
        if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
            set(OVXR_PACKAGE_SYSTEM "windows-x86_64")
        elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
            if (CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
                set(OVXR_PACKAGE_SYSTEM "manylinux_2_35_aarch64")
            elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
                set(OVXR_PACKAGE_SYSTEM "manylinux_2_35_x86_64")
            else()
                message(FATAL_ERROR "Unsupported system: ${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_PROCESSOR}")
            endif()
        else()
            message(FATAL_ERROR "Unsupported system: ${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_PROCESSOR}")
        endif()

        # The release receipt already records the exact SDK filenames and SHA256s.
        # GitHub sources include it; source ZIPs retrieve it from the same release tag.
        file(READ "${_OVXR_CMAKE_DIR}/../../../VERSION.md" OVXR_RELEASE_VERSION)
        string(STRIP "${OVXR_RELEASE_VERSION}" OVXR_RELEASE_VERSION)
        set(_OVXR_RECEIPT "${_OVXR_CMAKE_DIR}/../../../ovxr-release.json")
        if(NOT EXISTS "${_OVXR_RECEIPT}")
            file(MAKE_DIRECTORY "${FETCHCONTENT_BASE_DIR}")
            set(_OVXR_RECEIPT "${FETCHCONTENT_BASE_DIR}/ovxr-${OVXR_RELEASE_VERSION}-release.json")
            if(NOT EXISTS "${_OVXR_RECEIPT}")
                file(DOWNLOAD
                    "https://raw.githubusercontent.com/NVIDIA-Omniverse/ovxr/v${OVXR_RELEASE_VERSION}/ovxr-release.json"
                    "${_OVXR_RECEIPT}" TLS_VERIFY ON STATUS _OVXR_DOWNLOAD_STATUS
                )
                list(GET _OVXR_DOWNLOAD_STATUS 0 _OVXR_DOWNLOAD_CODE)
                if(NOT _OVXR_DOWNLOAD_CODE EQUAL 0)
                    file(REMOVE "${_OVXR_RECEIPT}")
                    message(FATAL_ERROR "Cannot download the OVXR release receipt: ${_OVXR_DOWNLOAD_STATUS}. Supply an SDK through CMAKE_PREFIX_PATH for an offline build.")
                endif()
            endif()
        endif()
        file(READ "${_OVXR_RECEIPT}" _OVXR_RELEASE)
        file(READ "${_OVXR_CMAKE_DIR}/../../../ovxr-build-info.json" _OVXR_BUILD)
        foreach(_OVXR_KEY source_sha version)
            string(JSON _OVXR_EXPECTED GET "${_OVXR_BUILD}" "${_OVXR_KEY}")
            string(JSON _OVXR_ACTUAL GET "${_OVXR_RELEASE}" "${_OVXR_KEY}")
            if(NOT _OVXR_ACTUAL STREQUAL _OVXR_EXPECTED)
                message(FATAL_ERROR "OVXR release receipt does not match this source distribution (${_OVXR_KEY}).")
            endif()
        endforeach()
        string(JSON _OVXR_ASSET_COUNT LENGTH "${_OVXR_RELEASE}" artifacts)
        math(EXPR _OVXR_LAST_ASSET "${_OVXR_ASSET_COUNT} - 1")
        set(OVXR_PACKAGE_FILENAME "")
        foreach(_OVXR_INDEX RANGE 0 ${_OVXR_LAST_ASSET})
            string(JSON _OVXR_FILENAME GET "${_OVXR_RELEASE}" artifacts ${_OVXR_INDEX} filename)
            if(_OVXR_FILENAME MATCHES "^ovxr@[^/]+[.]${OVXR_PACKAGE_SYSTEM}[.]zip$")
                set(OVXR_PACKAGE_FILENAME "${_OVXR_FILENAME}")
                string(JSON OVXR_HASH GET "${_OVXR_RELEASE}" artifacts ${_OVXR_INDEX} sha256)
                break()
            endif()
        endforeach()
        if(NOT OVXR_PACKAGE_FILENAME)
            message(FATAL_ERROR "OVXR ${OVXR_RELEASE_VERSION} has no SDK for ${OVXR_PACKAGE_SYSTEM}. Supply a compatible SDK through CMAKE_PREFIX_PATH.")
        endif()
        string(LENGTH "${OVXR_HASH}" _OVXR_HASH_LENGTH)
        if(NOT _OVXR_HASH_LENGTH EQUAL 64 OR NOT OVXR_HASH MATCHES "^[0-9a-fA-F]+$")
            message(FATAL_ERROR "The OVXR SDK receipt has an invalid SHA256 checksum.")
        endif()
        include(FetchContent)
        FetchContent_Declare(ovxr
            URL "https://github.com/NVIDIA-Omniverse/ovxr/releases/download/v${OVXR_RELEASE_VERSION}/${OVXR_PACKAGE_FILENAME}"
            URL_HASH "SHA256=${OVXR_HASH}"
        )
        FetchContent_MakeAvailable(ovxr)
        list(APPEND CMAKE_PREFIX_PATH "${ovxr_SOURCE_DIR}")
        find_package(ovxr REQUIRED)
    endif()
endmacro()

# Setup runtime dependencies for a target.
#
# Composes with the sibling modules' runtime models (call ovrtx_setup_runtime
# and ovstage_setup_runtime as well): ovrtx anchors the shared USD/fabric
# runtime beside the executable (its model #2) and ovstage shares it, so ovxr
# only has to anchor its own three binaries there too. ovxr-dynamic is a shim
# that loads the ovxr.dylib implementation payload from its OWN directory at
# ovxr_create() time; the payload's load-time imports (ovrtx-dynamic, ovstage,
# openxr_loader) then bind to the modules the application already loaded.
#
# Windows: copy the shim, the payload, and openxr_loader beside the exe
# (named files, never a *.dll glob -- OVXR_BINARY_DIR may be a shared build
# directory in the dev flow). Linux: rpath onto the package bin/; the shim
# resolves the payload via its own $ORIGIN-relative location.
function(ovxr_setup_runtime TARGET_NAME)
    if(NOT DEFINED OVXR_BINARY_DIR)
        message(FATAL_ERROR "OVXR_BINARY_DIR is not set; call ovxr_fetch() before ovxr_setup_runtime()")
    endif()

    if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
        set(_OVXR_RUNTIME_FILES
            "${OVXR_BINARY_DIR}/ovxr-dynamic.dll"
            "${OVXR_BINARY_DIR}/ovxr.dylib.dll"
            "${OVXR_BINARY_DIR}/openxr_loader.dll"
        )
        foreach(_OVXR_FILE ${_OVXR_RUNTIME_FILES})
            if(NOT EXISTS "${_OVXR_FILE}")
                message(FATAL_ERROR "ovxr runtime file not found: ${_OVXR_FILE}")
            endif()
        endforeach()
        add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${_OVXR_RUNTIME_FILES}
                "$<TARGET_FILE_DIR:${TARGET_NAME}>"
            COMMENT "Copying the ovxr shim, payload, and OpenXR loader beside the exe"
        )
        unset(_OVXR_RUNTIME_FILES)
    else()
        set_property(TARGET ${TARGET_NAME} APPEND PROPERTY
            BUILD_RPATH "${OVXR_BINARY_DIR}")
        set_property(TARGET ${TARGET_NAME} APPEND PROPERTY
            INSTALL_RPATH "${OVXR_BINARY_DIR}")
    endif()
endfunction()
