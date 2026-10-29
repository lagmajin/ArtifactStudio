#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <limits>

import Image.DeepImageBuffer;

using ArtifactCore::DeepImageBuffer;
using ArtifactCore::DeepImagePacked;
using ArtifactCore::DeepSample;

namespace {
DeepSample sample(float depth, std::array<float, 4> color, float alpha,
                  float coverage = 1.0f, bool holdout = false,
                  float depthBack = -1.0f)
{
    DeepSample value;
    value.depth = depth;
    value.depthBack = depthBack < 0.0f ? depth : depthBack;
    value.color = color;
    value.alpha = alpha;
    value.coverage = coverage;
    value.holdout = holdout;
    return value;
}
}

TEST(DeepImageBufferContractTest, RejectsInvalidSamplesAndNormalizesAcceptedValues)
{
    DeepImageBuffer image(1, 1);
    EXPECT_FALSE(image.addSample(-1, 0, sample(1.0f, {1, 0, 0, 1}, 1.0f)));

    auto invalid = sample(1.0f, {1, 0, 0, 1}, 1.0f);
    invalid.color[1] = std::numeric_limits<float>::quiet_NaN();
    EXPECT_FALSE(image.addSample(0, 0, invalid));

    auto value = sample(2.0f, {0.25f, 0.5f, 0.75f, 1.0f}, 2.0f, -1.0f,
                        false, 1.0f);
    ASSERT_TRUE(image.addSample(0, 0, value));
    const auto* pixel = image.pixel(0, 0);
    ASSERT_NE(pixel, nullptr);
    ASSERT_EQ(pixel->samples.size(), 1u);
    EXPECT_FLOAT_EQ(pixel->samples[0].depthBack, 2.0f);
    EXPECT_FLOAT_EQ(pixel->samples[0].alpha, 1.0f);
    EXPECT_FLOAT_EQ(pixel->samples[0].coverage, 0.0f);
    EXPECT_EQ(image.totalSampleCount(), 1u);
}

TEST(DeepImageBufferContractTest, ClipsDepthIntervalsInclusivelyAndRejectsReversedRange)
{
    DeepImageBuffer image(1, 1);
    ASSERT_TRUE(image.addSample(0, 0, sample(1.0f, {1, 0, 0, 1}, 1.0f)));
    ASSERT_TRUE(image.addSample(0, 0, sample(2.0f, {0, 1, 0, 1}, 1.0f)));
    ASSERT_TRUE(image.addSample(0, 0, sample(3.0f, {0, 0, 1, 1}, 1.0f)));

    EXPECT_FALSE(image.clipDepthRange(3.0f, 2.0f));
    EXPECT_EQ(image.totalSampleCount(), 3u);
    ASSERT_TRUE(image.clipDepthRange(1.0f, 2.0f));
    ASSERT_EQ(image.pixel(0, 0)->samples.size(), 2u);
    EXPECT_FLOAT_EQ(image.pixel(0, 0)->samples[0].depth, 1.0f);
    EXPECT_FLOAT_EQ(image.pixel(0, 0)->samples[1].depth, 2.0f);
}

TEST(DeepImageBufferContractTest, FlattensFrontToBackAndExportsInclusiveDepthMatte)
{
    DeepImageBuffer image(1, 1);
    ASSERT_TRUE(image.addSample(0, 0, sample(2.0f, {0, 1, 0, 1}, 0.5f)));
    ASSERT_TRUE(image.addSample(0, 0, sample(1.0f, {1, 0, 0, 1}, 0.5f)));
    image.sortSamplesByDepth();

    std::array<float, 4> flat{};
    ASSERT_TRUE(image.toFlatRGBA(flat.data(), flat.size()));
    EXPECT_FLOAT_EQ(flat[0], 0.5f);
    EXPECT_FLOAT_EQ(flat[1], 0.25f);
    EXPECT_FLOAT_EQ(flat[2], 0.0f);
    EXPECT_FLOAT_EQ(flat[3], 0.75f);

    std::array<float, 4> matte{};
    ASSERT_TRUE(image.toDepthMatteRGBA(matte.data(), matte.size(), 2.0f, 2.0f));
    EXPECT_FLOAT_EQ(matte[0], 1.0f);
    EXPECT_FLOAT_EQ(matte[1], 1.0f);
    EXPECT_FLOAT_EQ(matte[2], 1.0f);
    EXPECT_FLOAT_EQ(matte[3], 0.5f);
    EXPECT_FALSE(image.toFlatRGBA(flat.data(), flat.size() - 1));
    EXPECT_FALSE(image.toDepthMatteRGBA(matte.data(), matte.size(), 3.0f, 2.0f));
}

