cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreOpenExrStandalone LANGUAGES CXX)

find_package(GTest CONFIG REQUIRED)
find_package(Qt6 CONFIG REQUIRED COMPONENTS Core)
find_package(OpenImageIO REQUIRED)
find_package(TBB REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")

if(NOT TARGET ArtifactCoreParallelFeatureModules)
    artifact_add_standalone_module_interface(ArtifactCoreParallelFeatureModules
        BASE_DIRS "${ARTIFACT_CORE_ROOT}"
        MODULES "${ARTIFACT_CORE_ROOT}/include/Common/Parallel.ixx"
        INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
        LINK_LIBS Qt6::Core
    )
    artifact_add_standalone_module_implementation(ArtifactCoreParallelImplementations
        SOURCES "${ARTIFACT_CORE_ROOT}/src/Core/Parallel.cpp"
        LINK_LIBS ArtifactCoreParallelFeatureModules Qt6::Core
        INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
    )
    if(MSVC)
        add_dependencies(ArtifactCoreParallelImplementations ArtifactCoreParallelFeatureModules)
        set_property(SOURCE "${ARTIFACT_CORE_ROOT}/src/Core/Parallel.cpp"
            APPEND PROPERTY COMPILE_OPTIONS
                "/reference;Core.Parallel=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreParallelFeatureModules.dir/Core.Parallel.ifc")
    endif()
endif()

artifact_add_standalone_module_interface(OpenExrModuleInterfaces
    BASE_DIRS
        "${ARTIFACT_CORE_ROOT}/include"
    MODULES
        "${ARTIFACT_CORE_ROOT}/include/Image/DeepImageBuffer.ixx"
        "${ARTIFACT_CORE_ROOT}/include/Image/OpenEXR.ixx"
    INCLUDE_DIRS
        "${ARTIFACT_CORE_ROOT}/include"
    LINK_LIBS ArtifactCoreParallelFeatureModules Qt6::Core
)

artifact_add_standalone_module_implementation(DeepImageBufferUnderTest
    SOURCES "${ARTIFACT_CORE_ROOT}/src/Image/DeepImageBuffer.cpp"
    LINK_LIBS OpenExrModuleInterfaces ArtifactCoreParallelFeatureModules ArtifactCoreParallelImplementations Qt6::Core
    INCLUDE_DIRS
        "${ARTIFACT_CORE_ROOT}/include"
)

artifact_add_standalone_module_implementation(OpenExrUnderTest
    SOURCES "${ARTIFACT_CORE_ROOT}/src/Image/OpenEXR.cpp"
    LINK_LIBS OpenExrModuleInterfaces DeepImageBufferUnderTest ArtifactCoreParallelFeatureModules ArtifactCoreParallelImplementations Qt6::Core OpenImageIO::OpenImageIO TBB::tbb
    INCLUDE_DIRS "${ARTIFACT_CORE_ROOT}/include"
)

if(MSVC)
    add_dependencies(DeepImageBufferUnderTest OpenExrModuleInterfaces ArtifactCoreParallelFeatureModules)
    add_dependencies(OpenExrUnderTest OpenExrModuleInterfaces DeepImageBufferUnderTest ArtifactCoreParallelFeatureModules)
    get_target_property(_parallel_target_binary_dir ArtifactCoreParallelFeatureModules BINARY_DIR)
    get_target_property(_openexr_target_binary_dir OpenExrModuleInterfaces BINARY_DIR)
    set_property(SOURCE "${ARTIFACT_CORE_ROOT}/src/Image/DeepImageBuffer.cpp"
        APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Image.DeepImageBuffer=${_openexr_target_binary_dir}/CMakeFiles/OpenExrModuleInterfaces.dir/Image.DeepImageBuffer.ifc"
            "/reference;Core.Parallel=${_parallel_target_binary_dir}/CMakeFiles/ArtifactCoreParallelFeatureModules.dir/Core.Parallel.ifc")
    set_property(SOURCE "${ARTIFACT_CORE_ROOT}/src/Image/OpenEXR.cpp"
        APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Image.OpenEXR=${_openexr_target_binary_dir}/CMakeFiles/OpenExrModuleInterfaces.dir/Image.OpenEXR.ifc"
            "/reference;Image.DeepImageBuffer=${_openexr_target_binary_dir}/CMakeFiles/OpenExrModuleInterfaces.dir/Image.DeepImageBuffer.ifc"
            "/reference;Core.Parallel=${_parallel_target_binary_dir}/CMakeFiles/ArtifactCoreParallelFeatureModules.dir/Core.Parallel.ifc")
endif()

artifact_add_standalone_gtest(ArtifactCoreOpenExrRoundTripTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/OpenExrRoundTripTest.cpp"
    LINK_LIBS OpenExrUnderTest OpenExrModuleInterfaces Qt6::Core
    LABELS "image;openexr;io;standalone"
)

artifact_add_standalone_gtest(ArtifactCoreDeepImageBufferStandaloneTest
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../DeepImageBufferContractTest.cpp"
    LINK_LIBS DeepImageBufferUnderTest OpenExrModuleInterfaces Qt6::Core
    LABELS "image;deep-image;cpu;contract;standalone"
)

if(ARTIFACT_STANDALONE_GTEST_GROUP)
    artifact_finalize_standalone_gtest_group("${ARTIFACT_STANDALONE_GTEST_GROUP}")
endif()

enable_testing()
