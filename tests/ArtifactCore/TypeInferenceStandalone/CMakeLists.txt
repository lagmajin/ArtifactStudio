cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreTypeInferenceStandalone LANGUAGES CXX)

find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(TypeInferenceModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    MODULES
        "${ARTIFACT_CORE_ROOT}/src/Core/ArtifactOptional.cppm"
        "${ARTIFACT_CORE_ROOT}/include/Core/ArtifactArray.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Core/ArtifactString.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Data/ColumnType.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Data/TypeInference.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
)

artifact_add_standalone_gtest(ArtifactCoreTypeInferenceStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../TypeInferenceStandaloneTest.cpp"
    LINK_LIBS TypeInferenceModuleInterfaces
    LABELS "core;data;type-inference;cpu;contract;standalone"
)

enable_testing()
