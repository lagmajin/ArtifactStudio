add_executable(ArtifactVideoPreviewBench EXCLUDE_FROM_ALL
    main.cpp
)

target_link_libraries(ArtifactVideoPreviewBench
    PRIVATE
        ArtifactAppRuntime
)

set_target_properties(ArtifactVideoPreviewBench PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
)

if(MSVC)
    target_compile_options(ArtifactVideoPreviewBench PRIVATE /Zf /Zc:__cplusplus /utf-8 /MP)
endif()

if(WIN32)
    add_custom_command(TARGET ArtifactVideoPreviewBench POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            $<TARGET_RUNTIME_DLLS:ArtifactVideoPreviewBench>
            "$<TARGET_FILE_DIR:ArtifactVideoPreviewBench>/"
        COMMAND_EXPAND_LISTS
        VERBATIM
    )

    find_program(ARTIFACT_VIDEO_BENCH_WINDEPLOYQT_EXECUTABLE NAMES windeployqt)
    if(ARTIFACT_VIDEO_BENCH_WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET ArtifactVideoPreviewBench POST_BUILD
            COMMAND "${CMAKE_COMMAND}"
                "-DWINDEPLOYQT_EXECUTABLE=${ARTIFACT_VIDEO_BENCH_WINDEPLOYQT_EXECUTABLE}"
                "-DTARGET_FILE=$<TARGET_FILE:ArtifactVideoPreviewBench>"
                "-DVCPKG_INSTALLED=${VCPKG_INSTALLED}"
                "-DCONFIG=$<CONFIG>"
                -P "${CMAKE_SOURCE_DIR}/Artifact/cmake/RunWindeployqt.cmake"
            VERBATIM
        )
    endif()

    add_custom_command(TARGET ArtifactVideoPreviewBench POST_BUILD
        COMMAND "${CMAKE_COMMAND}"
            "-DTARGET_FILE_DIR=$<TARGET_FILE_DIR:ArtifactVideoPreviewBench>"
            "-DVCPKG_INSTALLED=${VCPKG_INSTALLED}"
            "-DCONFIG=$<CONFIG>"
            -P "${CMAKE_SOURCE_DIR}/Artifact/cmake/CopyQtPlatformPlugin.cmake"
        VERBATIM
    )
endif()
