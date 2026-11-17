#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <utility>

#include <QString>
#include <QVariant>

import Artifact.Effect.Abstract;
import Artifact.Effect.Context;
import Artifact.Effect.Creative;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Utils.String.UniString;

using namespace Artifact;
using namespace ArtifactCore;

namespace {

constexpr int kWidth = 17;
constexpr int kHeight = 17;
constexpr std::size_t kPixelCount = static_cast<std::size_t>(kWidth * kHeight * 4);

constexpr bool isFilled(int x, int y)
{
    return x >= 6 && x <= 10 && y >= 6 && y <= 10;
}

ImageF32x4RGBAWithCache makeSource()
{
    std::array<float, kPixelCount> pixels{};
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const std::size_t offset =
                (static_cast<std::size_t>(y) * kWidth + x) * 4u;
            const float value = isFilled(x, y) ? 0.9f : 0.2f;
            pixels[offset] = value;
            pixels[offset + 1] = value;
            pixels[offset + 2] = value;
            pixels[offset + 3] = 1.0f;
        }
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), kWidth, kHeight);
    return ImageF32x4RGBAWithCache(image);
}

ImageF32x4RGBAWithCache makeMask()
{
    std::array<float, kPixelCount> pixels{};
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const std::size_t offset =
                (static_cast<std::size_t>(y) * kWidth + x) * 4u;
            pixels[offset] = 0.0f;
            pixels[offset + 1] = 0.0f;
            pixels[offset + 2] = 0.0f;
            pixels[offset + 3] = isFilled(x, y) ? 1.0f : 0.0f;
        }
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), kWidth, kHeight);
    return ImageF32x4RGBAWithCache(image);
}

class FixedMaskSampler final : public IEffectFrameSampler {
public:
    explicit FixedMaskSampler(ImageF32x4RGBAWithCache mask)
        : mask_(std::move(mask))
    {
    }

    bool sampleCurrentLayerFrame(std::int64_t, ImageF32x4RGBAWithCache&) override
    {
        return false;
    }

    bool sampleCurrentLayerFrameRelative(
        std::int64_t, ImageF32x4RGBAWithCache&) override
    {
        return false;
    }

    bool sampleNamedInput(const QString& inputName, std::int64_t frame,
                          ImageF32x4RGBAWithCache& out) override
    {
        if (inputName != QStringLiteral("inpaint_mask")) return false;
        requestedInput = inputName;
        requestedFrame = frame;
        out = mask_;
        return true;
    }

    QString requestedInput;
    std::int64_t requestedFrame = -1;

private:
    ImageF32x4RGBAWithCache mask_;
};

} // namespace

TEST(InpaintEffectContractTest, MissingMaskIsExactPassthrough)
{
    const auto source = makeSource();
    ArtifactInpaintEffect effect;
    effect.setComputeMode(ComputeMode::CPU);
    ImageF32x4RGBAWithCache output;

    effect.applyCPUOnly(source, output);

    const float* expected = source.image().rgba32fData();
    const float* actual = output.image().rgba32fData();
    ASSERT_NE(expected, nullptr);
    ASSERT_NE(actual, nullptr);
    for (std::size_t i = 0; i < kPixelCount; ++i) {
        EXPECT_FLOAT_EQ(actual[i], expected[i]) << "channel=" << i;
    }
}

TEST(InpaintEffectContractTest, PreservesMaskOutRgbAndAlpha)
{
    const auto source = makeSource();
    FixedMaskSampler sampler(makeMask());
    EffectContext context;
    context.sampler = &sampler;
    context.compositionFrame = 7;

    ArtifactInpaintEffect effect;
    effect.setComputeMode(ComputeMode::CPU);
    effect.setContext(context);
    ImageF32x4RGBAWithCache output;

    effect.applyCPUOnly(source, output);

    const float* input = source.image().rgba32fData();
    const float* actual = output.image().rgba32fData();
    ASSERT_NE(input, nullptr);
    ASSERT_NE(actual, nullptr);

    int changedInside = 0;
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const std::size_t offset =
                (static_cast<std::size_t>(y) * kWidth + x) * 4u;
            const bool inside = isFilled(x, y);
            for (int channel = 0; channel < 3; ++channel) {
                if (inside) {
                    if (std::fabs(actual[offset + channel] -
                                  input[offset + channel]) > 1.0e-6f) {
                        ++changedInside;
                    }
                } else {
                    EXPECT_NEAR(actual[offset + channel], input[offset + channel],
                                1.0e-6f)
                        << "x=" << x << " y=" << y << " channel=" << channel;
                }
            }
            EXPECT_FLOAT_EQ(actual[offset + 3], input[offset + 3])
                << "x=" << x << " y=" << y << " alpha";
        }
    }

    EXPECT_GT(changedInside, 0);
    EXPECT_EQ(sampler.requestedInput, QStringLiteral("inpaint_mask"));
    EXPECT_EQ(sampler.requestedFrame, 7);
}
