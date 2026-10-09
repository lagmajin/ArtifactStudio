#include <gtest/gtest.h>

#include <cstdint>
#include <initializer_list>
#include <utility>
#include <vector>

#include <QImage>
#include <opencv2/core.hpp>

import FloatRGBA;
import Graphics.SurfaceColorContract;
import Image.ImageF32x4_RGBA;

using namespace ArtifactCore;

namespace {

ImageF32x4_RGBA makeImage(int width, int height, const std::vector<float>& rgba)
{
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(rgba.data(), width, height);
    return image;
}

void expectPixelNear(const ImageF32x4_RGBA& image, int x, int y,
                     const FloatRGBA& expected, float tolerance = 1e-6f)
{
    const FloatRGBA actual = image.getPixel(x, y);
    EXPECT_NEAR(actual.r(), expected.r(), tolerance);
    EXPECT_NEAR(actual.g(), expected.g(), tolerance);
    EXPECT_NEAR(actual.b(), expected.b(), tolerance);
    EXPECT_NEAR(actual.a(), expected.a(), tolerance);
}

} // namespace

TEST(RenderImageContractTest, WeightedBlendMatchesExpectedRgbaAndKeepsInputs)
{
    const ImageF32x4_RGBA base = makeImage(
        1, 1, {0.20f, 0.40f, 0.60f, 0.20f});
    const ImageF32x4_RGBA overlay = makeImage(
        1, 1, {0.80f, 0.60f, 0.40f, 0.80f});

    const ImageF32x4_RGBA result = base.blend(overlay, 0.25f);

    ASSERT_EQ(result.width(), 1);
    ASSERT_EQ(result.height(), 1);
    expectPixelNear(result, 0, 0, FloatRGBA(0.35f, 0.45f, 0.55f, 0.35f));
    expectPixelNear(base, 0, 0, FloatRGBA(0.20f, 0.40f, 0.60f, 0.20f));
    expectPixelNear(overlay, 0, 0, FloatRGBA(0.80f, 0.60f, 0.40f, 0.80f));
}

TEST(RenderImageContractTest, AlphaBlendUsesForegroundAlphaAndOpacity)
{
    ImageF32x4_RGBA base = makeImage(
        1, 1, {0.20f, 0.40f, 0.60f, 0.20f});
    const ImageF32x4_RGBA overlay = makeImage(
        1, 1, {0.80f, 0.60f, 0.40f, 0.80f});

    base.alphaBlend(overlay, 0.5f);

    expectPixelNear(base, 0, 0, FloatRGBA(0.44f, 0.48f, 0.52f, 0.52f));
    expectPixelNear(overlay, 0, 0, FloatRGBA(0.80f, 0.60f, 0.40f, 0.80f));
}

TEST(RenderImageContractTest, AlphaBlendTransparentAndOpaquePixelsKeepExpectedEndpoints)
{
    ImageF32x4_RGBA base = makeImage(2, 1, {
        0.20f, 0.30f, 0.40f, 0.60f,
        0.10f, 0.20f, 0.30f, 0.20f,
    });
    const ImageF32x4_RGBA overlay = makeImage(2, 1, {
        0.90f, 0.80f, 0.70f, 0.0f,
        0.80f, 0.60f, 0.40f, 1.0f,
    });

    base.alphaBlend(overlay, 1.0f);

    expectPixelNear(base, 0, 0, FloatRGBA(0.20f, 0.30f, 0.40f, 0.60f));
    expectPixelNear(base, 1, 0, FloatRGBA(0.80f, 0.60f, 0.40f, 1.0f));
}

TEST(RenderImageContractTest, AlphaBlendDimensionMismatchLeavesBaseUnchanged)
{
    ImageF32x4_RGBA base = makeImage(
        1, 1, {0.15f, 0.25f, 0.35f, 0.45f});
    const ImageF32x4_RGBA overlay = makeImage(
        2, 1, {0.8f, 0.7f, 0.6f, 0.5f,
               0.4f, 0.3f, 0.2f, 0.1f});

    base.alphaBlend(overlay, 0.75f);

    EXPECT_EQ(base.width(), 1);
    EXPECT_EQ(base.height(), 1);
    expectPixelNear(base, 0, 0, FloatRGBA(0.15f, 0.25f, 0.35f, 0.45f));
}

