cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreSchemaMigrationStandalone LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

artifact_add_standalone_module_interface(SchemaMigrationModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    MODULES "${ARTIFACT_CORE_ROOT}/include/Serialization/SchemaMigration.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    LINK_LIBS Qt6::Core
)

artifact_add_standalone_gtest(ArtifactCoreSchemaMigrationContractTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../SchemaMigrationContractTest.cpp"
    LINK_LIBS SchemaMigrationModuleInterfaces Qt6::Core
    LABELS "serialization;schema-migration;cpu;contract;standalone"
)
set_property(TEST ArtifactCoreSchemaMigrationContractTest APPEND PROPERTY
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
)

enable_testing()
