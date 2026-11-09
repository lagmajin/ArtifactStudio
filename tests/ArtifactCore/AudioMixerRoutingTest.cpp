#include <gtest/gtest.h>
#include <QJsonArray>
#include <QJsonObject>

#include <array>
#include <cstddef>
#include <limits>

import Audio.Bus;
import Audio.Effect;
import Audio.Mixer;
import Audio.Segment;
import Core.ArtifactString;

using namespace ArtifactCore;

namespace {
class ReportedLatencyEffect final : public AudioEffect {
public:
    explicit ReportedLatencyEffect(const qint64 latency) : latency_(latency) {}

    String getName() const override { return String("Reported Latency"); }
    void process(AudioSegment& segment, const AudioSegment*) override
    {
        if (latency_ <= 0 || latency_ > static_cast<qint64>(history_[0].size())) {
            return;
        }
        for (int frame = 0; frame < segment.frameCount(); ++frame) {
            for (int channel = 0; channel < segment.channelCount(); ++channel) {
                const float input = segment.channelData[channel][frame];
                segment.channelData[channel][frame] =
                    channel < 2 ? history_[channel][cursor_]
                        : 0.0f;
                if (channel < 2) {
                    history_[channel][cursor_] = input;
                }
            }
            cursor_ = (cursor_ + 1) % static_cast<std::size_t>(latency_);
        }
    }
    qint64 latencySamples() const override { return latency_; }

private:
    qint64 latency_ = 0;
    std::array<std::array<float, 8>, 2> history_{};
    std::size_t cursor_ = 0;
};

AudioSegment makeStereoInput(const float value, const int frames = 4)
{
    AudioSegment segment;
    segment.layout = AudioChannelLayout::Stereo;
    segment.sampleRate = 48000;
    segment.channelData.resize(2);
    segment.setFrameCount(frames);
    for (auto& channel : segment.channelData) channel.fill(value);
    return segment;
}
}

TEST(AudioMixerRoutingTest, PreFaderSendUsesSignalBeforeBusFader)
{
    AudioMixer mixer;
    const auto source = mixer.createBus(String("Source"));
    const auto target = mixer.createBus(String("Target"));
    ASSERT_TRUE(source);
    ASSERT_TRUE(target);

    source->setVolume(-6.0206f);
    source->clearInput(4, 48000);
    target->clearInput(4, 48000);
    source->addInput(makeStereoInput(1.0f));
    ASSERT_EQ(mixer.addSideChainSend(source, target, 1.0f, true),
              AudioRoutingResult::Applied);

    AudioSegment output = makeStereoInput(0.0f);
    mixer.process(output);

    ASSERT_EQ(target->getSideChainBuffer().channelCount(), 2);
    EXPECT_NEAR(target->getSideChainBuffer().channelData[0][0], 1.0f, 0.001f);
}

TEST(AudioMixerRoutingTest, PostFaderSendUsesSignalAfterBusFader)
{
    AudioMixer mixer;
    const auto source = mixer.createBus(String("Source"));
    const auto target = mixer.createBus(String("Target"));
    ASSERT_TRUE(source);
    ASSERT_TRUE(target);

    source->setVolume(-6.0206f);
    source->clearInput(4, 48000);
    source->addInput(makeStereoInput(1.0f));
    ASSERT_EQ(mixer.addSideChainSend(source, target, 1.0f),
              AudioRoutingResult::Applied);

    AudioSegment output = makeStereoInput(0.0f);
    mixer.process(output);

    ASSERT_EQ(target->getSideChainBuffer().channelCount(), 2);
    EXPECT_NEAR(target->getSideChainBuffer().channelData[0][0], 0.5f, 0.002f);
}

TEST(AudioMixerRoutingTest, MixedPreAndPostFaderSendsAccumulateIndependently)
{
    AudioMixer mixer;
    const auto preFaderSource = mixer.createBus(String("Pre-Fader Source"));
    const auto postFaderSource = mixer.createBus(String("Post-Fader Source"));
    const auto target = mixer.createBus(String("Combined Sidechain Target"));
    ASSERT_TRUE(preFaderSource);
    ASSERT_TRUE(postFaderSource);
    ASSERT_TRUE(target);

    preFaderSource->setVolume(-6.0206f);
    postFaderSource->setVolume(-6.0206f);
    for (const auto& source : {preFaderSource, postFaderSource}) {
        source->clearInput(4, 48000);
        source->addInput(makeStereoInput(1.0f));
    }
    target->clearInput(4, 48000);
    ASSERT_EQ(mixer.addSideChainSend(preFaderSource, target, 0.5f, true),
              AudioRoutingResult::Applied);
    ASSERT_EQ(mixer.addSideChainSend(postFaderSource, target, 0.25f, false),
              AudioRoutingResult::Applied);

    AudioSegment output = makeStereoInput(0.0f);
    mixer.process(output);

    const auto& sidechain = target->getSideChainBuffer();
    ASSERT_EQ(sidechain.channelCount(), 2);
    ASSERT_EQ(sidechain.frameCount(), 4);
    // 1.0 * 0.5 pre-fader + 1.0 * 0.5 fader gain * 0.25 post-fader.
    constexpr float expected = 0.625f;
    for (const auto& channel : sidechain.channelData) {
        for (const float sample : channel) {
            EXPECT_NEAR(sample, expected, 0.002f);
        }
    }
}

