#include <gtest/gtest.h>

#include <type_traits>

import Core.Defaults;

using namespace ArtifactCore;

static_assert(kDefaultWidth == 1920);
static_assert(kDefaultHeight == 1080);
static_assert(kDefaultFrameRate == 30.0);
static_assert(kDefaultBitrate == 8000);
static_assert(kDefaultDurationFrames == 100);
static_assert(kDefaultSampleRate == 48000);
static_assert(kDefaultAudioChannels == 2);
static_assert(kAudioRingBufferCapacity == kDefaultSampleRate * 8);
static_assert(kDefaultRenderBitrateKbps == 8000);
static_assert(kDefaultStartFrame == 0);
static_assert(std::is_same_v<std::remove_cv_t<decltype(kDefaultWidth)>, int>);
static_assert(std::is_same_v<std::remove_cv_t<decltype(kDefaultFrameRate)>, double>);
static_assert(std::is_same_v<std::remove_cv_t<decltype(kAudioRingBufferCapacity)>, int>);

TEST(DefaultsContractTest, CompositionDefaultsHaveExpectedCanvasAndTiming)
{
    EXPECT_EQ(kDefaultWidth, 1920);
    EXPECT_EQ(kDefaultHeight, 1080);
    EXPECT_DOUBLE_EQ(kDefaultFrameRate, 30.0);
    EXPECT_EQ(kDefaultDurationFrames, 100);
}

TEST(DefaultsContractTest, ProjectBitrateDefaultsAreConsistent)
{
    EXPECT_EQ(kDefaultBitrate, 8000);
    EXPECT_EQ(kDefaultRenderBitrateKbps, kDefaultBitrate);
}

TEST(DefaultsContractTest, AudioDefaultsUseExpectedSampleLayout)
{
    EXPECT_EQ(kDefaultSampleRate, 48000);
    EXPECT_EQ(kDefaultAudioChannels, 2);
}

TEST(DefaultsContractTest, AudioRingBufferCapacityRepresentsEightSeconds)
{
    EXPECT_EQ(kAudioRingBufferCapacity, kDefaultSampleRate * 8);
}

TEST(DefaultsContractTest, BitrateDefaultsArePositive)
{
    EXPECT_GT(kDefaultBitrate, 0);
    EXPECT_GT(kDefaultRenderBitrateKbps, 0);
}

TEST(DefaultsContractTest, RenderStartsAtFrameZero)
{
    EXPECT_EQ(kDefaultStartFrame, 0);
}

TEST(DefaultsContractTest, DefaultCompositionAndAudioDimensionsArePositive)
{
    EXPECT_GT(kDefaultWidth, 0);
    EXPECT_GT(kDefaultHeight, 0);
    EXPECT_GT(kDefaultFrameRate, 0.0);
    EXPECT_GT(kDefaultDurationFrames, 0);
    EXPECT_GT(kDefaultSampleRate, 0);
    EXPECT_GT(kDefaultAudioChannels, 0);
    EXPECT_GT(kAudioRingBufferCapacity, 0);
}
