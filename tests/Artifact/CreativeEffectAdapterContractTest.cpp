#include <gtest/gtest.h>

#include <array>
#include <cstddef>

#include <QString>
#include <QVariant>

import Artifact.Effect.Abstract;
import Artifact.Effect.Creative;
import Graphics.Effect.Creative.OldTV;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Video.VideoFrame;
import Channel;
import Utils.String.UniString;

TEST(CreativeEffectAdapterContractTest,
     CoreMirrorMapsRgbaChannelsAndForwardsParameters)
{
    constexpr int width = 4;
    constexpr int height = 1;
    constexpr std::array<float, width * height * 4> pixels{
        0.1f, 0.9f, 0.2f, 0.0f,
        0.3f, 0.7f, 0.4f, 0.2f,
        0.6f, 0.4f, 0.6f, 0.4f,
        0.9f, 0.1f, 0.8f, 0.6f};
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ArtifactCore::ImageF32x4RGBAWithCache source(image);
    ArtifactCore::ImageF32x4RGBAWithCache output;
    Artifact::ArtifactCoreCreativeEffect effect(
        "Mirror", "builtin.test_mirror", "Mirror Contract");
    effect.setPropertyValue(ArtifactCore::UniString(QStringLiteral("Angle")),
                            QVariant(0.0));
    effect.setPropertyValue(ArtifactCore::UniString(QStringLiteral("CenterX")),
                            QVariant(0.5));
    effect.setPropertyValue(ArtifactCore::UniString(QStringLiteral("CenterY")),
                            QVariant(0.5));

    effect.applyCPUOnly(source, output);

    ASSERT_EQ(output.image().width(), width);
    ASSERT_EQ(output.image().height(), height);
    const float* actual = output.image().rgba32fData();
    const float* original = source.image().rgba32fData();
    ASSERT_NE(actual, nullptr);
    ASSERT_NE(original, nullptr);
    constexpr std::array<int, width> expectedSources{0, 1, 2, 1};
    for (int x = 0; x < width; ++x) {
        const std::size_t outputOffset = static_cast<std::size_t>(x) * 4;
        const std::size_t sourceOffset =
            static_cast<std::size_t>(expectedSources[static_cast<std::size_t>(x)]) * 4;
        for (std::size_t channel = 0; channel < 3; ++channel) {
            EXPECT_FLOAT_EQ(actual[outputOffset + channel],
                            pixels[sourceOffset + channel])
                << "x=" << x << " channel=" << channel;
        }
        EXPECT_FLOAT_EQ(actual[outputOffset + 3], pixels[outputOffset + 3])
            << "alpha x=" << x;
    }
    for (std::size_t channel = 0; channel < pixels.size(); ++channel) {
        EXPECT_FLOAT_EQ(original[channel], pixels[channel])
            << "source channel=" << channel;
    }
}

TEST(CreativeEffectAdapterContractTest,
     CoreOldTvAdapterMatchesTheDirectCoreEffect)
{
    constexpr int width = 5;
    constexpr int height = 4;
    constexpr int pixelCount = width * height;
    std::array<float, pixelCount * 4> pixels{};
    ArtifactCore::VideoFrame directFrame(width, height);
    auto red = directFrame.getChannel(ArtifactCore::ChannelType::Red);
    auto green = directFrame.getChannel(ArtifactCore::ChannelType::Green);
    auto blue = directFrame.getChannel(ArtifactCore::ChannelType::Blue);
    auto alpha = directFrame.getChannel(ArtifactCore::ChannelType::Alpha);
    ASSERT_TRUE(red);
    ASSERT_TRUE(green);
    ASSERT_TRUE(blue);
    ASSERT_TRUE(alpha);
    for (int pixel = 0; pixel < pixelCount; ++pixel) {
        const float position = static_cast<float>(pixel) /
                               static_cast<float>(pixelCount - 1);
        pixels[static_cast<std::size_t>(pixel) * 4 + 0] =
            red->data()[pixel] = 0.1f + position * 0.7f;
        pixels[static_cast<std::size_t>(pixel) * 4 + 1] =
            green->data()[pixel] = 0.8f - position * 0.6f;
        pixels[static_cast<std::size_t>(pixel) * 4 + 2] =
            blue->data()[pixel] = 0.2f + position * 0.5f;
        pixels[static_cast<std::size_t>(pixel) * 4 + 3] =
            alpha->data()[pixel] = 0.15f + position * 0.75f;
    }

    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    const ArtifactCore::ImageF32x4RGBAWithCache source(image);
    ArtifactCore::ImageF32x4RGBAWithCache output;
    ArtifactCore::OldTVEffect directEffect;
    directEffect.setParameter("Scanline", 0.25f);
    directEffect.process(directFrame, ArtifactCore::CreativeEffectContext{});

    Artifact::ArtifactCoreCreativeEffect adapter(
        "OldTV", "builtin.test_old_tv", "Old TV Adapter Contract");
    adapter.setPropertyValue(ArtifactCore::UniString(QStringLiteral("Scanline")),
                             QVariant(0.25));
    adapter.applyCPUOnly(source, output);

    ASSERT_EQ(output.image().width(), width);
    ASSERT_EQ(output.image().height(), height);
    const float* actual = output.image().rgba32fData();
    const auto expectedRed = directFrame.getChannel(ArtifactCore::ChannelType::Red);
    const auto expectedGreen = directFrame.getChannel(ArtifactCore::ChannelType::Green);
    const auto expectedBlue = directFrame.getChannel(ArtifactCore::ChannelType::Blue);
    const auto expectedAlpha = directFrame.getChannel(ArtifactCore::ChannelType::Alpha);
    ASSERT_NE(actual, nullptr);
    ASSERT_TRUE(expectedRed);
    ASSERT_TRUE(expectedGreen);
    ASSERT_TRUE(expectedBlue);
    ASSERT_TRUE(expectedAlpha);
    for (int pixel = 0; pixel < pixelCount; ++pixel) {
        const std::size_t offset = static_cast<std::size_t>(pixel) * 4;
        EXPECT_FLOAT_EQ(actual[offset + 0], expectedRed->data()[pixel]);
        EXPECT_FLOAT_EQ(actual[offset + 1], expectedGreen->data()[pixel]);
        EXPECT_FLOAT_EQ(actual[offset + 2], expectedBlue->data()[pixel]);
        EXPECT_FLOAT_EQ(actual[offset + 3], expectedAlpha->data()[pixel]);
    }
}