TEST(AudioMixerRoutingTest, SurroundSidechainDownmixesAndTruncatesToTargetBlock)
{
    AudioMixer mixer;
    const auto source = mixer.createBus(String("Surround Sidechain Source"));
    const auto target = mixer.createBus(String("Stereo Sidechain Target"));
    ASSERT_TRUE(source);
    ASSERT_TRUE(target);
    source->setLayout(AudioChannelLayout::Surround51);
    target->setLayout(AudioChannelLayout::Stereo);
    source->clearInput(6, 48000);
    target->clearInput(4, 48000);

    AudioSegment surround;
    surround.layout = AudioChannelLayout::Surround51;
    surround.sampleRate = 48000;
    surround.startFrame = 300;
    surround.channelData.resize(6);
    for (int channel = 0; channel < 6; ++channel) {
        surround.channelData[channel].resize(6);
        for (int frame = 0; frame < 6; ++frame) {
            surround.channelData[channel][frame] =
                0.1f * static_cast<float>(channel + 1) +
                0.01f * static_cast<float>(frame);
        }
    }
    source->addInput(surround);
    ASSERT_EQ(mixer.addSideChainSend(source, target, 0.5f, true),
              AudioRoutingResult::Applied);

    AudioSegment output = makeStereoInput(0.0f, 4);
    mixer.process(output);

    const auto& sidechain = target->getSideChainBuffer();
    ASSERT_EQ(sidechain.layout, AudioChannelLayout::Stereo);
    ASSERT_EQ(sidechain.channelCount(), 2);
    ASSERT_EQ(sidechain.frameCount(), 4);
    constexpr float centerMix = 0.7071f;
    constexpr float lfeMix = 0.7071f;
    constexpr float surroundMix = 0.5f;
    for (int frame = 0; frame < sidechain.frameCount(); ++frame) {
        const auto sample = [frame](int channel) {
            return 0.1f * static_cast<float>(channel + 1) +
                   0.01f * static_cast<float>(frame);
        };
        const float expectedLeft = 0.5f * (
            sample(0) + centerMix * sample(2) + lfeMix * sample(3) +
            surroundMix * sample(4));
        const float expectedRight = 0.5f * (
            sample(1) + centerMix * sample(2) + lfeMix * sample(3) +
            surroundMix * sample(5));
        EXPECT_NEAR(sidechain.channelData[0][frame], expectedLeft, 1.0e-5f)
            << "frame=" << frame;
        EXPECT_NEAR(sidechain.channelData[1][frame], expectedRight, 1.0e-5f)
            << "frame=" << frame;
    }
}

TEST(AudioMixerRoutingTest, SameLayoutMalformedInputIsSanitizedAndTruncatedSafely)
{
    AudioMixer mixer;
    const auto source = mixer.createBus(String("Malformed Stereo Source"));
    ASSERT_TRUE(source);
    source->clearInput(4, 48000);

    AudioSegment malformed;
    malformed.layout = AudioChannelLayout::Stereo;
    malformed.sampleRate = 48000;
    malformed.startFrame = 120;
    malformed.channelData.resize(2);
    malformed.channelData[0] = {
        0.25f, std::numeric_limits<float>::quiet_NaN(), 0.75f, 1.0f};
    malformed.channelData[1] = {-0.5f, 0.5f};
    source->addInput(malformed);

    AudioSegment output = makeStereoInput(0.0f);
    mixer.process(output);

    const AudioSegment& mixed = source->getOutputBuffer();
    ASSERT_EQ(mixed.layout, AudioChannelLayout::Stereo);
    ASSERT_EQ(mixed.channelCount(), 2);
    ASSERT_EQ(mixed.frameCount(), 4);
    EXPECT_FLOAT_EQ(mixed.channelData[0][0], 0.25f);
    EXPECT_FLOAT_EQ(mixed.channelData[1][0], -0.5f);
    EXPECT_FLOAT_EQ(mixed.channelData[0][1], 0.0f)
        << "non-finite samples are sanitized before accumulation";
    EXPECT_FLOAT_EQ(mixed.channelData[1][1], 0.5f);
    EXPECT_FLOAT_EQ(mixed.channelData[0][2], 0.0f)
        << "the shorter channel bounds the converted block length";
    EXPECT_FLOAT_EQ(mixed.channelData[1][2], 0.0f);
    EXPECT_FLOAT_EQ(mixed.channelData[0][3], 0.0f);
    EXPECT_FLOAT_EQ(mixed.channelData[1][3], 0.0f);
}

