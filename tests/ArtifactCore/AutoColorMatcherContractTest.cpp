#include <gtest/gtest.h>

#include <array>

import Color.AutoMatch;

namespace {

using ArtifactCore::AutoColorMatcher;

auto sourcePixels() -> std::array<float, 8>
{
    // The public float-buffer contract stores channels as BGRA.
    return {0.0f, 0.0f, 0.0f, 0.2f,
            1.0f, 2.0f, 1.0f, 0.8f};
}

constexpr std::array<float, 8> kReferencePixels{
    1.0f, 2.0f, 0.25f, 0.1f,
    3.0f, 6.0f, 0.75f, 0.9f,
};

} // namespace

TEST(AutoColorMatcherContractTest, ComputesLogicalRgbTransformsFromBgraBuffers)
{
    const auto source = sourcePixels();
    const auto result = AutoColorMatcher::computeMatch(
        source.data(), kReferencePixels.data(), 2, 1, 2, 1,
        AutoColorMatcher::Method::MeanStddev);

    EXPECT_NEAR(result.scaleR, 0.5f, 1.0e-6f);
    EXPECT_NEAR(result.scaleG, 2.0f, 1.0e-6f);
    EXPECT_NEAR(result.scaleB, 2.0f, 1.0e-6f);
    EXPECT_NEAR(result.offsetR, 0.25f, 1.0e-6f);
    EXPECT_NEAR(result.offsetG, 2.0f, 1.0e-6f);
    EXPECT_NEAR(result.offsetB, 1.0f, 1.0e-6f);
    EXPECT_NEAR(result.confidence, 1.0f / 6.0f, 1.0e-6f);

    auto transformed = source;
    AutoColorMatcher::applyMatch(transformed.data(), 2, 1, result);
    for (int pixel = 0; pixel < 2; ++pixel) {
        const int index = pixel * 4;
        EXPECT_NEAR(transformed[index], kReferencePixels[index], 1.0e-6f);
        EXPECT_NEAR(transformed[index + 1], kReferencePixels[index + 1], 1.0e-6f);
        EXPECT_NEAR(transformed[index + 2], kReferencePixels[index + 2], 1.0e-6f);
        EXPECT_FLOAT_EQ(transformed[index + 3], source[index + 3]);
    }
}

TEST(AutoColorMatcherContractTest, MeanStddevDispatchMatchesUnequalSizedReferenceAndPreservesAlpha)
{
    auto source = sourcePixels();
    const std::array<float, 16> reference{
        1.0f, 2.0f, 0.25f, 0.0f,
        1.0f, 2.0f, 0.25f, 0.2f,
        3.0f, 6.0f, 0.75f, 0.4f,
        3.0f, 6.0f, 0.75f, 0.6f,
    };

    AutoColorMatcher::meanStddevMatch(source.data(), 2, 1,
                                      reference.data(), 4, 1);
    for (int pixel = 0; pixel < 2; ++pixel) {
        const int index = pixel * 4;
        EXPECT_NEAR(source[index], kReferencePixels[index], 1.0e-6f);
        EXPECT_NEAR(source[index + 1], kReferencePixels[index + 1], 1.0e-6f);
        EXPECT_NEAR(source[index + 2], kReferencePixels[index + 2], 1.0e-6f);
        EXPECT_FLOAT_EQ(source[index + 3], sourcePixels()[index + 3]);
    }
}

TEST(AutoColorMatcherContractTest, HistogramMatchMapsQuantilesWithoutChangingAlpha)
{
    std::array<float, 8> source{
        0.0f, 0.0f, 0.0f, 0.3f,
        1.0f, 1.0f, 1.0f, 0.7f,
    };
    const std::array<float, 8> reference{
        0.25f, 0.25f, 0.25f, 0.1f,
        0.75f, 0.75f, 0.75f, 0.9f,
    };

    AutoColorMatcher::histogramMatch(source.data(), 2, 1,
                                     reference.data(), 2, 1);
    for (int pixel = 0; pixel < 2; ++pixel) {
        const int index = pixel * 4;
        for (int channel = 0; channel < 3; ++channel) {
            EXPECT_NEAR(source[index + channel], reference[index + channel],
                        1.0f / 255.0f);
        }
        EXPECT_FLOAT_EQ(source[index + 3], pixel == 0 ? 0.3f : 0.7f);
    }
}

TEST(AutoColorMatcherContractTest, ZeroIntensityAndInvalidDimensionsLeaveSourceUntouched)
{
    const auto original = sourcePixels();
    constexpr std::array methods{
        AutoColorMatcher::Method::Reinhard,
        AutoColorMatcher::Method::MeanStddev,
        AutoColorMatcher::Method::Histogram,
    };
    for (const auto method : methods) {
        auto source = original;
        AutoColorMatcher::match(source.data(), kReferencePixels.data(),
                                2, 1, method, 0.0f);
        EXPECT_EQ(source, original);
    }

    auto invalidSource = original;
    AutoColorMatcher::match(invalidSource.data(), kReferencePixels.data(),
                            0, 1, AutoColorMatcher::Method::MeanStddev);
    EXPECT_EQ(invalidSource, original);
    AutoColorMatcher::match(nullptr, kReferencePixels.data(),
                            2, 1, AutoColorMatcher::Method::Histogram);
    AutoColorMatcher::applyMatch(nullptr, 2, 1, {});
}

TEST(AutoColorMatcherContractTest, IntensityBlendsPerChannelMatchAndLeavesAlphaAlone)
{
    auto pixels = sourcePixels();
    const AutoColorMatcher::MatchResult match{
        .scaleR = 2.0f, .scaleG = 3.0f, .scaleB = 4.0f,
        .offsetR = 0.1f, .offsetG = 0.2f, .offsetB = 0.3f,
    };
    AutoColorMatcher::applyMatch(pixels.data(), 2, 1, match, 0.5f);

    EXPECT_NEAR(pixels[0], 0.15f, 1.0e-6f);
    EXPECT_NEAR(pixels[1], 0.1f, 1.0e-6f);
    EXPECT_NEAR(pixels[2], 0.05f, 1.0e-6f);
    EXPECT_FLOAT_EQ(pixels[3], 0.2f);
    EXPECT_NEAR(pixels[4], 2.65f, 1.0e-6f);
    EXPECT_NEAR(pixels[5], 4.1f, 1.0e-6f);
    EXPECT_NEAR(pixels[6], 1.55f, 1.0e-6f);
    EXPECT_FLOAT_EQ(pixels[7], 0.8f);
}