TEST(RenderImageContractTest, WeightedBlendZeroAndOneWeightsMatchEndpoints)
{
    const ImageF32x4_RGBA base = makeImage(
        1, 1, {0.15f, 0.25f, 0.35f, 0.45f});
    const ImageF32x4_RGBA overlay = makeImage(
        1, 1, {0.85f, 0.75f, 0.65f, 0.55f});

    const ImageF32x4_RGBA zero = base.blend(overlay, 0.0f);
    const ImageF32x4_RGBA one = base.blend(overlay, 1.0f);

    expectPixelNear(zero, 0, 0, FloatRGBA(0.15f, 0.25f, 0.35f, 0.45f));
    expectPixelNear(one, 0, 0, FloatRGBA(0.85f, 0.75f, 0.65f, 0.55f));
}

TEST(RenderImageContractTest, WeightedBlendMatchesPerChannelFormulaAcrossImageShapes)
{
    for (int height = 1; height <= 5; ++height) {
        for (int width = 1; width <= 7; ++width) {
            std::vector<float> basePixels;
            std::vector<float> overlayPixels;
            basePixels.reserve(static_cast<size_t>(width * height * 4));
            overlayPixels.reserve(static_cast<size_t>(width * height * 4));
            for (int pixel = 0; pixel < width * height; ++pixel) {
                for (int channel = 0; channel < 4; ++channel) {
                    const int baseCode = (pixel * 37 + channel * 53 + 11) % 101;
                    const int overlayCode = (pixel * 71 + channel * 29 + 7) % 101;
                    basePixels.push_back(static_cast<float>(baseCode) / 100.0f);
                    overlayPixels.push_back(static_cast<float>(overlayCode) / 100.0f);
                }
            }

            const auto base = makeImage(width, height, basePixels);
            const auto overlay = makeImage(width, height, overlayPixels);
            for (const float weight : {0.0f, 0.125f, 0.5f, 0.875f, 1.0f}) {
                const auto result = base.blend(overlay, weight);
                ASSERT_EQ(result.width(), width);
                ASSERT_EQ(result.height(), height);
                for (int y = 0; y < height; ++y) {
                    for (int x = 0; x < width; ++x) {
                        const auto source = base.getPixel(x, y);
                        const auto foreground = overlay.getPixel(x, y);
                        const auto actual = result.getPixel(x, y);
                        const float expected[] = {
                            source.r() * (1.0f - weight) + foreground.r() * weight,
                            source.g() * (1.0f - weight) + foreground.g() * weight,
                            source.b() * (1.0f - weight) + foreground.b() * weight,
                            source.a() * (1.0f - weight) + foreground.a() * weight,
                        };
                        EXPECT_NEAR(actual.r(), expected[0], 2e-6f)
                            << "size=" << width << 'x' << height
                            << " pixel=" << x << ',' << y << " weight=" << weight;
                        EXPECT_NEAR(actual.g(), expected[1], 2e-6f)
                            << "size=" << width << 'x' << height
                            << " pixel=" << x << ',' << y << " weight=" << weight;
                        EXPECT_NEAR(actual.b(), expected[2], 2e-6f)
                            << "size=" << width << 'x' << height
                            << " pixel=" << x << ',' << y << " weight=" << weight;
                        EXPECT_NEAR(actual.a(), expected[3], 2e-6f)
                            << "size=" << width << 'x' << height
                            << " pixel=" << x << ',' << y << " weight=" << weight;
                    }
                }
            }
        }
    }
}

