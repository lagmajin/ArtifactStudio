cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreOpenExrStandalone LANGUAGES CXX)

find_package(GTest CONFIG REQUIRED)
find_package(Qt6 CONFIG REQUIRED COMPONENTS Core)
find_package(OpenImageIO REQUIRED)
find_package(TBB REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(OpenExrModuleInterfaces
    BASE_DIRS
        "${ARTIFACT_CORE_ROOT}/include"
    MODULES
        "${ARTIFACT_CORE_ROOT}/include/Common/Parallel.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Image/DeepImageBuffer.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Image/OpenEXR.ixx"
    INCLUDE_DIRS
        "${ARTIFACT_CORE_ROOT}/include"
    LINK_LIBS Qt6::Core
)

artifact_add_standalone_module_implementation(OpenExrUnderTest
    SOURCES
        "${ARTIFACT_CORE_ROOT}/src/Core/Parallel.cppm"
        "${ARTIFACT_CORE_ROOT}/src/Image/DeepImageBuffer.cppm"
        "${ARTIFACT_CORE_ROOT}/src/Image/OpenEXR.cppm"
    LINK_LIBS OpenExrModuleInterfaces Qt6::Core OpenImageIO::OpenImageIO TBB::tbb
    INCLUDE_DIRS
        "${ARTIFACT_CORE_ROOT}/include"
)

if(MSVC)
    add_dependencies(OpenExrUnderTest OpenExrModuleInterfaces)
endif()

artifact_add_standalone_gtest(ArtifactCoreOpenExrRoundTripTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/OpenExrRoundTripTest.cpp"
    LINK_LIBS OpenExrUnderTest OpenExrModuleInterfaces Qt6::Core
    LABELS "image;openexr;io;standalone"
)

enable_testing()
