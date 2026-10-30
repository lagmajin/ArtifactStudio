#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <limits>
#include <vector>
#include <QString>

#include <QVariant>

import Artifact.Effect.Abstract;
import Artifact.Effect.Rasterizer.VectorFlowGlitch;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Utils.String.UniString;

namespace {

const ArtifactCore::AbstractProperty* findProperty(
    const std::vector<ArtifactCore::AbstractProperty>& properties,
    const QString& name)
{
    for (const auto& property : properties) {
        if (property.getName() == name) return &property;
    }
    return nullptr;
}

} // namespace

TEST(VectorFlowGlitchEffectContractTest, ExposesAndClampsNamedControls)
{
    Artifact::VectorFlowGlitchEffect effect;
    effect.setPropertyValue(ArtifactCore::UniString("Glitch Amount"), QVariant(999.0));
    effect.setPropertyValue(ArtifactCore::UniString("Frequency"), QVariant(-1.0));
    effect.setPropertyValue(ArtifactCore::UniString("Chromatic Aberration"), QVariant(100.0));
    effect.setPropertyValue(ArtifactCore::UniString("Edge Flow Influence"), QVariant(-1.0));
    effect.setPropertyValue(ArtifactCore::UniString("Evolution"),
                           QVariant(std::numeric_limits<double>::infinity()));

    const auto properties = effect.getProperties();
    ASSERT_EQ(properties.size(), 5u);
    const auto* amount = findProperty(properties, QStringLiteral("Glitch Amount"));
    const auto* frequency = findProperty(properties, QStringLiteral("Frequency"));
    const auto* chroma = findProperty(properties, QStringLiteral("Chromatic Aberration"));
    const auto* flow = findProperty(properties, QStringLiteral("Edge Flow Influence"));
    const auto* evolution = findProperty(properties, QStringLiteral("Evolution"));
    ASSERT_NE(amount, nullptr);
    ASSERT_NE(frequency, nullptr);
    ASSERT_NE(chroma, nullptr);
    ASSERT_NE(flow, nullptr);
    ASSERT_NE(evolution, nullptr);
    EXPECT_FLOAT_EQ(amount->getValue().toFloat(), 200.0f);
    EXPECT_FLOAT_EQ(frequency->getValue().toFloat(), 0.001f);
    EXPECT_FLOAT_EQ(chroma->getValue().toFloat(), 50.0f);
    EXPECT_FLOAT_EQ(flow->getValue().toFloat(), 0.0f);
    EXPECT_FLOAT_EQ(evolution->getValue().toFloat(), 0.0f);
}

TEST(VectorFlowGlitchEffectContractTest, ZeroNoiseBandPreservesInputPixels)
{
    constexpr std::array<float, 12> pixels{
        0.1f, 0.2f, 0.3f, 0.4f,
        0.5f, 0.6f, 0.7f, 0.8f,
        0.9f, 0.8f, 0.7f, 0.6f,
    };
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), 3, 1);
    const ArtifactCore::ImageF32x4RGBAWithCache source(image);
    ArtifactCore::ImageF32x4RGBAWithCache output;
    Artifact::VectorFlowGlitchEffect effect;

    effect.applyCPUOnly(source, output);

    const float* result = output.image().rgba32fData();
    ASSERT_NE(result, nullptr);
    for (std::size_t channel = 0; channel < pixels.size(); ++channel) {
        EXPECT_FLOAT_EQ(result[channel], pixels[channel]);
    }
}

TEST(VectorFlowGlitchEffectContractTest,
     MaximumDisplacementOnSinglePixelSurfacePreservesAllChannels)
{
    constexpr std::array<float, 4> pixels{0.15f, 0.4f, 0.85f, 0.3f};
    ArtifactCore::ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), 1, 1);
    const ArtifactCore::ImageF32x4RGBAWithCache source(image);
    ArtifactCore::ImageF32x4RGBAWithCache output;
    Artifact::VectorFlowGlitchEffect effect;
    effect.setPropertyValue(ArtifactCore::UniString("Glitch Amount"),
                            QVariant(200.0));
    effect.setPropertyValue(ArtifactCore::UniString("Frequency"),
                            QVariant(1.0));
    effect.setPropertyValue(ArtifactCore::UniString("Chromatic Aberration"),
                            QVariant(50.0));
    effect.setPropertyValue(ArtifactCore::UniString("Edge Flow Influence"),
                            QVariant(1.0));
    effect.setPropertyValue(ArtifactCore::UniString("Evolution"),
                            QVariant(0.25));

    effect.applyCPUOnly(source, output);

    ASSERT_EQ(output.image().width(), 1);
    ASSERT_EQ(output.image().height(), 1);
    const float* actual = output.image().rgba32fData();
    ASSERT_NE(actual, nullptr);
    for (std::size_t channel = 0; channel < pixels.size(); ++channel) {
        EXPECT_FLOAT_EQ(actual[channel], pixels[channel])
            << "channel=" << channel;
    }
    const float* original = source.image().rgba32fData();
    ASSERT_NE(original, nullptr);
    for (std::size_t channel = 0; channel < pixels.size(); ++channel) {
        EXPECT_FLOAT_EQ(original[channel], pixels[channel])
            << "source channel=" << channel;
    }
}
