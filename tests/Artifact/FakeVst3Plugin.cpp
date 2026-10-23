#include <atomic>
#include <cstdint>
#include <cstring>
#include <string>

import VST3.Interfaces;

namespace {
using namespace Steinberg;
using namespace Steinberg::Vst;

constexpr char8 kPluginCid[16] = {
    static_cast<char8>(0x41), static_cast<char8>(0x72), static_cast<char8>(0x74),
    static_cast<char8>(0x69), static_cast<char8>(0x66), static_cast<char8>(0x61),
    static_cast<char8>(0x63), static_cast<char8>(0x74), static_cast<char8>(0x42),
    static_cast<char8>(0x75), static_cast<char8>(0x73), static_cast<char8>(0x54),
    static_cast<char8>(0x65), static_cast<char8>(0x73), static_cast<char8>(0x74),
    static_cast<char8>(0x31)};
constexpr char8 kInvalidBusPluginCid[16] = {
    static_cast<char8>(0x41), static_cast<char8>(0x72), static_cast<char8>(0x74),
    static_cast<char8>(0x69), static_cast<char8>(0x66), static_cast<char8>(0x61),
    static_cast<char8>(0x63), static_cast<char8>(0x74), static_cast<char8>(0x42),
    static_cast<char8>(0x75), static_cast<char8>(0x73), static_cast<char8>(0x54),
    static_cast<char8>(0x65), static_cast<char8>(0x73), static_cast<char8>(0x74),
    static_cast<char8>(0x32)};

bool hasIid(const TUID iid, FIDString iidText) {
    TUID expected{};
    return tuidFromString(iidText, expected) && tuidEquals(iid, expected);
}

class AudioBusPlugin : public IComponent, public IAudioProcessor {
public:
    tresult queryInterface(const TUID iid, void** object) override {
        if (!object) return kInvalidArgument;
        *object = nullptr;
        if (tuidEquals(iid, iidTuidIComponent()) ||
            hasIid(iid, iidStringIPluginBase()) ||
            hasIid(iid, iidStringFUnknown())) {
            *object = static_cast<IComponent*>(this);
        } else if (tuidEquals(iid, iidTuidIAudioProcessor())) {
            *object = static_cast<IAudioProcessor*>(this);
        } else {
            return kNoInterface;
        }
        addRef();
        return kResultOk;
    }

    uint32 addRef() override {
        return ++references_;
    }

    uint32 release() override {
        const uint32 remaining = --references_;
        if (remaining == 0) delete this;
        return remaining;
    }

    tresult initialize(FUnknown*) override { return kResultOk; }
    tresult terminate() override { return kResultOk; }

    tresult getControllerClassId(TUID) override { return kResultFalse; }
    tresult setIoMode(IoMode) override { return kResultOk; }

    int32 getBusCount(MediaType type, BusDirection direction) override {
        if (type != kAudio) return 0;
        return direction == kInput ? 2 : 1;
    }

    tresult getBusInfo(MediaType type, BusDirection direction, int32 index,
                       BusInfo& info) override {
        if (type != kAudio || (direction == kInput ? index < 0 || index >= 2
                                                   : index != 0))
            return kInvalidArgument;
        info = {};
        info.mediaType = kAudio;
        info.direction = direction;
        info.channelCount = direction == kInput && index == 1 ? 1 : 2;
        info.busType = direction == kInput && index == 1 ? kAux : kMain;
        info.flags = BusInfo::kDefaultActive;
        const char16* name = direction == kInput && index == 1
            ? u"Sidechain" : direction == kInput ? u"Main Input" : u"Main Output";
        std::memcpy(info.name, name,
                    (std::char_traits<char16>::length(name) + 1) * sizeof(char16));
        return kResultOk;
    }

    tresult getRoutingInfo(RoutingInfo&, RoutingInfo&) override {
        return kNotImplemented;
    }

    tresult activateBus(MediaType type, BusDirection direction, int32 index,
                        TBool state) override {
        if (type != kAudio || (direction == kInput ? index < 0 || index >= 2
                                                   : index != 0))
            return kInvalidArgument;
        if (direction == kInput) inputBusActive_[static_cast<size_t>(index)] = state != 0;
        else outputBusActive_ = state != 0;
        return kResultOk;
    }

    tresult setActive(TBool state) override {
        active_ = state != 0;
        return kResultOk;
    }
    tresult setState(IBStream*) override { return kResultOk; }
    tresult getState(IBStream*) override { return kResultOk; }

    tresult setBusArrangements(SpeakerArrangement* inputs, int32 numInputs,
                               SpeakerArrangement* outputs, int32 numOutputs) override {
        if (numInputs != 2 || numOutputs != 1 || !inputs || !outputs ||
            inputs[0] != kStereoArrangement || inputs[1] != kMonoArrangement ||
            outputs[0] != kStereoArrangement)
            return kResultFalse;
        return kResultOk;
    }

    tresult getBusArrangement(BusDirection direction, int32 index,
                              SpeakerArrangement& arrangement) override {
        if (direction == kInput && index == 0) arrangement = kStereoArrangement;
        else if (direction == kInput && index == 1) arrangement = kMonoArrangement;
        else if (direction == kOutput && index == 0) arrangement = kStereoArrangement;
        else return kInvalidArgument;
        return kResultOk;
    }

