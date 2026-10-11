cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreDiagnosticSnapshotStandalone LANGUAGES CXX)

find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(DiagnosticSnapshotModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}"
    MODULES
        "${ARTIFACT_CORE_ROOT}/src/Core/ArtifactOptional.cppm"
        "${ARTIFACT_CORE_ROOT}/include/Core/ArtifactArray.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Core/ArtifactString.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Utils/Result.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Diagnostics/CoreDiagnosticSnapshot.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
)

artifact_add_standalone_gtest(ArtifactCoreDiagnosticSnapshotStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../DiagnosticSnapshotStandaloneTest.cpp"
    LINK_LIBS DiagnosticSnapshotModuleInterfaces
    LABELS "diagnostics;snapshot;serialization;contract;standalone"
)

enable_testing()
