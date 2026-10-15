#include <QApplication>
#include <QByteArray>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QLabel>
#include <QMainWindow>
#include <QStatusBar>
#include <QString>
#include <QStringList>
#include <QtLogging>
#include <QVBoxLayout>
#include <QVector>
#include <QWidget>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <initializer_list>
#include <limits>
#include <utility>

#include "../ArtifactTestMcpServer.h"

import Audio.Bus;
import Audio.Effect;
import Audio.Effect.Compressor;
import Audio.Mixer;
import Audio.Segment;
import Artifact.Widgets.AudioMixer;
import Core.ArtifactString;
import Memory.SharedPtr;
import Utils.Id;

namespace {

using ArtifactCore::AudioBus;
using ArtifactCore::AudioBusKind;
using ArtifactCore::AudioChannelLayout;
using ArtifactCore::AudioCompressor;
using ArtifactCore::AudioEffect;
using ArtifactCore::AudioMixer;
using ArtifactCore::AudioRoutingResult;
using ArtifactCore::AudioSegment;
using ArtifactCore::SharedPtr;

SharedPtr<AudioEffect> restoreAudioEffect(const QString& type,
                                          const QJsonObject& payload)
{
    if (type != QStringLiteral("compressor")) return {};
    auto compressor = ArtifactCore::makeShared<AudioCompressor>();
    compressor->fromJson(payload);
    return compressor;
}

void installAudioEffectFactory(AudioMixer& mixer)
{
    mixer.setEffectFactoryHook(&restoreAudioEffect);
}

QString busName(const SharedPtr<AudioBus>& bus)
{
    if (!bus) return {};
    const auto name = bus->getName();
    return QString::fromUtf8(name.data(), static_cast<qsizetype>(name.length()));
}

QString busKindName(const AudioBusKind kind)
{
    switch (kind) {
    case AudioBusKind::Master: return QStringLiteral("master");
    case AudioBusKind::Layer: return QStringLiteral("layer");
    case AudioBusKind::Group: return QStringLiteral("group");
    case AudioBusKind::Return: return QStringLiteral("return");
    case AudioBusKind::Vca: return QStringLiteral("vca");
    }
    return QStringLiteral("unknown");
}

AudioSegment constantStereo(const int frames, const float value)
{
    AudioSegment segment;
    segment.layout = AudioChannelLayout::Stereo;
    segment.sampleRate = 48000;
    segment.channelData.resize(2);
    segment.setFrameCount(frames);
    for (auto& channel : segment.channelData) channel.fill(value);
    return segment;
}

double rms(const QVector<float>& samples)
{
    if (samples.isEmpty()) return 0.0;
    double sum = 0.0;
    for (const float sample : samples) {
        sum += static_cast<double>(sample) * sample;
    }
    return std::sqrt(sum / static_cast<double>(samples.size()));
}

struct Fixture {
    SharedPtr<AudioBus> dialogueGroup;
    SharedPtr<AudioBus> musicGroup;
    SharedPtr<AudioBus> stemGroup;
    SharedPtr<AudioBus> fxReturn;
    SharedPtr<AudioBus> dialogueVca;
    SharedPtr<AudioBus> dialogueTrimVca;
    SharedPtr<AudioBus> dialogue;
    SharedPtr<AudioBus> music;
    SharedPtr<AudioBus> ambience;
    SharedPtr<AudioBus> fxBed;
    SharedPtr<AudioCompressor> fxCompressor;
};

Fixture createFixture(AudioMixer& mixer)
{
    Fixture fixture;
    const auto createBus = [&mixer](const QString& name, AudioBusKind kind) {
        const QByteArray utf8 = name.toUtf8();
        return mixer.createBus(ArtifactCore::String(utf8.constData()), kind);
    };
    const auto createLayerBus = [&mixer](const QString& name) {
        auto bus = mixer.ensureLayerBus(ArtifactCore::LayerID{});
        if (bus) {
            const QByteArray utf8 = name.toUtf8();
            bus->setName(ArtifactCore::ZeroString(utf8.constData()));
        }
        return bus;
    };
    fixture.dialogueGroup = createBus(QStringLiteral("Dialogue Group"), AudioBusKind::Group);
    fixture.musicGroup = createBus(QStringLiteral("Music Group"), AudioBusKind::Group);
    fixture.stemGroup = createBus(QStringLiteral("Stem Bus"), AudioBusKind::Group);
    fixture.fxReturn = createBus(QStringLiteral("FX Return"), AudioBusKind::Return);
    fixture.dialogueVca = createBus(QStringLiteral("Dialogue VCA"), AudioBusKind::Vca);
    fixture.dialogueTrimVca = createBus(QStringLiteral("Dialogue Trim VCA"), AudioBusKind::Vca);
    fixture.dialogue = createLayerBus(QStringLiteral("Dialogue"));
    fixture.music = createLayerBus(QStringLiteral("Music"));
    fixture.ambience = createLayerBus(QStringLiteral("Ambience"));
    fixture.fxBed = createLayerBus(QStringLiteral("FX Bed"));
    fixture.fxCompressor = ArtifactCore::makeShared<AudioCompressor>();

    const auto master = mixer.getMasterBus();
    if (!fixture.dialogueGroup || !fixture.musicGroup || !fixture.stemGroup ||
        !fixture.fxReturn || !fixture.dialogueVca || !fixture.dialogueTrimVca ||
        !fixture.dialogue || !fixture.music || !fixture.ambience ||
        !fixture.fxBed || !fixture.fxCompressor || !master) {
        return {};
    }

    if (mixer.connect(fixture.dialogue, fixture.dialogueGroup) !=
            AudioRoutingResult::Applied ||
        mixer.connect(fixture.music, fixture.musicGroup) !=
            AudioRoutingResult::Applied ||
        mixer.connect(fixture.ambience, fixture.musicGroup) !=
            AudioRoutingResult::Applied ||
        mixer.connect(fixture.fxBed, fixture.fxReturn) !=
            AudioRoutingResult::Applied ||
        mixer.connect(fixture.dialogueGroup, fixture.stemGroup) !=
            AudioRoutingResult::Applied ||
        mixer.connect(fixture.musicGroup, fixture.stemGroup) !=
            AudioRoutingResult::Applied ||
        mixer.connect(fixture.stemGroup, master) !=
            AudioRoutingResult::Applied ||
        mixer.connect(fixture.fxReturn, master) !=
            AudioRoutingResult::Applied ||
        mixer.assignVcaMember(fixture.dialogueVca, fixture.dialogue) !=
            AudioRoutingResult::Applied ||
        mixer.assignVcaMember(fixture.dialogueTrimVca, fixture.dialogue) !=
            AudioRoutingResult::Applied ||
        mixer.addSideChainSend(fixture.dialogue, fixture.fxReturn, 0.25f, true) !=
            AudioRoutingResult::Applied ||
        mixer.addSideChainSend(fixture.music, fixture.fxReturn, 0.5f, false) !=
            AudioRoutingResult::Applied) {
        return {};
    }
    fixture.fxCompressor->setThreshold(-40.0f);
    fixture.fxCompressor->setRatio(4.0f);
    fixture.fxCompressor->setAttack(0.1f);
    fixture.fxCompressor->setRelease(100.0f);
    fixture.fxCompressor->setSideChain(true);
    fixture.fxReturn->addEffect(fixture.fxCompressor);
    return fixture;
}

bool hasCompleteFixture(const Fixture& fixture)
{
    return fixture.dialogueGroup && fixture.musicGroup && fixture.stemGroup &&
        fixture.fxReturn && fixture.dialogueVca && fixture.dialogueTrimVca &&
        fixture.dialogue && fixture.music && fixture.ambience && fixture.fxBed &&
        fixture.fxCompressor;
}

bool stageInputs(const Fixture& fixture, const int frames,
                 const float dialogue, const float music, const float ambience,
                 const float fxBed)
{
    if (!fixture.dialogue || !fixture.music || !fixture.ambience ||
        !fixture.fxReturn || !fixture.fxBed ||
        frames < 1 || frames > 8192) {
        return false;
    }
    for (const auto& bus : {fixture.dialogue, fixture.music,
                            fixture.ambience, fixture.fxBed}) {
        bus->clearInput(frames, 48000);
    }
    // Match the existing caller contract: sidechain destinations are prepared
    // before a block so each probe measures this block rather than accumulated history.
    fixture.fxReturn->clearInput(frames, 48000);
    fixture.dialogue->addInput(constantStereo(frames, dialogue));
    fixture.music->addInput(constantStereo(frames, music));
    fixture.ambience->addInput(constantStereo(frames, ambience));
    fixture.fxBed->addInput(constantStereo(frames, fxBed));
    return true;
}

QJsonObject processProbe(AudioMixer& mixer, const Fixture& fixture,
                         const int frames, const float dialogue,
                         const float music, const float ambience,
                         const float fxBed = 0.0f)
{
    if (!stageInputs(fixture, frames, dialogue, music, ambience, fxBed)) {
        return {{QStringLiteral("error"),
                 QStringLiteral("Invalid fixture or frame count")}};
    }
    AudioSegment output = constantStereo(frames, 0.0f);
    mixer.process(output);
    QJsonArray left;
    QJsonArray right;
    for (int index = 0; index < output.frameCount(); ++index) {
        left.append(output.channelData[0][index]);
        right.append(output.channelData[1][index]);
    }
    return {
        {QStringLiteral("frames"), output.frameCount()},
        {QStringLiteral("sampleRate"), output.sampleRate},
        {QStringLiteral("leftRms"), rms(output.channelData[0])},
        {QStringLiteral("rightRms"), rms(output.channelData[1])},
        {QStringLiteral("leftPeak"), fixture.dialogueGroup
             ? mixer.getMasterBus()->getPeakLevel(0) : 0.0f},
        {QStringLiteral("rightPeak"), fixture.dialogueGroup
             ? mixer.getMasterBus()->getPeakLevel(1) : 0.0f},
        {QStringLiteral("fxReturnOutputRms"), fixture.fxReturn
             ? rms(fixture.fxReturn->getOutputBuffer().channelData[0]) : 0.0},
        {QStringLiteral("fxReturnGainReduction"), fixture.fxReturn
             ? fixture.fxReturn->getGainReduction() : 1.0},
        {QStringLiteral("leftSamples"), left},
        {QStringLiteral("rightSamples"), right},
        {QStringLiteral("fxReturnSidechainRms"), fixture.fxReturn
             ? rms(fixture.fxReturn->getSideChainBuffer().channelData[0]) : 0.0},
    };
}

QJsonObject routingContract()
{
    AudioMixer mixer;
    installAudioEffectFactory(mixer);
    const Fixture fixture = createFixture(mixer);
    QJsonObject checks;
    if (!hasCompleteFixture(fixture)) {
        return {{QStringLiteral("passed"), false},
                {QStringLiteral("error"), QStringLiteral("Could not create routing fixture")}};
    }

    checks[QStringLiteral("fixtureCreated")] = mixer.busCount() == 11;
    checks[QStringLiteral("dialogueRoute")] =
        mixer.getRoutingTarget(fixture.dialogue) == fixture.dialogueGroup;
    checks[QStringLiteral("groupRoute")] =
        mixer.getRoutingTarget(fixture.dialogueGroup) == fixture.stemGroup;
    checks[QStringLiteral("nestedStemRoute")] =
        mixer.getRoutingTarget(fixture.stemGroup) == mixer.getMasterBus();
    checks[QStringLiteral("fxBedRoutesThroughReturn")] =
        mixer.getRoutingTarget(fixture.fxBed) == fixture.fxReturn &&
        mixer.getRoutingTarget(fixture.fxReturn) == mixer.getMasterBus() &&
        fixture.fxReturn->getEffectCount() == 1 &&
        fixture.fxCompressor->getParameterValue(ArtifactCore::String("sidechain")) == 1.0f;
    checks[QStringLiteral("multipleVcaMembership")] =
        mixer.getVcaMembers(fixture.dialogueVca).size() == 1 &&
        mixer.getVcaMembers(fixture.dialogueTrimVca).size() == 1;
    checks[QStringLiteral("preAndPostFaderSends")] =
        mixer.getSideChainSends(fixture.dialogue).size() == 1 &&
        mixer.getSideChainSends(fixture.music).size() == 1 &&
        mixer.isSideChainSendPreFader(fixture.dialogue, fixture.fxReturn) &&
        !mixer.isSideChainSendPreFader(fixture.music, fixture.fxReturn);

    const auto rejectedCycle = mixer.connect(fixture.dialogueGroup, fixture.dialogue);
    checks[QStringLiteral("cycleRejected")] =
        rejectedCycle == AudioRoutingResult::CycleDetected &&
        mixer.getRoutingTarget(fixture.dialogue) == fixture.dialogueGroup;

    AudioMixer cleanupMixer;
    const auto cleanupSource = cleanupMixer.createBus(QStringLiteral("Cleanup Source"));
    const auto cleanupGroup = cleanupMixer.createBus(
        QStringLiteral("Cleanup Group"), AudioBusKind::Group);
    const auto cleanupReturn = cleanupMixer.createBus(
        QStringLiteral("Cleanup Return"), AudioBusKind::Return);
    const auto cleanupVca = cleanupMixer.createBus(
        QStringLiteral("Cleanup VCA"), AudioBusKind::Vca);
    const auto cleanupTrimVca = cleanupMixer.createBus(
        QStringLiteral("Cleanup Trim VCA"), AudioBusKind::Vca);
    bool cleanupGraphCreated = cleanupSource && cleanupGroup && cleanupReturn &&
        cleanupVca && cleanupTrimVca &&
        cleanupMixer.connect(cleanupSource, cleanupGroup) == AudioRoutingResult::Applied &&
        cleanupMixer.connect(cleanupGroup, cleanupReturn) == AudioRoutingResult::Applied &&
        cleanupMixer.connect(cleanupReturn, cleanupMixer.getMasterBus()) == AudioRoutingResult::Applied &&
        cleanupMixer.addSideChainSend(cleanupSource, cleanupReturn, 0.4f, false) ==
            AudioRoutingResult::Applied &&
        cleanupMixer.assignVcaMember(cleanupVca, cleanupSource) == AudioRoutingResult::Applied &&
        cleanupMixer.assignVcaMember(cleanupTrimVca, cleanupSource) == AudioRoutingResult::Applied;
    if (cleanupGraphCreated) {
        cleanupMixer.removeBus(cleanupReturn);
        const bool returnCleanup =
            cleanupMixer.findBusByName(QStringLiteral("Cleanup Return")) == nullptr &&
            cleanupMixer.getRoutingTarget(cleanupGroup) == cleanupMixer.getMasterBus() &&
            cleanupMixer.getSideChainSends(cleanupSource).empty() &&
            cleanupMixer.getVcaMembers(cleanupVca).size() == 1 &&
            cleanupMixer.getVcaMembers(cleanupTrimVca).size() == 1;
        cleanupMixer.removeBus(cleanupSource);
        checks[QStringLiteral("busRemovalCleansRoutesSendsAndVcaMembers")] =
            returnCleanup &&
            cleanupMixer.findBusByName(QStringLiteral("Cleanup Source")) == nullptr &&
            cleanupMixer.getVcaMembers(cleanupVca).empty() &&
            cleanupMixer.getVcaMembers(cleanupTrimVca).empty();
    } else {
        checks[QStringLiteral("busRemovalCleansRoutesSendsAndVcaMembers")] = false;
    }

    const auto baseline = processProbe(mixer, fixture, 256, 0.20f, 0.10f, 0.05f);
    const double baselineRms = baseline.value(QStringLiteral("leftRms")).toDouble();
    checks[QStringLiteral("primaryRoutingMixesToMaster")] =
        std::abs(baselineRms - 0.35) < 0.003;
    checks[QStringLiteral("sidechainSumsIndependentSources")] =
        std::abs(baseline.value(QStringLiteral("fxReturnSidechainRms")).toDouble()
                 - 0.10) < 0.003;

    AudioMixer sidechainMixer;
    installAudioEffectFactory(sidechainMixer);
    const Fixture sidechainFixture = createFixture(sidechainMixer);
    if (!hasCompleteFixture(sidechainFixture)) {
        return {{QStringLiteral("passed"), false},
                {QStringLiteral("error"), QStringLiteral("Could not create sidechain fixture")}};
    }
    const auto sidechainDrivenMix = processProbe(
        sidechainMixer, sidechainFixture, 256, 0.20f, 0.10f, 0.05f, 0.20f);

    AudioMixer programDetectorMixer;
    installAudioEffectFactory(programDetectorMixer);
    const Fixture programDetectorFixture = createFixture(programDetectorMixer);
    if (!hasCompleteFixture(programDetectorFixture)) {
        return {{QStringLiteral("passed"), false},
                {QStringLiteral("error"), QStringLiteral("Could not create program detector fixture")}};
    }
    programDetectorFixture.fxCompressor->setSideChain(false);
    const auto programDetectorMix = processProbe(
        programDetectorMixer, programDetectorFixture, 256,
        0.20f, 0.10f, 0.05f, 0.20f);

    AudioMixer bypassMixer;
    installAudioEffectFactory(bypassMixer);
    const Fixture bypassFixture = createFixture(bypassMixer);
    if (!hasCompleteFixture(bypassFixture)) {
        return {{QStringLiteral("passed"), false},
                {QStringLiteral("error"), QStringLiteral("Could not create bypass fixture")}};
    }
    bypassFixture.fxCompressor->setBypass(true);
    const auto bypassedFxMix = processProbe(
        bypassMixer, bypassFixture, 256, 0.20f, 0.10f, 0.05f, 0.20f);
    checks[QStringLiteral("sidechainCompressesRoutedFxBed")] =
        sidechainDrivenMix.value(QStringLiteral("fxReturnOutputRms")).toDouble() < 0.20 &&
        sidechainDrivenMix.value(QStringLiteral("fxReturnGainReduction")).toDouble() < 1.0;
    checks[QStringLiteral("sidechainSelectsDetectorInput")] =
        sidechainDrivenMix.value(QStringLiteral("fxReturnGainReduction")).toDouble() >
        programDetectorMix.value(QStringLiteral("fxReturnGainReduction")).toDouble();
    checks[QStringLiteral("compressorBypassRestoresFxBedLevel")] =
        std::abs(bypassedFxMix.value(QStringLiteral("fxReturnOutputRms")).toDouble()
                 - 0.20) < 0.003 &&
        bypassedFxMix.value(QStringLiteral("fxReturnGainReduction")).toDouble() == 1.0;

    fixture.dialogue->setVolume(-6.0206f);
    const auto preFaderMix = processProbe(mixer, fixture, 256, 0.20f, 0.10f, 0.05f);
    checks[QStringLiteral("preFaderSendIgnoresSourceFader")] =
        std::abs(preFaderMix.value(QStringLiteral("fxReturnSidechainRms")).toDouble()
                 - 0.10) < 0.003;
    fixture.dialogue->setVolume(0.0f);

    fixture.music->setVolume(-6.0206f);
    const auto postFaderMix = processProbe(mixer, fixture, 256, 0.20f, 0.10f, 0.05f);
    checks[QStringLiteral("postFaderSendFollowsSourceFader")] =
        std::abs(postFaderMix.value(QStringLiteral("fxReturnSidechainRms")).toDouble()
                 - 0.075) < 0.003;
    fixture.music->setVolume(0.0f);

    fixture.dialogueGroup->setPan(1.0f);
    const auto pannedMix = processProbe(mixer, fixture, 256, 0.20f, 0.10f, 0.05f);
    checks[QStringLiteral("groupPanChangesStereoBalance")] =
        std::abs(pannedMix.value(QStringLiteral("leftRms")).toDouble() - 0.15) < 0.003 &&
        std::abs(pannedMix.value(QStringLiteral("rightRms")).toDouble() - 0.35) < 0.003;
    fixture.dialogueGroup->setPan(0.0f);

    fixture.dialogueVca->setVolume(-6.0206f);
    const auto vcaMix = processProbe(mixer, fixture, 256, 0.20f, 0.10f, 0.05f);
    checks[QStringLiteral("vcaControlsMemberGain")] =
        std::abs(vcaMix.value(QStringLiteral("leftRms")).toDouble() - 0.25) < 0.004;

    fixture.dialogueTrimVca->setVolume(-6.0206f);
    const auto multipleVcaMix = processProbe(mixer, fixture, 256, 0.20f, 0.10f, 0.05f);
    checks[QStringLiteral("multipleVcasMultiplyGain")] =
        std::abs(multipleVcaMix.value(QStringLiteral("leftRms")).toDouble() - 0.20) < 0.004;

    fixture.dialogueVca->setVolume(0.0f);
    fixture.dialogueTrimVca->setVolume(0.0f);
    fixture.dialogue->setMute(true);
    const auto mutedMix = processProbe(mixer, fixture, 256, 0.20f, 0.10f, 0.05f);
    checks[QStringLiteral("muteRemovesRoutedSignal")] =
        std::abs(mutedMix.value(QStringLiteral("leftRms")).toDouble() - 0.15) < 0.003;

    fixture.dialogue->setMute(false);
    fixture.music->setSolo(true);
    const auto soloMix = processProbe(mixer, fixture, 256, 0.20f, 0.10f, 0.05f);
    checks[QStringLiteral("soloKeepsOnlySelectedBranch")] =
        std::abs(soloMix.value(QStringLiteral("leftRms")).toDouble() - 0.10) < 0.003;
    fixture.music->setSolo(false);

    const auto master = mixer.getMasterBus();
    master->setVolume(-6.0206f);
    const auto masterGainMix = processProbe(mixer, fixture, 256, 0.20f, 0.10f, 0.05f);
    checks[QStringLiteral("masterGainControlsFinalOutput")] =
        std::abs(masterGainMix.value(QStringLiteral("leftRms")).toDouble() - 0.175) < 0.003;
    master->setVolume(0.0f);
    master->setMute(true);
    const auto masterMuteMix = processProbe(mixer, fixture, 256, 0.20f, 0.10f, 0.05f);
    checks[QStringLiteral("masterMuteSilencesFinalOutput")] =
        masterMuteMix.value(QStringLiteral("leftRms")).toDouble() < 0.001;
    master->setMute(false);

    const auto serialized = mixer.serialize();
    AudioMixer restored;
    installAudioEffectFactory(restored);
    const auto staleBus = restored.createBus(QStringLiteral("Stale Bus"));
    const bool staleBusCreated = static_cast<bool>(staleBus);
    const bool restoredOk = restored.deserialize(serialized);
    const auto restoredDialogue = restored.findBusByName(QStringLiteral("Dialogue"));
    const auto restoredVca = restored.findBusByName(QStringLiteral("Dialogue VCA"));
    const auto restoredTrimVca = restored.findBusByName(QStringLiteral("Dialogue Trim VCA"));
    const auto restoredDialogueGroup = restored.findBusByName(QStringLiteral("Dialogue Group"));
    const auto restoredStemGroup = restored.findBusByName(QStringLiteral("Stem Bus"));
    const auto restoredFxReturn = restored.findBusByName(QStringLiteral("FX Return"));
    const auto restoredMusicGroup = restored.findBusByName(QStringLiteral("Music Group"));
    const auto restoredMusic = restored.findBusByName(QStringLiteral("Music"));
    const auto restoredFxBed = restored.findBusByName(QStringLiteral("FX Bed"));
    const auto restoredCompressor = restoredFxReturn
        ? ArtifactCore::dynamicPointerCast<AudioCompressor>(restoredFxReturn->getEffect(0))
        : SharedPtr<AudioCompressor>{};
    Fixture restoredFixture;
    restoredFixture.dialogueGroup = restoredDialogueGroup;
    restoredFixture.musicGroup = restoredMusicGroup;
    restoredFixture.stemGroup = restoredStemGroup;
    restoredFixture.fxReturn = restoredFxReturn;
    restoredFixture.dialogueVca = restoredVca;
    restoredFixture.dialogueTrimVca = restoredTrimVca;
    restoredFixture.dialogue = restoredDialogue;
    restoredFixture.music = restoredMusic;
    restoredFixture.ambience = restored.findBusByName(QStringLiteral("Ambience"));
    restoredFixture.fxBed = restoredFxBed;
    restoredFixture.fxCompressor = restoredCompressor;
    const auto restoredMix = processProbe(
        restored, restoredFixture, 256, 0.20f, 0.10f, 0.05f);
    const auto restoredDuckedMix = processProbe(
        restored, restoredFixture, 256, 0.20f, 0.10f, 0.05f, 0.20f);
    checks[QStringLiteral("serializationRestoresRoutingAndControls")] =
        staleBusCreated && restoredOk && !restored.findBusByName(QStringLiteral("Stale Bus")) &&
        restored.serialize() == serialized && restoredDialogue &&
        restoredVca && restoredTrimVca && restoredDialogueGroup &&
        restoredMusicGroup && restoredStemGroup && restoredFxReturn &&
        restoredMusic && restoredFixture.ambience && restoredFxBed &&
        restoredCompressor && restoredFxReturn->getEffectCount() == 1 &&
        restoredCompressor->getParameterValue(ArtifactCore::String("sidechain")) == 1.0f &&
        restored.getRoutingTarget(restoredDialogue) == restoredDialogueGroup &&
        restored.getRoutingTarget(restoredDialogueGroup) == restoredStemGroup &&
        restored.getRoutingTarget(restoredMusicGroup) == restoredStemGroup &&
        restored.getRoutingTarget(restoredStemGroup) == restored.getMasterBus() &&
        restored.getRoutingTarget(restoredFxReturn) == restored.getMasterBus() &&
        restored.getVcaMembers(restoredVca).size() == 1 &&
        restored.getVcaMembers(restoredTrimVca).size() == 1 &&
        restored.getSideChainSends(restoredDialogue).size() == 1 &&
        restored.getSideChainSends(restoredMusic).size() == 1 &&
        restored.isSideChainSendPreFader(restoredDialogue, restoredFxReturn) &&
        !restored.isSideChainSendPreFader(restoredMusic, restoredFxReturn) &&
        std::abs(restoredMix.value(QStringLiteral("leftRms")).toDouble() - 0.35) < 0.003 &&
        std::abs(restoredMix.value(QStringLiteral("fxReturnSidechainRms")).toDouble()
                 - 0.10) < 0.003 &&
        restoredDuckedMix.value(QStringLiteral("fxReturnOutputRms")).toDouble() < 0.20;

    QJsonArray failures;
    for (auto it = checks.cbegin(); it != checks.cend(); ++it) {
        if (!it.value().toBool()) failures.append(it.key());
    }
    return {{QStringLiteral("passed"), failures.isEmpty()},
            {QStringLiteral("checks"), checks},
            {QStringLiteral("failures"), failures},
            {QStringLiteral("baseline"), baseline},
            {QStringLiteral("sidechainDuckedFx"), sidechainDrivenMix},
            {QStringLiteral("programDetectedFx"), programDetectorMix},
            {QStringLiteral("bypassedFx"), bypassedFxMix},
            {QStringLiteral("preFaderSend"), preFaderMix},
            {QStringLiteral("postFaderSend"), postFaderMix},
            {QStringLiteral("vca"), vcaMix},
            {QStringLiteral("multipleVca"), multipleVcaMix},
            {QStringLiteral("restoredMix"), restoredMix},
            {QStringLiteral("restoredDuckedMix"), restoredDuckedMix},
            {QStringLiteral("muted"), mutedMix},
            {QStringLiteral("solo"), soloMix},
            {QStringLiteral("panned"), pannedMix},
            {QStringLiteral("masterGain"), masterGainMix},
            {QStringLiteral("masterMute"), masterMuteMix},
            {QStringLiteral("cycleResult"),
             AudioMixer::routingResultDescription(rejectedCycle)}};
}

class AudioBusViewerWindow final : public QMainWindow {
public:
    AudioBusViewerWindow()
    {
        setWindowTitle(QStringLiteral("Artifact Audio Bus Test Viewer"));
        resize(1160, 540);
        auto* central = new QWidget(this);
        auto* layout = new QVBoxLayout(central);
        auto* instructions = new QLabel(
            QStringLiteral("Audio buses: nested Group-to-Stem routing, Pre/Post-Fader sends that duck a Return compressor, "
                           "multiple VCA gain, mute, solo, gain and pan. "
                           "Offline probes use deterministic 48 kHz stereo blocks; no audio device is opened."),
            central);
        instructions->setWordWrap(true);
        layout->addWidget(instructions);
        graphSummary_ = new QLabel(central);
        graphSummary_->setObjectName(QStringLiteral("AudioBusRoutingGraphSummary"));
        graphSummary_->setAccessibleName(QStringLiteral("Audio routing graph summary"));
        graphSummary_->setWordWrap(true);
        layout->addWidget(graphSummary_);
        mixerWidget_ = new Artifact::AudioMixerWidget(&mixer_, central);
        layout->addWidget(mixerWidget_, 1);
        setCentralWidget(central);
        statusBar()->showMessage(QStringLiteral("Ready — synthetic signal is processed only on request"));
        installAudioEffectFactory(mixer_);
        resetFixture();
    }