TEST(RenderImageContractTest, AlphaBlendMatchesStraightAlphaFormulaAcrossOpacityGrid)
{
    const float opacities[] = {0.0f, 0.125f, 0.5f, 0.875f, 1.0f};
    for (int height = 1; height <= 4; ++height) {
        for (int width = 1; width <= 6; ++width) {
            std::vector<float> basePixels;
            std::vector<float> overlayPixels;
            basePixels.reserve(static_cast<size_t>(width * height * 4));
            overlayPixels.reserve(static_cast<size_t>(width * height * 4));
            for (int pixel = 0; pixel < width * height; ++pixel) {
                for (int channel = 0; channel < 4; ++channel) {
                    const int baseCode = (pixel * 31 + channel * 43 + 13) % 101;
                    const int overlayCode = (pixel * 67 + channel * 19 + 3) % 101;
                    basePixels.push_back(static_cast<float>(baseCode) / 100.0f);
                    overlayPixels.push_back(static_cast<float>(overlayCode) / 100.0f);
                }
            }

            const auto base = makeImage(width, height, basePixels);
            const auto overlay = makeImage(width, height, overlayPixels);
            for (const float opacity : opacities) {
                auto result = base;
                result.alphaBlend(overlay, opacity);
                for (int y = 0; y < height; ++y) {
                    for (int x = 0; x < width; ++x) {
                        const auto background = base.getPixel(x, y);
                        const auto foreground = overlay.getPixel(x, y);
                        const float effectiveAlpha = foreground.a() * opacity;
                        const float expectedAlpha = background.a() +
                            effectiveAlpha * (1.0f - background.a());
                        const FloatRGBA expected(
                            foreground.r() * effectiveAlpha +
                                background.r() * (1.0f - effectiveAlpha),
                            foreground.g() * effectiveAlpha +
                                background.g() * (1.0f - effectiveAlpha),
                            foreground.b() * effectiveAlpha +
                                background.b() * (1.0f - effectiveAlpha),
                            expectedAlpha);
                        const auto actual = result.getPixel(x, y);
                        EXPECT_NEAR(actual.r(), expected.r(), 2e-6f)
                            << "size=" << width << 'x' << height
                            << " pixel=" << x << ',' << y << " opacity=" << opacity;
                        EXPECT_NEAR(actual.g(), expected.g(), 2e-6f)
                            << "size=" << width << 'x' << height
                            << " pixel=" << x << ',' << y << " opacity=" << opacity;
                        EXPECT_NEAR(actual.b(), expected.b(), 2e-6f)
                            << "size=" << width << 'x' << height
                            << " pixel=" << x << ',' << y << " opacity=" << opacity;
                        EXPECT_NEAR(actual.a(), expected.a(), 2e-6f)
                            << "size=" << width << 'x' << height
                            << " pixel=" << x << ',' << y << " opacity=" << opacity;
                    }
                }
            }
        }
    }
}

TEST(RenderImageContractTest, BlendMarksIncompatibleColorDescriptorsUnknown)
{
    ImageF32x4_RGBA base = makeImage(
        1, 1, {0.2f, 0.3f, 0.4f, 0.5f});
    ImageF32x4_RGBA overlay = makeImage(
        1, 1, {0.7f, 0.6f, 0.5f, 0.4f});
    base.setColorDescriptor(SurfaceColorDescriptor::canonicalLinearPremultiplied());
    overlay.setColorDescriptor(SurfaceColorDescriptor::linearStraightRgba32Float());

    const ImageF32x4_RGBA result = base.blend(overlay, 0.5f);

    EXPECT_EQ(result.colorDescriptor(), SurfaceColorDescriptor::unknown());
}

