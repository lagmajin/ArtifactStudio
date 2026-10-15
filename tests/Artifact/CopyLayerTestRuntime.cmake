if(NOT DEFINED TARGET_FILE_DIR OR TARGET_FILE_DIR STREQUAL "")
    message(FATAL_ERROR "TARGET_FILE_DIR is required for layer test deployment")
endif()

if(DEFINED SOURCE_RUNTIME_DIR AND EXISTS "${SOURCE_RUNTIME_DIR}")
    file(GLOB _layer_test_runtime_dlls "${SOURCE_RUNTIME_DIR}/*.dll")
    foreach(_dll IN LISTS _layer_test_runtime_dlls)
        get_filename_component(_dll_name "${_dll}" NAME)
        file(COPY_FILE "${_dll}" "${TARGET_FILE_DIR}/${_dll_name}" ONLY_IF_DIFFERENT)
    endforeach()
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}"
        "-DTARGET_FILE_DIR=${TARGET_FILE_DIR}"
        "-DVCPKG_INSTALLED=${VCPKG_INSTALLED}"
        "-DCONFIG=${CONFIG}"
        -P "${CMAKE_CURRENT_LIST_DIR}/../../Artifact/cmake/CopyQtPlatformPlugin.cmake"
    RESULT_VARIABLE _plugin_copy_result
)
if(NOT _plugin_copy_result EQUAL 0)
    message(FATAL_ERROR "Failed to deploy Qt plugins for layer test")
endif()