    bool resetFixture()
    {
        AudioMixer defaults;
        const Fixture fixture = createFixture(defaults);
        if (!hasCompleteFixture(fixture) || !mixer_.deserialize(defaults.serialize())) {
            statusBar()->showMessage(QStringLiteral("Could not initialize audio bus fixture"));
            return false;
        }
        lastProbe_ = {};
        mixerWidget_->refreshBuses();
        refreshGraphSummary();
        statusBar()->showMessage(QStringLiteral("Routing fixture restored"));
        return true;
    }

    AudioMixer& mixer() { return mixer_; }

    QJsonObject state() const
    {
        QJsonArray buses;
        for (const auto& bus : mixer_.getAllBuses()) {
            if (!bus) continue;
            const auto route = mixer_.getRoutingTarget(bus);
            QJsonArray sends;
            for (const auto& [target, amount] : mixer_.getSideChainSends(bus)) {
                sends.append(QJsonObject{
                    {QStringLiteral("target"), busName(target)},
                    {QStringLiteral("amount"), amount},
                    {QStringLiteral("preFader"),
                     mixer_.isSideChainSendPreFader(bus, target)}});
            }
            QJsonObject serialized = bus->toJson();
            serialized[QStringLiteral("name")] = busName(bus);
            serialized[QStringLiteral("kind")] = busKindName(mixer_.busKind(bus));
            serialized[QStringLiteral("routeTarget")] = busName(route);
            serialized[QStringLiteral("peakLeft")] = bus->getPeakLevel(0);
            serialized[QStringLiteral("peakRight")] = bus->getPeakLevel(1);
            serialized[QStringLiteral("rmsLeft")] = bus->getRMSLevel(0);
            serialized[QStringLiteral("rmsRight")] = bus->getRMSLevel(1);
            serialized[QStringLiteral("sends")] = sends;
            if (mixer_.busKind(bus) == AudioBusKind::Vca) {
                QJsonArray members;
                for (const auto& member : mixer_.getVcaMembers(bus)) {
                    members.append(busName(member));
                }
                serialized[QStringLiteral("members")] = members;
            }
            buses.append(serialized);
        }
        return {{QStringLiteral("busCount"), mixer_.busCount()},
                {QStringLiteral("buses"), buses},
                {QStringLiteral("graphSummary"), graphSummary_
                    ? graphSummary_->text() : QString{}},
                {QStringLiteral("graphLatencySamples"), mixer_.graphLatencySamples()},
                {QStringLiteral("graphTailSamples"), mixer_.graphTailSamples()},
                {QStringLiteral("lastProbe"), lastProbe_}};
    }

