cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreFrameRateStandaloneSuite LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")
set(ARTIFACT_CORE_INCLUDE "${ARTIFACT_CORE_ROOT}/include")
set(ARTIFACT_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../..")
artifact_ensure_shared_string_modules()

artifact_add_standalone_module_interface(ArtifactCoreFrameRateFeatureModules
    BASE_DIRS "${ARTIFACT_CORE_INCLUDE}"
    MODULES
        "${ARTIFACT_CORE_INCLUDE}/Frame/FrameRate.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Core/AspectRatio.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Serialization/ISerializable.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Serialization/SerializationRegistry.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Serialization/JsonSerializableAdapter.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Serialization/SchemaMigration.ixx"
    INCLUDE_DIRS
        "${ARTIFACT_CORE_INCLUDE}"
        "${ARTIFACT_ROOT}/third_party/verdigris/src"
    LINK_LIBS ArtifactCoreSharedStringModules Qt6::Core
)

artifact_add_standalone_module_implementation(ArtifactCoreFrameRateImplementations
    SOURCES
        "${ARTIFACT_CORE_ROOT}/src/Frame/FrameRate.cpp"
        "${ARTIFACT_CORE_ROOT}/src/Core/AspectRatio.cpp"
    LINK_LIBS ArtifactCoreFrameRateFeatureModules ArtifactCoreSharedStringImplementations Qt6::Core
    INCLUDE_DIRS "${ARTIFACT_CORE_INCLUDE}"
)

if(MSVC)
    add_dependencies(ArtifactCoreFrameRateImplementations ArtifactCoreFrameRateFeatureModules)
    get_target_property(_shared_string_ifc_dir ArtifactCoreSharedStringModules ARTIFACT_STANDALONE_IFC_DIR)
    set(_frame_rate_ifc_dir "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreFrameRateFeatureModules.dir")
    set_property(SOURCE "${ARTIFACT_CORE_ROOT}/src/Frame/FrameRate.cpp"
        APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Frame.Rate=${_frame_rate_ifc_dir}/Frame.Rate.ifc"
            "/reference;Utils.String.UniString=${_shared_string_ifc_dir}/Utils.String.UniString.ifc"
            "/reference;Serialization.JsonAdapter=${_frame_rate_ifc_dir}/Serialization.JsonAdapter.ifc"
            "/reference;Serialization.SchemaMigration=${_frame_rate_ifc_dir}/Serialization.SchemaMigration.ifc"
            "/reference;Serialization.ISerializable=${_frame_rate_ifc_dir}/Serialization.ISerializable.ifc"
            "/reference;Serialization.Registry=${_frame_rate_ifc_dir}/Serialization.Registry.ifc")
    set_property(SOURCE "${ARTIFACT_CORE_ROOT}/src/Core/AspectRatio.cpp"
        APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Core.AspectRatio=${_frame_rate_ifc_dir}/Core.AspectRatio.ifc"
            "/reference;Utils.String.UniString=${_shared_string_ifc_dir}/Utils.String.UniString.ifc")
    unset(_frame_rate_ifc_dir)
    unset(_shared_string_ifc_dir)
endif()

artifact_add_standalone_gtest(ArtifactCoreFrameRateContractTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../FrameRateContractTest.cpp"
    LINK_LIBS ArtifactCoreFrameRateImplementations ArtifactCoreFrameRateFeatureModules Qt6::Core
    LABELS "time;frame-rate;cpu;contract;standalone"
)
artifact_add_standalone_gtest(ArtifactCoreFrameRateStorageScaleTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../FrameRateStorageScaleTest.cpp"
    LINK_LIBS ArtifactCoreFrameRateImplementations ArtifactCoreFrameRateFeatureModules Qt6::Core
    LABELS "time;frame-rate;cpu;contract;standalone"
)
artifact_add_standalone_gtest(ArtifactCoreAspectRatioContractTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../AspectRatioContractTest.cpp"
    LINK_LIBS ArtifactCoreFrameRateImplementations ArtifactCoreFrameRateFeatureModules Qt6::Core
    LABELS "core;geometry;aspect-ratio;cpu;contract;standalone"
)
if(NOT ARTIFACT_STANDALONE_GTEST_GROUP)
    set_tests_properties(ArtifactCoreFrameRateContractTest ArtifactCoreFrameRateStorageScaleTest ArtifactCoreAspectRatioContractTest PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    )
endif()

if(ARTIFACT_STANDALONE_GTEST_GROUP)
    artifact_finalize_standalone_gtest_group("${ARTIFACT_STANDALONE_GTEST_GROUP}")
    set_tests_properties("${ARTIFACT_STANDALONE_GTEST_GROUP}" PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    )
endif()

enable_testing()