TEST(RenderImageContractTest, CropPreservesExpectedPixelCoordinatesAndAlpha)
{
    const ImageF32x4_RGBA source = makeImage(3, 2, {
        0.0f, 0.0f, 0.0f, 0.1f,  1.0f, 0.0f, 1.0f, 0.2f,  2.0f, 0.0f, 2.0f, 0.3f,
        0.0f, 1.0f, 1.0f, 0.4f,  1.0f, 1.0f, 2.0f, 0.5f,  2.0f, 1.0f, 3.0f, 0.6f,
    });

    const ImageF32x4_RGBA crop = source.crop(1, 0, 2, 2);

    ASSERT_EQ(crop.width(), 2);
    ASSERT_EQ(crop.height(), 2);
    expectPixelNear(crop, 0, 0, FloatRGBA(1.0f, 0.0f, 1.0f, 0.2f));
    expectPixelNear(crop, 1, 0, FloatRGBA(2.0f, 0.0f, 2.0f, 0.3f));
    expectPixelNear(crop, 0, 1, FloatRGBA(1.0f, 1.0f, 2.0f, 0.5f));
    expectPixelNear(crop, 1, 1, FloatRGBA(2.0f, 1.0f, 3.0f, 0.6f));
    expectPixelNear(source, 1, 1, FloatRGBA(1.0f, 1.0f, 2.0f, 0.5f));
}

TEST(RenderImageContractTest, FullBoundsAndLastPixelCropsKeepExactDimensions)
{
    const ImageF32x4_RGBA source = makeImage(2, 2, {
        0.1f, 0.2f, 0.3f, 0.4f,  0.5f, 0.6f, 0.7f, 0.8f,
        0.9f, 0.8f, 0.7f, 0.6f,  0.5f, 0.4f, 0.3f, 0.2f,
    });

    const ImageF32x4_RGBA full = source.crop(0, 0, 2, 2);
    const ImageF32x4_RGBA lastPixel = source.crop(1, 1, 1, 1);

    ASSERT_EQ(full.width(), 2);
    ASSERT_EQ(full.height(), 2);
    expectPixelNear(full, 0, 0, FloatRGBA(0.1f, 0.2f, 0.3f, 0.4f));
    expectPixelNear(full, 1, 1, FloatRGBA(0.5f, 0.4f, 0.3f, 0.2f));
    ASSERT_EQ(lastPixel.width(), 1);
    ASSERT_EQ(lastPixel.height(), 1);
    expectPixelNear(lastPixel, 0, 0, FloatRGBA(0.5f, 0.4f, 0.3f, 0.2f));
}

TEST(RenderImageContractTest, CropPreservesCompleteColorDescriptor)
{
    const SurfaceColorDescriptor descriptor =
        SurfaceColorDescriptor::canonicalLinearPremultiplied();
    ImageF32x4_RGBA source = makeImage(
        2, 1, {0.2f, 0.3f, 0.4f, 0.5f,
               0.6f, 0.7f, 0.8f, 0.9f});
    source.setColorDescriptor(descriptor);

    const ImageF32x4_RGBA crop = source.crop(1, 0, 1, 1);

    EXPECT_EQ(crop.colorDescriptor(), descriptor);
    expectPixelNear(crop, 0, 0, FloatRGBA(0.6f, 0.7f, 0.8f, 0.9f));
}

TEST(RenderImageContractTest, ZeroWidthCropProducesEmptyImage)
{
    const ImageF32x4_RGBA source = makeImage(
        2, 2, {0.0f, 0.0f, 0.0f, 1.0f,
               1.0f, 0.0f, 0.0f, 1.0f,
               0.0f, 1.0f, 0.0f, 1.0f,
               1.0f, 1.0f, 0.0f, 1.0f});

    const ImageF32x4_RGBA crop = source.crop(1, 0, 0, 2);

    EXPECT_TRUE(crop.isEmpty());
    EXPECT_EQ(crop.width(), 0);
    EXPECT_EQ(crop.height(), 0);
}

TEST(RenderImageContractTest, CropRejectsNegativeOriginsAndRangesPastImageEdges)
{
    const ImageF32x4_RGBA source = makeImage(
        2, 2, std::vector<float>(16, 0.5f));

    for (const auto& bounds : {
             std::array<int, 4>{-1, 0, 1, 1},
             std::array<int, 4>{0, -1, 1, 1},
             std::array<int, 4>{1, 0, 2, 1},
             std::array<int, 4>{0, 1, 1, 2},
             std::array<int, 4>{0, 0, -1, 1}}) {
        const ImageF32x4_RGBA crop =
            source.crop(bounds[0], bounds[1], bounds[2], bounds[3]);
        EXPECT_TRUE(crop.isEmpty()) << "bounds=" << bounds[0] << ','
                                    << bounds[1] << ' ' << bounds[2] << 'x'
                                    << bounds[3];
    }
}