    QJsonObject process(const QJsonObject& arguments)
    {
        const int frames = arguments.value(QStringLiteral("frames")).toInt(256);
        if (frames < 1 || frames > 8192) {
            return {{QStringLiteral("error"), QStringLiteral("frames must be 1..8192")}};
        }
        const auto fixture = currentFixture();
        if (!hasCompleteFixture(fixture)) {
            return {{QStringLiteral("error"), QStringLiteral("fixture buses are missing")}};
        }
        const double dialogue = arguments.value(QStringLiteral("dialogue")).toDouble(0.20);
        const double music = arguments.value(QStringLiteral("music")).toDouble(0.10);
        const double ambience = arguments.value(QStringLiteral("ambience")).toDouble(0.05);
        const double fxBed = arguments.value(QStringLiteral("fxBed")).toDouble(0.0);
        const auto validAmplitude = [](double value) {
            return std::isfinite(value) && value >= -1.0 && value <= 1.0;
        };
        if (!validAmplitude(dialogue) || !validAmplitude(music) ||
            !validAmplitude(ambience) || !validAmplitude(fxBed)) {
            return {{QStringLiteral("error"), QStringLiteral("signal amplitudes must be finite and within -1..1")}};
        }
        const auto result = processProbe(mixer_, fixture, frames,
            static_cast<float>(dialogue), static_cast<float>(music),
            static_cast<float>(ambience), static_cast<float>(fxBed));
        if (!result.contains(QStringLiteral("error"))) {
            lastProbe_ = result;
            statusBar()->showMessage(QStringLiteral("Processed %1 offline frames").arg(frames));
            mixerWidget_->updateAllMeters();
        }
        return result;
    }

