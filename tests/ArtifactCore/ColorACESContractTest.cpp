#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

import Color.ACES;
import Color.GamutConversion;
import Color.TransferFunction;

using namespace ArtifactCore;

TEST(ColorACESContractTest, EncodedAndLinearSrgbInputsConvergeToTheSameWorkingValues)
{
    float encodedRed = 0.5f;
    float encodedGreen = 0.25f;
    float encodedBlue = 0.75f;
    float linearRed = ColorTransferFunction::decode(encodedRed, TransferFunction::sRGB);
    float linearGreen = ColorTransferFunction::decode(encodedGreen, TransferFunction::sRGB);
    float linearBlue = ColorTransferFunction::decode(encodedBlue, TransferFunction::sRGB);

    ACESColorManager::applyInputTransform(
        encodedRed, encodedGreen, encodedBlue, ACESInputTransform::sRGB_Encoded);
    ACESColorManager::applyInputTransform(
        linearRed, linearGreen, linearBlue, ACESInputTransform::sRGB_Linear);

    EXPECT_NEAR(encodedRed, linearRed, 1e-5f);
    EXPECT_NEAR(encodedGreen, linearGreen, 1e-5f);
    EXPECT_NEAR(encodedBlue, linearBlue, 1e-5f);
}

TEST(ColorACESContractTest, EveryInputPresetMatchesItsEotfAndPrimaryConversion)
{
    struct InputCase {
        ACESInputTransform input;
        TransferFunction transfer;
        Gamut gamut;
    };
    constexpr InputCase cases[] = {
        {ACESInputTransform::sRGB_Linear, TransferFunction::Linear, Gamut::sRGB},
        {ACESInputTransform::sRGB_Encoded, TransferFunction::sRGB, Gamut::sRGB},
        {ACESInputTransform::Rec709_Encoded, TransferFunction::Rec709, Gamut::Rec709},
        {ACESInputTransform::Rec2020_Encoded, TransferFunction::Rec2020_10, Gamut::Rec2020},
        {ACESInputTransform::P3_Encoded, TransferFunction::sRGB, Gamut::DCI_P3},
        {ACESInputTransform::Linear_sRGB_Primaries, TransferFunction::Linear, Gamut::sRGB},
    };
    constexpr float values[] = {
        0.0f, 0.01f, 0.02f, 0.08124286f, 0.18f, 0.5f, 0.75f, 1.0f,
    };

    for (const InputCase& testCase : cases) {
        for (const float inputR : values) {
            for (const float inputG : values) {
                for (const float inputB : values) {
                    const float linearR = ColorTransferFunction::decode(inputR, testCase.transfer);
                    const float linearG = ColorTransferFunction::decode(inputG, testCase.transfer);
                    const float linearB = ColorTransferFunction::decode(inputB, testCase.transfer);
                    float expectedR = 0.0f;
                    float expectedG = 0.0f;
                    float expectedB = 0.0f;
                    ColorGamutConversion::convert(
                        linearR, linearG, linearB, testCase.gamut, Gamut::ACES_AP1,
                        expectedR, expectedG, expectedB);

                    float actualR = inputR;
                    float actualG = inputG;
                    float actualB = inputB;
                    ACESColorManager::applyInputTransform(
                        actualR, actualG, actualB, testCase.input);

                    SCOPED_TRACE(::testing::Message()
                        << "input=" << static_cast<int>(testCase.input)
                        << " sample=" << inputR << "," << inputG << "," << inputB);
                    EXPECT_NEAR(actualR, expectedR, 2.0e-6f);
                    EXPECT_NEAR(actualG, expectedG, 2.0e-6f);
                    EXPECT_NEAR(actualB, expectedB, 2.0e-6f);
                }
            }
        }
    }
}

TEST(ColorACESContractTest, InputTransformPreservesLinearSrgbBlack)
{
    float red = 0.0f;
    float green = 0.0f;
    float blue = 0.0f;

    ACESColorManager::applyInputTransform(red, green, blue, ACESInputTransform::sRGB_Linear);

    EXPECT_NEAR(red, 0.0f, 1e-6f);
    EXPECT_NEAR(green, 0.0f, 1e-6f);
    EXPECT_NEAR(blue, 0.0f, 1e-6f);
}