TEST(RenderImageContractTest, CopyConstructionAndAssignmentOwnIndependentPixels)
{
    const auto descriptor = SurfaceColorDescriptor::canonicalLinearPremultiplied();
    ImageF32x4_RGBA source = makeImage(
        1, 1, {0.2f, 0.4f, 0.6f, 0.8f});
    source.setColorDescriptor(descriptor);

    ImageF32x4_RGBA copyConstructed(source);
    ImageF32x4_RGBA copyAssigned;
    copyAssigned = source;
    copyConstructed.setPixel(0, 0, FloatRGBA(0.9f, 0.8f, 0.7f, 0.6f));
    copyAssigned.setPixel(0, 0, FloatRGBA(0.1f, 0.2f, 0.3f, 0.4f));

    expectPixelNear(source, 0, 0, FloatRGBA(0.2f, 0.4f, 0.6f, 0.8f));
    expectPixelNear(copyConstructed, 0, 0, FloatRGBA(0.9f, 0.8f, 0.7f, 0.6f));
    expectPixelNear(copyAssigned, 0, 0, FloatRGBA(0.1f, 0.2f, 0.3f, 0.4f));
    EXPECT_EQ(source.colorDescriptor(), descriptor);
    EXPECT_EQ(copyConstructed.colorDescriptor(), descriptor);
    EXPECT_EQ(copyAssigned.colorDescriptor(), descriptor);
}

TEST(RenderImageContractTest, MoveConstructionAndAssignmentPreserveImageAndReuseSources)
{
    const auto descriptor = SurfaceColorDescriptor::linearStraightRgba32Float();
    ImageF32x4_RGBA source = makeImage(
        1, 1, {0.15f, 0.35f, 0.55f, 0.75f});
    source.setColorDescriptor(descriptor);

    ImageF32x4_RGBA moved(std::move(source));
    EXPECT_EQ(moved.width(), 1);
    EXPECT_EQ(moved.height(), 1);
    EXPECT_EQ(moved.colorDescriptor(), descriptor);
    expectPixelNear(moved, 0, 0, FloatRGBA(0.15f, 0.35f, 0.55f, 0.75f));

    source.fill(FloatRGBA(0.8f, 0.6f, 0.4f, 0.2f));
    expectPixelNear(source, 0, 0, FloatRGBA(0.8f, 0.6f, 0.4f, 0.2f));

    ImageF32x4_RGBA assigned = makeImage(1, 1, {1.0f, 1.0f, 1.0f, 1.0f});
    assigned = std::move(moved);
    EXPECT_EQ(assigned.colorDescriptor(), descriptor);
    expectPixelNear(assigned, 0, 0, FloatRGBA(0.15f, 0.35f, 0.55f, 0.75f));
    moved.fill(FloatRGBA(0.1f, 0.2f, 0.3f, 0.4f));
    expectPixelNear(moved, 0, 0, FloatRGBA(0.1f, 0.2f, 0.3f, 0.4f));
}

