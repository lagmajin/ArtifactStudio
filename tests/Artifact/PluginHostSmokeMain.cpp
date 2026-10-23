#include <cmath>
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <QJsonObject>

import Audio.Segment;
import Audio.Bus;
import Audio.Effect;
import Audio.Mixer;
import CLAP.Host;
import Memory.SharedPtr;
import VST3.Interfaces;

namespace {
constexpr int kFrames = 256;
constexpr float kExpectedCenteredBusSample = 0.175f * 0.7071067811865475f;

struct FakeClapThreadCheckProbe {
    bool initializedOnMainThread = false;
    bool initializedOnAudioThread = true;
    bool processedOnMainThread = true;
    bool processedOnAudioThread = false;
};

bool finiteAudio(const ArtifactCore::AudioSegment& audio) {
    for (const auto& channel : audio.channelData) {
        for (float sample : channel) {
            if (!std::isfinite(sample)) return false;
        }
    }
    return true;
}

class Vst3AudioBusFixtureEffect final : public ArtifactCore::AudioEffect {
public:
    explicit Vst3AudioBusFixtureEffect(Steinberg::Vst::VST3EffectHost& host)
        : host_(&host) {}

    bool configure() {
        if (!host_ || host_->inputBusCount() != 2 || host_->outputBusCount() != 1 ||
            host_->inputBusChannelCount(0) != 2 ||
            host_->inputBusChannelCount(1) != 1 ||
            host_->outputBusChannelCount(0) != 2) return false;
        inputLeft_.resize(kFrames);
        inputRight_.resize(kFrames);
        inputSideChain_.resize(kFrames);
        outputLeft_.resize(kFrames);
        outputRight_.resize(kFrames);
        inputs_ = {inputLeft_.data(), inputRight_.data(), inputSideChain_.data()};
        outputs_ = {outputLeft_.data(), outputRight_.data()};
        return true;
    }

    ~Vst3AudioBusFixtureEffect() override = default;

    ArtifactCore::String getName() const override {
        return ArtifactCore::String("VST3 Audio Bus fixture");
    }

