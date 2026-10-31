# Helpers shared by small standalone projects under tests/ArtifactCore.
# Keep these projects independent of the repository's top-level Artifact build.

function(artifact_add_standalone_module_interface target_name)
    cmake_parse_arguments(ASMI "" "" "BASE_DIRS;MODULES;INCLUDE_DIRS;LINK_LIBS" ${ARGN})

    if(NOT ASMI_MODULES)
        message(FATAL_ERROR "${target_name}: MODULES must list at least one .ixx file")
    endif()

    add_library(${target_name} STATIC)
    target_sources(${target_name}
        PUBLIC
            FILE_SET standalone_modules TYPE CXX_MODULES
            BASE_DIRS ${ASMI_BASE_DIRS}
            FILES ${ASMI_MODULES}
    )
    if(ASMI_INCLUDE_DIRS)
        target_include_directories(${target_name} PRIVATE ${ASMI_INCLUDE_DIRS})
    endif()
    if(ASMI_LINK_LIBS)
        target_link_libraries(${target_name} PUBLIC ${ASMI_LINK_LIBS})
    endif()
    set_target_properties(${target_name} PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    )
    set_property(TARGET ${target_name} PROPERTY
        ARTIFACT_STANDALONE_IFC_DIR
        "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/${target_name}.dir")
    if(MSVC)
        target_compile_options(${target_name} PRIVATE
            /experimental:module /Zc:__cplusplus /permissive- /EHsc
        )
    endif()
endfunction()

function(artifact_add_standalone_module_implementation target_name)
    cmake_parse_arguments(ASMI "" "" "SOURCES;LINK_LIBS;INCLUDE_DIRS" ${ARGN})

    if(NOT ASMI_SOURCES)
        message(FATAL_ERROR "${target_name}: SOURCES must list at least one module implementation")
    endif()

    set(standalone_implementation_sources)
    foreach(source IN LISTS ASMI_SOURCES)
        get_filename_component(source_extension "${source}" LAST_EXT)
        get_filename_component(source_stem "${source}" NAME_WE)
        if(MSVC AND source_extension STREQUAL ".cppm")
            set(generated_source
                "${CMAKE_CURRENT_BINARY_DIR}/standalone_implementations/${target_name}_${source_stem}.cpp")
            configure_file("${source}" "${generated_source}" COPYONLY)
            list(APPEND standalone_implementation_sources "${generated_source}")
        else()
            list(APPEND standalone_implementation_sources "${source}")
        endif()
    endforeach()

    add_library(${target_name} STATIC ${standalone_implementation_sources})
    if(ASMI_LINK_LIBS)
        target_link_libraries(${target_name} PUBLIC ${ASMI_LINK_LIBS})
    endif()
    if(ASMI_INCLUDE_DIRS)
        target_include_directories(${target_name} PRIVATE ${ASMI_INCLUDE_DIRS})
    endif()
    set_target_properties(${target_name} PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    )
    if(MSVC)
        # A primary-interface attachment (`module Name;`) is not discovered
        # reliably as a dependency on MSVC. Compile copied .cppm implementations
        # as ordinary .cpp module units and keep them out of dyndep scanning.
        set_source_files_properties(${standalone_implementation_sources}
            PROPERTIES CXX_SCAN_FOR_MODULES OFF)
        target_compile_options(${target_name} PRIVATE
            /experimental:module /Zc:__cplusplus /permissive- /EHsc
        )
    endif()
endfunction()

function(artifact_add_standalone_gtest target_name)
    cmake_parse_arguments(ASGT "" "LABELS" "SOURCES;LINK_LIBS" ${ARGN})
    if(NOT ASGT_SOURCES)
        message(FATAL_ERROR "${target_name}: SOURCES must list at least one test source")
    endif()

    if(ARTIFACT_STANDALONE_GTEST_GROUP)
        set(_group_property_prefix "ARTIFACT_STANDALONE_GTEST_GROUP_${ARTIFACT_STANDALONE_GTEST_GROUP}")
        set_property(GLOBAL APPEND PROPERTY "${_group_property_prefix}_SOURCES" ${ASGT_SOURCES})
        set_property(GLOBAL APPEND PROPERTY "${_group_property_prefix}_LINK_LIBS" ${ASGT_LINK_LIBS})
        set_property(GLOBAL APPEND PROPERTY "${_group_property_prefix}_LABELS" ${ASGT_LABELS})
        return()
    endif()

    add_executable(${target_name} ${ASGT_SOURCES})
    target_link_libraries(${target_name} PRIVATE GTest::gtest GTest::gtest_main ${ASGT_LINK_LIBS})
    set_target_properties(${target_name} PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    )
    if(MSVC)
        target_compile_options(${target_name} PRIVATE
            /experimental:module /Zc:__cplusplus /permissive- /EHsc
        )
    endif()
    add_test(NAME ${target_name} COMMAND ${target_name})
    if(ASGT_LABELS)
        set_tests_properties(${target_name} PROPERTIES LABELS "${ASGT_LABELS}")
    endif()