TEST(RenderImageContractTest, DeepCopyOwnsIndependentPixelsAndPreservesDescriptor)
{
    const auto descriptor = SurfaceColorDescriptor::canonicalLinearPremultiplied();
    ImageF32x4_RGBA source = makeImage(
        2, 1, {0.2f, 0.4f, 0.6f, 0.8f,
               0.1f, 0.3f, 0.5f, 0.7f});
    source.setColorDescriptor(descriptor);

    ImageF32x4_RGBA copy = source.DeepCopy();
    copy.setPixel(0, 0, FloatRGBA(0.9f, 0.8f, 0.7f, 0.6f));

    EXPECT_EQ(copy.width(), 2);
    EXPECT_EQ(copy.height(), 1);
    EXPECT_EQ(copy.colorDescriptor(), descriptor);
    EXPECT_EQ(source.colorDescriptor(), descriptor);
    expectPixelNear(source, 0, 0, FloatRGBA(0.2f, 0.4f, 0.6f, 0.8f));
    expectPixelNear(copy, 0, 0, FloatRGBA(0.9f, 0.8f, 0.7f, 0.6f));
    expectPixelNear(copy, 1, 0, FloatRGBA(0.1f, 0.3f, 0.5f, 0.7f));
}

TEST(RenderImageContractTest, FlipOperationsReverseOnlyTheirAxisAndKeepAlpha)
{
    const auto descriptor = SurfaceColorDescriptor::canonicalLinearPremultiplied();
    ImageF32x4_RGBA source = makeImage(2, 2, {
        0.1f, 0.0f, 0.0f, 0.2f,  0.2f, 0.0f, 0.0f, 0.3f,
        0.3f, 0.0f, 0.0f, 0.4f,  0.4f, 0.0f, 0.0f, 0.5f,
    });
    source.setColorDescriptor(descriptor);
    ImageF32x4_RGBA horizontal = source;
    ImageF32x4_RGBA vertical = source;

    horizontal.flipHorizontal();
    vertical.flipVertical();

    expectPixelNear(horizontal, 0, 0, FloatRGBA(0.2f, 0.0f, 0.0f, 0.3f));
    expectPixelNear(horizontal, 1, 1, FloatRGBA(0.3f, 0.0f, 0.0f, 0.4f));
    expectPixelNear(vertical, 0, 0, FloatRGBA(0.3f, 0.0f, 0.0f, 0.4f));
    expectPixelNear(vertical, 1, 1, FloatRGBA(0.2f, 0.0f, 0.0f, 0.3f));
    EXPECT_EQ(horizontal.colorDescriptor(), descriptor);
    EXPECT_EQ(vertical.colorDescriptor(), descriptor);
    expectPixelNear(source, 0, 0, FloatRGBA(0.1f, 0.0f, 0.0f, 0.2f));
}

TEST(RenderImageContractTest, CreateMaskLikeFillsSourceDimensionsWithRequestedRgba)
{
    ImageF32x4_RGBA source = makeImage(3, 2, std::vector<float>(24, 0.25f));
    const FloatRGBA fill(0.0f, 0.5f, 1.0f, 0.75f);

    const ImageF32x4_RGBA mask = source.createMaskLike(source, fill);

    ASSERT_EQ(mask.width(), 3);
    ASSERT_EQ(mask.height(), 2);
    for (int y = 0; y < mask.height(); ++y) {
        for (int x = 0; x < mask.width(); ++x) {
            expectPixelNear(mask, x, y, fill);
        }
    }
    expectPixelNear(source, 0, 0, FloatRGBA(0.25f, 0.25f, 0.25f, 0.25f));
}

TEST(RenderImageContractTest, Rgba8InputExpandsChannelsAndAppliesDescriptor)
{
    const std::uint8_t bytes[] = {255, 128, 0, 64, 17, 34, 51, 68};
    ImageF32x4_RGBA image;
    image.setFromRGBA8(bytes, 2, 1);

    expectPixelNear(image, 0, 0,
                    FloatRGBA(1.0f, 128.0f / 255.0f, 0.0f, 64.0f / 255.0f));
    expectPixelNear(image, 1, 0,
                    FloatRGBA(17.0f / 255.0f, 34.0f / 255.0f,
                              51.0f / 255.0f, 68.0f / 255.0f));
    EXPECT_EQ(image.colorDescriptor(), SurfaceColorDescriptor::unknownRgba32Float());

    const auto descriptor = SurfaceColorDescriptor::canonicalLinearPremultiplied();
    image.setFromRGBA8(bytes, 2, 1, descriptor);
    EXPECT_EQ(image.colorDescriptor(), descriptor);
    expectPixelNear(image, 0, 0,
                    FloatRGBA(1.0f, 128.0f / 255.0f, 0.0f, 64.0f / 255.0f));
}