    QJsonObject setBus(const QJsonObject& arguments)
    {
        const QString name = arguments.value(QStringLiteral("name")).toString();
        const auto bus = mixer_.findBusByName(name);
        if (!bus) {
            return {{QStringLiteral("error"), QStringLiteral("Unknown bus")}};
        }
        if (arguments.contains(QStringLiteral("volumeDb"))) {
            const double value = arguments.value(QStringLiteral("volumeDb")).toDouble(
                std::numeric_limits<double>::quiet_NaN());
            if (!std::isfinite(value) || value < -60.0 || value > 12.0) {
                return {{QStringLiteral("error"), QStringLiteral("volumeDb must be -60..12")}};
            }
        }
        if (arguments.contains(QStringLiteral("pan"))) {
            const double value = arguments.value(QStringLiteral("pan")).toDouble(
                std::numeric_limits<double>::quiet_NaN());
            if (!std::isfinite(value) || value < -1.0 || value > 1.0) {
                return {{QStringLiteral("error"), QStringLiteral("pan must be -1..1")}};
            }
        }
        for (const auto& key : {QStringLiteral("mute"), QStringLiteral("solo")}) {
            if (arguments.contains(key) && !arguments.value(key).isBool()) {
                return {{QStringLiteral("error"), QStringLiteral("mute and solo must be boolean")}};
            }
        }
        if (arguments.contains(QStringLiteral("volumeDb"))) {
            const double value = arguments.value(QStringLiteral("volumeDb")).toDouble(
                std::numeric_limits<double>::quiet_NaN());
            bus->setVolume(static_cast<float>(value));
        }
        if (arguments.contains(QStringLiteral("pan"))) {
            const double value = arguments.value(QStringLiteral("pan")).toDouble(
                std::numeric_limits<double>::quiet_NaN());
            bus->setPan(static_cast<float>(value));
        }
        if (arguments.contains(QStringLiteral("mute"))) {
            bus->setMute(arguments.value(QStringLiteral("mute")).toBool());
        }
        if (arguments.contains(QStringLiteral("solo"))) {
            bus->setSolo(arguments.value(QStringLiteral("solo")).toBool());
        }
        mixerWidget_->refreshBuses();
        statusBar()->showMessage(QStringLiteral("Updated %1").arg(name));
        return state();
    }