TEST(AudioMixerRoutingTest, PrimaryPathsAlignAcrossBlockBoundaries)
{
    AudioMixer mixer;
    const auto shortPath = mixer.createBus(String("Short Latency Path"));
    const auto intermediateGroup = mixer.createBus(String("Intermediate Group"));
    const auto longPath = mixer.createBus(String("Long Latency Path"));
    const auto master = mixer.getMasterBus();
    ASSERT_TRUE(shortPath);
    ASSERT_TRUE(intermediateGroup);
    ASSERT_TRUE(longPath);
    ASSERT_TRUE(master);
    ASSERT_EQ(mixer.connect(shortPath, intermediateGroup),
              AudioRoutingResult::Applied);
    ASSERT_EQ(mixer.connect(intermediateGroup, master),
              AudioRoutingResult::Applied);
    ASSERT_EQ(mixer.connect(longPath, master), AudioRoutingResult::Applied);
    shortPath->addEffect(SharedPtr<AudioEffect>(new ReportedLatencyEffect(2)));
    intermediateGroup->addEffect(
        SharedPtr<AudioEffect>(new ReportedLatencyEffect(1)));
    longPath->addEffect(SharedPtr<AudioEffect>(new ReportedLatencyEffect(5)));

    auto shortInput = makeStereoInput(0.0f);
    auto longInput = makeStereoInput(0.0f);
    shortInput.channelData[0][3] = 1.0f;
    shortInput.channelData[1][3] = 1.0f;
    longInput.channelData[0][3] = 1.0f;
    longInput.channelData[1][3] = 1.0f;
    shortPath->clearInput(4, 48000);
    longPath->clearInput(4, 48000);
    shortPath->addInput(shortInput);
    longPath->addInput(longInput);
    AudioSegment firstBlock = makeStereoInput(0.0f);
    mixer.process(firstBlock);
    for (const auto& channel : firstBlock.channelData) {
        for (const float sample : channel) EXPECT_FLOAT_EQ(sample, 0.0f);
    }

    shortPath->clearInput(4, 48000);
    longPath->clearInput(4, 48000);
    AudioSegment secondBlock = makeStereoInput(0.0f);
    mixer.process(secondBlock);

    for (const auto& channel : secondBlock.channelData) {
        for (const float sample : channel) EXPECT_FLOAT_EQ(sample, 0.0f);
    }

    shortPath->clearInput(4, 48000);
    longPath->clearInput(4, 48000);
    AudioSegment thirdBlock = makeStereoInput(0.0f);
    mixer.process(thirdBlock);
    for (const auto& channel : thirdBlock.channelData) {
        ASSERT_EQ(channel.size(), 4);
        EXPECT_NEAR(channel[0], 2.0f, 1.0e-5f);
        EXPECT_FLOAT_EQ(channel[1], 0.0f);
        EXPECT_FLOAT_EQ(channel[2], 0.0f);
        EXPECT_FLOAT_EQ(channel[3], 0.0f);
    }
}

TEST(AudioMixerRoutingTest, BecomingAGroupClearsStaleSourceLatencyCompensation)
{
    AudioMixer mixer;
    const auto compensatedSource = mixer.createBus(String("Compensated Source"));
    const auto lateReference = mixer.createBus(String("Late Reference"));
    const auto upstream = mixer.createBus(String("New Upstream"));
    const auto master = mixer.getMasterBus();
    ASSERT_TRUE(compensatedSource);
    ASSERT_TRUE(lateReference);
    ASSERT_TRUE(upstream);
    ASSERT_TRUE(master);
    ASSERT_EQ(mixer.connect(compensatedSource, master), AudioRoutingResult::Applied);
    ASSERT_EQ(mixer.connect(lateReference, master), AudioRoutingResult::Applied);
    lateReference->addEffect(
        SharedPtr<AudioEffect>(new ReportedLatencyEffect(4)));

    compensatedSource->clearInput(4, 48000);
    lateReference->clearInput(4, 48000);
    upstream->clearInput(4, 48000);
    AudioSegment impulse = makeStereoInput(0.0f);
    impulse.channelData[0][0] = 1.0f;
    impulse.channelData[1][0] = 1.0f;
    compensatedSource->addInput(impulse);

    AudioSegment firstBlock = makeStereoInput(0.0f);
    mixer.process(firstBlock);
    for (const auto& channel : firstBlock.channelData) {
        for (const float sample : channel) EXPECT_FLOAT_EQ(sample, 0.0f);
    }

    // The bus changes from a source into a group when a primary input is
    // connected. Any not-yet-emitted sample in its old source compensation
    // history must be discarded at that graph boundary.
    ASSERT_EQ(mixer.connect(upstream, compensatedSource),
              AudioRoutingResult::Applied);
    upstream->clearInput(4, 48000);
    lateReference->clearInput(4, 48000);
    AudioSegment secondBlock = makeStereoInput(0.0f);
    mixer.process(secondBlock);

    ASSERT_EQ(secondBlock.channelCount(), 2);
    ASSERT_EQ(secondBlock.frameCount(), 4);
    for (const auto& channel : secondBlock.channelData) {
        for (const float sample : channel) {
            EXPECT_FLOAT_EQ(sample, 0.0f)
                << "the prior source impulse must not leak after the route edit";
        }
    }
}

