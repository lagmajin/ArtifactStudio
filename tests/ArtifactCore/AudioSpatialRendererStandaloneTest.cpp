#include <gtest/gtest.h>

#include <QVector>

#include <cmath>
#include <limits>

import Audio.Segment;
import Audio.Spatial.Params;
import Audio.Spatial.Math;
import Audio.Spatial.Renderer;

using namespace ArtifactCore;
using namespace ArtifactCore::Audio;
using namespace ArtifactCore::Audio::Spatial;

namespace {
AudioSegment makeSegment(int channels, int frames, float value = 1.0f)
{
    AudioSegment segment;
    segment.sampleRate = 48000;
    segment.layout = channels == 1 ? AudioChannelLayout::Mono : AudioChannelLayout::Stereo;
    segment.startFrame = 1234;
    segment.channelData.resize(channels);
    for (auto& channel : segment.channelData) {
        channel.fill(value, frames);
    }
    return segment;
}

void render(SpatialRenderer& renderer, const AudioSegment& input, AudioSegment& output,
            int frames, Vec3 sourcePosition = {0.0f, 0.0f, -1.0f})
{
    renderer.processBlock(input, output, frames, sourcePosition, Quat{},
                          Vec3{}, Quat{});
}
}

TEST(AudioSpatialRendererStandaloneTest, PublishesAndSnapshotsSanitizedParameters)
{
    SpatialRenderer renderer;
    SpatialParams params;
    params.roomSend = 4.0f;
    params.outputLayout = AudioChannelLayout::Surround714;
    renderer.publishParams(params);
    const auto result = renderer.snapshotParams();
    EXPECT_FLOAT_EQ(result.roomSend, 1.0f);
    EXPECT_EQ(result.outputLayout, AudioChannelLayout::Surround714);
}

TEST(AudioSpatialRendererStandaloneTest, NonPositiveFrameCountCopiesInputUnchanged)
{
    SpatialRenderer renderer;
    const auto input = makeSegment(1, 4, 0.25f);
    AudioSegment output;
    render(renderer, input, output, 0);
    ASSERT_EQ(output.channelCount(), 1);
    EXPECT_EQ(output.sampleRate, input.sampleRate);
    EXPECT_EQ(output.startFrame, input.startFrame);
    EXPECT_FLOAT_EQ(output.channelData[0][2], 0.25f);
}

TEST(AudioSpatialRendererStandaloneTest, EmptyInputCopiesItsMetadata)
{
    SpatialRenderer renderer;
    AudioSegment input;
    input.sampleRate = 32000;
    input.startFrame = 91;
    input.layout = AudioChannelLayout::Surround51;
    AudioSegment output;
    render(renderer, input, output, 8);
    EXPECT_EQ(output.sampleRate, 32000);
    EXPECT_EQ(output.startFrame, 91);
    EXPECT_EQ(output.layout, AudioChannelLayout::Surround51);
    EXPECT_EQ(output.channelCount(), 0);
}

TEST(AudioSpatialRendererStandaloneTest, RequestedFramesAreClampedToShortestInputChannel)
{
    SpatialRenderer renderer;
    auto input = makeSegment(2, 5);
    input.channelData[1].resize(3);
    AudioSegment output;
    render(renderer, input, output, 20);
    EXPECT_EQ(output.frameCount(), 3);
    EXPECT_EQ(output.channelCount(), 2);
}

TEST(AudioSpatialRendererStandaloneTest, MonoSpeakerPreviewProducesStereoWithSourceMetadata)
{
    SpatialRenderer renderer;
    const auto input = makeSegment(1, 8, 0.5f);
    AudioSegment output;
    render(renderer, input, output, 8);
    ASSERT_EQ(output.channelCount(), 2);
    EXPECT_EQ(output.frameCount(), 8);
    EXPECT_EQ(output.layout, AudioChannelLayout::Stereo);
    EXPECT_EQ(output.sampleRate, input.sampleRate);
    EXPECT_EQ(output.startFrame, input.startFrame);
    EXPECT_TRUE(std::isfinite(output.channelData[0][7]));
    EXPECT_TRUE(std::isfinite(output.channelData[1][7]));
}

TEST(AudioSpatialRendererStandaloneTest, MultichannelSpeakerModeProducesRequestedLayout)
{
    SpatialRenderer renderer;
    SpatialParams params;
    params.outputLayout = AudioChannelLayout::Surround51;
    renderer.publishParams(params);
    const auto input = makeSegment(1, 16, 0.5f);
    AudioSegment output;
    render(renderer, input, output, 16);
    EXPECT_EQ(output.channelCount(), 6);
    EXPECT_EQ(output.frameCount(), 16);
    EXPECT_EQ(output.layout, AudioChannelLayout::Surround51);
}

TEST(AudioSpatialRendererStandaloneTest, SevenOneFourLayoutCreatesTwelveOutputChannels)
{
    SpatialRenderer renderer;
    SpatialParams params;
    params.outputLayout = AudioChannelLayout::Surround714;
    params.stereoWidthDegrees = 0.0f;
    renderer.publishParams(params);
    const auto input = makeSegment(1, 4);
    AudioSegment output;
    render(renderer, input, output, 4);
    EXPECT_EQ(output.channelCount(), 12);
    for (const auto& channel : output.channelData) {
        EXPECT_EQ(channel.size(), 4);
    }
}

