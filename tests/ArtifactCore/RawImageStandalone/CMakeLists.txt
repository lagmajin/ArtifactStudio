cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreRawImageStandalone LANGUAGES CXX)

find_package(GTest CONFIG REQUIRED)
find_package(Qt6 CONFIG REQUIRED COMPONENTS Core)
find_package(OpenCV REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(RawImageModuleInterfaces
    BASE_DIRS
        "${ARTIFACT_CORE_ROOT}/include"
        "${ARTIFACT_CORE_ROOT}/src"
    MODULES
        "${ARTIFACT_CORE_ROOT}/src/Memory/SharedPtr.cppm"
        "${ARTIFACT_CORE_ROOT}/include/Image/RawImage.ixx"
    INCLUDE_DIRS
        "${ARTIFACT_CORE_ROOT}/include"
        ${OpenCV_INCLUDE_DIRS}
    LINK_LIBS Qt6::Core ${OpenCV_LIBS}
)

artifact_add_standalone_module_implementation(RawImageUnderTest
    SOURCES "${ARTIFACT_CORE_ROOT}/src/Image/RawImage.cppm"
    LINK_LIBS RawImageModuleInterfaces Qt6::Core ${OpenCV_LIBS}
    INCLUDE_DIRS
        "${ARTIFACT_CORE_ROOT}/include"
        ${OpenCV_INCLUDE_DIRS}
)

if(MSVC)
    add_dependencies(RawImageUnderTest RawImageModuleInterfaces)
endif()

artifact_add_standalone_gtest(ArtifactCoreRawImageContractTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../RawImageContractTest.cpp"
    LINK_LIBS RawImageUnderTest RawImageModuleInterfaces Qt6::Core ${OpenCV_LIBS}
    LABELS "image;raw-image;cpu;contract;standalone"
)

enable_testing()