TEST(AudioMixerRoutingTest, InvalidSidechainAmountLeavesExistingSendUnchanged)
{
    AudioMixer mixer;
    const auto source = mixer.createBus(String("Stable Source"));
    const auto target = mixer.createBus(String("Stable Target"));
    ASSERT_TRUE(source);
    ASSERT_TRUE(target);
    ASSERT_EQ(mixer.addSideChainSend(source, target, 0.25f),
              AudioRoutingResult::Applied);

    EXPECT_EQ(mixer.addSideChainSend(source, target,
                                     std::numeric_limits<float>::quiet_NaN()),
              AudioRoutingResult::InvalidAmount);

    source->clearInput(4, 48000);
    target->clearInput(4, 48000);
    source->addInput(makeStereoInput(1.0f));
    AudioSegment output = makeStereoInput(0.0f);
    mixer.process(output);

    ASSERT_EQ(target->getSideChainBuffer().channelCount(), 2);
    EXPECT_NEAR(target->getSideChainBuffer().channelData[0][0], 0.25f, 0.001f);
}

TEST(AudioMixerRoutingTest, FiniteSidechainAmountsClampToUnitInterval)
{
    AudioMixer mixer;
    const auto source = mixer.createBus(String("Clamped Source"));
    const auto target = mixer.createBus(String("Clamped Target"));
    ASSERT_TRUE(source);
    ASSERT_TRUE(target);
    ASSERT_EQ(mixer.addSideChainSend(source, target, 1.5f),
              AudioRoutingResult::Applied);

    source->clearInput(4, 48000);
    target->clearInput(4, 48000);
    source->addInput(makeStereoInput(1.0f));
    AudioSegment output = makeStereoInput(0.0f);
    mixer.process(output);
    ASSERT_EQ(target->getSideChainBuffer().channelCount(), 2);
    EXPECT_NEAR(target->getSideChainBuffer().channelData[0][0], 1.0f, 0.001f);

    EXPECT_EQ(mixer.addSideChainSend(source, target, -0.5f),
              AudioRoutingResult::Applied);
    source->clearInput(4, 48000);
    target->clearInput(4, 48000);
    source->addInput(makeStereoInput(1.0f));
    output = makeStereoInput(0.0f);
    mixer.process(output);
    ASSERT_EQ(target->getSideChainBuffer().channelCount(), 2);
    EXPECT_NEAR(target->getSideChainBuffer().channelData[0][0], 0.0f, 0.001f);
}

TEST(AudioMixerRoutingTest, LegacySidechainSourceMigratesToActiveSend)
{
    AudioMixer mixer;
    const auto source = mixer.createBus(String("Legacy Source"));
    const auto target = mixer.createBus(String("Legacy Target"));
    ASSERT_TRUE(source);
    ASSERT_TRUE(target);

    QJsonObject document = mixer.serialize();
    QJsonArray buses = document[QStringLiteral("buses")].toArray();
    for (int index = 0; index < buses.size(); ++index) {
        auto bus = buses[index].toObject();
        if (bus[QStringLiteral("name")] == QStringLiteral("Legacy Target")) {
            bus[QStringLiteral("sidechain_source")] = QStringLiteral("Legacy Source");
            bus[QStringLiteral("sends")] = QJsonArray{};
            buses[index] = bus;
        }
    }
    document[QStringLiteral("buses")] = buses;

    AudioMixer restored;
    ASSERT_TRUE(restored.deserialize(document));
    const auto restoredSource = restored.findBusByName(String("Legacy Source"));
    const auto restoredTarget = restored.findBusByName(String("Legacy Target"));
    ASSERT_TRUE(restoredSource);
    ASSERT_TRUE(restoredTarget);
    restoredSource->clearInput(4, 48000);
    restoredTarget->clearInput(4, 48000);
    restoredSource->addInput(makeStereoInput(1.0f));
    AudioSegment output = makeStereoInput(0.0f);
    restored.process(output);

    ASSERT_EQ(restoredTarget->getSideChainBuffer().channelCount(), 2);
    EXPECT_NEAR(restoredTarget->getSideChainBuffer().channelData[0][0], 1.0f, 0.001f);
}

