#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#ifdef _WIN32
#define CLAP_EXPORT extern "C" __declspec(dllexport)
#define CLAP_ABI __cdecl
#else
#define CLAP_EXPORT extern "C" __attribute__((visibility("default")))
#define CLAP_ABI
#endif

struct ClapVersion { uint32_t major, minor, revision; };
struct ClapPluginDescriptor {
    ClapVersion clap_version;
    const char* id;
    const char* name;
    const char* vendor;
    const char* url;
    const char* manual_url;
    const char* support_url;
    const char* version;
    const char* description;
    const char** features;
};
struct ClapHost;
struct ClapPlugin;
struct ClapHostThreadCheck {
    bool (CLAP_ABI *is_main_thread)(const ClapHost*);
    bool (CLAP_ABI *is_audio_thread)(const ClapHost*);
};
struct ClapAudioBuffer {
    float** data32;
    double** data64;
    uint32_t channel_count;
    uint32_t latency;
    uint64_t constant_mask;
};
struct ClapEventHeader {
    uint32_t size, time;
    uint16_t space_id, type;
    uint32_t flags;
};
struct ClapEventParamValue {
    ClapEventHeader header;
    uint32_t param_id;
    void* cookie;
    int32_t note_id;
    int16_t port_index, channel, key;
    double value;
};
struct ClapInputEvents {
    void* ctx;
    uint32_t (CLAP_ABI *size)(const ClapInputEvents*);
    const ClapEventHeader* (CLAP_ABI *get)(const ClapInputEvents*, uint32_t);
};
struct ClapOutputEvents {
    void* ctx;
    bool (CLAP_ABI *try_push)(const ClapOutputEvents*, const ClapEventHeader*);
};
struct ClapProcess {
    int64_t steady_time;
    uint32_t frames_count;
    const void* transport;
    const ClapAudioBuffer* audio_inputs;
    ClapAudioBuffer* audio_outputs;
    uint32_t audio_inputs_count, audio_outputs_count;
    const ClapInputEvents* in_events;
    const ClapOutputEvents* out_events;
};
struct ClapPlugin {
    const ClapPluginDescriptor* desc;
    void* plugin_data;
    bool (CLAP_ABI *init)(const ClapPlugin*);
    void (CLAP_ABI *destroy)(const ClapPlugin*);
    bool (CLAP_ABI *activate)(const ClapPlugin*, double, uint32_t, uint32_t);
    void (CLAP_ABI *deactivate)(const ClapPlugin*);
    bool (CLAP_ABI *start_processing)(const ClapPlugin*);
    void (CLAP_ABI *stop_processing)(const ClapPlugin*);
    void (CLAP_ABI *reset)(const ClapPlugin*);
    int32_t (CLAP_ABI *process)(const ClapPlugin*, const ClapProcess*);
    const void* (CLAP_ABI *get_extension)(const ClapPlugin*, const char*);
    void (CLAP_ABI *on_main_thread)(const ClapPlugin*);
};
struct ClapIStream {
    void* ctx;
    int64_t (CLAP_ABI *read)(const ClapIStream*, void*, uint64_t);
};
struct ClapOStream {
    void* ctx;
    int64_t (CLAP_ABI *write)(const ClapOStream*, const void*, uint64_t);
};
struct ClapPluginState {
    bool (CLAP_ABI *save)(const ClapPlugin*, const ClapOStream*);
    bool (CLAP_ABI *load)(const ClapPlugin*, const ClapIStream*);
};
struct ClapPluginLatency { uint32_t (CLAP_ABI *get)(const ClapPlugin*); };
struct ClapPluginTail { uint32_t (CLAP_ABI *get)(const ClapPlugin*); };
struct ClapAudioPortInfo {
    uint32_t id;
    char name[256];
    uint32_t flags, channel_count;
    const char* port_type;
    uint32_t in_place_pair;
};
struct ClapPluginAudioPorts {
    uint32_t (CLAP_ABI *count)(const ClapPlugin*, bool);
    bool (CLAP_ABI *get)(const ClapPlugin*, uint32_t, bool, ClapAudioPortInfo*);
};
struct ClapParamInfo {
    uint32_t id, flags;
    void* cookie;
    char name[256];
    char module[1024];
    double min_value, max_value, default_value;
};
struct ClapPluginParams {
    uint32_t (CLAP_ABI *count)(const ClapPlugin*);
    bool (CLAP_ABI *get_info)(const ClapPlugin*, uint32_t, ClapParamInfo*);
    bool (CLAP_ABI *get_value)(const ClapPlugin*, uint32_t, double*);
    bool (CLAP_ABI *value_to_text)(const ClapPlugin*, uint32_t, double, char*, uint32_t);
    bool (CLAP_ABI *text_to_value)(const ClapPlugin*, uint32_t, const char*, double*);
    void (CLAP_ABI *flush)(const ClapPlugin*, const ClapInputEvents*, const ClapOutputEvents*);
};
struct ThreadCheckProbe {
    bool initializedOnMainThread = false;
    bool initializedOnAudioThread = true;
    bool processedOnMainThread = true;
    bool processedOnAudioThread = false;
};
struct ClapHost {
    ClapVersion clap_version;
    void* host_data;
    const char* name;
    const char* vendor;
    const char* url;
    const char* version;
    const void* (CLAP_ABI *get_extension)(const ClapHost*, const char*);
    void (CLAP_ABI *request_restart)(const ClapHost*);
    void (CLAP_ABI *request_process)(const ClapHost*);
    void (CLAP_ABI *request_callback)(const ClapHost*);
};
struct ClapPluginFactory {
    uint32_t (CLAP_ABI *get_plugin_count)(const ClapPluginFactory*);
    const ClapPluginDescriptor* (CLAP_ABI *get_plugin_descriptor)(const ClapPluginFactory*, uint32_t);
    const ClapPlugin* (CLAP_ABI *create_plugin)(const ClapPluginFactory*, const ClapHost*, const char*);
};
struct ClapPluginEntry {
    ClapVersion clap_version;
    bool (CLAP_ABI *init)(const char*);
    void (CLAP_ABI *deinit)();
    const void* (CLAP_ABI *get_factory)(const char*);
};