TEST(ColorACESContractTest, OutputTransformMapsBlackToBlackAndProducesFiniteChannels)
{
    float red = 0.0f;
    float green = 0.0f;
    float blue = 0.0f;

    ACESColorManager::applyOutputTransform(red, green, blue, ACESOutputTransform::SDR_sRGB);

    EXPECT_FLOAT_EQ(red, 0.0f);
    EXPECT_FLOAT_EQ(green, 0.0f);
    EXPECT_FLOAT_EQ(blue, 0.0f);

    red = 0.18f;
    green = 0.18f;
    blue = 0.18f;
    ACESColorManager::applyOutputTransform(red, green, blue, ACESOutputTransform::SDR_sRGB);
    for (const float channel : {red, green, blue}) {
        EXPECT_TRUE(std::isfinite(channel));
        EXPECT_GE(channel, 0.0f);
        EXPECT_LE(channel, 1.0f);
    }
}

TEST(ColorACESContractTest,
     EveryOutputPresetMatchesReferenceAcrossFull16BitNeutralRamp)
{
    struct OutputCase {
        ACESOutputTransform output;
        Gamut gamut;
        TransferFunction transfer;
    };
    constexpr std::array<OutputCase, 5> cases = {{
        {ACESOutputTransform::SDR_sRGB, Gamut::Rec709, TransferFunction::sRGB},
        {ACESOutputTransform::SDR_Rec709, Gamut::Rec709, TransferFunction::Rec709},
        {ACESOutputTransform::SDR_P3_D65, Gamut::DCI_P3, TransferFunction::sRGB},
        {ACESOutputTransform::HDR_Rec2020_PQ, Gamut::Rec2020, TransferFunction::Rec2084_PQ},
        {ACESOutputTransform::HDR_P3_D65_HLG, Gamut::Rec2020, TransferFunction::HLG},
    }};
    constexpr float slope = 2.51f;
    constexpr float offset = 0.03f;
    constexpr float shoulder = 2.43f;
    constexpr float toe = 0.59f;
    constexpr float knee = 0.14f;
    const auto referenceRrt = [](const float value) {
        if (value <= 0.0f) return 0.0f;
        return value * (slope * value + offset)
            / (value * (shoulder * value + toe) + knee);
    };

    for (const OutputCase& testCase : cases) {
        float maximumDifference = 0.0f;
        std::uint32_t maximumDifferenceCode = 0;
        std::size_t maximumDifferenceChannel = 0;
        std::size_t nonFiniteCount = 0;
        std::size_t outOfRangeCount = 0;
        for (std::uint32_t code = 0; code <= 65535; ++code) {
            const float linear = static_cast<float>(code) / 65535.0f;
            std::array<float, 3> actual = {linear, linear, linear};
            ACESColorManager::applyOutputTransform(
                actual[0], actual[1], actual[2], testCase.output);

            std::array<float, 3> expected = {linear, linear, linear};
            ColorGamutConversion::convert(
                linear, linear, linear, Gamut::ACES_AP1, testCase.gamut,
                expected[0], expected[1], expected[2]);
            for (float& channel : expected) {
                channel = ColorTransferFunction::encode(
                    referenceRrt(channel), testCase.transfer);
            }

            for (std::size_t channel = 0; channel < actual.size(); ++channel) {
                if (!std::isfinite(actual[channel])) {
                    ++nonFiniteCount;
                } else if (actual[channel] < 0.0f || actual[channel] > 1.0f) {
                    ++outOfRangeCount;
                }
                const float difference = std::abs(actual[channel] - expected[channel]);
                if (difference > maximumDifference) {
                    maximumDifference = difference;
                    maximumDifferenceCode = code;
                    maximumDifferenceChannel = channel;
                }
            }
        }
        SCOPED_TRACE(static_cast<int>(testCase.output));
        EXPECT_EQ(nonFiniteCount, 0u);
        EXPECT_EQ(outOfRangeCount, 0u);
        EXPECT_LE(maximumDifference, 3.0e-6f)
            << "code=" << maximumDifferenceCode
            << " channel=" << maximumDifferenceChannel;
    }
}