TEST(AudioSpatialRendererStandaloneTest, LfeIsSilentUntilExplicitlyEnabled)
{
    SpatialRenderer renderer;
    SpatialParams params;
    params.outputLayout = AudioChannelLayout::Surround51;
    renderer.publishParams(params);
    const auto input = makeSegment(1, 32);
    AudioSegment output;
    render(renderer, input, output, 32);
    ASSERT_EQ(output.channelCount(), 6);
    for (float sample : output.channelData[3]) {
        EXPECT_FLOAT_EQ(sample, 0.0f);
    }
}

TEST(AudioSpatialRendererStandaloneTest, ExplicitLfeSendFeedsTheLfeChannel)
{
    SpatialRenderer renderer;
    SpatialParams params;
    params.outputLayout = AudioChannelLayout::Surround51;
    params.lfeSend = 1.0f;
    renderer.publishParams(params);
    const auto input = makeSegment(1, 32);
    AudioSegment output;
    render(renderer, input, output, 32);
    ASSERT_EQ(output.channelCount(), 6);
    EXPECT_GT(output.channelData[3][31], 0.0f);
}

TEST(AudioSpatialRendererStandaloneTest, HeadphoneModeAlwaysProducesStereo)
{
    SpatialRenderer renderer;
    SpatialParams params;
    params.renderMode = SpatialRenderMode::Headphone;
    params.outputLayout = AudioChannelLayout::Surround714;
    renderer.publishParams(params);
    const auto input = makeSegment(1, 64, 0.25f);
    AudioSegment output;
    render(renderer, input, output, 64);
    EXPECT_EQ(output.channelCount(), 2);
    EXPECT_EQ(output.frameCount(), 64);
    EXPECT_EQ(output.layout, AudioChannelLayout::Stereo);
}

TEST(AudioSpatialRendererStandaloneTest, CenteredHeadphoneSourceHasMatchedEars)
{
    SpatialRenderer renderer;
    SpatialParams params;
    params.renderMode = SpatialRenderMode::Headphone;
    renderer.publishParams(params);
    const auto input = makeSegment(1, 32, 0.75f);
    AudioSegment output;
    render(renderer, input, output, 32);
    ASSERT_EQ(output.channelCount(), 2);
    for (int i = 0; i < output.frameCount(); ++i) {
        EXPECT_NEAR(output.channelData[0][i], output.channelData[1][i], 1e-6f);
    }
}

TEST(AudioSpatialRendererStandaloneTest, RightSideHeadphoneSourceHasFarEarAttenuation)
{
    SpatialRenderer renderer;
    SpatialParams params;
    params.renderMode = SpatialRenderMode::Headphone;
    renderer.publishParams(params);
    const auto input = makeSegment(1, 512, 1.0f);
    AudioSegment output;
    render(renderer, input, output, 512, {1.0f, 0.0f, 0.0f});
    ASSERT_EQ(output.channelCount(), 2);
    EXPECT_LT(output.channelData[0][511], output.channelData[1][511]);
}

TEST(AudioSpatialRendererStandaloneTest, NonFiniteSamplesAreConvertedToFiniteOutput)
{
    SpatialRenderer renderer;
    auto input = makeSegment(1, 4);
    input.channelData[0][1] = std::numeric_limits<float>::quiet_NaN();
    input.channelData[0][2] = std::numeric_limits<float>::infinity();
    AudioSegment output;
    render(renderer, input, output, 4);
    for (const auto& channel : output.channelData) {
        for (float sample : channel) {
            EXPECT_TRUE(std::isfinite(sample));
        }
    }
}

TEST(AudioSpatialRendererStandaloneTest, InvalidTransformsDoNotPropagateNonFiniteSamples)
{
    SpatialRenderer renderer;
    const auto input = makeSegment(1, 4);
    AudioSegment output;
    renderer.processBlock(input, output, 4,
        {std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f},
        {0.0f, 0.0f, 0.0f, std::numeric_limits<float>::infinity()},
        Vec3{}, Quat{});
    for (const auto& channel : output.channelData) {
        for (float sample : channel) EXPECT_TRUE(std::isfinite(sample));
    }
}

TEST(AudioSpatialRendererStandaloneTest, CoincidentSourceAndListenerRemainFinite)
{
    SpatialRenderer renderer;
    const auto input = makeSegment(2, 8);
    AudioSegment output;
    render(renderer, input, output, 8, Vec3{});
    for (const auto& channel : output.channelData) {
        for (float sample : channel) EXPECT_TRUE(std::isfinite(sample));
    }
}

TEST(AudioSpatialRendererStandaloneTest, ResetRestoresDeterministicHeadphoneHistory)
{
    SpatialRenderer renderer;
    SpatialParams params;
    params.renderMode = SpatialRenderMode::Headphone;
    renderer.publishParams(params);
    const auto input = makeSegment(1, 64, 0.5f);
    AudioSegment first;
    AudioSegment second;
    render(renderer, input, first, 64, {1.0f, 0.0f, 0.0f});
    renderer.reset();
    render(renderer, input, second, 64, {1.0f, 0.0f, 0.0f});
    ASSERT_EQ(first.channelCount(), second.channelCount());
    for (int channel = 0; channel < first.channelCount(); ++channel) {
        for (int frame = 0; frame < first.frameCount(); ++frame) {
            EXPECT_FLOAT_EQ(first.channelData[channel][frame], second.channelData[channel][frame]);
        }
    }
}
