#include <gtest/gtest.h>

#include <QVector>

#include <cmath>
#include <initializer_list>
#include <limits>
#include <utility>

import Audio.DownMixer;
import Audio.Segment;

using namespace ArtifactCore;

namespace {
AudioSegment makeSegment(AudioChannelLayout layout,
                         std::initializer_list<std::initializer_list<float>> channels,
                         int sampleRate = 48000,
                         qint64 startFrame = 0)
{
    AudioSegment segment;
    segment.sampleRate = sampleRate;
    segment.layout = layout;
    segment.startFrame = startFrame;
    for (const auto samples : channels) {
        QVector<float> channel;
        for (const float sample : samples)
            channel.push_back(sample);
        segment.channelData.push_back(std::move(channel));
    }
    return segment;
}
}

TEST(AudioDownMixerStandaloneTest, DefaultsToStereoAndInvalidTargetFallsBackToStereo)
{
    AudioDownMixer mixer;
    EXPECT_EQ(mixer.targetLayout(), AudioChannelLayout::Stereo);

    mixer.setTargetLayout(static_cast<AudioChannelLayout>(999));

    EXPECT_EQ(mixer.targetLayout(), AudioChannelLayout::Stereo);
}

TEST(AudioDownMixerStandaloneTest, UnsupportedDeclaredAmbisonicsTargetFallsBackToStereo)
{
    AudioDownMixer mixer;
    mixer.setTargetLayout(AudioChannelLayout::Mono);
    ASSERT_EQ(mixer.targetLayout(), AudioChannelLayout::Mono);

    mixer.setTargetLayout(AudioChannelLayout::Ambisonics);

    EXPECT_EQ(mixer.targetLayout(), AudioChannelLayout::Stereo);
}

