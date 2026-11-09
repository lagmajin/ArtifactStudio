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

TEST(DeepImageBufferContractTest,
     ClippedDepthIntervalsAndMatteExportAgreeAtNearAndFarBoundaries)
{
    DeepImageBuffer image(1, 1);
    ASSERT_TRUE(image.addSample(0, 0,
        sample(0.0f, {1, 0, 0, 1}, 0.4f, 0.5f, false, 3.0f)));
    ASSERT_TRUE(image.addSample(0, 0,
        sample(5.0f, {0, 1, 0, 1}, 0.5f, 1.0f, false, 7.0f)));
    ASSERT_TRUE(image.addSample(0, 0,
        sample(1.0f, {0, 0, 1, 1}, 0.9f, 1.0f, false, 2.0f)));
    ASSERT_TRUE(image.addSample(0, 0,
        sample(4.0f, {1, 1, 0, 1}, 1.0f, 1.0f, true)));

    ASSERT_TRUE(image.clipDepthRange(3.0f, 5.0f));
    const auto* clipped = image.pixel(0, 0);
    ASSERT_NE(clipped, nullptr);
    ASSERT_EQ(clipped->samples.size(), 3u);
    EXPECT_FLOAT_EQ(clipped->samples[0].depth, 0.0f);
    EXPECT_FLOAT_EQ(clipped->samples[0].depthBack, 3.0f);
    EXPECT_FLOAT_EQ(clipped->samples[1].depth, 5.0f);
    EXPECT_FLOAT_EQ(clipped->samples[1].depthBack, 7.0f);
    EXPECT_TRUE(clipped->samples[2].holdout);

    std::array<float, 4> matte{};
    ASSERT_TRUE(image.toDepthMatteRGBA(matte.data(), matte.size(), 3.0f, 5.0f));
    EXPECT_FLOAT_EQ(matte[0], 1.0f);
    EXPECT_FLOAT_EQ(matte[1], 1.0f);
    EXPECT_FLOAT_EQ(matte[2], 1.0f);
    // The boundary sample contributes .4*.5=.2, the far-boundary sample .5.
    EXPECT_FLOAT_EQ(matte[3], 0.6f);
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

TEST(DeepImageBufferContractTest,
     OverlappingHoldoutIntervalsMultiplyCoverageAtInclusiveDepthBoundaries)
{
    DeepImageBuffer holdout(2, 1);
    DeepImageBuffer target(2, 1);
    ASSERT_TRUE(holdout.addSample(0, 0,
        sample(2.0f, {0, 0, 0, 1}, 0.25f, 0.8f, true, 3.0f)));
    ASSERT_TRUE(holdout.addSample(0, 0,
        sample(3.0f, {0, 0, 0, 1}, 0.5f, 0.5f, true, 4.0f)));
    ASSERT_TRUE(holdout.addSample(1, 0,
        sample(1.0f, {0, 0, 0, 1}, 0.4f, 0.5f, true, 2.0f)));

    ASSERT_TRUE(target.addSample(0, 0,
        sample(1.0f, {1, 0, 0, 1}, 0.8f, 0.7f)));
    ASSERT_TRUE(target.addSample(0, 0,
        sample(1.0f, {0, 1, 0, 1}, 0.8f, 0.7f, false, 2.0f)));
    ASSERT_TRUE(target.addSample(0, 0,
        sample(2.5f, {0, 0, 1, 1}, 0.9f, 0.5f, false, 3.5f)));
    ASSERT_TRUE(target.addSample(0, 0,
        sample(4.1f, {1, 1, 0, 1}, 0.6f, 0.4f)));
    ASSERT_TRUE(target.addSample(0, 0,
        sample(2.5f, {1, 0, 1, 1}, 0.7f, 0.3f, true, 3.5f)));
    ASSERT_TRUE(target.addSample(1, 0,
        sample(0.5f, {1, 0, 0, 1}, 0.6f, 0.9f)));
    ASSERT_TRUE(target.addSample(1, 0,
        sample(1.5f, {0, 1, 0, 1}, 0.8f, 0.6f)));
    ASSERT_TRUE(target.addSample(1, 0,
        sample(3.0f, {0, 0, 1, 1}, 0.5f, 0.4f)));

    ASSERT_TRUE(ArtifactCore::applyDeepHoldout(holdout, target));
    const auto* left = target.pixel(0, 0);
    const auto* right = target.pixel(1, 0);
    ASSERT_NE(left, nullptr);
    ASSERT_NE(right, nullptr);
    ASSERT_EQ(left->samples.size(), 5u);
    ASSERT_EQ(right->samples.size(), 3u);

    EXPECT_FLOAT_EQ(left->samples[0].alpha, 0.8f);
    EXPECT_FLOAT_EQ(left->samples[0].coverage, 0.7f);
    // The target interval ends exactly at the first matte depth; contact is included.
    EXPECT_FLOAT_EQ(left->samples[1].alpha, 0.64f);
    EXPECT_FLOAT_EQ(left->samples[1].coverage, 0.56f);
    // Both matte intervals overlap this sample, so remaining alpha is .8 * .75.
    EXPECT_FLOAT_EQ(left->samples[2].alpha, 0.54f);
    EXPECT_FLOAT_EQ(left->samples[2].coverage, 0.3f);
    EXPECT_FLOAT_EQ(left->samples[3].alpha, 0.6f);
    EXPECT_FLOAT_EQ(left->samples[3].coverage, 0.4f);
    EXPECT_FLOAT_EQ(left->samples[4].alpha, 0.7f);
    EXPECT_FLOAT_EQ(left->samples[4].coverage, 0.3f);

    EXPECT_FLOAT_EQ(right->samples[0].alpha, 0.6f);
    EXPECT_FLOAT_EQ(right->samples[0].coverage, 0.9f);
    EXPECT_FLOAT_EQ(right->samples[1].alpha, 0.64f);
    EXPECT_FLOAT_EQ(right->samples[1].coverage, 0.48f);
    EXPECT_FLOAT_EQ(right->samples[2].alpha, 0.5f);
    EXPECT_FLOAT_EQ(right->samples[2].coverage, 0.4f);
}

TEST(DeepImageBufferContractTest,
     RankedRGBAExportsPerPixelSamplesWithEffectivePremultipliedAlpha)
{
    DeepImageBuffer image(2, 1);
    ASSERT_TRUE(image.addSample(0, 0,
        sample(3.0f, {0.0f, 0.0f, 1.0f, 1.0f}, 0.8f, 0.5f)));
    ASSERT_TRUE(image.addSample(0, 0,
        sample(2.0f, {1.0f, 1.0f, 0.0f, 1.0f}, 0.7f, 0.9f, true)));
    ASSERT_TRUE(image.addSample(0, 0,
        sample(1.0f, {1.0f, 0.0f, 0.0f, 1.0f}, 0.5f, 0.5f)));
    ASSERT_TRUE(image.addSample(1, 0,
        sample(4.0f, {1.0f, 1.0f, 1.0f, 1.0f}, 1.0f, 0.25f)));
    ASSERT_TRUE(image.addSample(1, 0,
        sample(2.0f, {0.0f, 1.0f, 0.0f, 1.0f}, 0.6f, 1.0f)));
    image.sortSamplesByDepth();

    std::array<float, 8> rankZero{};
    ASSERT_TRUE(image.toRankedRGBA(rankZero.data(), rankZero.size(), 0));
    const std::array<float, 8> expectedRankZero{
        0.25f, 0.0f, 0.0f, 0.25f,
        0.0f, 0.6f, 0.0f, 0.6f,
    };
    for (std::size_t index = 0; index < rankZero.size(); ++index) {
        EXPECT_FLOAT_EQ(rankZero[index], expectedRankZero[index]) << "value=" << index;
    }

    std::array<float, 8> rankOne{};
    ASSERT_TRUE(image.toRankedRGBA(rankOne.data(), rankOne.size(), 1));
    const std::array<float, 8> expectedRankOne{
        0.0f, 0.0f, 0.0f, 0.0f,
        0.25f, 0.25f, 0.25f, 0.25f,
    };
    for (std::size_t index = 0; index < rankOne.size(); ++index) {
        EXPECT_FLOAT_EQ(rankOne[index], expectedRankOne[index]) << "value=" << index;
    }

    std::array<float, 8> rankTwo{};
    ASSERT_TRUE(image.toRankedRGBA(rankTwo.data(), rankTwo.size(), 2));
    const std::array<float, 8> expectedRankTwo{
        0.0f, 0.0f, 0.4f, 0.4f,
        0.0f, 0.0f, 0.0f, 0.0f,
    };
    EXPECT_EQ(rankTwo, expectedRankTwo);

    std::array<float, 8> missingRank{};
    ASSERT_TRUE(image.toRankedRGBA(missingRank.data(), missingRank.size(), 3));
    EXPECT_EQ(missingRank, (std::array<float, 8>{}));
    EXPECT_FALSE(image.toRankedRGBA(rankZero.data(), rankZero.size() - 1, 0));
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

TEST(DeepImageBufferContractTest,
     PackedOffsetsRoundTripAcrossEmptyPixelsAndVariableSampleCounts)
{
    DeepImageBuffer image(3, 2);
    ASSERT_TRUE(image.addSample(0, 0,
        sample(1.0f, {1.0f, 0.0f, 0.0f, 1.0f}, 0.25f, 0.5f)));
    ASSERT_TRUE(image.addSample(0, 0,
        sample(4.0f, {0.0f, 1.0f, 0.0f, 1.0f}, 0.75f, 0.8f, true, 6.0f)));
    ASSERT_TRUE(image.addSample(2, 0,
        sample(3.0f, {0.0f, 0.0f, 1.0f, 1.0f}, 0.6f, 0.9f)));
    ASSERT_TRUE(image.addSample(0, 1,
        sample(0.0f, {0.2f, 0.3f, 0.4f, 1.0f}, 1.0f)));
    ASSERT_TRUE(image.addSample(0, 1,
        sample(2.0f, {0.5f, 0.6f, 0.7f, 1.0f}, 0.4f, 0.25f)));
    ASSERT_TRUE(image.addSample(0, 1,
        sample(5.0f, {0.8f, 0.9f, 1.0f, 1.0f}, 0.2f, 0.75f)));
    ASSERT_TRUE(image.addSample(2, 1,
        sample(7.0f, {0.9f, 0.1f, 0.2f, 1.0f}, 0.3f, 0.6f)));

    DeepImagePacked packed;
    ASSERT_TRUE(ArtifactCore::packDeepImage(image, packed));
    const std::array<std::uint32_t, 6> expectedOffsets{0, 2, 2, 3, 6, 6};
    const std::array<std::uint16_t, 6> expectedCounts{2, 0, 1, 3, 0, 1};
    ASSERT_EQ(packed.sampleOffsets.size(), expectedOffsets.size());
    ASSERT_EQ(packed.sampleCounts.size(), expectedCounts.size());
    ASSERT_EQ(packed.gpuSampleCounts.size(), expectedCounts.size());
    for (std::size_t index = 0; index < expectedOffsets.size(); ++index) {
        EXPECT_EQ(packed.sampleOffsets[index], expectedOffsets[index]);
        EXPECT_EQ(packed.sampleCounts[index], expectedCounts[index]);
        EXPECT_EQ(packed.gpuSampleCounts[index], expectedCounts[index]);
    }

    DeepImageBuffer unpacked;
    ASSERT_TRUE(ArtifactCore::unpackDeepImage(packed, unpacked));
    EXPECT_EQ(unpacked.width(), 3);
    EXPECT_EQ(unpacked.height(), 2);
    EXPECT_EQ(unpacked.totalSampleCount(), 7u);
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const auto* expectedPixel = image.pixel(x, y);
            const auto* actualPixel = unpacked.pixel(x, y);
            ASSERT_NE(expectedPixel, nullptr);
            ASSERT_NE(actualPixel, nullptr);
            ASSERT_EQ(actualPixel->samples.size(), expectedPixel->samples.size())
                << "x=" << x << " y=" << y;
            for (std::size_t index = 0; index < expectedPixel->samples.size(); ++index) {
                const auto& expected = expectedPixel->samples[index];
                const auto& actual = actualPixel->samples[index];
                EXPECT_FLOAT_EQ(actual.depth, expected.depth);
                EXPECT_FLOAT_EQ(actual.depthBack, expected.depthBack);
                EXPECT_EQ(actual.color, expected.color);
                EXPECT_FLOAT_EQ(actual.alpha, expected.alpha);
                EXPECT_FLOAT_EQ(actual.coverage, expected.coverage);
                EXPECT_EQ(actual.holdout, expected.holdout);
            }
        }
    }

    DeepImagePacked repacked;
    ASSERT_TRUE(ArtifactCore::packDeepImage(unpacked, repacked));
    EXPECT_TRUE(repacked.buffersAreConsistent());
    EXPECT_EQ(repacked.sampleOffsets.size(), packed.sampleOffsets.size());
    EXPECT_EQ(repacked.sampleCounts.size(), packed.sampleCounts.size());
    for (std::size_t index = 0; index < packed.sampleOffsets.size(); ++index) {
        EXPECT_EQ(repacked.sampleOffsets[index], packed.sampleOffsets[index]);
        EXPECT_EQ(repacked.sampleCounts[index], packed.sampleCounts[index]);
    }
}

