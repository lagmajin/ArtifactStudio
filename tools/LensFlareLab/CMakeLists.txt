if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.20)
    project(LensFlareLabStandalone LANGUAGES CXX)
endif()

add_executable(LensFlareLab main.cpp)
target_compile_features(LensFlareLab PRIVATE cxx_std_17)
target_compile_definitions(LensFlareLab PRIVATE NOMINMAX)
