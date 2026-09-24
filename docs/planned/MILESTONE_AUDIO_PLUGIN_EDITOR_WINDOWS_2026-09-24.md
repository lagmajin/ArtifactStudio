# Audio Plugin Editor Windows Milestone

**最終更新:** 2026-09-24
**ステータス:** In Progress
**対象:** VST3 / CLAP audio effect editor windows

## Goal

Open the UI supplied by an inserted VST3 or CLAP effect in a normal floating desktop window. Closing the window must leave the effect instance inserted and processing. Plugins without a native editor must remain editable through the existing parameter surface.

## Pre-implementation code evidence

- `Artifact/src/VST/VSTHost.cppm` had a VST2 editor path but its `effEditOpen` / `effEditClose` numeric opcodes did not match the VST2 ABI. The path explicitly excludes VST3 instances.
- `Artifact/src/Widgets/AudioMixerWidget.cppm` offered “Open Editor” for `VSTEffect`, but called `openEditor(nullptr)`. `VSTHost::openEditor()` requires a non-null native window handle, so this UI action could not open an editor.
- The same audio effect menu has no CLAP insertion or editor action. The current visible route inserts VST effects only.
- `ArtifactCore/include/VST3/VST3Interfaces.ixx` declares `IEditController::createView()` and an opaque `IPlugView`, but does not declare the full `IPlugView` / `IPlugFrame` contract needed to attach, resize, focus, and detach a VST3 editor.
- `ArtifactCore/include/CLAP/CLAPHost.ixx` exposes plugin processing and parameter access, but no GUI extension contract or window lifecycle API.
- Existing effect editor calls are not evidence of a working VST2, VST3, or CLAP UI in the current mixer workflow.

## Implementation progress (2026-09-24)

- Added a native floating host dialog for the existing VST effect action. The effect is retained while the window is open; closing the dialog calls the editor close path and leaves the effect loaded.
- Connected `.vst3` effects to the existing `VST3EffectHost` instance path for processor setup, processing, normalized parameter access, and `IEditController::createView()` editor attachment.
- Added VST2 plug-in file selection and VST3 bundle-folder selection to the Audio Mixer insertion menu.
- Registered the existing private `Artifact.Layer.Abstract:Impl` partition in a private `CXX_MODULES` file set. CMake's Ninja dyndep rejected it while it was listed only as a private source, preventing the application module map from generating.
- Corrected the VST2 `effEditGetRect` / `effEditOpen` / `effEditClose` opcodes, queried the plugin editor rectangle, and resized the native host container before opening it.
- Corrected the hand-declared CLAP entry/factory, host, descriptor, plugin, process, audio-buffer, parameter-event, and parameter-info layouts against the official ABI. CLAP processing now uses preallocated scratch buffers and a bounded parameter-event handoff; plugin activation validates the single-main-bus mono/stereo layout this adapter can process. Mono/stereo host conversion maps mono input from stereo by averaging, duplicates mono into stereo inputs, duplicates mono outputs into stereo hosts, and averages stereo outputs for mono hosts. VST2 and VST3 use the same conversion policy with preallocated scratch. The Audio Mixer can insert `.clap` plugins and open an embedded CLAP GUI inside the same floating host window used by VST. The host resizes the client before attaching/showing the plugin UI and forwards host-driven resizing through `adjust_size()` / `set_size()`.
- CLAP `clap_host_gui` currently rejects plugin-originated resize/show/hide requests; host-driven resize is forwarded through `adjust_size()` / `set_size()`. VST2 windows use the fixed size returned by `effEditGetRect`; VST3 and CLAP can resize only where their editor API supports it. Supporting thread-safe CLAP plugin-originated requests requires a bounded UI-thread dispatch path. CLAP multi-bus, sidechain, non-mono/stereo layouts, floating-only, and GUI-less plugins are not yet supported through this insertion path.
- Build verification: `CLAPHost.ixx`, `CLAPHost.cppm`, and `VST3Loader.cppm` compiled successfully. CMake configuration and module scanning now register `Artifact.Layer.Abstract:Impl` in a private module file set. The first application compile exposed unqualified `int32` names in the new VST3 parameter loops; these are now qualified as `Steinberg::int32`. Generated MSVC commands and a serialized Ninja dependency build compiled `VSTHost.cppm`, `VSTEffect.cppm`, and `AudioMixerWidget.cppm` successfully. The application executable build progressed through 429 of 1,026 actions, then failed in unchanged `Artifact/src/Layer/ArtifactShapeLayer.cppm:4713` with C3483 (duplicate `this` lambda capture); that source has no worktree diff. A default-parallel attempt also hit a Ninja 1.13.2 internal `outputs_ready()` assertion. Earlier IFC C3474 errors did not recur in the serialized build. Full app linking and runtime remain unverified. The initial `ArtifactProxyWorker` RC failure was an environment `PATH` issue for the installed Windows SDK and was cleared by setting `PATH` for the build invocation.
- Plug-in loading, editor lifecycle, and audio processing have not been verified at runtime.

