#include <gtest/gtest.h>

#include <vector>

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
