cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreColorSpaceStandalone LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core Gui)
find_package(GTest CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../StandaloneTestHelpers.cmake")

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")
set(ARTIFACT_CORE_INCLUDE "${ARTIFACT_CORE_ROOT}/include")
set(ARTIFACT_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../..")

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
            "${ARTIFACT_CORE_ROOT}/src/Memory/TrackedPtr.cppm"
        INCLUDE_DIRS "${ARTIFACT_CORE_INCLUDE}"
    )
endif()

artifact_add_standalone_module_interface(ArtifactCoreParallelFeatureModules
    BASE_DIRS "${ARTIFACT_CORE_ROOT}"
    MODULES "${ARTIFACT_CORE_INCLUDE}/Common/Parallel.ixx"
    INCLUDE_DIRS "${ARTIFACT_CORE_INCLUDE}"
    LINK_LIBS Qt6::Core
)

artifact_add_standalone_module_implementation(ArtifactCoreParallelImplementations
    SOURCES "${ARTIFACT_CORE_ROOT}/src/Core/Parallel.cpp"
    LINK_LIBS ArtifactCoreParallelFeatureModules Qt6::Core
    INCLUDE_DIRS "${ARTIFACT_CORE_INCLUDE}"
)

artifact_add_standalone_module_interface(ArtifactCoreColorFeatureModules
    BASE_DIRS "${ARTIFACT_CORE_ROOT}"
    MODULES
        "${ARTIFACT_CORE_INCLUDE}/Color/ColorACES.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Color/ColorBlendMode.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Color/ColorBridge.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Color/ColorConversion.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Color/ColorGamutConversion.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Color/ColorHarmonizer.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Color/ColorLuminance.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Color/ColorLUT.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Color/ColorSpace.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Color/ColorTransferFunction.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Color/FloatColor.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Color/FloatRGBA.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Color/TaggedColor.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Color/ColorSwatch.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Graphics/SurfaceColorContract.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Image/ImageSurfaceView.ixx"
        "${ARTIFACT_CORE_INCLUDE}/Image/SurfacePixelConversion.ixx"
        "${ARTIFACT_CORE_INCLUDE}/ImageProcessing/ColorTransform/Colorama.ixx"
    INCLUDE_DIRS
        "${ARTIFACT_CORE_INCLUDE}"
        "${ARTIFACT_ROOT}/third_party/verdigris/src"
    LINK_LIBS ArtifactCoreStandaloneFoundationModules ArtifactCoreParallelFeatureModules Qt6::Core Qt6::Gui
)

artifact_add_standalone_module_implementation(ArtifactCoreColorImplementations
    SOURCES
        "${ARTIFACT_CORE_ROOT}/src/Color/ColorBlendMode.cpp"
        "${ARTIFACT_CORE_ROOT}/src/Color/ColorConversion.cpp"
        "${ARTIFACT_CORE_ROOT}/src/Color/ColorHarmonizer.cpp"
        "${ARTIFACT_CORE_ROOT}/src/Color/ColorLuminance.cpp"
        "${ARTIFACT_CORE_ROOT}/src/Color/ColorLUT.cpp"
        "${ARTIFACT_CORE_ROOT}/src/Color/ColorSpace.cpp"
        "${ARTIFACT_CORE_ROOT}/src/Color/FloatColor.cpp"
        "${ARTIFACT_CORE_ROOT}/src/Color/FloatRGBA.cpp"
        "${ARTIFACT_CORE_ROOT}/src/Color/ColorSwatch.cpp"
        "${ARTIFACT_CORE_ROOT}/src/ImageProcessing/ColorTransform/Colorama.cpp"
    LINK_LIBS ArtifactCoreColorFeatureModules ArtifactCoreStandaloneFoundationModules ArtifactCoreParallelImplementations Qt6::Core Qt6::Gui
    INCLUDE_DIRS "${ARTIFACT_CORE_INCLUDE}"
)

if(MSVC)
    add_dependencies(ArtifactCoreColorImplementations ArtifactCoreColorFeatureModules)
    add_dependencies(ArtifactCoreParallelImplementations ArtifactCoreParallelFeatureModules)
    add_dependencies(ArtifactCoreColorImplementations ArtifactCoreParallelFeatureModules)
    set(_parallel_ifc_dir "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreParallelFeatureModules.dir")
    set_property(SOURCE "${ARTIFACT_CORE_ROOT}/src/Core/Parallel.cpp"
        APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Core.Parallel=${_parallel_ifc_dir}/Core.Parallel.ifc")
    set(_color_ifc_dir "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreColorFeatureModules.dir")
    get_target_property(_foundation_ifc_dir ArtifactCoreStandaloneFoundationModules ARTIFACT_STANDALONE_IFC_DIR)
    set(_parallel_ifc_dir "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreParallelFeatureModules.dir")
    set_property(SOURCE "${ARTIFACT_CORE_ROOT}/src/Color/ColorLUT.cpp"
        APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Core.Parallel=${_parallel_ifc_dir}/Core.Parallel.ifc")
    set_property(SOURCE "${ARTIFACT_CORE_ROOT}/src/ImageProcessing/ColorTransform/Colorama.cpp"
        APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Core.Parallel=${_parallel_ifc_dir}/Core.Parallel.ifc")
    set_property(SOURCE "${ARTIFACT_CORE_ROOT}/src/Color/ColorSwatch.cpp"
        APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Color.Swatch=${_color_ifc_dir}/Color.Swatch.ifc"
            "/reference;Color.Float=${_color_ifc_dir}/Color.Float.ifc"
            "/reference;Memory.TrackedPtr=${_foundation_ifc_dir}/Memory.TrackedPtr.ifc"
            "/reference;Core.ArtifactString=${_foundation_ifc_dir}/Core.ArtifactString.ifc")
    unset(_color_ifc_dir)
    unset(_foundation_ifc_dir)
    unset(_parallel_ifc_dir)
endif()

set(_color_test_targets
    ArtifactCoreColorSpaceTest
    ArtifactCoreColorPipelineTest
    ArtifactCoreColorTransferFunctionTest
    ArtifactCoreColorConversionTest
    ArtifactCoreColorACESTest
    ArtifactCoreColorGamutConversionTest
    ArtifactCoreColorBridgeTest
    ArtifactCoreColoramaProcessorTest
    ArtifactCoreSurfacePixelConversionTest
    ArtifactCoreImageSurfaceViewTest
    ArtifactCoreColorLUTTest
    ArtifactCoreColorBlendModeTest
    ArtifactCoreColorLuminanceTest
    ArtifactCoreColorHarmonizerTest
    ArtifactCoreFloatRGBAContractTest
    ArtifactCoreColorSwatchStandaloneTest
)
set(_color_test_sources
    ColorSpaceTest.cpp
    ColorPipelineStandaloneTest.cpp
    ColorTransferFunctionStandaloneTest.cpp
    ColorConversionTest.cpp
    ColorACESContractTest.cpp
    ColorGamutConversionTest.cpp
    ColorBridgeTest.cpp
    ColoramaProcessorContractTest.cpp
    SurfacePixelConversionTest.cpp
    ImageSurfaceViewTest.cpp
    ColorLUTContractTest.cpp
    ColorBlendModeStandaloneTest.cpp
    ColorLuminanceContractTest.cpp
    ColorHarmonizerContractTest.cpp
    FloatRGBAContractTest.cpp
    ColorSwatchStandaloneTest.cpp
)
foreach(_target _source IN ZIP_LISTS _color_test_targets _color_test_sources)
    artifact_add_standalone_gtest(${_target}
        SOURCES "${CMAKE_CURRENT_LIST_DIR}/../${_source}"
        LINK_LIBS ArtifactCoreColorImplementations ArtifactCoreColorFeatureModules ArtifactCoreStandaloneFoundationModules Qt6::Core Qt6::Gui
        LABELS "color;contract;standalone"
    )
endforeach()

if(ARTIFACT_STANDALONE_GTEST_GROUP)
    artifact_finalize_standalone_gtest_group("${ARTIFACT_STANDALONE_GTEST_GROUP}")
endif()

enable_testing()
