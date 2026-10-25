cmake_minimum_required(VERSION 3.30)

# ArtifactPr is split into a static library plus two thin entry points:
#
#   ArtifactPrApp (STATIC)  -- every ArtifactPr.* module unit
#      +-- ArtifactPr     (WIN32)     src/main.cpp
#      +-- ArtifactPrCLI             src/main_cli.cpp
#      +-- tests/ArtifactPr/*Test    contract tests
#
# Tests link ArtifactPrApp instead of re-compiling the module set, so the
# module interfaces are exported as a PUBLIC CXX_MODULES file set and the MSVC
# module scan runs once per configure instead of once per test target.
#
# Module declaration convention (same as ArtifactCore):
#   include/*.ixx   -> module interface unit      ("export module ArtifactPr.X;")
#   src/*.cpp       -> module implementation unit ("module ArtifactPr.X;")
# A module implementation must not redeclare its module as an interface, and
# must not import the module it implements (self-import breaks Ninja dyndep).
set(ARTIFACT_PR_MODULE_SOURCES
    include/AppTheme.ixx
    include/ArtifactPrCLI.ixx
    include/ArtifactPrDemoData.ixx
    include/ArtifactPrEditorEngine.ixx
    include/ArtifactPrMainWindow.ixx
    include/AudioPreviewMixer.ixx
    include/ClipEffects.ixx
    include/EditCommand.ixx
    include/ExportDialog.ixx
    include/GpuProgramMonitor.ixx
    include/MarkerEditDialog.ixx
    include/MediaFrameDecoder.ixx
    include/MediaPanel.ixx
    include/MediaThumbnailer.ixx
    include/PrColorPickerDialog.ixx
    include/ProjectPanel.ixx
    include/PrShortcut.ixx
    include/PrStatusNotifier.ixx
    include/SequenceAudioRenderer.ixx
    include/SequenceCompositor.ixx
    include/SequenceExporter.ixx
    include/ShortcutHelpDialog.ixx
    include/TimecodeOverlayWidget.ixx
    include/TimelinePanel.ixx
    include/Translation.ixx
    include/TransportBarWidget.ixx
    include/VideoPlayerWidget.ixx
    include/VideoSurface.ixx
)

set(ARTIFACT_PR_IMPL_SOURCES
    src/AppTheme.cpp
    src/ArtifactPrCLI.cpp
    src/ArtifactPrDemoData.cpp
    src/ArtifactPrEditorEngine.cpp
    src/ArtifactPrMainWindow.cpp
    src/AudioPreviewMixer.cpp
    src/ClipEffects.cpp
    src/EditCommand.cpp
    src/ExportDialog.cpp
    src/GpuProgramMonitor.cpp
    src/MarkerEditDialog.cpp
    src/MediaFrameDecoder.cpp
    src/MediaPanel.cpp
    src/MediaThumbnailer.cpp
    src/PrColorPickerDialog.cpp
    src/ProjectPanel.cpp
    src/PrShortcut.cpp
    src/PrStatusNotifier.cpp
    src/SequenceAudioRenderer.cpp
    src/SequenceCompositor.cpp
    src/SequenceExporter.cpp
    src/ShortcutHelpDialog.cpp
    src/TimecodeOverlayWidget.cpp
    src/TimelinePanel.cpp
    src/Translation.cpp
    src/TransportBarWidget.cpp
    src/VideoPlayerWidget.cpp
    src/VideoSurface.cpp
)

