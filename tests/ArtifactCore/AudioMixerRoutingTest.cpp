#include <gtest/gtest.h>

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