TEST(AudioMixerRoutingTest, PreFaderSendModeSurvivesSerialization)
{
    AudioMixer mixer;
    const auto source = mixer.createBus(String("Serialized Source"));
    const auto target = mixer.createBus(String("Serialized Target"));
    ASSERT_TRUE(source);
    ASSERT_TRUE(target);
    source->setVolume(-6.0206f);
    ASSERT_EQ(mixer.addSideChainSend(source, target, 1.0f, true),
              AudioRoutingResult::Applied);

    AudioMixer restored;
    ASSERT_TRUE(restored.deserialize(mixer.serialize()));
    const auto restoredSource = restored.findBusByName(String("Serialized Source"));
    const auto restoredTarget = restored.findBusByName(String("Serialized Target"));
    ASSERT_TRUE(restoredSource);
    ASSERT_TRUE(restoredTarget);
    restoredSource->clearInput(4, 48000);
    restoredTarget->clearInput(4, 48000);
    restoredSource->addInput(makeStereoInput(1.0f));
    AudioSegment output = makeStereoInput(0.0f);
    restored.process(output);

    ASSERT_EQ(restoredTarget->getSideChainBuffer().channelCount(), 2);
    EXPECT_NEAR(restoredTarget->getSideChainBuffer().channelData[0][0], 1.0f, 0.001f);
}

TEST(AudioMixerRoutingTest, DeserializationResolvesForwardRouteByStableBusId)
{
    AudioMixer mixer;
    const auto target = mixer.createBus(String("Original Target Label"));
    const auto source = mixer.createBus(String("Forward Source"));
    ASSERT_TRUE(target);
    ASSERT_TRUE(source);
    ASSERT_EQ(mixer.connect(source, target), AudioRoutingResult::Applied);

    QJsonObject document = mixer.serialize();
    const QJsonArray serializedBuses = document.value(QStringLiteral("buses")).toArray();
    QJsonObject sourceObject;
    QJsonObject targetObject;
    for (const auto& value : serializedBuses) {
        const QJsonObject bus = value.toObject();
        const QString name = bus.value(QStringLiteral("name")).toString();
        if (name == QStringLiteral("Forward Source")) sourceObject = bus;
        if (name == QStringLiteral("Original Target Label")) targetObject = bus;
    }
    ASSERT_FALSE(sourceObject.isEmpty());
    ASSERT_FALSE(targetObject.isEmpty());
    ASSERT_EQ(sourceObject.value(QStringLiteral("targetId")).toString(),
              target->id().toQString());

    // Put the referencing bus first and rename the target while keeping its
    // stable ID. The legacy name fallback can no longer resolve this edge.
    targetObject.insert(QStringLiteral("name"), QStringLiteral("Renamed Target"));
    document.insert(QStringLiteral("buses"),
                    QJsonArray{sourceObject, targetObject});

    AudioMixer restored;
    ASSERT_TRUE(restored.deserialize(document));
    const auto restoredSource = restored.findBusByName(String("Forward Source"));
    const auto restoredTarget = restored.findBusByName(String("Renamed Target"));
    ASSERT_TRUE(restoredSource);
    ASSERT_TRUE(restoredTarget);
    EXPECT_EQ(restored.getRoutingTarget(restoredSource), restoredTarget);
}

TEST(AudioMixerRoutingTest, DeserializationReplacesStaleBusesAndRoutingEdges)
{
    AudioMixer serializedMixer;
    const auto first = serializedMixer.createBus(String("Loaded First"));
    const auto second = serializedMixer.createBus(String("Loaded Second"));
    ASSERT_TRUE(first);
    ASSERT_TRUE(second);
    ASSERT_EQ(serializedMixer.connect(first, second), AudioRoutingResult::Applied);
    const QJsonObject fullState = serializedMixer.serialize();

    AudioMixer restored;
    const auto staleSource = restored.createBus(String("Stale Source"));
    const auto staleTarget = restored.createBus(String("Stale Target"));
    ASSERT_TRUE(staleSource);
    ASSERT_TRUE(staleTarget);
    ASSERT_EQ(restored.connect(staleSource, staleTarget), AudioRoutingResult::Applied);
    ASSERT_TRUE(restored.deserialize(fullState));

    EXPECT_EQ(restored.busCount(), 3); // Two loaded buses plus Master.
    EXPECT_FALSE(restored.findBusByName(String("Stale Source")));
    EXPECT_FALSE(restored.findBusByName(String("Stale Target")));
    const auto loadedFirst = restored.findBusByName(String("Loaded First"));
    const auto loadedSecond = restored.findBusByName(String("Loaded Second"));
    ASSERT_TRUE(loadedFirst);
    ASSERT_TRUE(loadedSecond);
    EXPECT_EQ(restored.getRoutingTarget(loadedFirst), loadedSecond);

    // Loading a valid empty graph is also a complete replacement.
    QJsonObject emptyGraph;
    emptyGraph.insert(QStringLiteral("buses"), QJsonArray{});
    ASSERT_TRUE(restored.deserialize(emptyGraph));
    EXPECT_EQ(restored.busCount(), 1);
    EXPECT_FALSE(restored.findBusByName(String("Loaded First")));
    EXPECT_FALSE(restored.findBusByName(String("Loaded Second")));

    // A structurally invalid document is rejected before replacing live state.
    EXPECT_FALSE(restored.deserialize(QJsonObject{}));
    EXPECT_EQ(restored.busCount(), 1);
    ASSERT_TRUE(restored.getMasterBus());
    EXPECT_EQ(restored.busKind(restored.getMasterBus()), AudioBusKind::Master);
}

