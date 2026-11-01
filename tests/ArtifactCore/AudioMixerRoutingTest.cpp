#include <gtest/gtest.h>
#include <QJsonArray>
#include <QJsonObject>

#include <limits>

import Audio.Bus;
import Audio.Mixer;
import Audio.Segment;
import Core.ArtifactString;

using namespace ArtifactCore;

namespace {
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