    QJsonObject setRoute(const QJsonObject& arguments)
    {
        const auto source = mixer_.findBusByName(
            arguments.value(QStringLiteral("source")).toString());
        const auto target = mixer_.findBusByName(
            arguments.value(QStringLiteral("target")).toString());
        if (!source || !target) {
            return {{QStringLiteral("error"), QStringLiteral("source and target must name existing buses")}};
        }
        const auto result = mixer_.connect(source, target);
        if (result == AudioRoutingResult::Applied) {
            mixerWidget_->refreshBuses();
            refreshGraphSummary();
        }
        QJsonObject response = state();
        response[QStringLiteral("operationResult")] =
            AudioMixer::routingResultDescription(result);
        response[QStringLiteral("routeAccepted")] = result == AudioRoutingResult::Applied;
        return response;
    }

    QJsonObject setSend(const QJsonObject& arguments)
    {
        const auto source = mixer_.findBusByName(
            arguments.value(QStringLiteral("source")).toString());
        const auto target = mixer_.findBusByName(
            arguments.value(QStringLiteral("target")).toString());
        const double amount = arguments.value(QStringLiteral("amount")).toDouble(
            std::numeric_limits<double>::quiet_NaN());
        if (!source || !target || !std::isfinite(amount) || amount < 0.0 || amount > 1.0) {
            return {{QStringLiteral("error"), QStringLiteral("source, target, and amount 0..1 are required")}};
        }
        const bool preFader = arguments.value(QStringLiteral("preFader")).toBool(false);
        const auto result = mixer_.addSideChainSend(
            source, target, static_cast<float>(amount), preFader);
        if (result == AudioRoutingResult::Applied) {
            mixerWidget_->refreshBuses();
            refreshGraphSummary();
        }
        QJsonObject response = state();
        response[QStringLiteral("operationResult")] =
            AudioMixer::routingResultDescription(result);
        response[QStringLiteral("sendAccepted")] = result == AudioRoutingResult::Applied;
        return response;
    }