## Required behavior

1. An inserted VST2, VST3, or CLAP effect exposes one editor-open action from its existing effect controls.
2. The host asks the plugin for its native GUI and reports unsupported/missing editors without affecting audio processing.
3. The plugin UI is shown in a floating host window. The host resizes the editor when the format supports it; VST2 uses its reported fixed size. Plugin-originated CLAP resize/show/hide is still pending.
4. Closing the window destroys/detaches only the editor view; it does not remove, unload, deactivate, or reset the effect.
5. Editor create/show/resize/close calls run from the UI thread. A bounded cross-thread handoff for plugin-originated CLAP UI requests remains pending; those requests currently return unsupported.
6. Editor handles cannot outlive the plugin instance or its loaded library. Unload first closes the editor and then releases plugin resources.
7. Plugins without a native editor keep the existing generic parameter editor available.

## Implementation sequence

### A. Host lifecycle contracts

- Complete CLAP host GUI callbacks and floating-only API fallback after a bounded UI-thread dispatch contract is established.
- Add CLAP plugin descriptor selection for bundles that expose multiple audio effects and a bounded UI-thread dispatch path for plugin-originated requests.
- Define ownership and failure cleanup before connecting either format to widgets.

### B. Floating Qt host window

- Add a single reusable floating editor window/controller in the application repository.
- Use native window handles only at the plugin boundary. Keep Qt widget ownership, close handling, and DPI/resize behavior in the application layer.
- Do not add signal/slot connections; use the existing command/service path and a bounded UI-thread dispatch mechanism.

### C. Existing effect controls

- Add plugin descriptor selection for CLAP bundles that expose multiple audio effects.
- Keep effect enable/bypass, chain ordering, and plugin processing state independent from window visibility.
- Preserve generic parameter editing for headless plugins.

### D. Verification (requires user-authorized build/runtime work)

- Check CLAP embedded GUI, VST3 editor attach/resize/close, and no-editor fallback with representative plugins.
- Verify closing a window leaves audio processing and the inserted effect intact; unloading closes UI before plugin/library teardown.
- Check main-thread requirements, repeated open/close, plugin-requested resize, DPI changes, and host shutdown.

## Constraints

- Do not modify `ArtifactWidgets` or `libs/DiligentEngine` for this feature.
- Keep changes limited to audio plugin hosting and existing audio effect controls.
- Do not run CMake, builds, tests, or runtime checks without explicit user instruction.
- The current worktree has unrelated uncommitted changes; preserve them and avoid broad formatting or generated-file updates.

## Sources

- CLAP GUI contract: https://github.com/free-audio/clap/blob/main/include/clap/ext/gui.h
- VST3 editor contract: https://github.com/steinbergmedia/vst3_pluginterfaces/blob/master/gui/iplugview.h
- VST3 controller editor creation: https://github.com/steinbergmedia/vst3_pluginterfaces/blob/master/vst/ivsteditcontroller.h
