cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreRawImageStandalone LANGUAGES CXX)

find_package(GTest CONFIG REQUIRED)
find_package(Qt6 CONFIG REQUIRED COMPONENTS Core)
find_package(OpenCV REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")
set(ARTIFACT_CORE_INCLUDE "${ARTIFACT_CORE_ROOT}/include")

if(NOT TARGET ArtifactCoreStandaloneFoundationModules)
    artifact_add_standalone_module_interface(ArtifactCoreStandaloneFoundationModules
        BASE_DIRS "${ARTIFACT_CORE_ROOT}"
        MODULES
            "${ARTIFACT_CORE_INCLUDE}/Container/ContainerDebug.ixx"
            "${ARTIFACT_CORE_INCLUDE}/Container/ContainerDebugRegistry.ixx"
            "${ARTIFACT_CORE_ROOT}/src/Core/ArtifactOptional.cppm"
            "${ARTIFACT_CORE_INCLUDE}/Core/ArtifactArray.ixx"
            "${ARTIFACT_CORE_INCLUDE}/Core/ArtifactString.ixx"
            "${ARTIFACT_CORE_INCLUDE}/Container/NamedVector.ixx"
            "${ARTIFACT_CORE_ROOT}/src/Memory/SharedPtr.cppm"
        INCLUDE_DIRS "${ARTIFACT_CORE_INCLUDE}"
    )
endif()

artifact_add_standalone_module_interface(RawImageModuleInterfaces
    BASE_DIRS
        "${ARTIFACT_CORE_ROOT}"
    MODULES
        "${ARTIFACT_CORE_ROOT}/include/Image/RawImage.ixx"
    INCLUDE_DIRS
        "${ARTIFACT_CORE_INCLUDE}"
        ${OpenCV_INCLUDE_DIRS}
    LINK_LIBS ArtifactCoreStandaloneFoundationModules Qt6::Core ${OpenCV_LIBS}
)

artifact_add_standalone_module_implementation(RawImageUnderTest
    SOURCES "${ARTIFACT_CORE_ROOT}/src/Image/RawImage.cpp"
    LINK_LIBS RawImageModuleInterfaces Qt6::Core ${OpenCV_LIBS}
    INCLUDE_DIRS
        "${ARTIFACT_CORE_INCLUDE}"
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

if(ARTIFACT_STANDALONE_GTEST_GROUP)
    artifact_finalize_standalone_gtest_group("${ARTIFACT_STANDALONE_GTEST_GROUP}")
endif()

enable_testing()