TEST(DeepImageBufferContractTest, HoldoutOnlyReducesSamplesAtOrBehindItsDepth)
{
    DeepImageBuffer matte(1, 1);
    DeepImageBuffer target(1, 1);
    ASSERT_TRUE(matte.addSample(0, 0,
        sample(2.0f, {0, 0, 0, 1}, 0.25f, 1.0f, true, 3.0f)));
    ASSERT_TRUE(target.addSample(0, 0, sample(1.0f, {1, 0, 0, 1}, 0.8f)));
    ASSERT_TRUE(target.addSample(0, 0, sample(2.5f, {0, 1, 0, 1}, 0.8f)));

    ASSERT_TRUE(ArtifactCore::applyDeepHoldout(matte, target));
    const auto* pixel = target.pixel(0, 0);
    ASSERT_NE(pixel, nullptr);
    ASSERT_EQ(pixel->samples.size(), 2u);
    EXPECT_FLOAT_EQ(pixel->samples[0].alpha, 0.8f);
    EXPECT_FLOAT_EQ(pixel->samples[1].alpha, 0.6f);
    EXPECT_FLOAT_EQ(pixel->samples[1].coverage, 0.75f);
}

TEST(DeepImageBufferContractTest, PackedBuffersRoundTripAndRejectCorruption)
{
    DeepImageBuffer image(2, 1);
    ASSERT_TRUE(image.addSample(0, 0, sample(4.0f, {0.1f, 0.2f, 0.3f, 1.0f},
                                               0.7f, 0.5f, true)));
    ASSERT_TRUE(image.addSample(1, 0, sample(1.0f, {0.8f, 0.4f, 0.2f, 1.0f},
                                               0.9f)));
    DeepImagePacked packed;
    ASSERT_TRUE(ArtifactCore::packDeepImage(image, packed));
    ASSERT_TRUE(packed.isValid());
    ASSERT_TRUE(packed.buffersAreConsistent());
    ASSERT_EQ(packed.sampleOffsets.size(), 2u);
    EXPECT_EQ(packed.sampleOffsets[0], 0u);
    EXPECT_EQ(packed.sampleOffsets[1], 1u);
    EXPECT_EQ(packed.sampleCounts[0], 1u);
    EXPECT_EQ(packed.gpuSamples[0].holdout, 1u);
    EXPECT_EQ(packed.gpuSampleStrideBytes(), 40u);

    DeepImageBuffer unpacked;
    ASSERT_TRUE(ArtifactCore::unpackDeepImage(packed, unpacked));
    DeepImagePacked roundTrip;
    ASSERT_TRUE(ArtifactCore::packDeepImage(unpacked, roundTrip));
    EXPECT_TRUE(roundTrip.buffersAreConsistent());
    EXPECT_EQ(roundTrip.flatSamples.size(), packed.flatSamples.size());
    EXPECT_FLOAT_EQ(roundTrip.flatSamples[0].depth, packed.flatSamples[0].depth);
    EXPECT_FLOAT_EQ(roundTrip.flatSamples[0].coverage, packed.flatSamples[0].coverage);
    EXPECT_TRUE(roundTrip.flatSamples[0].holdout);

    packed.gpuSamples[0].alpha = 0.1f;
    EXPECT_FALSE(packed.buffersAreConsistent());
    EXPECT_FALSE(ArtifactCore::unpackDeepImage(packed, unpacked));
}

TEST(DeepImageBufferContractTest, MergeSortsAllSamplesAndEnforcesPerPixelLimit)
{
    DeepImageBuffer front(1, 1);
    DeepImageBuffer back(1, 1);
    ASSERT_TRUE(front.addSample(0, 0, sample(3.0f, {0, 0, 1, 1}, 0.6f)));
    ASSERT_TRUE(front.addSample(0, 0, sample(1.0f, {1, 0, 0, 1}, 0.6f)));
    ASSERT_TRUE(back.addSample(0, 0, sample(2.0f, {0, 1, 0, 1}, 0.6f)));

    DeepImageBuffer merged;
    ASSERT_TRUE(ArtifactCore::mergeDeepOver(front, back, merged));
    ASSERT_EQ(merged.pixel(0, 0)->samples.size(), 3u);
    EXPECT_FLOAT_EQ(merged.pixel(0, 0)->samples[0].depth, 1.0f);
    EXPECT_FLOAT_EQ(merged.pixel(0, 0)->samples[1].depth, 2.0f);
    EXPECT_FLOAT_EQ(merged.pixel(0, 0)->samples[2].depth, 3.0f);

    ASSERT_TRUE(ArtifactCore::mergeDeepOver(front, back, merged, 2));
    ASSERT_EQ(merged.pixel(0, 0)->samples.size(), 2u);
    EXPECT_FLOAT_EQ(merged.pixel(0, 0)->samples[0].depth, 1.0f);
    EXPECT_FLOAT_EQ(merged.pixel(0, 0)->samples[1].depth, 2.0f);
    EXPECT_FALSE(ArtifactCore::mergeDeepOver(front, DeepImageBuffer(2, 1), merged));
}