TEST(AudioMixerRoutingTest, VcaMembershipSurvivesSerialization)
{
    AudioMixer mixer;
    const auto vca = mixer.createBus(String("Dialogue VCA"), AudioBusKind::Vca);
    const auto member = mixer.createBus(String("Dialogue"));
    ASSERT_TRUE(vca);
    ASSERT_TRUE(member);
    ASSERT_EQ(mixer.assignVcaMember(vca, member), AudioRoutingResult::Applied);

    AudioMixer restored;
    ASSERT_TRUE(restored.deserialize(mixer.serialize()));
    const auto restoredVca = restored.findBusByName(String("Dialogue VCA"));
    const auto restoredMember = restored.findBusByName(String("Dialogue"));
    ASSERT_TRUE(restoredVca);
    ASSERT_TRUE(restoredMember);
    ASSERT_EQ(restored.getVcaMembers(restoredVca).size(), 1u);
    EXPECT_EQ(restored.getVcaMembers(restoredVca).front(), restoredMember);
}

TEST(AudioMixerRoutingTest, VcaVolumeControlsMemberOutput)
{
    AudioMixer mixer;
    const auto vca = mixer.createBus(String("Music VCA"), AudioBusKind::Vca);
    const auto member = mixer.createBus(String("Music"));
    ASSERT_TRUE(vca);
    ASSERT_TRUE(member);
    ASSERT_EQ(mixer.assignVcaMember(vca, member), AudioRoutingResult::Applied);

    vca->setVolume(-6.0206f);
    member->clearInput(4, 48000);
    member->addInput(makeStereoInput(1.0f));
    AudioSegment output = makeStereoInput(0.0f);
    mixer.process(output);

    ASSERT_EQ(output.channelCount(), 2);
    EXPECT_NEAR(output.channelData[0][0], 0.5f, 0.002f);
}

TEST(AudioMixerRoutingTest, MultipleVcasMultiplyMemberGain)
{
    AudioMixer mixer;
    const auto primaryVca = mixer.createBus(String("Primary VCA"), AudioBusKind::Vca);
    const auto trimVca = mixer.createBus(String("Trim VCA"), AudioBusKind::Vca);
    const auto member = mixer.createBus(String("Shared Member"));
    ASSERT_TRUE(primaryVca);
    ASSERT_TRUE(trimVca);
    ASSERT_TRUE(member);
    ASSERT_EQ(mixer.assignVcaMember(primaryVca, member), AudioRoutingResult::Applied);
    ASSERT_EQ(mixer.assignVcaMember(trimVca, member), AudioRoutingResult::Applied);

    primaryVca->setVolume(-6.0206f);
    trimVca->setVolume(-6.0206f);
    member->clearInput(4, 48000);
    member->addInput(makeStereoInput(1.0f));
    AudioSegment output = makeStereoInput(0.0f);
    mixer.process(output);

    ASSERT_EQ(output.channelCount(), 2);
    EXPECT_NEAR(output.channelData[0][0], 0.25f, 0.003f);
}

TEST(AudioMixerRoutingTest, ReassigningSameVcaMemberDoesNotDuplicateItsGain)
{
    AudioMixer mixer;
    const auto vca = mixer.createBus(String("Single Membership VCA"),
                                     AudioBusKind::Vca);
    const auto member = mixer.createBus(String("Single Membership Member"));
    ASSERT_TRUE(vca);
    ASSERT_TRUE(member);
    vca->setVolume(-6.0206f);

    ASSERT_EQ(mixer.assignVcaMember(vca, member), AudioRoutingResult::Applied);
    ASSERT_EQ(mixer.assignVcaMember(vca, member), AudioRoutingResult::Applied);
    ASSERT_EQ(mixer.getVcaMembers(vca).size(), 1u);

    member->clearInput(4, 48000);
    member->addInput(makeStereoInput(1.0f));
    AudioSegment output = makeStereoInput(0.0f);
    mixer.process(output);

    ASSERT_EQ(output.channelCount(), 2);
    EXPECT_NEAR(output.channelData[0][0], 0.5f, 0.002f);
}

