# Burn Away effect image-evaluation lab.
#
# The lab renders the shipped Burn Away effect over a synthetic paper surface
# and writes PNG frames plus alpha statistics, so the burn look can be judged
# from images instead of from code reading.  It never modifies product code.
#
# The lab is excluded from the default build because it is a research tool, not
# a shipping target. Enable it with:
#   cmake -DARTIFACT_BUILD_BURN_AWAY_LAB=ON
# and build the target:
#   cmake --build <tree> --target ArtifactBurnAwayLab

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.30)
    project(BurnAwayLabStandalone LANGUAGES CXX)
    find_package(Qt6 REQUIRED COMPONENTS Core)
    return()
endif()

if(NOT TARGET ArtifactEffectsStylize)
    message(FATAL_ERROR
        "ArtifactBurnAwayLab must be configured from the ArtifactStudio root project")
endif()

# OpenImageIO writes the PNG frames.  The lab declares it explicitly instead
# of relying on a transitive link.
find_package(OpenImageIO REQUIRED)

# The lab is added before the root project reaches its own Qt6 lookup, so the
# components it needs are requested here instead of relying on that later call.
find_package(Qt6 REQUIRED COMPONENTS Core Gui)

add_executable(ArtifactBurnAwayLab EXCLUDE_FROM_ALL
    main.cpp
)

# The lab imports the effect module directly.  ArtifactEffectsStylize exports
# it through a PUBLIC FILE_SET CXX_MODULES.  The particle layer lives in the
# application runtime, which the lab links to render ash and embers.
target_link_libraries(ArtifactBurnAwayLab
    PRIVATE
        ArtifactEffectsStylize
        ArtifactAppRuntime
        OpenImageIO::OpenImageIO
        Qt6::Core
        Qt6::Gui
)

set_target_properties(ArtifactBurnAwayLab PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
)

if(ARTIFACT_FRONTEND_MSVC)
    target_compile_options(ArtifactBurnAwayLab PRIVATE
        /Zf /Zc:__cplusplus /utf-8 /MP)
endif()
