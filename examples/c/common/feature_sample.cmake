# Copyright (c) 2026, NVIDIA CORPORATION. All rights reserved.
# Included by the feature examples after their optional standalone setup.
add_executable(${PROJECT_NAME} main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE ovxr::ovxr ovrtx::ovrtx ovstage::ovstage)
ovrtx_setup_runtime(${PROJECT_NAME})
ovstage_setup_runtime(${PROJECT_NAME})
ovxr_setup_runtime(${PROJECT_NAME})
# Refresh scene edits even when no C++ source needs recompiling.
add_custom_target(${PROJECT_NAME}-assets ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_LIST_DIR}/../../assets"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/assets"
    VERBATIM)
add_dependencies(${PROJECT_NAME} ${PROJECT_NAME}-assets)
