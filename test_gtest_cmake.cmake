cmake_minimum_required(VERSION 3.20)
project(TestGTest)

# Use the same vcpkg settings as the main project
set(VCPKG_INSTALLED_DIR "X:/dev/artifactstudio/out/vcpkg_installed" CACHE STRING "vcpkg installed dir" FORCE)

message(STATUS "========== GTest Find Test ==========")
message(STATUS "VCPKG_INSTALLED_DIR: ${VCPKG_INSTALLED_DIR}")

# Try to find GTest
message(STATUS "Searching for GTest...")
find_package(GTest CONFIG QUIET)

if(GTest_FOUND)
    message(STATUS "✓ GTest FOUND!")
    message(STATUS "  GTest_VERSION: ${GTest_VERSION}")
    message(STATUS "  GTest_DIR: ${GTest_DIR}")
    message(STATUS "  GTest::gtest target: FOUND")
    message(STATUS "  GTest::gtest_main target: FOUND")
else()
    message(STATUS "✗ GTest NOT FOUND")
    message(STATUS "  Checking GTest_DIR at: ${VCPKG_INSTALLED_DIR}/x64-windows/share/gtest")
    if(EXISTS "${VCPKG_INSTALLED_DIR}/x64-windows/share/gtest/GTestConfig.cmake")
        message(STATUS "  GTestConfig.cmake exists!")
    else()
        message(STATUS "  GTestConfig.cmake NOT found!")
    endif()
endif()

message(STATUS "========== Test Complete ==========")
