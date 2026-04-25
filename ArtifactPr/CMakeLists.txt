cmake_minimum_required(VERSION 3.30)

set(ARTIFACT_PR_SOURCES
    src/main.cpp
    src/ArtifactPrMainWindow.cpp
    src/ArtifactPrDemoData.cpp
    src/ArtifactPrEditorEngine.cpp
)

add_executable(ArtifactPr WIN32 ${ARTIFACT_PR_SOURCES})

set_target_properties(ArtifactPr PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
)

target_link_libraries(ArtifactPr PRIVATE ArtifactCore ArtifactWidgets Qt6::Multimedia)

target_include_directories(ArtifactPr PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

if(ARTIFACT_COMPILER_MSVC_CL)
    target_compile_options(ArtifactPr PRIVATE /Zf /Zc:__cplusplus /utf-8 /wd5050)
    target_compile_definitions(ArtifactPr PRIVATE
        PLATFORM_WIN32
        _GUARDOVERFLOW_CRT_ALLOCATORS=1
        _SILENCE_CXX23_ALIGNED_STORAGE_DEPRECATION_WARNING
        _WIN32_WINNT=0x0A00
    )
    target_compile_options(ArtifactPr PRIVATE /WX-)
elseif(ARTIFACT_FRONTEND_MSVC)
    target_compile_options(ArtifactPr PRIVATE /Zc:__cplusplus /utf-8 /WX- /wd5050)
    target_compile_definitions(ArtifactPr PRIVATE
        PLATFORM_WIN32
        _GUARDOVERFLOW_CRT_ALLOCATORS=1
        _SILENCE_CXX23_ALIGNED_STORAGE_DEPRECATION_WARNING
        _WIN32_WINNT=0x0A00
    )
endif()

find_package(qtadvanceddocking-qt6 REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Multimedia MultimediaWidgets)
target_link_libraries(ArtifactPr PRIVATE ads::qtadvanceddocking-qt6 Qt6::Multimedia Qt6::MultimediaWidgets)
