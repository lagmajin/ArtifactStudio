cmake_minimum_required(VERSION 3.30)

project(ClangMsvcAbiModuleSmoke LANGUAGES CXX)

if(NOT WIN32 OR NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    message(FATAL_ERROR "This smoke project requires Windows and upstream Clang.")
endif()
if(NOT CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "GNU")
    message(FATAL_ERROR "Expected clang++ GNU frontend, got '${CMAKE_CXX_COMPILER_FRONTEND_VARIANT}'.")
endif()
if(NOT CMAKE_CXX_COMPILER_TARGET MATCHES "windows-msvc")
    message(FATAL_ERROR "Expected the MSVC ABI target, got '${CMAKE_CXX_COMPILER_TARGET}'.")
endif()
if(NOT CMAKE_GENERATOR MATCHES "Ninja")
    message(FATAL_ERROR "The Clang P1689 scan smoke currently requires Ninja.")
endif()
if(NOT CMAKE_CXX_COMPILER_CLANG_SCAN_DEPS)
    message(FATAL_ERROR "CMake did not find clang-scan-deps.")
endif()
if(CMAKE_CXX_LINK_MODE STREQUAL "DRIVER" AND CMAKE_CXX_FLAGS MATCHES "-nostartfiles|-nostdlib")
    message(FATAL_ERROR "CMake retained MinGW-only link flags for the MSVC ABI target.")
endif()

message(STATUS "Smoke compiler: ${CMAKE_CXX_COMPILER} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "Smoke target: ${CMAKE_CXX_COMPILER_TARGET}")
message(STATUS "Smoke frontend: ${CMAKE_CXX_COMPILER_FRONTEND_VARIANT}")
message(STATUS "Smoke linker: ${CMAKE_LINKER}")
message(STATUS "Smoke scanner: ${CMAKE_CXX_COMPILER_CLANG_SCAN_DEPS}")

add_executable(ClangMsvcAbiModuleSmoke
    main.cpp
    implementation.cpp)
target_sources(ClangMsvcAbiModuleSmoke
    PUBLIC
        FILE_SET CXX_MODULES FILES module_interface.ixx)
target_compile_features(ClangMsvcAbiModuleSmoke PRIVATE cxx_std_23)
set_property(TARGET ClangMsvcAbiModuleSmoke PROPERTY CXX_SCAN_FOR_MODULES ON)
if(MSVC)
    message(FATAL_ERROR "The smoke lane must remain the upstream Clang GNU frontend, not clang-cl/MSVC mode.")
endif()

# Keep an ordinary, non-module C++ translation unit separate so compiler-cache
# behavior can be measured without C++20 module flags or module-map response files.
add_executable(ClangMsvcAbiPlainSmoke
    plain_main.cpp
    plain_implementation.cpp)
target_compile_features(ClangMsvcAbiPlainSmoke PRIVATE cxx_std_23)