endfunction()

function(artifact_finalize_standalone_gtest_group target_name)
    set(_group_property_prefix "ARTIFACT_STANDALONE_GTEST_GROUP_${target_name}")
    get_property(_sources GLOBAL PROPERTY "${_group_property_prefix}_SOURCES")
    get_property(_link_libs GLOBAL PROPERTY "${_group_property_prefix}_LINK_LIBS")
    get_property(_labels GLOBAL PROPERTY "${_group_property_prefix}_LABELS")

    if(NOT _sources)
        message(FATAL_ERROR "${target_name}: no test sources were registered for this group")
    endif()

    list(REMOVE_DUPLICATES _sources)
    list(REMOVE_DUPLICATES _link_libs)
    list(REMOVE_DUPLICATES _labels)

    add_executable(${target_name} ${_sources})
    target_link_libraries(${target_name} PRIVATE GTest::gtest GTest::gtest_main ${_link_libs})
    set_target_properties(${target_name} PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    )
    if(MSVC)
        target_compile_options(${target_name} PRIVATE
            /experimental:module /Zc:__cplusplus /permissive- /EHsc
        )
    endif()
    add_test(NAME ${target_name} COMMAND ${target_name})
    if(_labels)
        set_tests_properties(${target_name} PROPERTIES LABELS "${_labels}")
    endif()

    set_property(GLOBAL PROPERTY "${_group_property_prefix}_SOURCES" "")
    set_property(GLOBAL PROPERTY "${_group_property_prefix}_LINK_LIBS" "")
    set_property(GLOBAL PROPERTY "${_group_property_prefix}_LABELS" "")
endfunction()

function(artifact_ensure_shared_string_modules)
    if(TARGET ArtifactCoreSharedStringModules)
        return()
    endif()

    find_package(Qt6 CONFIG REQUIRED COMPONENTS Core)
    set(artifact_core_root "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../../ArtifactCore")
    set(artifact_core_include "${artifact_core_root}/include")

    artifact_add_standalone_module_interface(ArtifactCoreSharedStringModules
        BASE_DIRS "${artifact_core_include}"
        MODULES
            "${artifact_core_include}/Utils/StringConvertor.ixx"
            "${artifact_core_include}/Utils/UniString.ixx"
            "${artifact_core_include}/Utils/StringLike.ixx"
        INCLUDE_DIRS "${artifact_core_include}"
        LINK_LIBS Qt6::Core
    )
    set_property(TARGET ArtifactCoreSharedStringModules PROPERTY
        ARTIFACT_STANDALONE_IFC_DIR
        "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreSharedStringModules.dir")

    artifact_add_standalone_module_implementation(ArtifactCoreSharedStringImplementations
        SOURCES "${artifact_core_root}/src/Utils/UniString.cpp"
        LINK_LIBS ArtifactCoreSharedStringModules Qt6::Core
        INCLUDE_DIRS "${artifact_core_include}"
    )

    if(MSVC)
        add_dependencies(ArtifactCoreSharedStringImplementations ArtifactCoreSharedStringModules)
        set(_artifact_core_shared_string_ifc_dir
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ArtifactCoreSharedStringModules.dir")
        set_property(SOURCE "${artifact_core_root}/src/Utils/UniString.cpp"
            APPEND PROPERTY COMPILE_OPTIONS
                "/reference;Utils.String.UniString=${_artifact_core_shared_string_ifc_dir}/Utils.String.UniString.ifc"
                "/reference;Utils.Convertor.String=${_artifact_core_shared_string_ifc_dir}/Utils.Convertor.String.ifc")
        unset(_artifact_core_shared_string_ifc_dir)
    endif()
endfunction()