    QJsonObject removeSend(const QJsonObject& arguments)
    {
        const auto source = mixer_.findBusByName(
            arguments.value(QStringLiteral("source")).toString());
        const auto target = mixer_.findBusByName(
            arguments.value(QStringLiteral("target")).toString());
        if (!source || !target) {
            return {{QStringLiteral("error"), QStringLiteral("source and target must name existing buses")}};
        }
        const auto result = mixer_.removeSideChainSend(source, target);
        if (result == AudioRoutingResult::Applied) {
            mixerWidget_->refreshBuses();
            refreshGraphSummary();
        }
        QJsonObject response = state();
        response[QStringLiteral("operationResult")] =
            AudioMixer::routingResultDescription(result);
        response[QStringLiteral("sendRemoved")] = result == AudioRoutingResult::Applied;
        return response;
    }

    QJsonObject setVcaMember(const QJsonObject& arguments)
    {
        const auto vca = mixer_.findBusByName(
            arguments.value(QStringLiteral("vca")).toString());
        const auto member = mixer_.findBusByName(
            arguments.value(QStringLiteral("member")).toString());
        if (!vca || !member || mixer_.busKind(vca) != AudioBusKind::Vca ||
            !arguments.value(QStringLiteral("assigned")).isBool()) {
            return {{QStringLiteral("error"), QStringLiteral(
                "vca must name a VCA bus, member must name a bus, and assigned must be boolean")}};
        }
        const auto result = arguments.value(QStringLiteral("assigned")).toBool()
            ? mixer_.assignVcaMember(vca, member)
            : mixer_.removeVcaMember(vca, member);
        if (result == AudioRoutingResult::Applied) {
            mixerWidget_->refreshBuses();
            refreshGraphSummary();
        }
        QJsonObject response = state();
        response[QStringLiteral("operationResult")] =
            AudioMixer::routingResultDescription(result);
        response[QStringLiteral("membershipChanged")] =
            result == AudioRoutingResult::Applied;
        return response;
    }

private:
    void refreshGraphSummary()
    {
        QStringList primaryRoutes;
        QStringList sends;
        QStringList vcaMemberships;
        for (const auto& bus : mixer_.getAllBuses()) {
            if (!bus) continue;
            const auto kind = mixer_.busKind(bus);
            if (kind != AudioBusKind::Master && kind != AudioBusKind::Vca) {
                const auto target = mixer_.getRoutingTarget(bus);
                primaryRoutes.append(QStringLiteral("%1 [%2] → %3")
                    .arg(busName(bus), busKindName(kind), busName(target)));
            }
            for (const auto& [target, amount] : mixer_.getSideChainSends(bus)) {
                const auto mode = mixer_.isSideChainSendPreFader(bus, target)
                    ? QStringLiteral("Pre") : QStringLiteral("Post");
                sends.append(QStringLiteral("%1 → %2 (%3%, %4)")
                    .arg(busName(bus), busName(target))
                    .arg(static_cast<int>(std::lround(amount * 100.0f)))
                    .arg(mode));
            }
            if (kind == AudioBusKind::Vca) {
                QStringList members;
                for (const auto& member : mixer_.getVcaMembers(bus)) {
                    members.append(busName(member));
                }
                if (!members.isEmpty()) {
                    vcaMemberships.append(QStringLiteral("%1 → %2")
                        .arg(busName(bus), members.join(QStringLiteral(", "))));
                }
            }
        }
        const auto section = [](const QString& title, const QStringList& entries) {
            return QStringLiteral("%1: %2").arg(title,
                entries.isEmpty() ? QStringLiteral("(none)")
                                  : entries.join(QStringLiteral(" | ")));
        };
        graphSummary_->setText(QStringLiteral("%1\n%2\n%3")
            .arg(section(QStringLiteral("Primary"), primaryRoutes),
                 section(QStringLiteral("Sidechain"), sends),
                 section(QStringLiteral("VCA"), vcaMemberships)));
    }

