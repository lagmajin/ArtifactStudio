#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>

import Graphics.SurfaceColorContract;
import Image.ImageF32x4_RGBA;
import ImageProcessing.ChromaSpreadGlow;

using namespace ArtifactCore;

namespace {

ImageF32x4_RGBA makeBgraImage(const std::array<float, 12>& pixels)
{
    ImageF32x4_RGBA image;
    image.setFromBGRA32F(pixels.data(), 3, 1);
    image.setColorDescriptor(SurfaceColorDescriptor::unknownBgra32Float());
    return image;
}

}

TEST(ChromaSpreadGlowContractTest, ThresholdEqualityIsSelectedAndBelowThresholdIsNot)
{
    auto image = makeBgraImage({
        0.4f, 0.4f, 0.4f, 0.2f,
        0.5f, 0.5f, 0.5f, 0.4f,
        0.2f, 0.2f, 0.2f, 0.6f,
    });
    ChromaSpreadGlowSettings settings;
    settings.threshold = 0.5f;
    settings.glowRadius = 0.0f;
    settings.intensity = 0.5f;
    settings.aberrationScale = 1.0f;
    settings.dispersionSteps = 1;
    ChromaSpreadGlow effect;
    effect.process(image, settings);

    const float* pixels = image.rgba32fData();
    EXPECT_FLOAT_EQ(pixels[0], 0.4f);
    EXPECT_FLOAT_EQ(pixels[1], 0.4f);
    EXPECT_FLOAT_EQ(pixels[2], 0.4f);
    EXPECT_FLOAT_EQ(pixels[4], 0.75f);
    EXPECT_FLOAT_EQ(pixels[5], 0.75f);
    EXPECT_FLOAT_EQ(pixels[6], 0.75f);
    EXPECT_FLOAT_EQ(pixels[8], 0.2f);
    EXPECT_FLOAT_EQ(pixels[9], 0.2f);
    EXPECT_FLOAT_EQ(pixels[10], 0.2f);
    EXPECT_FLOAT_EQ(pixels[3], 0.2f);
    EXPECT_FLOAT_EQ(pixels[7], 0.4f);
    EXPECT_FLOAT_EQ(pixels[11], 0.6f);
}

TEST(ChromaSpreadGlowContractTest, ZeroIntensityLeavesBrightAndDarkPixelsUnchanged)
{
    auto image = makeBgraImage({
        0.1f, 0.2f, 0.9f, 0.3f,
        0.8f, 0.7f, 0.1f, 0.6f,
        0.2f, 0.4f, 0.6f, 0.9f,
    });
    std::array<float, 12> original{};
    std::copy_n(image.rgba32fData(), original.size(), original.data());
    ChromaSpreadGlowSettings settings;
    settings.threshold = 0.0f;
    settings.glowRadius = 8.0f;
    settings.intensity = 0.0f;
    ChromaSpreadGlow effect;
    effect.process(image, settings);

    for (std::size_t index = 0; index < original.size(); ++index) {
        EXPECT_FLOAT_EQ(image.rgba32fData()[index], original[index]);
    }
}

TEST(ChromaSpreadGlowContractTest, UnsupportedChannelOrderLeavesImageUnchanged)
{
    auto image = makeBgraImage({
        0.9f, 0.8f, 0.7f, 0.6f,
        0.5f, 0.4f, 0.3f, 0.2f,
        0.1f, 0.2f, 0.3f, 0.4f,
    });
    image.setColorDescriptor(SurfaceColorDescriptor::unknown());
    std::array<float, 12> original{};
    std::copy_n(image.rgba32fData(), original.size(), original.data());
    ChromaSpreadGlow effect;
    effect.process(image, {});

    for (std::size_t index = 0; index < original.size(); ++index) {
        EXPECT_FLOAT_EQ(image.rgba32fData()[index], original[index]);
    }
}

TEST(ChromaSpreadGlowContractTest, AdditiveCompositeClampsColorAndPreservesAlpha)
{
    auto image = makeBgraImage({
        0.95f, 0.95f, 0.95f, 0.25f,
        0.95f, 0.95f, 0.95f, 0.5f,
        0.95f, 0.95f, 0.95f, 0.75f,
    });
    ChromaSpreadGlowSettings settings;
    settings.threshold = 0.0f;
    settings.glowRadius = 0.0f;
    settings.intensity = 5.0f;
    settings.aberrationScale = 1.0f;
    settings.dispersionSteps = 1;
    ChromaSpreadGlow effect;
    effect.process(image, settings);

    const float* pixels = image.rgba32fData();
    for (std::size_t pixel = 0; pixel < 3; ++pixel) {
        EXPECT_FLOAT_EQ(pixels[pixel * 4], 1.0f);
        EXPECT_FLOAT_EQ(pixels[pixel * 4 + 1], 1.0f);
        EXPECT_FLOAT_EQ(pixels[pixel * 4 + 2], 1.0f);
        EXPECT_FLOAT_EQ(pixels[pixel * 4 + 3], 0.25f + 0.25f * static_cast<float>(pixel));
    }
}

