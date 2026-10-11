cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreDiagnosticRecorderStandalone LANGUAGES CXX)

find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(DiagnosticRecorderModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}"
    MODULES
        "${ARTIFACT_CORE_ROOT}/src/Core/ArtifactOptional.cppm"
        "${ARTIFACT_CORE_ROOT}/include/Core/ArtifactArray.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Core/ArtifactString.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Utils/Result.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Utils/Optional.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Container/ContainerDebug.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Container/ContainerDebugRegistry.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Container/NamedVector.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Diagnostics/CoreDiagnosticSnapshot.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Diagnostics/CoreDiagnosticRecorder.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
)

artifact_add_standalone_gtest(ArtifactCoreDiagnosticRecorderStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../DiagnosticRecorderStandaloneTest.cpp"
    LINK_LIBS DiagnosticRecorderModuleInterfaces
    LABELS "diagnostics;recorder;scope;contract;standalone"
)

enable_testing()
