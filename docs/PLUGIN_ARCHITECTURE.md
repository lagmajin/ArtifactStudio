# Plugin Architecture

**作成日:** 2026-07-25

## Overview

The plugin system spans two projects — **ArtifactCore** defines the abstract interfaces and ABI contract, while **Artifact** provides the concrete loading, sandboxing, and adapter infrastructure.

```
External DLL (C ABI)
      │
      ▼ ArtifactPluginABI.h (C-compatible ABI types)
      │
      ├──► ArtifactPluginLoader (DLL scan + load + registry registration)
      │         │
      │         ├── loadDllPlugin()     — in-process DLL loading (QLibrary)
      │         └── loadSubprocessPlugin() — subprocess sandbox (TODO: runner executable)
      │
      ├──► LayerPluginAdapter (C vtable → C++ ILayerPlugin)
      │
      └──► PluginLayerFactory (scans + registers layer plugins)
```

## Module Map

| Module | Project | Role |
|--------|---------|------|
| `ArtifactCore.Plugin.Common` | ArtifactCore | `PluginCategory`, `PluginState`, `PluginDescriptor`, `LoadResult` |
| `ArtifactCore.Plugin.Registry` | ArtifactCore | `ArtifactPluginRegistry` singleton — thread-safe registration/query |
| `ArtifactCore.Plugin.Layer.Interface` | ArtifactCore | `ILayerPlugin` abstract base class |
| `Artifact.Plugin.Loader` | Artifact | `ArtifactPluginLoader` — DLL discovery and loading |
| `Artifact.Plugin.Sandbox` | Artifact | `ArtifactPluginSandbox` — subprocess isolation with heartbeat/crash recovery |
| `Artifact.Plugin.Layer.Adapter` | Artifact | `LayerPluginAdapter` — wraps C ABI vtable → C++ ILayerPlugin |
| `Artifact.Plugin.Layer.Factory` | Artifact | `PluginLayerFactory` — singleton that creates adapter-wrapped layer plugins |
| `(ArtifactPluginABI.h)` | ArtifactCore | C header — ABI types, vtable structs, export function signatures |

## Plugin Categories

- **`PluginCategory::Effect`** (0) — Image processing effects. Consumer: `ArtifactGlobalEffectManager`
- **`PluginCategory::Layer`** (1) — Custom layer types. Consumer: `PluginLayerFactory` → `LayerPluginAdapter`
- **`PluginCategory::Tool`** (2) — Editor tools. Consumer: TBD (ToolPluginVTable defined but no consumer yet)
- **`PluginCategory::ImportExport`** (3) — File import/export. Consumer: TBD

## Loading Pipeline

```
PluginLoader::discoverAndLoad()
  │
  ├── scanDirectory() — iterate *.dll in search paths
  │     │
  │     └── loadDllPlugin() — for each DLL:
  │           │
  │           ├── QLibrary::load()
  │           ├── Resolve ArtifactPlugin_GetAPIVersion()
  │           ├── Resolve ArtifactPlugin_GetPluginCount()
  │           ├── Resolve ArtifactPlugin_GetPlugin(index)
  │           ├── Validate API version (1..current)
  │           ├── Create PluginDescriptor per plugin
  │           ├── registry.registerPlugin(descriptor)   ← every consumer sees it
  │           ├── onPluginLoaded(dllPath, libHandle, desc) ← callback for category consumers
  │           └── Keep QLibrary loaded while consumers hold refs
  │
  └── loadSubprocessPlugin() — TODO (needs runner executable)
```

### Layer Plugin Registration Flow

```
PluginLayerFactory::scanAndRegister()
  │
  ├── Creates ArtifactPluginLoader
  ├── Sets onPluginLoaded callback:
  │     │
  │     └── For each layer-category DLL:
  │           ├── Resolve ArtifactPlugin_CreateLayer(id)
  │           ├── Resolve ArtifactPlugin_GetLayerVTable(id)
  │           ├── instance = CreateLayer(id)
  │           ├── vtable = GetLayerVTable(id)
  │           └── registerFromDll(pluginId, instance, *vtable)
  │
  └── loader.discoverAndLoad(["plugins/layers/"])
```

## C ABI Contract (ArtifactPluginABI.h)

Every plugin DLL **must** export these three functions:

```c
int                 ArtifactPlugin_GetAPIVersion(void);
int                 ArtifactPlugin_GetPluginCount(void);
ArtifactPluginDescriptor* ArtifactPlugin_GetPlugin(int index);
```

**Layer plugins may additionally export:**

```c
ArtifactPluginInstance           ArtifactPlugin_CreateLayer(const char* id);
void                             ArtifactPlugin_DestroyLayer(ArtifactPluginInstance);
const ArtifactLayerPluginVTable* ArtifactPlugin_GetLayerVTable(const char* id);
```

**Tool plugins may additionally export:**

```c
ArtifactPluginInstance           ArtifactPlugin_CreateTool(const char* id);
void                             ArtifactPlugin_DestroyTool(ArtifactPluginInstance);
const ArtifactToolPluginVTable*  ArtifactPlugin_GetToolVTable(const char* id);
```

### Layer Plugin VTable

```c
typedef struct {
    const char* (*getId)(ArtifactPluginInstance);
    const char* (*getDisplayName)(ArtifactPluginInstance);
    int         (*initialize)(ArtifactPluginInstance);
    void        (*shutdown)(ArtifactPluginInstance);
    void        (*drawContent)(ArtifactPluginInstance, const void* layerPtr,
                               float currentTime, int frameNumber,
                               int compWidth, int compHeight);
    int         (*serializeExtra)(ArtifactPluginInstance, const void* layerPtr, char** jsonOut);
    int         (*deserializeExtra)(ArtifactPluginInstance, void* layerPtr, const char* jsonIn);
    int         (*getPropertyGroupCount)(ArtifactPluginInstance);
    int         (*getPropertyGroupDef)(ArtifactPluginInstance, int index,
                                       char** nameOut, char** jsonSchemaOut);
} ArtifactLayerPluginVTable;
```

### Tool Plugin VTable

```c
typedef struct {
    const char* (*getId)(ArtifactPluginInstance);
    const char* (*getDisplayName)(ArtifactPluginInstance);
    const char* (*getDefaultShortcut)(ArtifactPluginInstance);
    int         (*activate)(ArtifactPluginInstance);
    void        (*deactivate)(ArtifactPluginInstance);
    int         (*onMousePress)(ArtifactPluginInstance, int x, int y,
                                int buttons, int modifiers, int64_t timestampMs);
    int         (*onMouseMove)(ArtifactPluginInstance, int x, int y,
                               int buttons, int modifiers, int64_t timestampMs);
    int         (*onMouseRelease)(ArtifactPluginInstance, int x, int y,
                                  int buttons, int modifiers, int64_t timestampMs);
    int         (*onKeyPress)(ArtifactPluginInstance, int keyCode,
                              int modifiers, int isAutoRepeat);
    int         (*onKeyRelease)(ArtifactPluginInstance, int keyCode, int modifiers);
    int         (*getCursorShape)(ArtifactPluginInstance);
} ArtifactToolPluginVTable;
```

## Subprocess Protocol (Planned)

The subprocess plugin system uses `PluginSandbox` which manages a child process with JSON-over-stdin/stdout IPC. The protocol spec is TBD in `docs/PLUGIN_SUBPROCESS_PROTOCOL.md` (not yet created).

Current stubs:
- `PluginLoader::loadSubprocessPlugin()` — returns descriptive error
- `PluginSandbox` — fully implemented (heartbeat 500ms, timeout 1000ms, max 3 crashes)

## Existing Consumers

- **`ArtifactGlobalEffectManager`** — loads plugin effects from `plugins/effects/` using `ArtifactPluginLoader`
- **`PluginLayerFactory`** — loads layer plugins via `scanAndRegister()` using the callback mechanism

## Creating a Plugin DLL (Quick Start)

1. Create a C/C++ DLL project
2. Export `ArtifactPlugin_GetAPIVersion`, `_GetPluginCount`, `_GetPlugin`
3. For layer plugins, also export `ArtifactPlugin_CreateLayer` and `ArtifactPlugin_GetLayerVTable`
4. Set `category` to `ARTIFACT_PLUGIN_CATEGORY_LAYER` (1) in the descriptor
5. Place the DLL in `plugins/layers/` (or `plugins/effects/` for effects)
6. Restart the application — `PluginLayerFactory::scanAndRegister()` discovers and loads it
