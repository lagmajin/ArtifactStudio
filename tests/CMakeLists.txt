# Artifact Suite Test Configuration

enable_testing()

# Common test settings
find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

function(artifact_add_test target_name)
    cmake_parse_arguments(AAT "" "" "SOURCES;LIBS" ${ARGN})
    
    add_executable(${target_name} ${AAT_SOURCES})
    
    target_link_libraries(${target_name}
        PRIVATE
            GTest::gtest
            GTest::gtest_main
            Qt6::Core
            Qt6::Widgets
            ${AAT_LIBS}
    )
    
    set_target_properties(${target_name} PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
    )
    
    if(ARTIFACT_FRONTEND_MSVC)
        target_compile_options(${target_name} PRIVATE /Zf /Zc:__cplusplus /utf-8 /MP)
    endif()
    
    add_test(NAME ${target_name} COMMAND ${target_name})
endfunction()

# ArtifactCore tests
add_subdirectory(ArtifactCore)

# Artifact tests (later)
# add_subdirectory(Artifact)

# ArtifactWidgets tests (later)
# add_subdirectory(ArtifactWidgets)