TEST(RenderImageContractTest, InvalidRgba8InputClearsPixelsAndDescriptor)
{
    const std::uint8_t bytes[] = {255, 128, 0, 64};
    ImageF32x4_RGBA image;
    image.setFromRGBA8(bytes, 1, 1,
                       SurfaceColorDescriptor::canonicalLinearPremultiplied());
    ASSERT_FALSE(image.isEmpty());

    image.setFromRGBA8(nullptr, 1, 1);

    EXPECT_TRUE(image.isEmpty());
    EXPECT_EQ(image.width(), 0);
    EXPECT_EQ(image.height(), 0);
    EXPECT_EQ(image.colorDescriptor(), SurfaceColorDescriptor::unknown());
}

TEST(RenderImageContractTest, CvMatInputClonesFloatPixelsAndKeepsDescriptor)
{
    cv::Mat input(1, 2, CV_32FC4);
    input.at<cv::Vec4f>(0, 0) = cv::Vec4f(0.1f, 0.2f, 0.3f, 0.4f);
    input.at<cv::Vec4f>(0, 1) = cv::Vec4f(0.5f, 0.6f, 0.7f, 0.8f);
    const auto descriptor = SurfaceColorDescriptor::canonicalLinearPremultiplied();
    ImageF32x4_RGBA image;
    image.setFromCVMat(input, descriptor);

    input.at<cv::Vec4f>(0, 0) = cv::Vec4f(0.9f, 0.8f, 0.7f, 0.6f);

    EXPECT_EQ(image.width(), 2);
    EXPECT_EQ(image.height(), 1);
    EXPECT_EQ(image.colorDescriptor(), descriptor);
    expectPixelNear(image, 0, 0, FloatRGBA(0.1f, 0.2f, 0.3f, 0.4f));
    expectPixelNear(image, 1, 0, FloatRGBA(0.5f, 0.6f, 0.7f, 0.8f));
}

TEST(RenderImageContractTest, ToQImageEncodesSrgbAndZerosTransparentRgb)
{
    ImageF32x4_RGBA source = makeImage(
        2, 1, {0.5f, 0.25f, 0.0f, 0.5f,
               0.8f, 0.4f, 0.2f, 0.0f});
    source.setColorDescriptor(SurfaceColorDescriptor::linearStraightRgba32Float());

    const QImage converted = source.toQImage();

    ASSERT_FALSE(converted.isNull());
    EXPECT_EQ(converted.format(), QImage::Format_RGBA8888);
    const QRgb visible = converted.pixel(0, 0);
    EXPECT_EQ(qRed(visible), 188);
    EXPECT_EQ(qGreen(visible), 137);
    EXPECT_EQ(qBlue(visible), 0);
    EXPECT_EQ(qAlpha(visible), 128);
    const QRgb transparent = converted.pixel(1, 0);
    EXPECT_EQ(qRed(transparent), 0);
    EXPECT_EQ(qGreen(transparent), 0);
    EXPECT_EQ(qBlue(transparent), 0);
    EXPECT_EQ(qAlpha(transparent), 0);
}