TEST(AudioMixerRoutingTest, RemovingVcaMembershipStopsApplyingItsGain)
{
    AudioMixer mixer;
    const auto vca = mixer.createBus(String("Remove VCA"), AudioBusKind::Vca);
    const auto member = mixer.createBus(String("Unassigned Member"));
    ASSERT_TRUE(vca);
    ASSERT_TRUE(member);
    ASSERT_EQ(mixer.assignVcaMember(vca, member), AudioRoutingResult::Applied);
    vca->setVolume(-6.0206f);

    EXPECT_EQ(mixer.removeVcaMember(vca, member), AudioRoutingResult::Applied);
    EXPECT_EQ(mixer.removeVcaMember(vca, member), AudioRoutingResult::NoRoute);
    EXPECT_TRUE(mixer.getVcaMembers(vca).empty());

    member->clearInput(4, 48000);
    member->addInput(makeStereoInput(1.0f));
    AudioSegment output = makeStereoInput(0.0f);
    mixer.process(output);

    ASSERT_EQ(output.channelCount(), 2);
    EXPECT_NEAR(output.channelData[0][0], 1.0f, 0.001f);
}

TEST(AudioMixerRoutingTest,
     RemovingBusPrunesPrimarySidechainAndVcaReferencesTogether)
{
    AudioMixer mixer;
    const auto source = mixer.createBus(String("Removal Source"));
    const auto child = mixer.createBus(String("Removal Child"));
    const auto removed = mixer.createBus(String("Removed Bus"));
    const auto survivor = mixer.createBus(String("Surviving Sidechain"));
    const auto vca = mixer.createBus(String("Removal VCA"), AudioBusKind::Vca);
    const auto master = mixer.getMasterBus();
    ASSERT_TRUE(source);
    ASSERT_TRUE(child);
    ASSERT_TRUE(removed);
    ASSERT_TRUE(survivor);
    ASSERT_TRUE(vca);
    ASSERT_TRUE(master);

    ASSERT_EQ(mixer.connect(source, removed), AudioRoutingResult::Applied);
    ASSERT_EQ(mixer.connect(child, removed), AudioRoutingResult::Applied);
    ASSERT_EQ(mixer.addSideChainSend(source, removed, 0.75f, true),
              AudioRoutingResult::Applied);
    ASSERT_EQ(mixer.addSideChainSend(removed, survivor, 0.5f, false),
              AudioRoutingResult::Applied);
    ASSERT_EQ(mixer.addSideChainSend(source, survivor, 0.25f, false),
              AudioRoutingResult::Applied);
    ASSERT_EQ(mixer.assignVcaMember(vca, removed), AudioRoutingResult::Applied);
    ASSERT_EQ(mixer.assignVcaMember(vca, child), AudioRoutingResult::Applied);

    mixer.removeBus(removed);

    EXPECT_EQ(mixer.busCount(), 5);
    EXPECT_EQ(mixer.getRoutingTarget(source), master);
    EXPECT_EQ(mixer.getRoutingTarget(child), master);
    EXPECT_EQ(mixer.getSideChainSends(removed).size(), 0u);
    const auto survivingSends = mixer.getSideChainSends(source);
    ASSERT_EQ(survivingSends.size(), 1u);
    EXPECT_EQ(survivingSends.front().first, survivor);
    EXPECT_FLOAT_EQ(survivingSends.front().second, 0.25f);
    EXPECT_FALSE(mixer.isSideChainSendPreFader(source, removed));
    const auto vcaMembers = mixer.getVcaMembers(vca);
    ASSERT_EQ(vcaMembers.size(), 1u);
    EXPECT_EQ(vcaMembers.front(), child);

    source->clearInput(4, 48000);
    child->clearInput(4, 48000);
    survivor->clearInput(4, 48000);
    source->addInput(makeStereoInput(1.0f));
    child->addInput(makeStereoInput(2.0f));
    AudioSegment output = makeStereoInput(0.0f);
    mixer.process(output);

    ASSERT_EQ(output.channelCount(), 2);
    ASSERT_EQ(output.frameCount(), 4);
    for (const auto& channel : output.channelData) {
        for (const float sample : channel)
            EXPECT_NEAR(sample, 3.0f, 0.001f);
    }
    ASSERT_EQ(survivor->getSideChainBuffer().channelCount(), 2);
    EXPECT_NEAR(survivor->getSideChainBuffer().channelData[0][0],
                0.25f, 0.001f);
}

TEST(AudioMixerRoutingTest, SoloedGroupKeepsItsChildrenAndMutesSiblingRoutes)
{
    AudioMixer mixer;
    const auto child = mixer.createBus(String("Solo Group Child"));
    const auto group = mixer.createBus(String("Solo Group"));
    const auto sibling = mixer.createBus(String("Sibling Route"));
    const auto master = mixer.getMasterBus();
    ASSERT_TRUE(child);
    ASSERT_TRUE(group);
    ASSERT_TRUE(sibling);
    ASSERT_TRUE(master);
    ASSERT_EQ(mixer.connect(child, group), AudioRoutingResult::Applied);
    ASSERT_EQ(mixer.connect(group, master), AudioRoutingResult::Applied);
    ASSERT_EQ(mixer.connect(sibling, master), AudioRoutingResult::Applied);
    group->setSolo(true);

    child->clearInput(4, 48000);
    sibling->clearInput(4, 48000);
    child->addInput(makeStereoInput(1.0f));
    sibling->addInput(makeStereoInput(2.0f));
    AudioSegment output = makeStereoInput(0.0f);
    mixer.process(output);

    ASSERT_EQ(output.channelCount(), 2);
    ASSERT_EQ(output.frameCount(), 4);
    for (const auto& channel : output.channelData) {
        for (const float sample : channel) {
            EXPECT_NEAR(sample, 1.0f, 0.001f);
        }
    }
}

