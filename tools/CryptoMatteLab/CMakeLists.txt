# Cryptomatte multi-ID / coverage native pass R&D lab.
#
# The lab is a read-only measurement harness. It links ArtifactCore and
# exercises the shipped Cryptomatte API, but it never modifies it. Its second
# half is an independent specification reference model, so a mismatch between
# the two is evidence rather than a restatement of the same code.
#
# The lab is excluded from the default build because it is a research tool, not
# a shipping target. Enable it with:
#   cmake -DARTIFACT_BUILD_CRYPTOMATTE_LAB=ON

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.30)
    project(CryptoMatteLabStandalone LANGUAGES CXX)
    find_package(Qt6 REQUIRED COMPONENTS Core)
    return()
endif()

if(NOT TARGET ArtifactCore)
    message(FATAL_ERROR
        "ArtifactCryptoMatteLab must be configured from the ArtifactStudio root project")
endif()

# The lab reads the EXR header back with OpenImageIO to check the written
# metadata and channel names mechanically. ArtifactCore already exposes
# OpenImageIO publicly, but the lab declares it explicitly rather than relying
# on a transitive link.
find_package(OpenImageIO REQUIRED)

add_executable(ArtifactCryptoMatteLab EXCLUDE_FROM_ALL
    main.cpp
    crypto_spec_reference.cpp
)

# The lab consumes ArtifactCore's Cryptomatte modules directly. ArtifactCore
# exports them through a PUBLIC FILE_SET CXX_MODULES, so the module
# interfaces are importable from this target without a separate wrapper.
target_link_libraries(ArtifactCryptoMatteLab
    PRIVATE
        ArtifactCore
        OpenImageIO::OpenImageIO
        Qt6::Core
)

target_include_directories(ArtifactCryptoMatteLab PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

set_target_properties(ArtifactCryptoMatteLab PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
)

if(ARTIFACT_FRONTEND_MSVC)
    target_compile_options(ArtifactCryptoMatteLab PRIVATE
        /Zf /Zc:__cplusplus /utf-8 /MP)
endif()