namespace {
struct PluginState {
    double gain = 0.5;
    const ClapHost* host = nullptr;
    const ClapHostThreadCheck* threadCheck = nullptr;
    ThreadCheckProbe threadCheckProbe;
};
const char* features[] = {"audio-effect", "stereo", nullptr};
const ClapPluginDescriptor descriptor{
    {1, 2, 10}, "org.artifactstudio.test.gain", "Artifact CLAP Test Gain",
    "ArtifactStudio", "", "", "", "1.0.0", "Deterministic host smoke fixture", features};

bool CLAP_ABI pluginInit(const ClapPlugin* plugin) {
    auto* state = static_cast<PluginState*>(plugin->plugin_data);
    if (!state || !state->host || !state->host->get_extension) return false;
    state->threadCheck = static_cast<const ClapHostThreadCheck*>(
        state->host->get_extension(state->host, "clap.thread-check"));
    if (!state->threadCheck || !state->threadCheck->is_main_thread ||
        !state->threadCheck->is_audio_thread) return false;
    state->threadCheckProbe.initializedOnMainThread =
        state->threadCheck->is_main_thread(state->host);
    state->threadCheckProbe.initializedOnAudioThread =
        state->threadCheck->is_audio_thread(state->host);
    if (!state->threadCheckProbe.initializedOnMainThread ||
        state->threadCheckProbe.initializedOnAudioThread) return false;
    if (state->host->request_restart) state->host->request_restart(state->host);
    if (state->host->request_process) state->host->request_process(state->host);
    if (state->host->request_callback) state->host->request_callback(state->host);
    return true;
}
void CLAP_ABI pluginDestroy(const ClapPlugin* plugin) {
    delete static_cast<PluginState*>(plugin->plugin_data);
    delete plugin;
}
bool CLAP_ABI pluginActivate(const ClapPlugin*, double sampleRate, uint32_t minFrames,
                             uint32_t maxFrames) {
    return sampleRate > 0.0 && minFrames > 0 && maxFrames >= minFrames;
}
void CLAP_ABI pluginDeactivate(const ClapPlugin*) {}
bool CLAP_ABI pluginStart(const ClapPlugin*) { return true; }
void CLAP_ABI pluginStop(const ClapPlugin*) {}
void CLAP_ABI pluginReset(const ClapPlugin*) {}
int32_t CLAP_ABI pluginProcess(const ClapPlugin* plugin, const ClapProcess* process) {
    if (!process || !process->audio_inputs || !process->audio_outputs ||
        process->audio_inputs_count != 2 || process->audio_outputs_count != 1) return 0;
    const auto& input = process->audio_inputs[0];
    const auto& sideChain = process->audio_inputs[1];
    auto& output = process->audio_outputs[0];
    if (input.channel_count != 2 || output.channel_count != 2 ||
        sideChain.channel_count != 1 || !sideChain.data32 || !sideChain.data32[0] ||
        !input.data32 || !output.data32 || !input.data32[0] || !input.data32[1] ||
        !output.data32[0] || !output.data32[1]) return 0;
    auto* state = static_cast<PluginState*>(plugin->plugin_data);
    if (!state->threadCheck || !state->threadCheck->is_main_thread ||
        !state->threadCheck->is_audio_thread) return 0;
    state->threadCheckProbe.processedOnMainThread =
        state->threadCheck->is_main_thread(state->host);
    state->threadCheckProbe.processedOnAudioThread =
        state->threadCheck->is_audio_thread(state->host);
    if (!state->threadCheckProbe.processedOnAudioThread) return 0;
    if (process->in_events && process->in_events->size && process->in_events->get) {
        const uint32_t eventCount = process->in_events->size(process->in_events);
        for (uint32_t i = 0; i < eventCount; ++i) {
            const auto* header = process->in_events->get(process->in_events, i);
            if (header && header->type == 5 && header->size >= sizeof(ClapEventParamValue)) {
                const auto* event = reinterpret_cast<const ClapEventParamValue*>(header);
                if (event->param_id == 1) state->gain = std::clamp(event->value, 0.0, 1.0);
            }
        }
    }
    for (uint32_t channel = 0; channel < 2; ++channel) {
        for (uint32_t frame = 0; frame < process->frames_count; ++frame)
            output.data32[channel][frame] =
                input.data32[channel][frame] * static_cast<float>(state->gain) +
                sideChain.data32[0][frame] * 0.25f;
    }
    if (process->out_events && process->out_events->try_push) {
        ClapEventParamValue event{};
        event.header.size = sizeof(event);
        event.header.time = 7;
        event.header.space_id = 0;
        event.header.type = 5;
        event.param_id = 1;
        event.note_id = -1;
        event.port_index = -1;
        event.channel = -1;
        event.key = -1;
        event.value = state->gain;
        (void)process->out_events->try_push(process->out_events, &event.header);
    }
    return 1;
}
bool CLAP_ABI stateSave(const ClapPlugin* plugin, const ClapOStream* stream) {
    const double gain = static_cast<const PluginState*>(plugin->plugin_data)->gain;
    return stream && stream->write && stream->write(stream, &gain, sizeof(gain)) == sizeof(gain);
}
bool CLAP_ABI stateLoad(const ClapPlugin* plugin, const ClapIStream* stream) {
    double gain = 0.0;
    if (!stream || !stream->read || stream->read(stream, &gain, sizeof(gain)) != sizeof(gain)) return false;
    static_cast<PluginState*>(plugin->plugin_data)->gain = std::clamp(gain, 0.0, 1.0);
    return true;
}
uint32_t CLAP_ABI latencyGet(const ClapPlugin*) { return 0; }
uint32_t CLAP_ABI tailGet(const ClapPlugin*) { return 0; }
uint32_t CLAP_ABI portsCount(const ClapPlugin*, bool input) {
    return input ? 2 : 1;
}
bool CLAP_ABI portsGet(const ClapPlugin*, uint32_t index, bool input, ClapAudioPortInfo* info) {
    if (!info || (input ? index >= 2 : index != 0)) return false;
    *info = {};
    info->id = input ? (index == 0 ? 1 : 3) : 2;
    const char* name = input ? (index == 0 ? "Stereo Input" : "Sidechain")
                             : "Stereo Output";
    std::strncpy(info->name, name, sizeof(info->name) - 1);
    info->flags = (!input || index == 0) ? 1 : 0;
    info->channel_count = input && index == 1 ? 1 : 2;
    info->port_type = info->channel_count == 1 ? "mono" : "stereo";
    info->in_place_pair = input ? (index == 0 ? 2 : UINT32_MAX) : 1;
    return true;
}
uint32_t CLAP_ABI paramsCount(const ClapPlugin*) { return 1; }
bool CLAP_ABI paramsInfo(const ClapPlugin*, uint32_t index, ClapParamInfo* info) {
    if (index != 0 || !info) return false;
    *info = {};
    info->id = 1;
    info->flags = 1;
    std::strncpy(info->name, "Gain", sizeof(info->name) - 1);
    info->min_value = 0.0;
    info->max_value = 1.0;
    info->default_value = 0.5;
    return true;
}
bool CLAP_ABI paramsValue(const ClapPlugin* plugin, uint32_t id, double* value) {
    if (id != 1 || !value) return false;
    *value = static_cast<const PluginState*>(plugin->plugin_data)->gain;
    return true;
}
bool CLAP_ABI paramsValueText(const ClapPlugin*, uint32_t id, double value, char* text, uint32_t capacity) {
    if (id != 1 || !text || capacity == 0) return false;
    return std::snprintf(text, capacity, "%.3f", value) > 0;
}
bool CLAP_ABI paramsTextValue(const ClapPlugin*, uint32_t id, const char* text, double* value) {
    if (id != 1 || !text || !value) return false;
    char* end = nullptr;
    *value = std::strtod(text, &end);
    return end != text && *end == '\0' && *value >= 0.0 && *value <= 1.0;
}
void CLAP_ABI paramsFlush(const ClapPlugin*, const ClapInputEvents*, const ClapOutputEvents*) {}

const ClapPluginState stateExtension{stateSave, stateLoad};
const ClapPluginLatency latencyExtension{latencyGet};
const ClapPluginTail tailExtension{tailGet};
const ClapPluginAudioPorts audioPortsExtension{portsCount, portsGet};
const ClapPluginParams paramsExtension{paramsCount, paramsInfo, paramsValue,
                                       paramsValueText, paramsTextValue, paramsFlush};
const void* CLAP_ABI getExtension(const ClapPlugin* plugin, const char* id) {
    if (!id) return nullptr;
    if (std::strcmp(id, "org.artifactstudio.test.thread-check") == 0)
        return &static_cast<const PluginState*>(plugin->plugin_data)->threadCheckProbe;
    if (std::strcmp(id, "clap.state") == 0) return &stateExtension;
    if (std::strcmp(id, "clap.latency") == 0) return &latencyExtension;
    if (std::strcmp(id, "clap.tail") == 0) return &tailExtension;
    if (std::strcmp(id, "clap.audio-ports") == 0) return &audioPortsExtension;
    if (std::strcmp(id, "clap.params") == 0) return &paramsExtension;
    return nullptr;
}
void CLAP_ABI pluginOnMainThread(const ClapPlugin*) {}

const ClapPlugin* CLAP_ABI createPlugin(const ClapPluginFactory*, const ClapHost* host,
                                        const char* id) {
    if (!id || std::strcmp(id, descriptor.id) != 0) return nullptr;
    auto* state = new PluginState{};
    state->host = host;
    auto* plugin = new ClapPlugin{&descriptor, state, pluginInit, pluginDestroy,
        pluginActivate, pluginDeactivate, pluginStart, pluginStop, pluginReset,
        pluginProcess, getExtension, pluginOnMainThread};
    return plugin;
}
uint32_t CLAP_ABI factoryCount(const ClapPluginFactory*) { return 1; }
const ClapPluginDescriptor* CLAP_ABI factoryDescriptor(const ClapPluginFactory*, uint32_t index) {
    return index == 0 ? &descriptor : nullptr;
}
const ClapPluginFactory factory{factoryCount, factoryDescriptor, createPlugin};
bool CLAP_ABI entryInit(const char*) { return true; }
void CLAP_ABI entryDeinit() {}
const void* CLAP_ABI entryGetFactory(const char* id) {
    return id && std::strcmp(id, "clap.plugin-factory") == 0 ? &factory : nullptr;
}
} // namespace

CLAP_EXPORT ClapPluginEntry clap_entry{{1, 2, 10}, entryInit, entryDeinit, entryGetFactory};