TEST(AudioMixerRoutingTest, RejectingPrimaryRouteCyclePreservesExistingRoute)
{
    AudioMixer mixer;
    const auto first = mixer.createBus(String("Cycle First"));
    const auto second = mixer.createBus(String("Cycle Second"));
    const auto third = mixer.createBus(String("Cycle Third"));
    const auto existingTarget = mixer.createBus(String("Existing Target"));
    ASSERT_TRUE(first);
    ASSERT_TRUE(second);
    ASSERT_TRUE(third);
    ASSERT_TRUE(existingTarget);

    ASSERT_EQ(mixer.connect(first, second), AudioRoutingResult::Applied);
    ASSERT_EQ(mixer.connect(second, third), AudioRoutingResult::Applied);
    ASSERT_EQ(mixer.connect(third, existingTarget), AudioRoutingResult::Applied);

    EXPECT_EQ(mixer.connect(third, first), AudioRoutingResult::CycleDetected);
    EXPECT_EQ(mixer.getRoutingTarget(first), second);
    EXPECT_EQ(mixer.getRoutingTarget(second), third);
    EXPECT_EQ(mixer.getRoutingTarget(third), existingTarget);
}

TEST(AudioMixerRoutingTest,
     InvalidPrimaryRouteEndpointsPreserveCurrentRoute)
{
    AudioMixer mixer;
    AudioMixer foreignMixer;
    const auto source = mixer.createBus(String("Validated Source"));
    const auto target = mixer.createBus(String("Validated Target"));
    const auto foreignTarget = foreignMixer.createBus(String("Foreign Target"));
    ASSERT_TRUE(source);
    ASSERT_TRUE(target);
    ASSERT_TRUE(foreignTarget);
    ASSERT_EQ(mixer.connect(source, target), AudioRoutingResult::Applied);

    EXPECT_EQ(mixer.connect(nullptr, target), AudioRoutingResult::InvalidSource);
    EXPECT_EQ(mixer.connect(source, nullptr), AudioRoutingResult::InvalidTarget);
    EXPECT_EQ(mixer.connect(source, foreignTarget),
              AudioRoutingResult::InvalidTarget);
    EXPECT_EQ(mixer.connect(mixer.getMasterBus(), target),
              AudioRoutingResult::MasterSource);
    EXPECT_EQ(mixer.connect(source, source), AudioRoutingResult::SelfRoute);
    EXPECT_EQ(mixer.getRoutingTarget(source), target);
}

TEST(AudioMixerRoutingTest, DisconnectIsIdempotentAfterRemovingPrimaryRoute)
{
    AudioMixer mixer;
    const auto source = mixer.createBus(String("Disconnect Source"));
    const auto target = mixer.createBus(String("Disconnect Target"));
    ASSERT_TRUE(source);
    ASSERT_TRUE(target);
    ASSERT_EQ(mixer.connect(source, target), AudioRoutingResult::Applied);

    EXPECT_EQ(mixer.disconnect(source), AudioRoutingResult::Applied);
    EXPECT_EQ(mixer.getRoutingTarget(source), nullptr);
    EXPECT_EQ(mixer.disconnect(source), AudioRoutingResult::NoRoute);
}

TEST(AudioMixerRoutingTest,
     ReaddingSidechainPairReplacesSettingsAndRemovalIsIdempotent)
{
    AudioMixer mixer;
    const auto source = mixer.createBus(String("Sidechain Source"));
    const auto target = mixer.createBus(String("Sidechain Target"));
    ASSERT_TRUE(source);
    ASSERT_TRUE(target);

    ASSERT_EQ(mixer.addSideChainSend(source, target, 0.25f, true),
              AudioRoutingResult::Applied);
    ASSERT_EQ(mixer.addSideChainSend(source, target, 0.75f, false),
              AudioRoutingResult::Applied);

    auto sends = mixer.getSideChainSends(source);
    ASSERT_EQ(sends.size(), 1u);
    EXPECT_EQ(sends.front().first.get(), target.get());
    EXPECT_FLOAT_EQ(sends.front().second, 0.75f);
    EXPECT_FALSE(mixer.isSideChainSendPreFader(source, target));

    EXPECT_EQ(mixer.removeSideChainSend(source, target),
              AudioRoutingResult::Applied);
    EXPECT_TRUE(mixer.getSideChainSends(source).empty());
    EXPECT_EQ(mixer.removeSideChainSend(source, target),
              AudioRoutingResult::NoSend);
}
