if(NOT DEFINED dll_list_file OR NOT EXISTS "${dll_list_file}")
    message(FATAL_ERROR "CopyRuntimeDlls.cmake requires an existing runtime DLL list file.")
endif()
if(NOT DEFINED destination OR NOT IS_DIRECTORY "${destination}")
    message(FATAL_ERROR "CopyRuntimeDlls.cmake requires an existing destination directory.")
endif()

file(STRINGS "${dll_list_file}" _runtime_dlls)

foreach(_runtime_dll IN LISTS _runtime_dlls)
    if(EXISTS "${_runtime_dll}")
        get_filename_component(_runtime_dll_name "${_runtime_dll}" NAME)
        file(COPY_FILE "${_runtime_dll}"
            "${destination}/${_runtime_dll_name}" ONLY_IF_DIFFERENT)
    endif()
endforeach()