    void process(ArtifactCore::AudioSegment& segment,
                 const ArtifactCore::AudioSegment* sideChain) override {
        if (segment.channelCount() != 2 || segment.frameCount() <= 0 ||
            segment.frameCount() > kFrames || !sideChain ||
            sideChain->channelCount() < 1 ||
            sideChain->frameCount() != segment.frameCount()) return;
        const int frames = segment.frameCount();
        const auto& side = sideChain->channelData[0];
        if (segment.channelData[0].size() < frames ||
            segment.channelData[1].size() < frames || side.size() < frames) return;
        std::copy_n(segment.channelData[0].constData(), frames, inputLeft_.data());
        std::copy_n(segment.channelData[1].constData(), frames, inputRight_.data());
        std::copy_n(side.constData(), frames, inputSideChain_.data());
        if (!host_ || !host_->processFloat(inputs_.data(), static_cast<int>(inputs_.size()),
                                           outputs_.data(), static_cast<int>(outputs_.size()),
                                           frames))
            return;
        std::copy_n(outputLeft_.data(), frames, segment.channelData[0].data());
        std::copy_n(outputRight_.data(), frames, segment.channelData[1].data());
    }

private:
    Steinberg::Vst::VST3EffectHost* host_ = nullptr;
    std::vector<float> inputLeft_;
    std::vector<float> inputRight_;
    std::vector<float> inputSideChain_;
    std::vector<float> outputLeft_;
    std::vector<float> outputRight_;
    std::array<Steinberg::Sample32*, 3> inputs_{};
    std::array<Steinberg::Sample32*, 2> outputs_{};
};

int runClap(const std::string& path) {
    clap::Host host;
    clap::Plugin* plugin = host.loadPlugin(path);
    if (!plugin) {
        std::cerr << "CLAP load failed: " << path << '\n';
        return 1;
    }
    std::atomic<uint32_t> offThreadCallbacks{0};
    std::thread offThreadDispatch([&host, &offThreadCallbacks] {
        offThreadCallbacks.store(host.dispatchMainThreadCallbacks(),
                                std::memory_order_release);
    });
    offThreadDispatch.join();
    const uint32_t callbacksDispatched = host.dispatchMainThreadCallbacks();
    const clap::Host::Requests hostRequests = host.takeRequests();
    const bool hostCallbacks = hostRequests.restart && hostRequests.process &&
                               hostRequests.mainThreadCallback;
    const auto consumedRequests = host.takeRequests();
    const bool requestsConsumed = !consumedRequests.restart &&
                                  !consumedRequests.process &&
                                  !consumedRequests.mainThreadCallback;
    if (!plugin->activate(48000.0, 1, kFrames) || !plugin->startProcessing()) {
        std::cerr << "CLAP activation failed: " << plugin->descriptor().name << '\n';
        host.unloadAll();
        return 1;
    }

    ArtifactCore::AudioSegment input;
    input.sampleRate = 48000;
    input.layout = ArtifactCore::AudioChannelLayout::Stereo;
    input.channelData.resize(2);
    input.channelData[0].resize(kFrames);
    input.channelData[1].resize(kFrames);
    for (int frame = 0; frame < kFrames; ++frame) {
        const float impulse = frame == 0 ? 0.25f : 0.0f;
        input.channelData[0][frame] = impulse;
        input.channelData[1][frame] = impulse;
    }
    std::array<float*, 2> directInputChannels{
        input.channelData[0].data(), input.channelData[1].data()};
    std::vector<float> directSideChainStorage(kFrames, 0.0f);
    std::array<float*, 2> directSideChainChannels{
        directSideChainStorage.data(), nullptr};
    std::array<float*, 2> directOutputChannels{};
    std::array<std::vector<float>, 2> directOutputStorage{
        std::vector<float>(kFrames), std::vector<float>(kFrames)};
    for (size_t channel = 0; channel < directOutputChannels.size(); ++channel)
        directOutputChannels[channel] = directOutputStorage[channel].data();
    std::array<clap::AudioBuffer, 2> directInputs{};
    directInputs[0].channels = directInputChannels;
    directInputs[0].channelCount = 2;
    directInputs[0].frameCount = kFrames;
    directInputs[1].channels = directSideChainChannels;
    directInputs[1].channelCount = 1;
    directInputs[1].frameCount = kFrames;
    clap::AudioBuffer directOutput{};
    directOutput.channels = directOutputChannels;
    directOutput.channelCount = 2;
    directOutput.frameCount = kFrames;
    clap::Process directProcess{};
    directProcess.framesCount = kFrames;
    directProcess.audioInputs = directInputs.data();
    directProcess.audioInputsCount = static_cast<uint32_t>(directInputs.size());
    directProcess.audioOutputs = &directOutput;
    directProcess.audioOutputsCount = 1;
    std::atomic<bool> directProcessSucceeded{false};
    std::thread directProcessThread([&] {
        directProcessSucceeded.store(plugin->process(directProcess),
                                     std::memory_order_release);
    });
    directProcessThread.join();
    const bool directProcessed = directProcessSucceeded.load(
        std::memory_order_acquire);
    const auto* threadCheckProbe = static_cast<const FakeClapThreadCheckProbe*>(
        plugin->getExtension("org.artifactstudio.test.thread-check"));
    const bool threadCheckPassed = threadCheckProbe &&
        threadCheckProbe->initializedOnMainThread &&
        !threadCheckProbe->initializedOnAudioThread &&
        !threadCheckProbe->processedOnMainThread &&
        threadCheckProbe->processedOnAudioThread;
    clap::Plugin::OutputParamEvent outputParamEvent{};
    const bool gotOutputParamEvent = plugin->popOutputParamEvent(outputParamEvent);

    ArtifactCore::AudioSegment output = input;
    output.zero();

    const bool processed = plugin->processSegment(input, output);
    const float firstOutputSample = output.channelData[0].isEmpty()
        ? 0.0f : output.channelData[0][0];

    ArtifactCore::AudioSegment sideChain;
    sideChain.sampleRate = 48000;
    sideChain.layout = ArtifactCore::AudioChannelLayout::Stereo;
    sideChain.channelData.resize(2);
    sideChain.channelData[0].resize(kFrames);
    sideChain.channelData[1].resize(kFrames);
    sideChain.channelData[0][0] = 0.2f;
    sideChain.channelData[1][0] = 0.2f;
    ArtifactCore::AudioSegment sideChainOutput = input;
    sideChainOutput.zero();
    const bool sideChainProcessed = plugin->processSegmentWithSideChain(
        input, &sideChain, sideChainOutput);
    const float sideChainFirstSample = sideChainOutput.channelData[0][0];
    bool audioBusIntegrated = false;
    {
        clap::Host busHost;
        clap::Plugin* busPlugin = busHost.loadPlugin(path);
        ArtifactCore::AudioBus bus;
        bus.setLayout(ArtifactCore::AudioChannelLayout::Stereo);
        bus.clearInput(kFrames, 48000);
        bus.addInput(input);
        bus.addSideChain(sideChain);
        auto effectOwner = busHost.createEffect(busPlugin);
        std::shared_ptr<ArtifactCore::AudioEffect> sharedEffect(std::move(effectOwner));
        bus.addEffect(ArtifactCore::SharedPtr<ArtifactCore::AudioEffect>(
            std::move(sharedEffect)));
        bus.process(bus.getOutputBuffer());
        audioBusIntegrated = busPlugin &&
            std::abs(bus.getOutputBuffer().channelData[0][0] -
                     kExpectedCenteredBusSample) < 1e-6f &&
            std::abs(bus.getOutputBuffer().channelData[1][0] -
                     kExpectedCenteredBusSample) < 1e-6f;
    }
    plugin->stopProcessing();
    plugin->deactivate();
    std::vector<uint8_t> state;
    const bool hasState = plugin->saveState(state);
    const bool stateRoundTrip = !hasState || plugin->loadState(state);
    const std::string pluginName = plugin->descriptor().name;
    const uint32_t parameterCount = plugin->paramsCount();
    const uint32_t latency = plugin->latencySamples();
    const uint32_t tail = plugin->tailSamples();
    QJsonObject savedEffect;
    {
        auto effect = host.createEffect(plugin);
        savedEffect = effect->toJson();
    }
    bool persistenceRoundTrip = false;
    {
        auto restored = host.createEffect(nullptr);
        restored->fromJson(savedEffect);
        const QJsonObject restoredJson = restored->toJson();
        persistenceRoundTrip =
            restoredJson.value("pluginId").toString() ==
                savedEffect.value("pluginId").toString() &&
            restoredJson.value("pluginPath").toString() ==
                savedEffect.value("pluginPath").toString();
        restored->process(output);
        persistenceRoundTrip = persistenceRoundTrip && finiteAudio(output);
    }
    constexpr float expectedFirstSample = 0.125f;
    constexpr float expectedSideChainSample = 0.175f;
    const bool audioMatches =
        std::abs(directOutputStorage[0][0] - expectedFirstSample) < 1e-6f &&
        std::abs(firstOutputSample - expectedFirstSample) < 1e-6f &&
        std::abs(sideChainFirstSample - expectedSideChainSample) < 1e-6f;
    const bool valid = processed && directProcessed && sideChainProcessed && audioMatches &&
                       gotOutputParamEvent && outputParamEvent.id == 1 &&
                       outputParamEvent.time == 7 &&
                       std::abs(outputParamEvent.value - 0.5) < 1e-9 &&
                       hostCallbacks && requestsConsumed && threadCheckPassed &&
                       offThreadCallbacks.load(std::memory_order_acquire) == 0 &&
                       callbacksDispatched == 1 &&
                       audioBusIntegrated &&
                       stateRoundTrip && persistenceRoundTrip && finiteAudio(output);
    std::cout << "CLAP " << pluginName
              << ": processed=" << processed
              << ", directProcessed=" << directProcessed
              << ", directSample=" << directOutputStorage[0][0]
              << ", outputParamEvent=" << gotOutputParamEvent
              << ", hostCallbacks=" << hostCallbacks
              << ", offThreadCallbacks="
              << offThreadCallbacks.load(std::memory_order_acquire)
              << ", callbacksDispatched=" << callbacksDispatched
              << ", threadCheck=" << threadCheckPassed
              << ", sideChainProcessed=" << sideChainProcessed
              << ", sideChainSample=" << sideChainFirstSample
              << ", audioBusIntegrated=" << audioBusIntegrated
              << ", params=" << parameterCount
              << ", firstSample=" << firstOutputSample
              << ", latency=" << latency
              << ", tail=" << tail
              << ", state=" << (hasState ? "supported" : "unsupported") << '\n';
    host.unloadAll();
    return valid ? 0 : 1;
}

int runVst3(const std::string& path, bool requireBusFixture = false) {
    Steinberg::Vst::VST3EffectHost host;
    if (!host.load(ArtifactCore::String(path))) {
        std::cerr << "VST3 load failed: " << host.lastError() << '\n';
        return 1;
    }
    if (!host.setup(48000.0, kFrames)) {
        std::cerr << "VST3 setup failed: " << host.lastError() << '\n';
        return 1;
    }
    if (!host.setProcessing(true)) {
        std::cerr << "VST3 processing start failed: " << host.lastError() << '\n';
        return 1;
    }

    const int inputCount = host.inputChannelCount();
    const int outputCount = host.outputChannelCount();
    if (inputCount < 0 || outputCount <= 0 || inputCount > 64 || outputCount > 64) {
        std::cerr << "Unsupported VST3 smoke-test bus layout.\n";
        return 1;
    }
    std::vector<std::vector<float>> inputStorage(
        static_cast<size_t>(inputCount), std::vector<float>(kFrames, 0.0f));
    std::vector<std::vector<float>> outputStorage(
        static_cast<size_t>(outputCount), std::vector<float>(kFrames, 0.0f));
    std::vector<Steinberg::Sample32*> inputs(
        std::max<size_t>(1, static_cast<size_t>(inputCount)));
    std::vector<Steinberg::Sample32*> outputs(
        std::max<size_t>(1, static_cast<size_t>(outputCount)));
    for (int channel = 0; channel < inputCount; ++channel) {
        inputStorage[static_cast<size_t>(channel)][0] =
            requireBusFixture && channel == 2 ? 0.2f : 0.25f;
        inputs[static_cast<size_t>(channel)] =
            inputStorage[static_cast<size_t>(channel)].data();
    }
    for (int channel = 0; channel < outputCount; ++channel) {
        outputs[static_cast<size_t>(channel)] =
            outputStorage[static_cast<size_t>(channel)].data();
    }
    const bool processed = host.processFloat(
        inputs.data(), inputCount, outputs.data(), outputCount, kFrames);
    bool finite = true;
    for (const auto& channel : outputStorage) {
        finite = finite && std::all_of(channel.begin(), channel.end(),
                                       [](float v) { return std::isfinite(v); });
    }
    std::vector<uint8_t> state;
    const bool stateRoundTrip = host.saveState(state) && host.loadState(state);
    const float firstOutputSample = outputStorage[0][0];
    const bool busLayoutMatches = !requireBusFixture ||
        (host.inputBusCount() == 2 && host.outputBusCount() == 1 &&
         host.inputBusChannelCount(0) == 2 &&
         host.inputBusChannelCount(1) == 1 &&
         host.outputBusChannelCount(0) == 2 &&
         host.inputBusType(0) == Steinberg::Vst::kMain &&
         host.inputBusType(1) == Steinberg::Vst::kAux &&
         host.outputBusType(0) == Steinberg::Vst::kMain);
    const bool outputMatches = !requireBusFixture ||
        std::abs(firstOutputSample - 0.175f) < 1e-6f;
    bool audioBusIntegrated = !requireBusFixture;
    if (requireBusFixture) {
        auto effect = std::make_shared<Vst3AudioBusFixtureEffect>(host);
        const bool effectLoaded = effect->configure();
        ArtifactCore::AudioSegment busInput;
        busInput.sampleRate = 48000;
        busInput.layout = ArtifactCore::AudioChannelLayout::Stereo;
        busInput.channelData.resize(2);
        busInput.channelData[0].resize(kFrames);
        busInput.channelData[1].resize(kFrames);
        busInput.channelData[0][0] = 0.25f;
        busInput.channelData[1][0] = 0.25f;
        ArtifactCore::AudioSegment busSideChain = busInput;
        busSideChain.zero();
        busSideChain.channelData[0][0] = 0.2f;
        busSideChain.channelData[1][0] = 0.2f;
        ArtifactCore::AudioBus bus;
        bus.setLayout(ArtifactCore::AudioChannelLayout::Stereo);
        bus.clearInput(kFrames, 48000);
        bus.addInput(busInput);
        bus.addSideChain(busSideChain);
        bus.addEffect(ArtifactCore::SharedPtr<ArtifactCore::AudioEffect>(
            std::static_pointer_cast<ArtifactCore::AudioEffect>(effect)));
        bus.process(bus.getOutputBuffer());
        audioBusIntegrated = effectLoaded &&
            std::abs(bus.getOutputBuffer().channelData[0][0] -
                     kExpectedCenteredBusSample) < 1e-6f &&
            std::abs(bus.getOutputBuffer().channelData[1][0] -
                     kExpectedCenteredBusSample) < 1e-6f;
    }
    std::cout << "VST3 " << host.className()
              << ": processed=" << processed
              << ", inputBuses=" << host.inputBusCount()
              << ", outputBuses=" << host.outputBusCount()
              << ", firstSample=" << firstOutputSample
              << ", audioBusIntegrated=" << audioBusIntegrated
              << ", params=" << host.parameterCount()
              << ", latency=" << host.latencySamples()
              << ", tail=" << host.tailSamples()
              << ", state=" << stateRoundTrip << '\n';
    host.setProcessing(false);
    host.setActive(false);
    return processed && finite && stateRoundTrip && busLayoutMatches && outputMatches &&
                   audioBusIntegrated
        ? 0 : 1;
}

int listVst3(const std::string& path) {
    Steinberg::Vst::VST3Module module;
    if (!module.load(ArtifactCore::String(path))) {
        std::cerr << "VST3 module load failed: " << module.lastError() << '\n';
        return 1;
    }
    std::cout << "VST3 classes: " << module.classCount() << '\n';
    for (int32_t index = 0; index < module.classCount(); ++index) {
        Steinberg::PClassInfo info{};
        if (!module.classInfo(index, info)) continue;
        char classId[33]{};
        Steinberg::tuidToString(info.cid, classId);
        std::cout << "  " << info.category << " | " << info.name
                  << " | " << classId;
        void* componentProbe = module.createInstance(
            info.cid, Steinberg::Vst::iidTuidIComponent());
        std::cout << " | component-probe="
                  << (componentProbe ? "ok" : module.lastError());
        if (componentProbe)
            static_cast<Steinberg::Vst::IComponent*>(componentProbe)->release();
        void* processorProbe = module.createInstance(
            info.cid, Steinberg::Vst::iidTuidIAudioProcessor());
        std::cout << " | processor-probe="
                  << (processorProbe ? "ok" : module.lastError());
        if (processorProbe)
            static_cast<Steinberg::Vst::IAudioProcessor*>(processorProbe)->release();
        std::cout << '\n';
    }
    return 0;
}

int runVst3InvalidBus(const std::string& path) {
    Steinberg::Vst::VST3EffectHost host;
    const bool loaded = host.load(ArtifactCore::String(path), 1);
    const std::string error = host.lastError();
    const bool rejectedForLimit = !loaded &&
        error.find("channel layout exceeds the host limit") != std::string::npos;
    std::cout << "VST3 invalid bus rejected=" << rejectedForLimit
              << ", error=" << error << '\n';
    return rejectedForLimit ? 0 : 1;
}

int runCombinedAudioBus(const std::string& vst3Path,
                        const std::string& clapPath) {
    Steinberg::Vst::VST3EffectHost vst3Host;
    if (!vst3Host.load(ArtifactCore::String(vst3Path)) ||
        !vst3Host.setup(48000.0, kFrames) || !vst3Host.setProcessing(true)) {
        std::cerr << "VST3 setup failed for combined AudioBus test: "
                  << vst3Host.lastError() << '\n';
        return 1;
    }

    clap::Host clapHost;
    clap::Plugin* clapPlugin = clapHost.loadPlugin(clapPath);
    auto clapEffect = clapHost.createEffect(clapPlugin);
    auto vst3Effect = std::make_shared<Vst3AudioBusFixtureEffect>(vst3Host);
    const bool vst3Configured = vst3Effect->configure();
    if (!clapPlugin || !clapEffect || !vst3Configured) {
        std::cerr << "Could not create both plugin effects for AudioBus.\n";
        return 1;
    }

    ArtifactCore::AudioSegment input;
    input.sampleRate = 48000;
    input.layout = ArtifactCore::AudioChannelLayout::Stereo;
    input.channelData.resize(2);
    input.channelData[0].resize(kFrames);
    input.channelData[1].resize(kFrames);
    input.channelData[0][0] = 0.25f;
    input.channelData[1][0] = 0.25f;
    ArtifactCore::AudioSegment sideChain = input;
    sideChain.zero();
    sideChain.channelData[0][0] = 0.2f;
    sideChain.channelData[1][0] = 0.2f;

    ArtifactCore::AudioMixer mixer;
    const auto bus = mixer.createBus(ArtifactCore::String("CLAP + VST3"));
    const auto sideChainBus = mixer.createBus(ArtifactCore::String("Sidechain"));
    if (!bus || !sideChainBus ||
        mixer.disconnect(sideChainBus) != ArtifactCore::AudioRoutingResult::Applied ||
        mixer.addSideChainSend(sideChainBus, bus, 1.0f, true) !=
            ArtifactCore::AudioRoutingResult::Applied) {
        std::cerr << "Could not create AudioMixer routing for plugin chain.\n";
        return 1;
    }
    bus->setLayout(ArtifactCore::AudioChannelLayout::Stereo);
    sideChainBus->setLayout(ArtifactCore::AudioChannelLayout::Stereo);
    bus->clearInput(kFrames, 48000);
    sideChainBus->clearInput(kFrames, 48000);
    bus->addInput(input);
    sideChainBus->addInput(sideChain);
    std::shared_ptr<ArtifactCore::AudioEffect> sharedClapEffect(
        std::move(clapEffect));
    bus->addEffect(ArtifactCore::SharedPtr<ArtifactCore::AudioEffect>(
        std::move(sharedClapEffect)));
    bus->addEffect(ArtifactCore::SharedPtr<ArtifactCore::AudioEffect>(
        std::static_pointer_cast<ArtifactCore::AudioEffect>(vst3Effect)));
    ArtifactCore::AudioSegment output;
    output.sampleRate = 48000;
    output.layout = ArtifactCore::AudioChannelLayout::Stereo;
    output.channelData.resize(2);
    output.setFrameCount(kFrames);
    mixer.process(output);

    const float expected = 0.1375f * 0.5f;
    const bool valid = output.channelCount() == 2 &&
        std::abs(output.channelData[0][0] - expected) < 1e-6f &&
        std::abs(output.channelData[1][0] - expected) < 1e-6f &&
        finiteAudio(output);
    std::cout << "AudioBus CLAP->VST3: firstLeft=" << output.channelData[0][0]
              << ", firstRight=" << output.channelData[1][0]
              << ", expected=" << expected << '\n';
    vst3Host.setProcessing(false);
    vst3Host.setActive(false);
    // The AudioBus still owns the ClapEffect here. Let it release the plugin
    // before clapHost is destroyed so the effect cannot unload it twice.
    return valid ? 0 : 1;
}
} // namespace

int main(int argc, char** argv) {
    if (argc != 3 && argc != 4) {
        std::cerr << "Usage: ArtifactPluginHostSmoke <clap|vst3|vst3-list|vst3-bus|vst3-invalid-bus|audio-bus-combined> <plugin-path> [clap-path]\n";
        return 2;
    }
    const std::string format = argv[1];
    if (format == "clap") return runClap(argv[2]);
    if (format == "vst3") return runVst3(argv[2]);
    if (format == "vst3-bus") return runVst3(argv[2], true);
    if (format == "vst3-list") return listVst3(argv[2]);
    if (format == "vst3-invalid-bus") return runVst3InvalidBus(argv[2]);
    if (format == "audio-bus-combined" && argc == 4)
        return runCombinedAudioBus(argv[2], argv[3]);
    std::cerr << "Invalid format or argument count.\n";
    return 2;
}