# Keep module implementation units distinct from importable module interfaces
# for MSVC and Clang. Upstream clang++ otherwise treats .cppm as a module
# interface; migrated .cpp implementations use ordinary C++ language handling.
if(MSVC OR ARTIFACT_COMPILER_CLANG_GNU OR ARTIFACT_COMPILER_CLANG_CL)
    foreach(_artifact_pr_impl IN LISTS ARTIFACT_PR_IMPL_SOURCES)
        get_filename_component(_artifact_pr_impl_path "${_artifact_pr_impl}"
            ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
        if(_artifact_pr_impl_path MATCHES "[.]cppm$")
            if(MSVC)
                set_property(SOURCE "${_artifact_pr_impl_path}" APPEND PROPERTY
                    VS_SETTINGS "CompileAs=CompileAsCpp")
            endif()
            if(ARTIFACT_COMPILER_CLANG_GNU OR ARTIFACT_COMPILER_CLANG_CL)
                set_source_files_properties("${_artifact_pr_impl_path}" PROPERTIES
                    CXX_SCAN_FOR_MODULES ON)
                if(ARTIFACT_COMPILER_CLANG_GNU)
                    set_property(SOURCE "${_artifact_pr_impl_path}" APPEND PROPERTY
                        COMPILE_OPTIONS -x c++)
                endif()
            endif()
        elseif(_artifact_pr_impl_path MATCHES "[.]cpp$" AND
               (ARTIFACT_COMPILER_CLANG_GNU OR ARTIFACT_COMPILER_CLANG_CL))
            file(STRINGS "${_artifact_pr_impl_path}" _artifact_pr_module_declarations
                REGEX "^[ \t]*module[ \t]+[A-Za-z_][A-Za-z0-9_.]*(:[A-Za-z_][A-Za-z0-9_.]*)?[ \t]*;")
            if(_artifact_pr_module_declarations)
                set_source_files_properties("${_artifact_pr_impl_path}" PROPERTIES
                    CXX_SCAN_FOR_MODULES ON)
            endif()
        endif()
    endforeach()
endif()

# These executable entry points are attached directly to their targets and
# import modules without defining one. Keep their Clang P1689 scan explicit;
# they do not pass through ARTIFACT_PR_IMPL_SOURCES above.
if(ARTIFACT_COMPILER_CLANG_GNU OR ARTIFACT_COMPILER_CLANG_CL)
    foreach(_artifact_pr_entry IN ITEMS
            "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/main_cli.cpp")
        file(STRINGS "${_artifact_pr_entry}" _artifact_pr_entry_imports
            REGEX "^[ \t]*import[ \t]+")
        if(_artifact_pr_entry_imports)
            set_source_files_properties("${_artifact_pr_entry}" PROPERTIES
                CXX_SCAN_FOR_MODULES ON)
        endif()
    endforeach()
endif()

# MOC disabled - causes issues with drive paths
# qt6_wrap_cpp(ARTIFACT_PR_MOC_SOURCES
#     include/ArtifactPrEditorEngine.hpp
# )
#
# QtAdvancedDocking is no longer used. The workspace dock is the shared
# Artifact.DockManager contract plus Artifact.NativeDockSurface, provided by
# the ArtifactDockFoundation target in the Artifact subtree.
# The "qt-advanced-docking-system" vcpkg dependency remains only for the
# opt-in ARTIFACT_QADS_COMPAT_* backends in the Artifact target.

find_package(Qt6 REQUIRED COMPONENTS Multimedia MultimediaWidgets)

function(artifact_pr_configure_target target)
    set_target_properties(${target} PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    )
    target_include_directories(${target} PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
    )
    if(ARTIFACT_COMPILER_MSVC_CL)
        target_compile_options(${target} PRIVATE /Zf /Zc:__cplusplus /utf-8 /wd5050 /WX-)
        target_compile_definitions(${target} PRIVATE
            PLATFORM_WIN32
            _GUARDOVERFLOW_CRT_ALLOCATORS=1
            _SILENCE_CXX23_ALIGNED_STORAGE_DEPRECATION_WARNING
            _WIN32_WINNT=0x0A00
        )
    elseif(ARTIFACT_FRONTEND_MSVC)
        target_compile_options(${target} PRIVATE /Zc:__cplusplus /utf-8 /WX- /wd5050)
        target_compile_definitions(${target} PRIVATE
            PLATFORM_WIN32
            _GUARDOVERFLOW_CRT_ALLOCATORS=1
            _SILENCE_CXX23_ALIGNED_STORAGE_DEPRECATION_WARNING
            _WIN32_WINNT=0x0A00
        )
    endif()
endfunction()

add_library(ArtifactPrApp STATIC)
target_sources(ArtifactPrApp
    PRIVATE ${ARTIFACT_PR_IMPL_SOURCES}
    PUBLIC FILE_SET CXX_MODULES FILES ${ARTIFACT_PR_MODULE_SOURCES}
)
target_link_libraries(ArtifactPrApp PUBLIC
    ArtifactCore
    ArtifactCoreCommand
    ArtifactCoreAudio
    ArtifactCoreVideo
    ArtifactCoreNLE
    # Core.Localization の実装 (Localization.cppm) はこのパック側にある。
    # ArtifactCore は interface (include/Utils/Localization.ixx) しか持たないため、
    # 翻訳カタログを使う ArtifactPr.Translation は明示的にリンクする必要がある。
    ArtifactCoreLocalization
    ArtifactWidgets
    ArtifactGpuFoundation
    ArtifactDockFoundation
    Qt6::Multimedia
    Qt6::MultimediaWidgets
)
artifact_pr_configure_target(ArtifactPrApp)
set_target_properties(ArtifactPrApp PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
)

# GUI entry point. The shared --cli switch inside main.cpp keeps the console
# path on the same EditorEngine document/undo/render owner as the GUI.
add_executable(ArtifactPr WIN32 src/main.cpp)
target_link_libraries(ArtifactPr PRIVATE ArtifactPrApp)
artifact_pr_configure_target(ArtifactPr)
set_target_properties(ArtifactPr PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
)

# Translation catalogs are deployed next to the executable so the runtime can
# load them from QCoreApplication::applicationDirPath()/translations.
add_custom_command(TARGET ArtifactPr POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/translations"
        "$<TARGET_FILE_DIR:ArtifactPr>/translations"
    COMMENT "Deploying translation files for ArtifactPr"
    VERBATIM
)

# Console-only entry point.
add_executable(ArtifactPrCLI src/main_cli.cpp)
target_link_libraries(ArtifactPrCLI PRIVATE ArtifactPrApp)
artifact_pr_configure_target(ArtifactPrCLI)
set_target_properties(ArtifactPrCLI PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
)
