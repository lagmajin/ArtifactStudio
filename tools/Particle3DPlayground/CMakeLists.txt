cmake_minimum_required(VERSION 3.30)

if(NOT TARGET ArtifactAppRuntime)
    message(FATAL_ERROR
        "ArtifactParticle3DPlayground must be configured from the ArtifactStudio root project")
endif()

add_executable(ArtifactParticle3DPlayground EXCLUDE_FROM_ALL main.cpp)
set_target_properties(ArtifactParticle3DPlayground PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
)

target_link_libraries(ArtifactParticle3DPlayground PRIVATE ArtifactAppRuntime)

if(MSVC)
    target_compile_options(ArtifactParticle3DPlayground PRIVATE
        /Zc:__cplusplus /utf-8 /permissive- /wd5050)
endif()