TEST(AudioDownMixerStandaloneTest, BackMixLevelGetterReportsDefaultFiniteSettingAndFallback)
{
    AudioDownMixer mixer;
    EXPECT_FLOAT_EQ(mixer.backMixLevel(), 0.5f);

    mixer.setBackMixLevel(0.25f);
    EXPECT_FLOAT_EQ(mixer.backMixLevel(), 0.25f);

    mixer.setBackMixLevel(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(mixer.backMixLevel(), 0.5f);
}

TEST(AudioDownMixerStandaloneTest, SameLayoutPathPreservesMetadataAndSanitizesNonFiniteSamples)
{
    AudioDownMixer mixer;
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();
    const auto source = makeSegment(
        AudioChannelLayout::Stereo,
        {{1.0f, nan}, {-2.0f, infinity}}, 96000, 1234);

    const AudioSegment output = mixer.process(source);

    EXPECT_EQ(output.sampleRate, 96000);
    EXPECT_EQ(output.layout, AudioChannelLayout::Stereo);
    EXPECT_EQ(output.startFrame, 1234);
    ASSERT_EQ(output.channelData.size(), 2);
    EXPECT_FLOAT_EQ(output.channelData[0][0], 1.0f);
    EXPECT_FLOAT_EQ(output.channelData[0][1], 0.0f);
    EXPECT_FLOAT_EQ(output.channelData[1][0], -2.0f);
    EXPECT_TRUE(std::isfinite(output.channelData[1][1]));
    EXPECT_GT(output.channelData[1][1], 0.0f);
}

TEST(AudioDownMixerStandaloneTest, SanitizingOutputLeavesSourceSamplesUntouched)
{
    AudioDownMixer mixer;
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();
    const auto source = makeSegment(
        AudioChannelLayout::Stereo, {{nan}, {infinity}}, 48000, 4);

    const AudioSegment output = mixer.process(source);

    ASSERT_EQ(output.channelData.size(), 2);
    EXPECT_FLOAT_EQ(output.channelData[0][0], 0.0f);
    EXPECT_TRUE(std::isfinite(output.channelData[1][0]));
    ASSERT_EQ(source.channelData.size(), 2);
    EXPECT_TRUE(std::isnan(source.channelData[0][0]));
    EXPECT_TRUE(std::isinf(source.channelData[1][0]));
}

TEST(AudioDownMixerStandaloneTest, SameLayoutRaggedChannelsAreNormalizedToShortestFrameCount)
{
    AudioDownMixer mixer;
    const auto source = makeSegment(
        AudioChannelLayout::Stereo,
        {{0.1f, 0.2f, 0.3f}, {-0.1f, -0.2f}}, 32000, 765);

    const AudioSegment output = mixer.process(source);

    EXPECT_EQ(output.sampleRate, 32000);
    EXPECT_EQ(output.layout, AudioChannelLayout::Stereo);
    EXPECT_EQ(output.startFrame, 765);
    ASSERT_EQ(output.channelData.size(), 2);
    EXPECT_EQ(output.channelData[0], (QVector<float>{0.1f, 0.2f}));
    EXPECT_EQ(output.channelData[1], (QVector<float>{-0.1f, -0.2f}));
    EXPECT_EQ(output.frameCount(), 2);
}

TEST(AudioDownMixerStandaloneTest, MalformedStereoWithOneChannelKeepsLeftAndSilencesRight)
{
    AudioDownMixer mixer;
    const auto source = makeSegment(
        AudioChannelLayout::Stereo,
        {{0.25f, -0.5f}}, 44100, 812);

    const AudioSegment output = mixer.process(source);

    EXPECT_EQ(output.sampleRate, 44100);
    EXPECT_EQ(output.layout, AudioChannelLayout::Stereo);
    EXPECT_EQ(output.startFrame, 812);
    ASSERT_EQ(output.channelData.size(), 2);
    EXPECT_EQ(output.channelData[0], (QVector<float>{0.25f, -0.5f}));
    EXPECT_EQ(output.channelData[1], (QVector<float>{0.0f, 0.0f}));
}

TEST(AudioDownMixerStandaloneTest, MultichannelOverflowSaturatesToFiniteFloatLimits)
{
    AudioDownMixer mixer;
    const float maximum = std::numeric_limits<float>::max();
    const auto source = makeSegment(
        AudioChannelLayout::Surround51,
        {{maximum}, {maximum}, {maximum}, {maximum}, {maximum}, {maximum}});

    const AudioSegment positive = mixer.process(source);
    const auto negativeSource = makeSegment(
        AudioChannelLayout::Surround51,
        {{-maximum}, {-maximum}, {-maximum}, {-maximum}, {-maximum}, {-maximum}});
    const AudioSegment negative = mixer.process(negativeSource);

    ASSERT_EQ(positive.channelData.size(), 2);
    ASSERT_EQ(negative.channelData.size(), 2);
    for (int channel = 0; channel < 2; ++channel) {
        EXPECT_TRUE(std::isfinite(positive.channelData[channel][0]));
        EXPECT_FLOAT_EQ(positive.channelData[channel][0], maximum);
        EXPECT_TRUE(std::isfinite(negative.channelData[channel][0]));
        EXPECT_FLOAT_EQ(negative.channelData[channel][0], -maximum);
    }
}

TEST(AudioDownMixerStandaloneTest, MonoInputIsDuplicatedToBothStereoChannels)
{
    AudioDownMixer mixer;
    const auto source = makeSegment(AudioChannelLayout::Mono,
                                    {{0.25f, -0.5f}}, 44100, 77);

    const auto output = mixer.process(source);

    EXPECT_EQ(output.layout, AudioChannelLayout::Stereo);
    EXPECT_EQ(output.sampleRate, 44100);
    EXPECT_EQ(output.startFrame, 77);
    ASSERT_EQ(output.channelData.size(), 2);
    EXPECT_EQ(output.channelData[0], (QVector<float>{0.25f, -0.5f}));
    EXPECT_EQ(output.channelData[1], (QVector<float>{0.25f, -0.5f}));
}

TEST(AudioDownMixerStandaloneTest, Surround51ToStereoUsesConfigurableChannelWeights)
{
    AudioDownMixer mixer;
    mixer.setCenterMixLevel(0.5f);
    mixer.setLFEMixLevel(0.25f);
    mixer.setSurroundMixLevel(0.125f);
    const auto source = makeSegment(AudioChannelLayout::Surround51,
                                    {{1.0f}, {2.0f}, {4.0f}, {8.0f}, {16.0f}, {32.0f}});

    const auto output = mixer.process(source);

    ASSERT_EQ(output.channelData.size(), 2);
    EXPECT_FLOAT_EQ(output.channelData[0][0], 7.0f);
    EXPECT_FLOAT_EQ(output.channelData[1][0], 10.0f);
}

TEST(AudioDownMixerStandaloneTest, Surround71ToStereoAppliesEachConfiguredChannelWeight)
{
    AudioDownMixer mixer;
    mixer.setCenterMixLevel(0.1f);
    mixer.setLFEMixLevel(0.2f);
    mixer.setSurroundMixLevel(0.3f);
    mixer.setBackMixLevel(0.4f);
    const auto source = makeSegment(
        AudioChannelLayout::Surround71,
        {{1.0f}, {2.0f}, {2.0f}, {3.0f}, {4.0f}, {5.0f}, {6.0f}, {7.0f}},
        96000, 4321);

    const auto output = mixer.process(source);

    EXPECT_EQ(output.sampleRate, 96000);
    EXPECT_EQ(output.startFrame, 4321);
    EXPECT_EQ(output.layout, AudioChannelLayout::Stereo);
    ASSERT_EQ(output.channelData.size(), 2);
    EXPECT_NEAR(output.channelData[0][0], 5.4f, 1.0e-6f);
    EXPECT_NEAR(output.channelData[1][0], 7.1f, 1.0e-6f);
}

TEST(AudioDownMixerStandaloneTest, NonFiniteMixLevelsFallBackToDocumentedDefaults)
{
    AudioDownMixer mixer;
    mixer.setCenterMixLevel(std::numeric_limits<float>::quiet_NaN());
    mixer.setLFEMixLevel(std::numeric_limits<float>::infinity());
    mixer.setSurroundMixLevel(-std::numeric_limits<float>::infinity());
    mixer.setBackMixLevel(std::numeric_limits<float>::quiet_NaN());

    const auto surround51 = makeSegment(
        AudioChannelLayout::Surround51,
        {{0.0f}, {0.0f}, {1.0f}, {1.0f}, {1.0f}, {1.0f}});
    const auto stereo = mixer.process(surround51);
    ASSERT_EQ(stereo.channelData.size(), 2);
    EXPECT_NEAR(stereo.channelData[0][0], 0.7071f + 0.7071f + 0.5f, 1.0e-6f);
    EXPECT_NEAR(stereo.channelData[1][0], 0.7071f + 0.7071f + 0.5f, 1.0e-6f);

    mixer.setTargetLayout(AudioChannelLayout::Surround51);
    const auto surround71 = makeSegment(
        AudioChannelLayout::Surround71,
        {{0.0f}, {0.0f}, {0.0f}, {0.0f}, {0.0f}, {0.0f}, {1.0f}, {1.0f}});
    const auto folded = mixer.process(surround71);
    ASSERT_EQ(folded.channelData.size(), 6);
    EXPECT_FLOAT_EQ(folded.channelData[4][0], 0.5f);
    EXPECT_FLOAT_EQ(folded.channelData[5][0], 0.5f);
}

TEST(AudioDownMixerStandaloneTest, StereoToMonoAveragesChannelsAndPreservesTiming)
{
    AudioDownMixer mixer;
    mixer.setTargetLayout(AudioChannelLayout::Mono);
    const auto source = makeSegment(AudioChannelLayout::Stereo,
                                    {{1.0f, -1.0f}, {3.0f, 1.0f}}, 32000, 900);

    const auto output = mixer.process(source);

    EXPECT_EQ(output.layout, AudioChannelLayout::Mono);
    EXPECT_EQ(output.sampleRate, 32000);
    EXPECT_EQ(output.startFrame, 900);
    ASSERT_EQ(output.channelData.size(), 1);
    EXPECT_EQ(output.channelData[0], (QVector<float>{2.0f, 0.0f}));
}

TEST(AudioDownMixerStandaloneTest, Surround51ToMonoAveragesEveryChannel)
{
    AudioDownMixer mixer;
    mixer.setTargetLayout(AudioChannelLayout::Mono);
    const auto source = makeSegment(
        AudioChannelLayout::Surround51,
        {{1.0f, 2.0f}, {2.0f, 4.0f}, {3.0f, 6.0f},
         {4.0f, 8.0f}, {5.0f, 10.0f}, {6.0f, 12.0f}},
        48000, 321);

    const auto output = mixer.process(source);

    EXPECT_EQ(output.layout, AudioChannelLayout::Mono);
    EXPECT_EQ(output.sampleRate, 48000);
    EXPECT_EQ(output.startFrame, 321);
    ASSERT_EQ(output.channelData.size(), 1);
    EXPECT_EQ(output.channelData[0], (QVector<float>{3.5f, 7.0f}));
}

TEST(AudioDownMixerStandaloneTest, ExplicitChannelMapReordersDuplicatesAndSilencesInvalidIndices)
{
    AudioDownMixer mixer;
    mixer.setTargetLayout(AudioChannelLayout::Surround51);
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const auto source = makeSegment(AudioChannelLayout::Stereo,
                                    {{1.0f, 2.0f}, {3.0f, nan}}, 44100, 19);

    const auto output = mixer.processChannelMap(
        source, QVector<int>{1, 0, 1, -1, 9, 0});

    EXPECT_EQ(output.layout, AudioChannelLayout::Surround51);
    EXPECT_EQ(output.sampleRate, 44100);
    EXPECT_EQ(output.startFrame, 19);
    ASSERT_EQ(output.channelData.size(), 6);
    EXPECT_EQ(output.channelData[0], (QVector<float>{3.0f, 0.0f}));
    EXPECT_EQ(output.channelData[1], (QVector<float>{1.0f, 2.0f}));
    EXPECT_EQ(output.channelData[2], (QVector<float>{3.0f, 0.0f}));
    EXPECT_EQ(output.channelData[3], (QVector<float>{0.0f, 0.0f}));
    EXPECT_EQ(output.channelData[4], (QVector<float>{0.0f, 0.0f}));
    EXPECT_EQ(output.channelData[5], (QVector<float>{1.0f, 2.0f}));
}

TEST(AudioDownMixerStandaloneTest, EmptyExplicitChannelMapReturnsMetadataWithoutChannels)
{
    AudioDownMixer mixer;
    mixer.setTargetLayout(AudioChannelLayout::Surround51);
    const auto source = makeSegment(AudioChannelLayout::Stereo,
                                    {{1.0f, 2.0f}, {3.0f, 4.0f}}, 96000, 75);

    const auto output = mixer.processChannelMap(source, QVector<int>{});

    EXPECT_EQ(output.layout, AudioChannelLayout::Surround51);
    EXPECT_EQ(output.sampleRate, 96000);
    EXPECT_EQ(output.startFrame, 75);
    EXPECT_TRUE(output.channelData.isEmpty());
}

TEST(AudioDownMixerStandaloneTest, ExplicitChannelMapWithZeroFramesReturnsNoChannels)
{
    AudioDownMixer mixer;
    mixer.setTargetLayout(AudioChannelLayout::Mono);
    const auto source = makeSegment(AudioChannelLayout::Stereo, {}, 22050, 456);

    const auto output = mixer.processChannelMap(source, QVector<int>{0});

    EXPECT_EQ(output.layout, AudioChannelLayout::Mono);
    EXPECT_EQ(output.sampleRate, 22050);
    EXPECT_EQ(output.startFrame, 456);
    EXPECT_TRUE(output.channelData.isEmpty());
}

TEST(AudioDownMixerStandaloneTest, Surround71ToSurround51FoldsBackChannelsIntoSurrounds)
{
    AudioDownMixer mixer;
    mixer.setTargetLayout(AudioChannelLayout::Surround51);
    mixer.setBackMixLevel(0.5f);
    const auto source = makeSegment(AudioChannelLayout::Surround71,
                                    {{1.0f}, {2.0f}, {3.0f}, {4.0f},
                                     {5.0f}, {6.0f}, {7.0f}, {8.0f}});

    const auto output = mixer.process(source);

    ASSERT_EQ(output.channelData.size(), 6);
    EXPECT_EQ(output.layout, AudioChannelLayout::Surround51);
    EXPECT_FLOAT_EQ(output.channelData[0][0], 1.0f);
    EXPECT_FLOAT_EQ(output.channelData[1][0], 2.0f);
    EXPECT_FLOAT_EQ(output.channelData[2][0], 3.0f);
    EXPECT_FLOAT_EQ(output.channelData[3][0], 4.0f);
    EXPECT_FLOAT_EQ(output.channelData[4][0], 8.5f);
    EXPECT_FLOAT_EQ(output.channelData[5][0], 10.0f);
}

TEST(AudioDownMixerStandaloneTest, Surround714ToStereoFoldsHeightChannelsBySide)
{
    AudioDownMixer mixer;
    const auto source = makeSegment(
        AudioChannelLayout::Surround714,
        {{0.0f}, {0.0f}, {0.0f}, {0.0f}, {0.0f}, {0.0f},
         {0.0f}, {0.0f}, {2.0f}, {3.0f}, {4.0f}, {5.0f}});

    const auto output = mixer.process(source);

    ASSERT_EQ(output.layout, AudioChannelLayout::Stereo);
    ASSERT_EQ(output.channelData.size(), 2);
    EXPECT_NEAR(output.channelData[0][0], (2.0f + 4.0f) * 0.35355339f, 1.0e-6f);
    EXPECT_NEAR(output.channelData[1][0], (3.0f + 5.0f) * 0.35355339f, 1.0e-6f);
}

TEST(AudioDownMixerStandaloneTest, Surround714ToSurround71PreservesBedAndDropsHeightChannels)
{
    AudioDownMixer mixer;
    mixer.setTargetLayout(AudioChannelLayout::Surround71);
    const auto source = makeSegment(
        AudioChannelLayout::Surround714,
        {{0.1f}, {0.2f}, {0.3f}, {0.4f}, {0.5f}, {0.6f},
         {0.7f}, {0.8f}, {1.1f}, {1.2f}, {1.3f}, {1.4f}},
        96000, 789);

    const auto output = mixer.process(source);

    EXPECT_EQ(output.sampleRate, 96000);
    EXPECT_EQ(output.startFrame, 789);
    EXPECT_EQ(output.layout, AudioChannelLayout::Surround71);
    ASSERT_EQ(output.channelData.size(), 8);
    for (int channel = 0; channel < 8; ++channel)
        EXPECT_EQ(output.channelData[channel], source.channelData[channel]) << channel;
}

TEST(AudioDownMixerStandaloneTest, Surround51PromotionToSurround71PreservesBedAndSilencesBackPair)
{
    AudioDownMixer mixer;
    mixer.setTargetLayout(AudioChannelLayout::Surround71);
    const auto source = makeSegment(
        AudioChannelLayout::Surround51,
        {{0.1f, 0.2f}, {0.3f, 0.4f}, {0.5f, 0.6f}, {0.7f, 0.8f},
         {0.9f, 1.0f}, {1.1f, 1.2f}},
        48000, 321);

    const auto output = mixer.process(source);

    EXPECT_EQ(output.sampleRate, 48000);
    EXPECT_EQ(output.startFrame, 321);
    EXPECT_EQ(output.layout, AudioChannelLayout::Surround71);
    ASSERT_EQ(output.channelData.size(), 8);
    for (int channel = 0; channel < 6; ++channel)
        EXPECT_EQ(output.channelData[channel], source.channelData[channel]) << channel;
    EXPECT_EQ(output.channelData[6], (QVector<float>{0.0f, 0.0f}));
    EXPECT_EQ(output.channelData[7], (QVector<float>{0.0f, 0.0f}));
}

TEST(AudioDownMixerStandaloneTest, StereoPromotionToSurround714PreservesFrontAndSilencesRest)
{
    AudioDownMixer mixer;
    mixer.setTargetLayout(AudioChannelLayout::Surround714);
    const auto source = makeSegment(AudioChannelLayout::Stereo,
                                    {{0.25f, -0.5f}, {0.75f, 1.0f}},
                                    96000, 246);

    const auto output = mixer.process(source);

    EXPECT_EQ(output.sampleRate, 96000);
    EXPECT_EQ(output.startFrame, 246);
    EXPECT_EQ(output.layout, AudioChannelLayout::Surround714);
    ASSERT_EQ(output.channelData.size(), 12);
    EXPECT_EQ(output.channelData[0], (QVector<float>{0.25f, -0.5f}));
    EXPECT_EQ(output.channelData[1], (QVector<float>{0.75f, 1.0f}));
    for (int channel = 2; channel < output.channelData.size(); ++channel) {
        EXPECT_EQ(output.channelData[channel], (QVector<float>{0.0f, 0.0f}))
            << channel;
    }
}

TEST(AudioDownMixerStandaloneTest, Surround51PromotionToSurround714PreservesBedAndSilencesHeight)
{
    AudioDownMixer mixer;
    mixer.setTargetLayout(AudioChannelLayout::Surround714);
    const auto source = makeSegment(
        AudioChannelLayout::Surround51,
        {{0.1f, 0.2f}, {0.3f, 0.4f}, {0.5f, 0.6f}, {0.7f, 0.8f},
         {0.9f, 1.0f}, {1.1f, 1.2f}},
        48000, 135);

    const auto output = mixer.process(source);

    EXPECT_EQ(output.sampleRate, 48000);
    EXPECT_EQ(output.startFrame, 135);
    EXPECT_EQ(output.layout, AudioChannelLayout::Surround714);
    ASSERT_EQ(output.channelData.size(), 12);
    for (int channel = 0; channel < 6; ++channel) {
        EXPECT_EQ(output.channelData[channel], source.channelData[channel])
            << channel;
    }
    for (int channel = 6; channel < 12; ++channel) {
        EXPECT_EQ(output.channelData[channel], (QVector<float>{0.0f, 0.0f}))
            << channel;
    }
}

TEST(AudioDownMixerStandaloneTest, StereoPromotionToCustom10chPreservesFrontAndSilencesRest)
{
    AudioDownMixer mixer;
    mixer.setTargetLayout(AudioChannelLayout::Custom10ch);
    const auto source = makeSegment(AudioChannelLayout::Stereo,
                                    {{0.25f, -0.5f}, {0.75f, 1.0f}},
                                    96000, 246);

    const auto output = mixer.process(source);

    EXPECT_EQ(output.sampleRate, 96000);
    EXPECT_EQ(output.startFrame, 246);
    EXPECT_EQ(output.layout, AudioChannelLayout::Custom10ch);
    ASSERT_EQ(output.channelData.size(), 10);
    EXPECT_EQ(output.channelData[0], (QVector<float>{0.25f, -0.5f}));
    EXPECT_EQ(output.channelData[1], (QVector<float>{0.75f, 1.0f}));
    for (int channel = 2; channel < output.channelData.size(); ++channel) {
        EXPECT_EQ(output.channelData[channel], (QVector<float>{0.0f, 0.0f}))
            << channel;
    }
}

TEST(AudioDownMixerStandaloneTest, Custom10chToStereoKeepsFrontPairAndIgnoresExtraChannels)
{
    AudioDownMixer mixer;
    const auto source = makeSegment(
        AudioChannelLayout::Custom10ch,
        {{0.25f, -0.5f}, {0.75f, 1.0f}, {10.0f, 10.0f}, {20.0f, 20.0f},
         {30.0f, 30.0f}, {40.0f, 40.0f}, {50.0f, 50.0f}, {60.0f, 60.0f},
         {70.0f, 70.0f}, {80.0f, 80.0f}},
        48000, 654);

    const auto output = mixer.process(source);

    EXPECT_EQ(output.layout, AudioChannelLayout::Stereo);
    EXPECT_EQ(output.sampleRate, 48000);
    EXPECT_EQ(output.startFrame, 654);
    ASSERT_EQ(output.channelData.size(), 2);
    EXPECT_EQ(output.channelData[0], (QVector<float>{0.25f, -0.5f}));
    EXPECT_EQ(output.channelData[1], (QVector<float>{0.75f, 1.0f}));
}

TEST(AudioDownMixerStandaloneTest, MonoPromotionToSurround51DuplicatesFrontAndSilencesOtherChannels)
{
    AudioDownMixer mixer;
    mixer.setTargetLayout(AudioChannelLayout::Surround51);
    const auto source = makeSegment(AudioChannelLayout::Mono,
                                    {{0.25f, -0.5f}}, 32000, 987);

    const auto output = mixer.process(source);

    EXPECT_EQ(output.layout, AudioChannelLayout::Surround51);
    EXPECT_EQ(output.sampleRate, 32000);
    EXPECT_EQ(output.startFrame, 987);
    ASSERT_EQ(output.channelData.size(), 6);
    EXPECT_EQ(output.channelData[0], (QVector<float>{0.25f, -0.5f}));
    EXPECT_EQ(output.channelData[1], (QVector<float>{0.25f, -0.5f}));
    for (int channel = 2; channel < output.channelData.size(); ++channel) {
        EXPECT_EQ(output.channelData[channel], (QVector<float>{0.0f, 0.0f}))
            << channel;
    }
}

TEST(AudioDownMixerStandaloneTest, EmptyInputProducesRequestedLayoutAndKeepsMetadata)
{
    AudioDownMixer mixer;
    mixer.setTargetLayout(AudioChannelLayout::Mono);
    const auto source = makeSegment(AudioChannelLayout::Stereo, {}, 22050, 456);

    const auto output = mixer.process(source);

    EXPECT_EQ(output.sampleRate, 22050);
    EXPECT_EQ(output.layout, AudioChannelLayout::Mono);
    EXPECT_EQ(output.startFrame, 456);
    EXPECT_TRUE(output.channelData.isEmpty());
}

TEST(AudioDownMixerStandaloneTest, ZeroFrameChannelsProduceNoOutputChannels)
{
    AudioDownMixer mixer;
    mixer.setTargetLayout(AudioChannelLayout::Surround51);
    const auto source = makeSegment(
        AudioChannelLayout::Stereo, {{}, {}}, 32000, 654);

    const auto output = mixer.process(source);

    EXPECT_EQ(output.sampleRate, 32000);
    EXPECT_EQ(output.layout, AudioChannelLayout::Surround51);
    EXPECT_EQ(output.startFrame, 654);
    EXPECT_EQ(output.frameCount(), 0);
    EXPECT_TRUE(output.channelData.isEmpty());
}