TEST(ColorACESContractTest, EveryOutputPresetMatchesGamutRrtAndOetfReference)
{
    struct OutputCase {
        ACESOutputTransform output;
        Gamut gamut;
        TransferFunction transfer;
    };
    constexpr OutputCase cases[] = {
        {ACESOutputTransform::SDR_sRGB, Gamut::Rec709, TransferFunction::sRGB},
        {ACESOutputTransform::SDR_Rec709, Gamut::Rec709, TransferFunction::Rec709},
        {ACESOutputTransform::SDR_P3_D65, Gamut::DCI_P3, TransferFunction::sRGB},
        {ACESOutputTransform::HDR_Rec2020_PQ, Gamut::Rec2020, TransferFunction::Rec2084_PQ},
        {ACESOutputTransform::HDR_P3_D65_HLG, Gamut::Rec2020, TransferFunction::HLG},
    };
    constexpr float samples[][3] = {
        {0.0f, 0.0f, 0.0f},
        {0.18f, 0.18f, 0.18f},
        {0.7f, 0.2f, 0.05f},
        {0.02f, 0.4f, 1.5f},
        {4.0f, 1.0f, 0.25f},
    };
    constexpr float slope = 2.51f;
    constexpr float offset = 0.03f;
    constexpr float shoulder = 2.43f;
    constexpr float toe = 0.59f;
    constexpr float knee = 0.14f;

    const auto filmic = [](float value) {
        if (value <= 0.0f) return 0.0f;
        return value * (slope * value + offset)
            / (value * (shoulder * value + toe) + knee);
    };

    for (const OutputCase& testCase : cases) {
        for (const auto& sample : samples) {
            float expected[3] = {};
            ColorGamutConversion::convert(
                sample[0], sample[1], sample[2], Gamut::ACES_AP1, testCase.gamut,
                expected[0], expected[1], expected[2]);
            for (float& channel : expected) {
                channel = ColorTransferFunction::encode(filmic(channel), testCase.transfer);
            }

            float actualR = sample[0];
            float actualG = sample[1];
            float actualB = sample[2];
            ACESColorManager::applyOutputTransform(
                actualR, actualG, actualB, testCase.output);

            SCOPED_TRACE(::testing::Message()
                << "output=" << static_cast<int>(testCase.output)
                << " sample=" << sample[0] << "," << sample[1] << "," << sample[2]);
            EXPECT_NEAR(actualR, expected[0], 3.0e-6f);
            EXPECT_NEAR(actualG, expected[1], 3.0e-6f);
            EXPECT_NEAR(actualB, expected[2], 3.0e-6f);
        }
    }
}

TEST(ColorACESContractTest, SoftClipLeavesValuesBelowLimitAndCompressesHighlights)
{
    float red = 0.5f;
    float green = 1.0f;
    float blue = 2.0f;

    ACESColorManager::softClip(red, green, blue);

    EXPECT_FLOAT_EQ(red, 0.5f);
    EXPECT_FLOAT_EQ(green, 1.0f);
    EXPECT_NEAR(blue, 1.5f, 1e-6f);
}

TEST(ColorACESContractTest, SoftClipMatchesIndependentReferenceAcrossLimitAndHdrGrid)
{
    constexpr std::array<float, 5> limits = {-0.5f, 0.0f, 0.5f, 1.0f, 2.0f};
    constexpr std::array<float, 5> sceneSamples = {
        -3.0f, 0.0f, 0.18f, 4.0f, 1.0e20f,
    };
    const auto reference = [](const float value, const float limit) {
        if (value <= limit) return static_cast<double>(value);
        const double excess = static_cast<double>(value) - limit;
        return static_cast<double>(limit) + excess / (1.0 + excess);
    };

    for (const float limit : limits) {
        const std::array<float, 8> samples = {
            std::nextafter(limit, -std::numeric_limits<float>::infinity()),
            limit,
            std::nextafter(limit, std::numeric_limits<float>::infinity()),
            sceneSamples[0], sceneSamples[1], sceneSamples[2],
            sceneSamples[3], sceneSamples[4],
        };
        for (const float sample : samples) {
            float red = sample;
            float green = sample;
            float blue = sample;
            ACESColorManager::softClip(red, green, blue, limit);
            const double expected = reference(sample, limit);
            SCOPED_TRACE(::testing::Message()
                << "limit=" << limit << " sample=" << sample);
            EXPECT_TRUE(std::isfinite(red));
            EXPECT_TRUE(std::isfinite(green));
            EXPECT_TRUE(std::isfinite(blue));
            EXPECT_NEAR(red, expected, std::max(1.0e-7, std::abs(expected) * 2.0e-7));
            EXPECT_NEAR(green, expected, std::max(1.0e-7, std::abs(expected) * 2.0e-7));
            EXPECT_NEAR(blue, expected, std::max(1.0e-7, std::abs(expected) * 2.0e-7));
        }
    }
}

TEST(ColorACESContractTest, GamutPredicateAllowsSceneLinearHighlightsButRejectsNegativeChannels)
{
    EXPECT_TRUE(ACESColorManager::isInGamut(1.5f, 0.2f, 8.0f));
    EXPECT_FALSE(ACESColorManager::isInGamut(0.1f, -0.001f, 0.2f));
}
