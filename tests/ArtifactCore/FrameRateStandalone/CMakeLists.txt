cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreFrameRateStandalone LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")
set(ARTIFACT_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../..")

artifact_add_standalone_module_interface(FrameRateModuleInterfaces
    BASE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    MODULES
        "${ARTIFACT_CORE_ROOT}/include/Frame/FrameRate.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Utils/StringConvertor.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Utils/UniString.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Serialization/ISerializable.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Serialization/SerializationRegistry.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Serialization/JsonSerializableAdapter.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Serialization/SchemaMigration.ixx"
    INCLUDE_DIRS
        "${ARTIFACT_CORE_ROOT}/include"
        "${ARTIFACT_ROOT}/third_party/verdigris/src"
    LINK_LIBS Qt6::Core
)

artifact_add_standalone_module_implementation(FrameRateUnderTest
    SOURCES
        "${ARTIFACT_CORE_ROOT}/src/Utils/UniString.cpp"
        "${ARTIFACT_CORE_ROOT}/src/Frame/FrameRate.cpp"
    LINK_LIBS FrameRateModuleInterfaces Qt6::Core
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
)

if(MSVC)
    # Scanner-disabled module implementation units in a separate target need
    # explicit references to their interface and imported IFCs.  Keep this
    # wiring local to the standalone test project; ArtifactCore's production
    # target graph remains untouched.
    set(_frame_rate_ifc_dir "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/FrameRateModuleInterfaces.dir")
    set_property(SOURCE "${CMAKE_CURRENT_BINARY_DIR}/standalone_implementations/FrameRateUnderTest_UniString.cpp"
        APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Utils.String.UniString=${_frame_rate_ifc_dir}/Utils.String.UniString.ifc"
            "/reference;Utils.Convertor.String=${_frame_rate_ifc_dir}/Utils.Convertor.String.ifc")
    set_property(SOURCE "${CMAKE_CURRENT_BINARY_DIR}/standalone_implementations/FrameRateUnderTest_FrameRate.cpp"
        APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Frame.Rate=${_frame_rate_ifc_dir}/Frame.Rate.ifc"
            "/reference;Utils.String.UniString=${_frame_rate_ifc_dir}/Utils.String.UniString.ifc"
            "/reference;Serialization.JsonAdapter=${_frame_rate_ifc_dir}/Serialization.JsonAdapter.ifc"
            "/reference;Serialization.SchemaMigration=${_frame_rate_ifc_dir}/Serialization.SchemaMigration.ifc"
            "/reference;Serialization.ISerializable=${_frame_rate_ifc_dir}/Serialization.ISerializable.ifc"
            "/reference;Serialization.Registry=${_frame_rate_ifc_dir}/Serialization.Registry.ifc")
    unset(_frame_rate_ifc_dir)
endif()

if(MSVC)
    add_dependencies(FrameRateUnderTest FrameRateModuleInterfaces)
endif()

artifact_add_standalone_gtest(ArtifactCoreFrameRateContractTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../FrameRateContractTest.cpp"
    LINK_LIBS FrameRateUnderTest FrameRateModuleInterfaces Qt6::Core
    LABELS "time;frame-rate;cpu;contract;standalone"
)
set_property(TEST ArtifactCoreFrameRateContractTest APPEND PROPERTY
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
)

enable_testing()
