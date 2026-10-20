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
#   src/*.cppm      -> module implementation unit ("module ArtifactPr.X;")
# A .cppm must not redeclare its module as an interface, and must not import
# the module it implements (self-import breaks Ninja dyndep).
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
    include/TransportBarWidget.ixx
    include/VideoPlayerWidget.ixx
    include/VideoSurface.ixx
)

set(ARTIFACT_PR_IMPL_SOURCES
    src/AppTheme.cppm
    src/ArtifactPrCLI.cppm
    src/ArtifactPrDemoData.cppm
    src/ArtifactPrEditorEngine.cppm
    src/ArtifactPrMainWindow.cppm
    src/AudioPreviewMixer.cppm
    src/ClipEffects.cppm
    src/EditCommand.cppm
    src/ExportDialog.cppm
    src/GpuProgramMonitor.cppm
    src/MarkerEditDialog.cppm
    src/MediaFrameDecoder.cppm
    src/MediaPanel.cppm
    src/MediaThumbnailer.cppm
    src/PrColorPickerDialog.cppm
    src/ProjectPanel.cppm
    src/PrShortcut.cppm
    src/PrStatusNotifier.cppm
    src/SequenceAudioRenderer.cppm
    src/SequenceCompositor.cppm
    src/SequenceExporter.cppm
    src/ShortcutHelpDialog.cppm
    src/TimecodeOverlayWidget.cppm
    src/TimelinePanel.cppm
    src/TransportBarWidget.cppm
    src/VideoPlayerWidget.cppm
    src/VideoSurface.cppm
)

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

# Console-only entry point.
add_executable(ArtifactPrCLI src/main_cli.cpp)
target_link_libraries(ArtifactPrCLI PRIVATE ArtifactPrApp)
artifact_pr_configure_target(ArtifactPrCLI)
set_target_properties(ArtifactPrCLI PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
)
