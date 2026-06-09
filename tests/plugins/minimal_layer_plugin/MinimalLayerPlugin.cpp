#define ARTIFACT_PLUGIN_DLL_EXPORT
#include "ArtifactPluginABI.h"

#include <string>

static std::string g_pluginId = "com.example.minimal_layer";
static std::string g_displayName = "Minimal Layer Plugin";

extern "C" {

ARTIFACT_PLUGIN_API int ArtifactPlugin_GetAPIVersion() {
    return ARTIFACT_PLUGIN_API_VERSION;
}

ARTIFACT_PLUGIN_API int ArtifactPlugin_GetPluginCount() {
    return 1;
}

ARTIFACT_PLUGIN_API const ArtifactPluginDescriptor* ArtifactPlugin_GetPlugin(int index) {
    static ArtifactPluginDescriptor desc = {
        g_pluginId.c_str(),
        g_displayName.c_str(),
        "1.0.0",
        "Example Author",
        "A minimal layer plugin for testing the plugin system",
        ARTIFACT_PLUGIN_CATEGORY_LAYER,
        ARTIFACT_PLUGIN_API_VERSION
    };

    if (index == 0) return &desc;
    return nullptr;
}

ARTIFACT_PLUGIN_API ArtifactPluginInstance ArtifactPlugin_CreateLayer(const char* id) {
    (void)id;
    return nullptr;
}

ARTIFACT_PLUGIN_API void ArtifactPlugin_DestroyLayer(ArtifactPluginInstance instance) {
    (void)instance;
}

ARTIFACT_PLUGIN_API const ArtifactLayerPluginVTable* ArtifactPlugin_GetLayerVTable(const char* id) {
    (void)id;
    return nullptr;
}

ARTIFACT_PLUGIN_API ArtifactPluginInstance ArtifactPlugin_CreateTool(const char* id) {
    (void)id;
    return nullptr;
}

ARTIFACT_PLUGIN_API void ArtifactPlugin_DestroyTool(ArtifactPluginInstance instance) {
    (void)instance;
}

ARTIFACT_PLUGIN_API const ArtifactToolPluginVTable* ArtifactPlugin_GetToolVTable(const char* id) {
    (void)id;
    return nullptr;
}

}