TEST(RenderImageContractTest, CanonicalMatOutputsRespectBgraDescriptor)
{
    cv::Mat input(1, 1, CV_32FC4);
    input.at<cv::Vec4f>(0, 0) = cv::Vec4f(0.1f, 0.2f, 0.3f, 0.4f);
    ImageF32x4_RGBA image;
    image.setFromCVMat(input, SurfaceColorDescriptor::legacyOpenCvBgra32Float());

    const cv::Mat rgba = image.toCanonicalRGBA32FC4();
    const cv::Mat bgra = image.toCanonicalBGRA32FC4();
    ASSERT_EQ(rgba.type(), CV_32FC4);
    ASSERT_EQ(bgra.type(), CV_32FC4);
    const cv::Vec4f rgbaPixel = rgba.at<cv::Vec4f>(0, 0);
    const cv::Vec4f bgraPixel = bgra.at<cv::Vec4f>(0, 0);
    EXPECT_FLOAT_EQ(rgbaPixel[0], 0.3f);
    EXPECT_FLOAT_EQ(rgbaPixel[1], 0.2f);
    EXPECT_FLOAT_EQ(rgbaPixel[2], 0.1f);
    EXPECT_FLOAT_EQ(rgbaPixel[3], 0.4f);
    EXPECT_FLOAT_EQ(bgraPixel[0], 0.1f);
    EXPECT_FLOAT_EQ(bgraPixel[1], 0.2f);
    EXPECT_FLOAT_EQ(bgraPixel[2], 0.3f);
    EXPECT_FLOAT_EQ(bgraPixel[3], 0.4f);
}

TEST(RenderImageContractTest, FillAlphaReplacesOnlyAlphaAcrossImage)
{
    ImageF32x4_RGBA image = makeImage(2, 1, {
        0.1f, 0.2f, 0.3f, 0.4f,
        0.5f, 0.6f, 0.7f, 0.8f,
    });
    image.setColorDescriptor(SurfaceColorDescriptor::linearStraightRgba32Float());

    image.fillAlpha(0.25f);

    EXPECT_EQ(image.width(), 2);
    EXPECT_EQ(image.height(), 1);
    expectPixelNear(image, 0, 0, FloatRGBA(0.1f, 0.2f, 0.3f, 0.25f));
    expectPixelNear(image, 1, 0, FloatRGBA(0.5f, 0.6f, 0.7f, 0.25f));
    EXPECT_EQ(image.colorDescriptor(), SurfaceColorDescriptor::linearStraightRgba32Float());
}

TEST(RenderImageContractTest, FillSetsAllRgbaChannelsAndKeepsDimensions)
{
    ImageF32x4_RGBA image = makeImage(2, 2, std::vector<float>(16, 0.2f));
    const auto descriptor = SurfaceColorDescriptor::linearStraightRgba32Float();
    image.setColorDescriptor(descriptor);
    const FloatRGBA fill(0.8f, 0.6f, 0.4f, 0.2f);

    image.fill(fill);

    EXPECT_EQ(image.width(), 2);
    EXPECT_EQ(image.height(), 2);
    EXPECT_EQ(image.colorDescriptor(), descriptor);
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            expectPixelNear(image, x, y, fill);
        }
    }
}

TEST(RenderImageContractTest, ResizeKeepsConstantPixelsAndDescriptor)
{
    ImageF32x4_RGBA image = makeImage(
        1, 1, {0.2f, 0.4f, 0.6f, 0.8f});
    const auto descriptor = SurfaceColorDescriptor::linearStraightRgba32Float();
    image.setColorDescriptor(descriptor);

    image.resize(3, 2);

    EXPECT_EQ(image.width(), 3);
    EXPECT_EQ(image.height(), 2);
    EXPECT_EQ(image.colorDescriptor(), descriptor);
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            expectPixelNear(image, x, y, FloatRGBA(0.2f, 0.4f, 0.6f, 0.8f));
        }
    }
}

TEST(RenderImageContractTest, MismatchedBlendDimensionsReturnUnchangedBase)
{
    const ImageF32x4_RGBA base = makeImage(
        1, 1, {0.15f, 0.25f, 0.35f, 0.45f});
    const ImageF32x4_RGBA overlay = makeImage(
        2, 1, {0.8f, 0.7f, 0.6f, 0.5f,
               0.4f, 0.3f, 0.2f, 0.1f});

    const ImageF32x4_RGBA result = base.blend(overlay, 0.5f);

    ASSERT_EQ(result.width(), 1);
    ASSERT_EQ(result.height(), 1);
    expectPixelNear(result, 0, 0, FloatRGBA(0.15f, 0.25f, 0.35f, 0.45f));
}