    tresult canProcessSampleSize(int32 size) override {
        return size == kSample32 ? kResultTrue : kResultFalse;
    }
    uint32 getLatencySamples() override { return 0; }
    tresult setupProcessing(ProcessSetup& setup) override {
        return setup.sampleRate > 0.0 && setup.maxSamplesPerBlock > 0
            ? kResultOk : kInvalidArgument;
    }
    tresult setProcessing(TBool state) override {
        processing_ = state != 0;
        return kResultOk;
    }
    tresult process(ProcessData& data) override {
        if (!active_ || !processing_ || data.symbolicSampleSize != kSample32 ||
            data.numSamples <= 0 || data.numInputs != 2 || data.numOutputs != 1 ||
            !data.inputs || !data.outputs || data.inputs[0].numChannels != 2 ||
            data.inputs[1].numChannels != 1 || data.outputs[0].numChannels != 2 ||
            !data.inputs[0].channelBuffers32 || !data.inputs[1].channelBuffers32 ||
            !data.outputs[0].channelBuffers32) return kResultFalse;
        for (int32 channel = 0; channel < 2; ++channel) {
            const Sample32* main = data.inputs[0].channelBuffers32[channel];
            Sample32* output = data.outputs[0].channelBuffers32[channel];
            if (!main || !output) return kInvalidArgument;
            for (int32 frame = 0; frame < data.numSamples; ++frame) {
                const Sample32* sideChain = data.inputs[1].channelBuffers32[0];
                if (!sideChain) return kInvalidArgument;
                output[frame] = main[frame] * 0.5f + sideChain[frame] * 0.25f;
            }
        }
        return kResultOk;
    }
    uint32 getTailSamples() override { return kNoTail; }

private:
    static constexpr SpeakerArrangement kMonoArrangement = 1u;
    static constexpr SpeakerArrangement kStereoArrangement = 3u;
    std::atomic<uint32> references_{1};
    bool active_ = false;
    bool processing_ = false;
    bool inputBusActive_[2]{};
    bool outputBusActive_ = false;
};

class InvalidAudioBusPlugin final : public AudioBusPlugin {
public:
    tresult getBusInfo(MediaType type, BusDirection direction, int32 index,
                       BusInfo& info) override {
        const tresult result = AudioBusPlugin::getBusInfo(
            type, direction, index, info);
        if (result == kResultOk && type == kAudio && direction == kInput && index == 0)
            info.channelCount = 65;
        return result;
    }
};

class TestFactory final : public IPluginFactory {
public:
    tresult queryInterface(const TUID iid, void** object) override {
        if (!object) return kInvalidArgument;
        *object = nullptr;
        if (hasIid(iid, iidStringIPluginFactory()) ||
            hasIid(iid, iidStringFUnknown())) {
            *object = static_cast<IPluginFactory*>(this);
            addRef();
            return kResultOk;
        }
        return kNoInterface;
    }
    uint32 addRef() override { return ++references_; }
    uint32 release() override { return --references_; }
    tresult getFactoryInfo(PFactoryInfo* info) override {
        if (!info) return kInvalidArgument;
        *info = {};
        std::strncpy(info->vendor, "ArtifactStudio", sizeof(info->vendor) - 1);
        std::strncpy(info->url, "https://example.invalid", sizeof(info->url) - 1);
        info->flags = PFactoryInfo::kUnicode;
        return kResultOk;
    }
    int32 countClasses() override { return 2; }
    tresult getClassInfo(int32 index, PClassInfo* info) override {
        if ((index != 0 && index != 1) || !info) return kInvalidArgument;
        *info = {};
        const char8* cid = index == 0 ? kPluginCid : kInvalidBusPluginCid;
        std::memcpy(info->cid, cid, sizeof(kPluginCid));
        info->cardinality = PClassInfo::kManyInstances;
        std::strncpy(info->category, kVstAudioEffectClass,
                     sizeof(info->category) - 1);
        std::strncpy(info->name, index == 0 ? "Artifact VST3 Audio Bus Test"
                                            : "Artifact VST3 Invalid Bus Test",
                     sizeof(info->name) - 1);
        return kResultOk;
    }
    tresult createInstance(FIDString cid, FIDString iid, void** object) override {
        if (!object) return kInvalidArgument;
        *object = nullptr;
        if (!cid || !iid ||
            (std::memcmp(cid, kPluginCid, sizeof(kPluginCid)) != 0 &&
             std::memcmp(cid, kInvalidBusPluginCid,
                         sizeof(kInvalidBusPluginCid)) != 0) ||
            !tuidEquals(iid, iidTuidIComponent())) return kNoInterface;
        *object = std::memcmp(cid, kInvalidBusPluginCid,
                              sizeof(kInvalidBusPluginCid)) == 0
            ? static_cast<IComponent*>(new InvalidAudioBusPlugin())
            : static_cast<IComponent*>(new AudioBusPlugin());
        return kResultOk;
    }

private:
    std::atomic<uint32> references_{1};
};

TestFactory gFactory;
} // namespace

#ifdef _WIN32
#define VST3_EXPORT extern "C" __declspec(dllexport)
#else
#define VST3_EXPORT extern "C" __attribute__((visibility("default")))
#endif

VST3_EXPORT Steinberg::IPluginFactory* GetPluginFactory() {
    gFactory.addRef();
    return &gFactory;
}