    Fixture currentFixture() const
    {
        Fixture fixture;
        fixture.dialogueGroup = mixer_.findBusByName(QStringLiteral("Dialogue Group"));
        fixture.musicGroup = mixer_.findBusByName(QStringLiteral("Music Group"));
        fixture.stemGroup = mixer_.findBusByName(QStringLiteral("Stem Bus"));
        fixture.fxReturn = mixer_.findBusByName(QStringLiteral("FX Return"));
        fixture.dialogueVca = mixer_.findBusByName(QStringLiteral("Dialogue VCA"));
        fixture.dialogueTrimVca = mixer_.findBusByName(QStringLiteral("Dialogue Trim VCA"));
        fixture.dialogue = mixer_.findBusByName(QStringLiteral("Dialogue"));
        fixture.music = mixer_.findBusByName(QStringLiteral("Music"));
        fixture.ambience = mixer_.findBusByName(QStringLiteral("Ambience"));
        fixture.fxBed = mixer_.findBusByName(QStringLiteral("FX Bed"));
        if (fixture.fxReturn) {
            fixture.fxCompressor = ArtifactCore::dynamicPointerCast<AudioCompressor>(
                fixture.fxReturn->getEffect(0));
        }
        return fixture;
    }

    AudioMixer mixer_;
    QLabel* graphSummary_ = nullptr;
    Artifact::AudioMixerWidget* mixerWidget_ = nullptr;
    QJsonObject lastProbe_;
};

QJsonObject requireString(const QJsonObject& arguments, const QString& name)
{
    if (arguments.value(name).isString() &&
        !arguments.value(name).toString().trimmed().isEmpty()) return {};
    return {{QStringLiteral("error"), QStringLiteral("%1 must be a non-empty string").arg(name)}};
}

} // namespace

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    qInstallMessageHandler([](QtMsgType type, const QMessageLogContext&,
                             const QString& message) {
        const QByteArray utf8 = message.toUtf8();
        std::fwrite(utf8.constData(), 1, static_cast<size_t>(utf8.size()), stderr);
        std::fputc('\n', stderr);
        if (type == QtFatalMsg) std::fflush(stderr);
    });

    bool mcpStdio = false;
    bool alwaysOnTop = false;
    for (int index = 1; index < argc; ++index) {
        const QString argument = QString::fromLocal8Bit(argv[index]);
        if (argument == QStringLiteral("--mcp-stdio")) mcpStdio = true;
        else if (argument == QStringLiteral("--always-on-top")) alwaysOnTop = true;
    }

    AudioBusViewerWindow window;
    if (alwaysOnTop) window.setWindowFlag(Qt::WindowStaysOnTopHint, true);
    ArtifactTestMcpServer server(QStringLiteral("artifact-audio-bus-viewer"),
                                 QStringLiteral("1.0.0"));
    if (mcpStdio) {
        bool registered = true;
        registered &= server.registerTool(QStringLiteral("audio.state"),
            QStringLiteral("Read the audio routing graph, mixer controls, meter state, and last probe"),
            QJsonObject{{QStringLiteral("type"), QStringLiteral("object")}},
            [&window](const QJsonObject&) { return window.state(); });
        registered &= server.registerTool(QStringLiteral("audio.resetFixture"),
            QStringLiteral("Restore the deterministic Group, Return, VCA, and layer-bus fixture"),
            QJsonObject{{QStringLiteral("type"), QStringLiteral("object")}},
            [&window](const QJsonObject&) {
                if (!window.resetFixture()) return QJsonObject{{QStringLiteral("error"), QStringLiteral("Fixture reset failed")}};
                return window.state();
            });
        registered &= server.registerTool(QStringLiteral("audio.processProbe"),
            QStringLiteral("Process a bounded synthetic stereo block through the real AudioMixer graph without opening an audio device"),
            QJsonObject{{QStringLiteral("type"), QStringLiteral("object")},
                {QStringLiteral("properties"), QJsonObject{
                    {QStringLiteral("frames"), QJsonObject{{QStringLiteral("type"), QStringLiteral("integer")}, {QStringLiteral("minimum"), 1}, {QStringLiteral("maximum"), 8192}}},
                    {QStringLiteral("dialogue"), QJsonObject{{QStringLiteral("type"), QStringLiteral("number")}}},
                    {QStringLiteral("music"), QJsonObject{{QStringLiteral("type"), QStringLiteral("number")}}},
                    {QStringLiteral("ambience"), QJsonObject{{QStringLiteral("type"), QStringLiteral("number")}}},
                    {QStringLiteral("fxBed"), QJsonObject{{QStringLiteral("type"), QStringLiteral("number")}}}}}},
            [&window](const QJsonObject& arguments) { return window.process(arguments); });
        registered &= server.registerTool(QStringLiteral("audio.setBus"),
            QStringLiteral("Set a bus or master fader, pan, mute, or solo state and refresh the production mixer surface"),
            QJsonObject{{QStringLiteral("type"), QStringLiteral("object")},
                {QStringLiteral("properties"), QJsonObject{
                    {QStringLiteral("name"), QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}}},
                    {QStringLiteral("volumeDb"), QJsonObject{{QStringLiteral("type"), QStringLiteral("number")}}},
                    {QStringLiteral("pan"), QJsonObject{{QStringLiteral("type"), QStringLiteral("number")}}},
                    {QStringLiteral("mute"), QJsonObject{{QStringLiteral("type"), QStringLiteral("boolean")}}},
                    {QStringLiteral("solo"), QJsonObject{{QStringLiteral("type"), QStringLiteral("boolean")}}}}},
                {QStringLiteral("required"), QJsonArray{QStringLiteral("name")}}},
            [&window](const QJsonObject& arguments) {
                const auto invalid = requireString(arguments, QStringLiteral("name"));
                return invalid.isEmpty() ? window.setBus(arguments) : invalid;
            });
        registered &= server.registerTool(QStringLiteral("audio.setRoute"),
            QStringLiteral("Change one primary route using the Core AudioMixer graph validator"),
            QJsonObject{{QStringLiteral("type"), QStringLiteral("object")},
                {QStringLiteral("properties"), QJsonObject{
                    {QStringLiteral("source"), QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}}},
                    {QStringLiteral("target"), QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}}}}},
                {QStringLiteral("required"), QJsonArray{QStringLiteral("source"), QStringLiteral("target")}}},
            [&window](const QJsonObject& arguments) {
                const auto invalid = requireString(arguments, QStringLiteral("source"));
                if (!invalid.isEmpty()) return invalid;
                const auto invalidTarget = requireString(arguments, QStringLiteral("target"));
                return invalidTarget.isEmpty() ? window.setRoute(arguments) : invalidTarget;
            });
        registered &= server.registerTool(QStringLiteral("audio.addSidechainSend"),
            QStringLiteral("Add a validated sidechain send and inspect its amount and pre/post-fader mode"),
            QJsonObject{{QStringLiteral("type"), QStringLiteral("object")},
                {QStringLiteral("properties"), QJsonObject{
                    {QStringLiteral("source"), QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}}},
                    {QStringLiteral("target"), QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}}},
                    {QStringLiteral("amount"), QJsonObject{
                        {QStringLiteral("type"), QStringLiteral("number")},
                        {QStringLiteral("minimum"), 0},
                        {QStringLiteral("maximum"), 1}}},
                    {QStringLiteral("preFader"), QJsonObject{{QStringLiteral("type"), QStringLiteral("boolean")}}}}},
                {QStringLiteral("required"), QJsonArray{QStringLiteral("source"), QStringLiteral("target"), QStringLiteral("amount")}}},
            [&window](const QJsonObject& arguments) {
                const auto invalid = requireString(arguments, QStringLiteral("source"));
                if (!invalid.isEmpty()) return invalid;
                const auto invalidTarget = requireString(arguments, QStringLiteral("target"));
                return invalidTarget.isEmpty() ? window.setSend(arguments) : invalidTarget;
            });
        registered &= server.registerTool(QStringLiteral("audio.removeSidechainSend"),
            QStringLiteral("Remove a sidechain send from the routing graph and refresh the production mixer surface"),
            QJsonObject{{QStringLiteral("type"), QStringLiteral("object")},
                {QStringLiteral("properties"), QJsonObject{
                    {QStringLiteral("source"), QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}}},
                    {QStringLiteral("target"), QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}}}}},
                {QStringLiteral("required"), QJsonArray{QStringLiteral("source"), QStringLiteral("target")}}},
            [&window](const QJsonObject& arguments) {
                const auto invalid = requireString(arguments, QStringLiteral("source"));
                if (!invalid.isEmpty()) return invalid;
                const auto invalidTarget = requireString(arguments, QStringLiteral("target"));
                return invalidTarget.isEmpty() ? window.removeSend(arguments) : invalidTarget;
            });
        registered &= server.registerTool(QStringLiteral("audio.setVcaMember"),
            QStringLiteral("Assign or remove a bus from a VCA and refresh the routing surface"),
            QJsonObject{{QStringLiteral("type"), QStringLiteral("object")},
                {QStringLiteral("properties"), QJsonObject{
                    {QStringLiteral("vca"), QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}}},
                    {QStringLiteral("member"), QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}}},
                    {QStringLiteral("assigned"), QJsonObject{{QStringLiteral("type"), QStringLiteral("boolean")}}}}},
                {QStringLiteral("required"), QJsonArray{
                    QStringLiteral("vca"), QStringLiteral("member"), QStringLiteral("assigned")}}},
            [&window](const QJsonObject& arguments) {
                const auto invalidVca = requireString(arguments, QStringLiteral("vca"));
                if (!invalidVca.isEmpty()) return invalidVca;
                const auto invalidMember = requireString(arguments, QStringLiteral("member"));
                return invalidMember.isEmpty()
                    ? window.setVcaMember(arguments) : invalidMember;
            });
        registered &= server.registerTool(QStringLiteral("audio.runContract"),
            QStringLiteral("Run offline checks for nested routing, Pre/Post-Fader sidechain compression, multiple VCAs, cycle rejection, mute, solo, and serialization"),
            QJsonObject{{QStringLiteral("type"), QStringLiteral("object")}},
            [](const QJsonObject&) { return routingContract(); });
        if (!registered || !server.start()) {
            qCritical("Could not register or start the audio bus MCP test server");
            return 2;
        }
    }

    window.show();
    return app.exec();
}