TEST(DeepImageBufferContractTest,
     UnpackClearsPartiallyRestoredImageWhenMatchingBuffersContainNonFiniteSample)
{
    DeepImageBuffer source(2, 1);
    ASSERT_TRUE(source.addSample(0, 0,
        sample(1.0f, {1, 0, 0, 1}, 1.0f)));
    ASSERT_TRUE(source.addSample(1, 0,
        sample(2.0f, {0, 1, 0, 1}, 0.5f)));
    DeepImagePacked packed;
    ASSERT_TRUE(ArtifactCore::packDeepImage(source, packed));

    const float infinity = std::numeric_limits<float>::infinity();
    packed.flatSamples[1].depth = infinity;
    packed.gpuSamples[1].depth = infinity;
    ASSERT_TRUE(packed.buffersAreConsistent());

    DeepImageBuffer output(1, 1);
    ASSERT_TRUE(output.addSample(0, 0,
        sample(9.0f, {0.25f, 0.5f, 0.75f, 1.0f}, 1.0f)));
    ASSERT_FALSE(output.isEmpty());

    EXPECT_FALSE(ArtifactCore::unpackDeepImage(packed, output));
    EXPECT_TRUE(output.isEmpty());
    EXPECT_EQ(output.width(), 0);
    EXPECT_EQ(output.height(), 0);
    EXPECT_EQ(output.totalSampleCount(), 0u);
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

TEST(DeepImageBufferContractTest,
     MergeSampleLimitIsPerPixelAfterPruningAndStableDepthSort)
{
    DeepImageBuffer front(2, 2);
    DeepImageBuffer back(2, 2);
    ASSERT_TRUE(front.addSample(0, 0,
        sample(2.0f, {1, 0, 0, 1}, 0.5f)));
    ASSERT_TRUE(front.addSample(0, 0,
        sample(1.0f, {1, 1, 0, 1}, 0.0f)));
    ASSERT_TRUE(back.addSample(0, 0,
        sample(3.0f, {0, 1, 0, 1}, 0.6f)));

    ASSERT_TRUE(front.addSample(1, 0,
        sample(4.0f, {1, 0, 0, 1}, 0.5f)));
    ASSERT_TRUE(back.addSample(1, 0,
        sample(1.0f, {0, 0, 1, 1}, 0.5f)));
    ASSERT_TRUE(back.addSample(1, 0,
        sample(5.0f, {1, 1, 1, 1}, 0.5f)));

    ASSERT_TRUE(front.addSample(0, 1,
        sample(2.0f, {1, 0, 0, 1}, 0.5f)));
    ASSERT_TRUE(front.addSample(0, 1,
        sample(2.0f, {0, 1, 0, 1}, 0.5f)));
    ASSERT_TRUE(back.addSample(0, 1,
        sample(2.0f, {0, 0, 1, 1}, 0.5f)));

    DeepImageBuffer merged;
    ASSERT_TRUE(ArtifactCore::mergeDeepOver(front, back, merged, 2));
    ASSERT_EQ(merged.pixel(0, 0)->samples.size(), 2u);
    EXPECT_FLOAT_EQ(merged.pixel(0, 0)->samples[0].depth, 2.0f);
    EXPECT_FLOAT_EQ(merged.pixel(0, 0)->samples[1].depth, 3.0f);
    ASSERT_EQ(merged.pixel(1, 0)->samples.size(), 2u);
    EXPECT_FLOAT_EQ(merged.pixel(1, 0)->samples[0].depth, 1.0f);
    EXPECT_EQ(merged.pixel(1, 0)->samples[0].color[2], 1.0f);
    EXPECT_FLOAT_EQ(merged.pixel(1, 0)->samples[1].depth, 4.0f);
    ASSERT_EQ(merged.pixel(0, 1)->samples.size(), 2u);
    EXPECT_EQ(merged.pixel(0, 1)->samples[0].color[0], 1.0f)
        << "stable sorting keeps same-depth front samples ahead of the back sample";
    EXPECT_EQ(merged.pixel(0, 1)->samples[1].color[1], 1.0f);
    ASSERT_NE(merged.pixel(1, 1), nullptr);
    EXPECT_TRUE(merged.pixel(1, 1)->samples.empty());
}

TEST(DeepImageBufferContractTest,
     FlatAndDeepCompositingMatchIndependentFrontToBackAlphaOracle)
{
    DeepImageBuffer deepBack(1, 1);
    ASSERT_TRUE(deepBack.addSample(0, 0,
        sample(2.0f, {0.2f, 0.6f, 0.1f, 1.0f}, 0.5f)));
    ASSERT_TRUE(deepBack.addSample(0, 0,
        sample(3.0f, {0.1f, 0.3f, 0.9f, 1.0f}, 0.25f)));

    const std::array<float, 4> flatFront{0.8f, 0.25f, 0.5f, 0.4f};
    ASSERT_TRUE(ArtifactCore::compositeFlatOverDeep(
        flatFront.data(), 1, 1, 1.0f, deepBack));
    std::array<float, 4> flattenedFront{};
    ASSERT_TRUE(deepBack.toFlatRGBA(flattenedFront.data(), flattenedFront.size()));
    const float frontTransmittance = 1.0f - flatFront[3];
    const float firstDeepAlpha = 0.5f;
    const float secondDeepAlpha = 0.25f;
    EXPECT_NEAR(flattenedFront[0],
        flatFront[0] * flatFront[3] +
            frontTransmittance * 0.2f * firstDeepAlpha +
            frontTransmittance * (1.0f - firstDeepAlpha) * 0.1f * secondDeepAlpha,
        1.0e-6f);
    EXPECT_NEAR(flattenedFront[1],
        flatFront[1] * flatFront[3] +
            frontTransmittance * 0.6f * firstDeepAlpha +
            frontTransmittance * (1.0f - firstDeepAlpha) * 0.3f * secondDeepAlpha,
        1.0e-6f);
    EXPECT_NEAR(flattenedFront[2],
        flatFront[2] * flatFront[3] +
            frontTransmittance * 0.1f * firstDeepAlpha +
            frontTransmittance * (1.0f - firstDeepAlpha) * 0.9f * secondDeepAlpha,
        1.0e-6f);
    EXPECT_NEAR(flattenedFront[3],
        flatFront[3] + frontTransmittance * firstDeepAlpha +
            frontTransmittance * (1.0f - firstDeepAlpha) * secondDeepAlpha,
        1.0e-6f);

    DeepImageBuffer deepFront(1, 1);
    ASSERT_TRUE(deepFront.addSample(0, 0,
        sample(1.0f, {0.3f, 0.9f, 0.2f, 1.0f}, 0.25f)));
    ASSERT_TRUE(deepFront.addSample(0, 0,
        sample(4.0f, {0.8f, 0.1f, 0.4f, 1.0f}, 0.5f)));
    const std::array<float, 4> flatBack{0.2f, 0.4f, 0.7f, 0.6f};
    std::array<float, 4> flattenedDeep{};
    ASSERT_TRUE(ArtifactCore::compositeDeepOverFlat(
        deepFront, flatBack.data(), flatBack.size(), 10.0f,
        flattenedDeep.data()));
    const float deepTransmittance = (1.0f - 0.25f) * (1.0f - 0.5f);
    EXPECT_NEAR(flattenedDeep[0],
        0.3f * 0.25f + (1.0f - 0.25f) * 0.8f * 0.5f +
            deepTransmittance * flatBack[0] * flatBack[3], 1.0e-6f);
    EXPECT_NEAR(flattenedDeep[1],
        0.9f * 0.25f + (1.0f - 0.25f) * 0.1f * 0.5f +
            deepTransmittance * flatBack[1] * flatBack[3], 1.0e-6f);
    EXPECT_NEAR(flattenedDeep[2],
        0.2f * 0.25f + (1.0f - 0.25f) * 0.4f * 0.5f +
            deepTransmittance * flatBack[2] * flatBack[3], 1.0e-6f);
    EXPECT_NEAR(flattenedDeep[3],
        0.25f + (1.0f - 0.25f) * 0.5f + deepTransmittance * flatBack[3],
        1.0e-6f);
}

TEST(DeepImageBufferContractTest,
     MultiPixelFlatOverDeepCompositionMatchesPerPixelOracle)
{
    constexpr int width = 2;
    constexpr int height = 2;
    const std::array<float, width * height * 4> flatRgba{
        0.9f, 0.2f, 0.1f, 0.25f,
        0.1f, 0.8f, 0.3f, 0.5f,
        0.4f, 0.1f, 0.9f, 0.75f,
        0.7f, 0.6f, 0.2f, 1.0f,
    };
    const std::array<float, 4> nearAlpha{0.2f, 0.35f, 0.5f, 0.65f};
    const std::array<float, 4> farAlpha{0.3f, 0.4f, 0.45f, 0.25f};
    DeepImageBuffer deepBack(width, height);
    for (int index = 0; index < width * height; ++index) {
        const int x = index % width;
        const int y = index / width;
        const float offset = static_cast<float>(index);
        // Insert the farther sample first; compositing must sort per pixel.
        ASSERT_TRUE(deepBack.addSample(x, y, sample(
            8.0f + offset,
            {0.15f + 0.1f * offset, 0.7f - 0.1f * offset,
             0.25f + 0.05f * offset, 1.0f}, farAlpha[index], 0.8f)));
        ASSERT_TRUE(deepBack.addSample(x, y, sample(
            2.0f + offset,
            {0.8f - 0.1f * offset, 0.1f + 0.15f * offset,
             0.4f + 0.1f * offset, 1.0f}, nearAlpha[index], 0.6f)));
    }

    ASSERT_TRUE(ArtifactCore::compositeFlatOverDeep(
        flatRgba.data(), width, height, 1.0f, deepBack));
    std::array<float, width * height * 4> actual{};
    ASSERT_TRUE(deepBack.toFlatRGBA(actual.data(), actual.size()));

    for (int index = 0; index < width * height; ++index) {
        const float* flat = flatRgba.data() + index * 4;
        const float flatAlpha = flat[3];
        const float nearEffectiveAlpha = nearAlpha[index] * 0.6f;
        const float farEffectiveAlpha = farAlpha[index] * 0.8f;
        const float nearTransmittance = 1.0f - nearEffectiveAlpha;
        const float flatTransmittance = 1.0f - flatAlpha;
        const float offset = static_cast<float>(index);
        const std::array<float, 3> nearColor{
            0.8f - 0.1f * offset, 0.1f + 0.15f * offset,
            0.4f + 0.1f * offset};
        const std::array<float, 3> farColor{
            0.15f + 0.1f * offset, 0.7f - 0.1f * offset,
            0.25f + 0.05f * offset};
        for (int channel = 0; channel < 3; ++channel) {
            const float expected = flat[channel] * flatAlpha +
                flatTransmittance * nearColor[channel] * nearEffectiveAlpha +
                flatTransmittance * nearTransmittance * farColor[channel] *
                    farEffectiveAlpha;
            EXPECT_NEAR(actual[index * 4 + channel], expected, 1.0e-6f)
                << "pixel=" << index << " channel=" << channel;
        }
        const float expectedAlpha = flatAlpha +
            flatTransmittance * nearEffectiveAlpha +
            flatTransmittance * nearTransmittance * farEffectiveAlpha;
        EXPECT_NEAR(actual[index * 4 + 3], expectedAlpha, 1.0e-6f)
            << "pixel=" << index << " alpha";
    }
}

TEST(DeepImageBufferContractTest, DepthOfFieldUsesCircularKernelAndClampsAtImageEdges)
{
    DeepImageBuffer image(3, 3);
    const std::array<std::array<float, 3>, 3> colors = {{
        {{0.0f, 0.1f, 0.2f}},
        {{0.3f, 0.4f, 0.5f}},
        {{0.6f, 0.7f, 0.8f}},
    }};
    for (int y = 0; y < 3; ++y) {
        for (int x = 0; x < 3; ++x) {
            ASSERT_TRUE(image.addSample(x, y, sample(
                5.0f, {colors[y][x], 0.0f, 0.0f, 1.0f}, 1.0f)));
        }
    }

    std::array<float, 3 * 3 * 4> output{};
    ASSERT_TRUE(image.toDepthOfFieldRGBA(output.data(), output.size(),
                                         5.0f, 1.0f, 2));
    for (int pixelIndex = 0; pixelIndex < 9; ++pixelIndex) {
        EXPECT_FLOAT_EQ(output[static_cast<std::size_t>(pixelIndex) * 4],
                        colors[pixelIndex / 3][pixelIndex % 3]);
        EXPECT_FLOAT_EQ(output[static_cast<std::size_t>(pixelIndex) * 4 + 3], 1.0f);
    }

    DeepImageBuffer blurred(3, 3);
    for (int y = 0; y < 3; ++y) {
        for (int x = 0; x < 3; ++x) {
            const float depth = (x == 0 && y == 0) ? 6.0f : 5.0f;
            ASSERT_TRUE(blurred.addSample(x, y, sample(
                depth, {colors[y][x], 0.0f, 0.0f, 1.0f}, 1.0f)));
        }
    }
    ASSERT_TRUE(blurred.toDepthOfFieldRGBA(output.data(), output.size(),
                                           5.0f, 1.0f, 2));

    const float cornerWeight = std::exp(-1.0f / 2.0f);
    const float expectedCorner =
        (colors[0][0][0] + cornerWeight * colors[0][1][0] +
         cornerWeight * colors[1][0][0] + colors[1][1][0]) /
        (1.0f + 2.0f * cornerWeight + cornerWeight * cornerWeight);
    EXPECT_NEAR(output[0], expectedCorner, 1.0e-6f);
    EXPECT_FLOAT_EQ(output[3], 1.0f);

    EXPECT_FALSE(blurred.toDepthOfFieldRGBA(output.data(), output.size(),
                                            5.0f, 0.0f, 2));
    EXPECT_FALSE(blurred.toDepthOfFieldRGBA(output.data(), output.size(),
                                            5.0f, 1.0f, 65));
}

TEST(DeepImageBufferContractTest,
     DepthOfFieldChoosesFocusDepthFromFirstVisibleSample)
{
    DeepImageBuffer image(3, 1);
    ASSERT_TRUE(image.addSample(0, 0,
        sample(0.0f, {1.0f, 0.0f, 0.0f, 1.0f}, 1.0f)));
    ASSERT_TRUE(image.addSample(1, 0,
        sample(-2.0f, {1.0f, 1.0f, 1.0f, 1.0f}, 1.0f, 0.0f)));
    ASSERT_TRUE(image.addSample(1, 0,
        sample(0.0f, {0.0f, 1.0f, 0.0f, 1.0f}, 1.0f)));
    ASSERT_TRUE(image.addSample(2, 0,
        sample(0.0f, {0.0f, 0.0f, 1.0f, 1.0f}, 1.0f)));
    image.sortSamplesByDepth();

    std::array<float, 12> output{};
    ASSERT_TRUE(image.toDepthOfFieldRGBA(output.data(), output.size(),
                                         0.0f, 1.0f, 2));

    // The nearest sample is fully uncovered and contributes no color or alpha.
    // The visible green sample is at the focal depth, so this pixel's radius is 0.
    EXPECT_FLOAT_EQ(output[4], 0.0f);
    EXPECT_FLOAT_EQ(output[5], 1.0f);
    EXPECT_FLOAT_EQ(output[6], 0.0f);
    EXPECT_FLOAT_EQ(output[7], 1.0f);
}
