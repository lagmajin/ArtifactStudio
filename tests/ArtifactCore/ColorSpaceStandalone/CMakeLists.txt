cmake_minimum_required(VERSION 3.30)

project(ArtifactCoreColorSpaceStandalone LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core)
find_package(GTest CONFIG REQUIRED)

set(ARTIFACT_CORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../ArtifactCore")
set(ARTIFACT_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../..")

add_library(ColorSpaceModuleInterfaces STATIC)
target_sources(ColorSpaceModuleInterfaces
    PUBLIC
        FILE_SET color_space_modules TYPE CXX_MODULES
        FILES
            "${ARTIFACT_CORE_ROOT}/include/Color/ColorSpace.ixx"
            "${ARTIFACT_CORE_ROOT}/include/Color/ColorGamutConversion.ixx"
)
target_include_directories(ColorSpaceModuleInterfaces
    PRIVATE
        "${ARTIFACT_CORE_ROOT}/include"
        "${ARTIFACT_ROOT}/third_party/verdigris/src"
)
target_link_libraries(ColorSpaceModuleInterfaces PUBLIC Qt6::Core)
set_target_properties(ColorSpaceModuleInterfaces PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

add_library(ColorSpaceUnderTest STATIC
    "${ARTIFACT_CORE_ROOT}/src/Color/ColorSpace.cppm"
)
target_link_libraries(ColorSpaceUnderTest PUBLIC ColorSpaceModuleInterfaces)
target_include_directories(ColorSpaceUnderTest PRIVATE
    "${ARTIFACT_CORE_ROOT}/include"
)
set_target_properties(ColorSpaceUnderTest PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

if(MSVC)
    set_source_files_properties(
        "${ARTIFACT_CORE_ROOT}/src/Color/ColorSpace.cppm"
        PROPERTIES CXX_SCAN_FOR_MODULES OFF
    )
    set_property(SOURCE "${ARTIFACT_CORE_ROOT}/src/Color/ColorSpace.cppm"
        APPEND PROPERTY COMPILE_OPTIONS
            "/reference;Color.ColorSpace=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ColorSpaceModuleInterfaces.dir/Color.ColorSpace.ifc"
            "/reference;Color.GamutConversion=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ColorSpaceModuleInterfaces.dir/Color.GamutConversion.ifc"
    )
    add_dependencies(ColorSpaceUnderTest ColorSpaceModuleInterfaces)
    target_compile_options(ColorSpaceUnderTest PRIVATE
        /experimental:module /Zc:__cplusplus /permissive- /EHsc
    )
    target_compile_options(ColorSpaceModuleInterfaces PRIVATE
        /experimental:module /Zc:__cplusplus /permissive- /EHsc
    )
endif()

add_executable(ArtifactCoreColorSpaceTest ../ColorSpaceTest.cpp)
target_link_libraries(ArtifactCoreColorSpaceTest
    PRIVATE
        ColorSpaceUnderTest
        GTest::gtest
        GTest::gtest_main
)
set_target_properties(ArtifactCoreColorSpaceTest PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
if(MSVC)
    target_compile_options(ArtifactCoreColorSpaceTest PRIVATE
        /experimental:module /Zc:__cplusplus /permissive- /EHsc
    )
endif()

enable_testing()
add_test(NAME ArtifactCoreColorSpaceTest COMMAND ArtifactCoreColorSpaceTest)
set_tests_properties(ArtifactCoreColorSpaceTest PROPERTIES
    LABELS "color;color-space;cpu;unit"
)
