cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreDataStandalone LANGUAGES CXX)

find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")
set(ARTIFACT_CORE_INCLUDE "${ARTIFACT_CORE_ROOT}/include")

# The combined standalone project defines this target once above both suites.
# Keep this directory independently configurable as well.
if(NOT TARGET ArtifactCoreStandaloneFoundationModules)
artifact_add_standalone_module_interface(ArtifactCoreStandaloneFoundationModules
    BASE_DIRS "${ARTIFACT_CORE_ROOT}"
    MODULES
        "${ARTIFACT_CORE_ROOT}/include/Container/ContainerDebug.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Container/ContainerDebugRegistry.ixx"
        "${ARTIFACT_CORE_ROOT}/src/Core/ArtifactOptional.cppm"
        "${ARTIFACT_CORE_ROOT}/include/Core/ArtifactArray.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Core/ArtifactString.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Container/NamedVector.ixx"
        "${ARTIFACT_CORE_ROOT}/src/Memory/SharedPtr.cppm"
    INCLUDE_DIRS "${ARTIFACT_CORE_INCLUDE}"
)
endif()

artifact_add_standalone_module_interface(DataColumnTypeModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}"
    MODULES "${ARTIFACT_CORE_ROOT}/include/Data/ColumnType.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_INCLUDE}"
    LINK_LIBS ArtifactCoreStandaloneFoundationModules
)

artifact_add_standalone_module_interface(DataCsvParserModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}"
    MODULES "${ARTIFACT_CORE_ROOT}/include/Data/CsvParser.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_INCLUDE}"
    LINK_LIBS ArtifactCoreStandaloneFoundationModules
)

artifact_add_standalone_module_interface(DataTableModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}"
    MODULES "${ARTIFACT_CORE_ROOT}/include/Data/DataTable.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_INCLUDE}"
    LINK_LIBS DataCsvParserModuleInterfaces ArtifactCoreStandaloneFoundationModules
)

artifact_add_standalone_module_interface(DataTypeInferenceModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}"
    MODULES "${ARTIFACT_CORE_ROOT}/include/Data/TypeInference.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_INCLUDE}"
    LINK_LIBS DataColumnTypeModuleInterfaces ArtifactCoreStandaloneFoundationModules
)

artifact_add_standalone_module_interface(DataTypedColumnModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}"
    MODULES "${ARTIFACT_CORE_ROOT}/include/Data/TypedColumn.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_INCLUDE}"
    LINK_LIBS
        DataTypeInferenceModuleInterfaces
        DataColumnTypeModuleInterfaces
        ArtifactCoreStandaloneFoundationModules
)

artifact_add_standalone_module_interface(DataSourceModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}"
    MODULES "${ARTIFACT_CORE_ROOT}/include/Data/DataSource.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_INCLUDE}"
    LINK_LIBS
        DataTableModuleInterfaces
        DataColumnTypeModuleInterfaces
        ArtifactCoreStandaloneFoundationModules
)

artifact_add_standalone_module_interface(DataCsvDataSourceModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}"
    MODULES "${ARTIFACT_CORE_ROOT}/include/Data/CsvDataSource.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_INCLUDE}"
    LINK_LIBS
        DataSourceModuleInterfaces
        DataTableModuleInterfaces
        DataCsvParserModuleInterfaces
        ArtifactCoreStandaloneFoundationModules
)

artifact_add_standalone_module_interface(DataCacheModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}"
    MODULES "${ARTIFACT_CORE_ROOT}/include/Data/DataCache.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_INCLUDE}"
    LINK_LIBS DataSourceModuleInterfaces DataTableModuleInterfaces ArtifactCoreStandaloneFoundationModules
)

artifact_add_standalone_gtest(ArtifactCoreColumnTypeStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../ColumnTypeStandaloneTest.cpp"
    LINK_LIBS DataColumnTypeModuleInterfaces ArtifactCoreStandaloneFoundationModules
    LABELS "core;data;column-type;cpu;contract;standalone"
)

artifact_add_standalone_gtest(ArtifactCoreTypedColumnStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../TypedColumnStandaloneTest.cpp"
    LINK_LIBS
        DataTypedColumnModuleInterfaces
        DataTypeInferenceModuleInterfaces
        DataColumnTypeModuleInterfaces
        ArtifactCoreStandaloneFoundationModules
    LABELS "core;data;typed-column;cpu;contract;standalone"
)

artifact_add_standalone_gtest(ArtifactCoreCsvParserStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../CsvParserStandaloneTest.cpp"
    LINK_LIBS DataCsvParserModuleInterfaces ArtifactCoreStandaloneFoundationModules
    LABELS "core;data;csv;parser;cpu;contract;standalone"
)

artifact_add_standalone_gtest(ArtifactCoreDataTableStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../DataTableStandaloneTest.cpp"
    LINK_LIBS
        DataTableModuleInterfaces
        DataCsvParserModuleInterfaces
        ArtifactCoreStandaloneFoundationModules
    LABELS "core;data;table;cpu;contract;standalone"
)

artifact_add_standalone_gtest(ArtifactCoreDataSourceStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../DataSourceStandaloneTest.cpp"
    LINK_LIBS
        DataSourceModuleInterfaces
        DataTableModuleInterfaces
        DataColumnTypeModuleInterfaces
        ArtifactCoreStandaloneFoundationModules
    LABELS "core;data;data-source;cpu;contract;standalone"
)

artifact_add_standalone_gtest(ArtifactCoreCsvDataSourceStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../CsvDataSourceStandaloneTest.cpp"
    LINK_LIBS
        DataCsvDataSourceModuleInterfaces
        DataSourceModuleInterfaces
        ArtifactCoreStandaloneFoundationModules
    LABELS "core;data;csv-data-source;cpu;contract;standalone"
)

artifact_add_standalone_gtest(ArtifactCoreDataCacheStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../DataCacheStandaloneTest.cpp"
    LINK_LIBS
        DataCacheModuleInterfaces
        DataCsvDataSourceModuleInterfaces
        DataSourceModuleInterfaces
        ArtifactCoreStandaloneFoundationModules
    LABELS "core;data;data-cache;cpu;contract;standalone"
)

if(ARTIFACT_STANDALONE_GTEST_GROUP)
    artifact_finalize_standalone_gtest_group("${ARTIFACT_STANDALONE_GTEST_GROUP}")
endif()

enable_testing()
