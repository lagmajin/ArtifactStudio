#include <gtest/gtest.h>
#include <cmath>
#include <initializer_list>
#include <vector>

import Image.ImageF32x4_RGBA;
import ImageProcessing.Distortion;

using namespace ArtifactCore;

namespace {

/// Build a deterministic RGBA test image. Each pixel encodes its own
/// coordinates so a warp can be checked by asking "where did this come from".
ImageF32x4_RGBA makeCoordinateImage(int width, int height) {
    std::vector<float> pixels(static_cast<size_t>(width) * height * 4u);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const size_t o = (static_cast<size_t>(y) * width + x) * 4u;
            pixels[o + 0] = static_cast<float>(x);
            pixels[o + 1] = static_cast<float>(y);
            pixels[o + 2] = 0.0f;
            pixels[o + 3] = 1.0f;
        }
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    return image;
}

ImageF32x4_RGBA makeFlatImage(int width, int height,
                               float r, float g, float b, float a) {
    std::vector<float> pixels(static_cast<size_t>(width) * height * 4u);
    for (size_t offset = 0; offset < pixels.size(); offset += 4u) {
        pixels[offset + 0u] = r;
        pixels[offset + 1u] = g;
        pixels[offset + 2u] = b;
        pixels[offset + 3u] = a;
    }
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels.data(), width, height);
    return image;
}

FloatRGBA pixelAt(const ImageF32x4_RGBA& image, int x, int y) {
    const float* data = image.rgba32fData();
    const size_t o = (static_cast<size_t>(y) * image.width() + x) * 4u;
    return FloatRGBA(data[o + 0], data[o + 1], data[o + 2], data[o + 3]);
}

} // namespace

TEST(ImageMorphTest, NoControlPointsCrossDissolveAllRGBAChannels) {
    const auto source = makeFlatImage(1, 1, 0.0f, 0.2f, 0.4f, 0.2f);
    const auto target = makeFlatImage(1, 1, 0.8f, 0.6f, 0.8f, 0.8f);
    ImageF32x4_RGBA output;

    morphImages(source, target, output, {}, 0.25f);

    const FloatRGBA result = pixelAt(output, 0, 0);
    EXPECT_NEAR(result.r(), 0.2f, 1e-6f);
    EXPECT_NEAR(result.g(), 0.3f, 1e-6f);
    EXPECT_NEAR(result.b(), 0.5f, 1e-6f);
    EXPECT_NEAR(result.a(), 0.35f, 1e-6f);
}

TEST(ImageMorphTest, AmountIsClampedToSourceAndTargetEndpoints) {
    const auto source = makeFlatImage(1, 1, 0.1f, 0.2f, 0.3f, 0.4f);
    const auto target = makeFlatImage(1, 1, 0.6f, 0.7f, 0.8f, 0.9f);
    ImageF32x4_RGBA output;

    morphImages(source, target, output, {}, -1.0f);
    EXPECT_NEAR(pixelAt(output, 0, 0).r(), 0.1f, 1e-6f);
    EXPECT_NEAR(pixelAt(output, 0, 0).a(), 0.4f, 1e-6f);

    morphImages(source, target, output, {}, 2.0f);
    EXPECT_NEAR(pixelAt(output, 0, 0).r(), 0.6f, 1e-6f);
    EXPECT_NEAR(pixelAt(output, 0, 0).a(), 0.9f, 1e-6f);
}

TEST(ImageMorphTest, OutputUsesOverlapOfMismatchedInputDimensions) {
    const auto source = makeFlatImage(3, 2, 0.2f, 0.4f, 0.6f, 0.8f);
    const auto target = makeFlatImage(2, 1, 0.6f, 0.2f, 0.4f, 0.4f);
    ImageF32x4_RGBA output;

    morphImages(source, target, output, {}, 0.5f);

    EXPECT_EQ(output.width(), 2);
    EXPECT_EQ(output.height(), 1);
    const FloatRGBA pixel = pixelAt(output, 1, 0);
    EXPECT_NEAR(pixel.r(), 0.4f, 1e-6f);
    EXPECT_NEAR(pixel.g(), 0.3f, 1e-6f);
    EXPECT_NEAR(pixel.b(), 0.5f, 1e-6f);
    EXPECT_NEAR(pixel.a(), 0.6f, 1e-6f);
}

TEST(ImageMorphTest, CorrespondingControlPointPullsSamplesFromBothImages) {
    const auto source = makeCoordinateImage(3, 1);
    const float targetPixels[] = {
        10.0f, 0.0f, 0.0f, 1.0f,
        11.0f, 0.0f, 0.0f, 1.0f,
        12.0f, 0.0f, 0.0f, 1.0f,
    };
    ImageF32x4_RGBA target;
    target.setFromRGBA32F(targetPixels, 3, 1);
    ImageF32x4_RGBA output;
    const std::vector<MorphControlPoint> points = {{0.0f, 0.0f, 2.0f, 0.0f, 1.0f}};

    morphImages(source, target, output, points, 0.5f, false);

    const FloatRGBA center = pixelAt(output, 1, 0);
    EXPECT_NEAR(center.r(), 6.0f, 1e-6f);
    EXPECT_FLOAT_EQ(center.a(), 1.0f);
}

TEST(ImageMorphTest, NegativeControlPointWeightBehavesLikeNoWarp) {
    const auto source = makeCoordinateImage(3, 1);
    const float targetPixels[] = {
        10.0f, 0.0f, 0.0f, 1.0f,
        11.0f, 0.0f, 0.0f, 1.0f,
        12.0f, 0.0f, 0.0f, 1.0f,
    };
    ImageF32x4_RGBA target;
    target.setFromRGBA32F(targetPixels, 3, 1);
    ImageF32x4_RGBA withoutWarp;
    ImageF32x4_RGBA negativeWeight;
    morphImages(source, target, withoutWarp, {}, 0.5f, false);
    const std::vector<MorphControlPoint> points = {{0.0f, 0.0f, 2.0f, 0.0f, -3.0f}};
    morphImages(source, target, negativeWeight, points, 0.5f, false);

    for (int x = 0; x < 3; ++x) {
        const FloatRGBA expected = pixelAt(withoutWarp, x, 0);
        const FloatRGBA actual = pixelAt(negativeWeight, x, 0);
        EXPECT_FLOAT_EQ(actual.r(), expected.r());
        EXPECT_FLOAT_EQ(actual.g(), expected.g());
        EXPECT_FLOAT_EQ(actual.b(), expected.b());
        EXPECT_FLOAT_EQ(actual.a(), expected.a());
    }
}

TEST(ImageDistortionSamplingTest, BilinearSamplerWrapsAcrossHorizontalSeamAndInterpolatesAlpha) {
    const float pixels[] = {
        0.0f, 2.0f, 4.0f, 0.2f,
        10.0f, 12.0f, 14.0f, 0.8f,
    };
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels, 2, 1);

    const FloatRGBA positiveSeam = sampleBilinear(image, 1.5f, 0.0f);
    const FloatRGBA negativeSeam = sampleBilinear(image, -0.5f, 0.0f);
    for (const FloatRGBA& sample : {positiveSeam, negativeSeam}) {
        EXPECT_NEAR(sample.r(), 5.0f, 1e-6f);
        EXPECT_NEAR(sample.g(), 7.0f, 1e-6f);
        EXPECT_NEAR(sample.b(), 9.0f, 1e-6f);
        EXPECT_NEAR(sample.a(), 0.5f, 1e-6f);
    }
}

TEST(ImageDistortionSamplingTest, EmptyImageSamplersReturnTransparentBlack) {
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(nullptr, 0, 0);

    const FloatRGBA bilinear = sampleBilinear(image, 1.0f, 2.0f);
    const FloatRGBA nearest = sampleNearest(image, 1.0f, 2.0f);
    for (const FloatRGBA& sample : {bilinear, nearest}) {
        EXPECT_FLOAT_EQ(sample.r(), 0.0f);
        EXPECT_FLOAT_EQ(sample.g(), 0.0f);
        EXPECT_FLOAT_EQ(sample.b(), 0.0f);
        EXPECT_FLOAT_EQ(sample.a(), 0.0f);
    }
}

TEST(ImageDistortionSamplingTest, SinglePixelBilinearSamplingIsConstantAcrossWrappedCoordinates) {
    const float pixels[] = {0.25f, 0.5f, 0.75f, 0.125f};
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels, 1, 1);

    for (const FloatRGBA& sample : {
             sampleBilinear(image, -0.25f, 0.75f),
             sampleBilinear(image, 3.5f, -2.25f)}) {
        EXPECT_FLOAT_EQ(sample.r(), 0.25f);
        EXPECT_FLOAT_EQ(sample.g(), 0.5f);
        EXPECT_FLOAT_EQ(sample.b(), 0.75f);
        EXPECT_FLOAT_EQ(sample.a(), 0.125f);
    }
}

TEST(ImageDistortionSamplingTest, NearestSamplerWrapsIntegerCoordinates) {
    const float pixels[] = {
        0.0f, 2.0f, 4.0f, 0.2f,
        10.0f, 12.0f, 14.0f, 0.8f,
    };
    ImageF32x4_RGBA image;
    image.setFromRGBA32F(pixels, 2, 1);

    const FloatRGBA wrappedNegative = sampleNearest(image, -1.6f, 0.0f);
    const FloatRGBA wrappedPositive = sampleNearest(image, 2.0f, 0.0f);
    EXPECT_FLOAT_EQ(wrappedNegative.r(), 10.0f);
    EXPECT_FLOAT_EQ(wrappedNegative.a(), 0.8f);
    EXPECT_FLOAT_EQ(wrappedPositive.r(), 0.0f);
    EXPECT_FLOAT_EQ(wrappedPositive.a(), 0.2f);
}

TEST(ImageDistortionSamplingTest, ApplyDisplacementSelectsBilinearOrNearestAndKeepsAlpha) {
    const float pixels[] = {
        0.0f, 2.0f, 4.0f, 0.2f,
        10.0f, 12.0f, 14.0f, 0.8f,
    };
    ImageF32x4_RGBA source;
    source.setFromRGBA32F(pixels, 2, 1);
    const auto halfPixelShift = [](float x, float y, float, float,
                                   float& sourceX, float& sourceY) {
        sourceX = x + 0.5f;
        sourceY = y;
    };
    ImageF32x4_RGBA bilinear;
    ImageF32x4_RGBA nearest;
    applyDisplacement(source, bilinear, halfPixelShift, true);
    applyDisplacement(source, nearest, halfPixelShift, false);

    const FloatRGBA interpolated = pixelAt(bilinear, 1, 0);
    EXPECT_NEAR(interpolated.r(), 5.0f, 1e-6f);
    EXPECT_NEAR(interpolated.a(), 0.5f, 1e-6f);
    const FloatRGBA selected = pixelAt(nearest, 1, 0);
    EXPECT_FLOAT_EQ(selected.r(), 0.0f);
    EXPECT_FLOAT_EQ(selected.a(), 0.2f);
    EXPECT_FLOAT_EQ(pixelAt(source, 1, 0).r(), 10.0f);
}

TEST(ImageDistortionSpherizeTest, CenterAndOutsideStayFixedWhileInsideBulgesOutward) {
    const auto mapper = makeSpherize(16.0f, 16.0f, 8.0f, 80.0f);
    float centerX = 0.0f;
    float centerY = 0.0f;
    mapper(16.0f, 16.0f, 32.0f, 32.0f, centerX, centerY);
    EXPECT_FLOAT_EQ(centerX, 16.0f);
    EXPECT_FLOAT_EQ(centerY, 16.0f);

    float insideX = 0.0f;
    float insideY = 0.0f;
    mapper(20.0f, 16.0f, 32.0f, 32.0f, insideX, insideY);
    EXPECT_GT(insideX, 20.0f);
    EXPECT_NEAR(insideY, 16.0f, 1e-6f);

    float outsideX = 0.0f;
    float outsideY = 0.0f;
    mapper(28.0f, 16.0f, 32.0f, 32.0f, outsideX, outsideY);
    EXPECT_FLOAT_EQ(outsideX, 28.0f);
    EXPECT_FLOAT_EQ(outsideY, 16.0f);
}

TEST(ImageDistortionTwirlTest, RotatesTowardCenterAndLeavesOutsideRadiusAlone) {
    const auto mapper = makeTwirl(32.0f, 32.0f, 32.0f, 90.0f);
    float rotatedX = 0.0f;
    float rotatedY = 0.0f;
    mapper(48.0f, 32.0f, 64.0f, 64.0f, rotatedX, rotatedY);
    EXPECT_NEAR(rotatedX, 32.0f + 16.0f / std::sqrt(2.0f), 1e-5f);
    EXPECT_NEAR(rotatedY, 32.0f + 16.0f / std::sqrt(2.0f), 1e-5f);

    float outsideX = 0.0f;
    float outsideY = 0.0f;
    mapper(64.0f, 32.0f, 64.0f, 64.0f, outsideX, outsideY);
    EXPECT_FLOAT_EQ(outsideX, 64.0f);
    EXPECT_FLOAT_EQ(outsideY, 32.0f);
}

TEST(ImageDistortionWaveTest, XAndYDisplacementsUseCrossAxisFrequency) {
    const auto mapper = makeWave(2.0f, 3.0f, 0.25f, 0.25f, 0.0f, 0.0f);
    float eastX = 0.0f;
    float eastY = 0.0f;
    mapper(1.0f, 0.0f, 8.0f, 8.0f, eastX, eastY);
    EXPECT_NEAR(eastX, 1.0f, 1e-6f);
    EXPECT_NEAR(eastY, 3.0f, 1e-6f);

    float northX = 0.0f;
    float northY = 0.0f;
    mapper(0.0f, 1.0f, 8.0f, 8.0f, northX, northY);
    EXPECT_NEAR(northX, 2.0f, 1e-6f);
    EXPECT_NEAR(northY, 1.0f, 1e-6f);
}

TEST(ImageDistortionBilinearWarpTest, MapsFourCornersAndCenter) {
    const auto mapper = makeBilinearWarp(
        10.0f, 20.0f, 110.0f, 30.0f,
        20.0f, 120.0f, 130.0f, 140.0f);
    const float input[][2] = {{0.0f, 0.0f}, {100.0f, 0.0f},
                              {0.0f, 100.0f}, {100.0f, 100.0f},
                              {50.0f, 50.0f}};
    const float expected[][2] = {{10.0f, 20.0f}, {110.0f, 30.0f},
                                 {20.0f, 120.0f}, {130.0f, 140.0f},
                                 {67.5f, 77.5f}};

    for (int index = 0; index < 5; ++index) {
        float mappedX = 0.0f;
        float mappedY = 0.0f;
        mapper(input[index][0], input[index][1], 100.0f, 100.0f,
               mappedX, mappedY);
        EXPECT_NEAR(mappedX, expected[index][0], 1e-5f);
        EXPECT_NEAR(mappedY, expected[index][1], 1e-5f);
    }
}

TEST(ImageDistortionOffsetTest, AddsPixelSpaceDelta) {
    const auto mapper = makeOffset(-3.5f, 2.25f);
    float mappedX = 0.0f;
    float mappedY = 0.0f;
    mapper(12.0f, 7.0f, 100.0f, 80.0f, mappedX, mappedY);
    EXPECT_FLOAT_EQ(mappedX, 8.5f);
    EXPECT_FLOAT_EQ(mappedY, 9.25f);
}

TEST(ImageDistortionScaleTest, KeepsCenterAndUsesInverseAxisScale) {
    const auto mapper = makeScale(10.0f, 20.0f, 2.0f, 4.0f);
    float centerX = 0.0f;
    float centerY = 0.0f;
    mapper(10.0f, 20.0f, 100.0f, 80.0f, centerX, centerY);
    EXPECT_FLOAT_EQ(centerX, 10.0f);
    EXPECT_FLOAT_EQ(centerY, 20.0f);

    float mappedX = 0.0f;
    float mappedY = 0.0f;
    mapper(14.0f, 28.0f, 100.0f, 80.0f, mappedX, mappedY);
    EXPECT_FLOAT_EQ(mappedX, 12.0f);
    EXPECT_FLOAT_EQ(mappedY, 22.0f);
}

TEST(ImageDistortionRotateTest, PositiveAngleUsesInverseRotationConvention) {
    const auto mapper = makeRotate(10.0f, 10.0f, 90.0f);
    float mappedX = 0.0f;
    float mappedY = 0.0f;
    mapper(12.0f, 10.0f, 100.0f, 80.0f, mappedX, mappedY);
    EXPECT_NEAR(mappedX, 10.0f, 1e-6f);
    EXPECT_NEAR(mappedY, 8.0f, 1e-6f);
}

TEST(ImageDistortionMirrorTest, ReflectsEachEnabledAxisAroundItsPivot) {
    const auto mapper = makeMirror(10.0f, 20.0f, true, true);
    float mappedX = 0.0f;
    float mappedY = 0.0f;
    mapper(13.0f, 25.0f, 100.0f, 80.0f, mappedX, mappedY);
    EXPECT_FLOAT_EQ(mappedX, 7.0f);
    EXPECT_FLOAT_EQ(mappedY, 15.0f);
}

TEST(ImageDistortionKaleidoscopeTest, FoldsAnglesSymmetricallyWithinEachSegment) {
    constexpr float pi = 3.14159265358979323846f;
    const auto mapper = makeKaleidoscope(0.0f, 0.0f, 4, 0.0f);
    float firstX = 0.0f;
    float firstY = 0.0f;
    mapper(10.0f * std::cos(pi / 6.0f),
           10.0f * std::sin(pi / 6.0f),
           100.0f, 100.0f, firstX, firstY);
    float reflectedX = 0.0f;
    float reflectedY = 0.0f;
    mapper(10.0f * std::cos(pi / 3.0f),
           10.0f * std::sin(pi / 3.0f),
           100.0f, 100.0f, reflectedX, reflectedY);

    EXPECT_NEAR(reflectedX, firstX, 1e-5f);
    EXPECT_NEAR(reflectedY, firstY, 1e-5f);
}

TEST(ImageDistortionNoiseTest, FixedParametersProduceRepeatableCoordinates) {
    const auto noiseA = makeNoiseDisplace(5.0f, 12.0f, 73, 0.25f);
    const auto noiseB = makeNoiseDisplace(5.0f, 12.0f, 73, 0.25f);
    float noiseAX = 0.0f;
    float noiseAY = 0.0f;
    float noiseBX = 0.0f;
    float noiseBY = 0.0f;
    noiseA(17.25f, 9.5f, 64.0f, 64.0f, noiseAX, noiseAY);
    noiseB(17.25f, 9.5f, 64.0f, 64.0f, noiseBX, noiseBY);
    EXPECT_FLOAT_EQ(noiseAX, noiseBX);
    EXPECT_FLOAT_EQ(noiseAY, noiseBY);

    const auto turbulentA = makeTurbulentDisplace(4.0f, 16.0f, 2.0f, 4, 0.5f);
    const auto turbulentB = makeTurbulentDisplace(4.0f, 16.0f, 2.0f, 4, 0.5f);
    float turbulentAX = 0.0f;
    float turbulentAY = 0.0f;
    float turbulentBX = 0.0f;
    float turbulentBY = 0.0f;
    turbulentA(17.25f, 9.5f, 64.0f, 64.0f, turbulentAX, turbulentAY);
    turbulentB(17.25f, 9.5f, 64.0f, 64.0f, turbulentBX, turbulentBY);
    EXPECT_FLOAT_EQ(turbulentAX, turbulentBX);
    EXPECT_FLOAT_EQ(turbulentAY, turbulentBY);
}

// The resident GPU shaders in Artifact/src/Effects re-implement these CPU
// mappers in HLSL. These tests pin the CPU contract those shaders are written
// against: center in pixels, amplitude in pixels, frequency in cycles/pixel,
// and a radial push that leaves concentric rings concentric.

TEST(ImageDistortionRippleTest, CenterPixelIsIdentity) {
    ImageF32x4_RGBA src = makeCoordinateImage(64, 64);
    ImageF32x4_RGBA dst;
    const float cx = 32.0f;
    const float cy = 32.0f;
    applyDisplacement(src, dst, makeRipple(cx, cy, 10.0f, 0.01f, 0.005f, 0.0f));

    // dist < 0.001 short-circuits to identity, so the exact center is untouched.
    const FloatRGBA center = pixelAt(dst, 32, 32);
    EXPECT_NEAR(center.r(), 32.0f, 1e-3f);
    EXPECT_NEAR(center.g(), 32.0f, 1e-3f);
}

TEST(ImageDistortionRippleTest, RadialPushStaysRadial) {
    ImageF32x4_RGBA src = makeCoordinateImage(64, 64);
    ImageF32x4_RGBA dst;
    const float cx = 32.0f;
    const float cy = 32.0f;
    // Zero amplitude leaves the mapping as identity everywhere.
    applyDisplacement(src, dst, makeRipple(cx, cy, 0.0f, 0.05f, 0.005f, 0.0f));

    // With no displacement every pixel must sample itself.
    for (int y = 0; y < 64; ++y) {
        for (int x = 0; x < 64; ++x) {
            const FloatRGBA p = pixelAt(dst, x, y);
            ASSERT_NEAR(p.r(), static_cast<float>(x), 1e-2f) << "at " << x << "," << y;
            ASSERT_NEAR(p.g(), static_cast<float>(y), 1e-2f) << "at " << x << "," << y;
        }
    }
}

TEST(ImageDistortionRippleTest, FalloffAttenuatesWithDistance) {
    ImageF32x4_RGBA src = makeCoordinateImage(128, 128);
    const float cx = 64.0f;
    const float cy = 64.0f;
    const float amplitude = 12.0f;
    const float frequency = 0.05f;
    const float decay = 0.05f;

    // Measure the radial displacement magnitude directly from the mapper so the
    // assertion does not depend on the sampling grid.
    const auto mapper = makeRipple(cx, cy, amplitude, frequency, decay, 0.0f);

    const float nearX = cx + 8.0f;
    float nearMappedX = 0.0f;
    float nearMappedY = 0.0f;
    mapper(nearX, cy, 128.0f, 128.0f, nearMappedX, nearMappedY);
    const float nearMagnitude = std::fabs(nearMappedX - nearX);

    const float farX = cx + 48.0f;
    float farMappedX = 0.0f;
    float farMappedY = 0.0f;
    mapper(farX, cy, 128.0f, 128.0f, farMappedX, farMappedY);
    const float farMagnitude = std::fabs(farMappedX - farX);

    // exp(-decay*d) must shrink the push as the wave travels outward.
    EXPECT_GT(nearMagnitude, 0.0f);
    EXPECT_LT(farMagnitude, nearMagnitude);
}

TEST(ImageDistortionOpticsCompensationTest, CenterStaysFixed) {
    ImageF32x4_RGBA src = makeCoordinateImage(64, 64);
    ImageF32x4_RGBA dst;
    const float cx = 32.0f;
    const float cy = 32.0f;
    applyDisplacement(src, dst,
                      makeOpticsCompensation(cx, cy, 45.0f, 1));

    const FloatRGBA center = pixelAt(dst, 32, 32);
    EXPECT_NEAR(center.r(), 32.0f, 0.5f);
    EXPECT_NEAR(center.g(), 32.0f, 0.5f);
}

TEST(ImageDistortionOpticsCompensationTest, NormalizedCenterBreaksTheWarp) {
    // makeOpticsCompensation divides by (sx - cx) / cx, so cx must be a pixel
    // distance, not a normalized 0..1 coordinate. OpticsCompensationEffect
    // passes centerX_ straight through, which lands here: passing 0.5 makes
    // every sample diverge instead of warping. This test documents that the
    // caller must pre-multiply by the image size; it will fail loudly if the
    // effect is ever "fixed" in the wrong place (inside the mapper).
    const auto mapper = makeOpticsCompensation(0.5f, 0.5f, 45.0f, 1);
    float ox = 0.0f;
    float oy = 0.0f;
    mapper(10.0f, 10.0f, 64.0f, 64.0f, ox, oy);
    // dx = (10 - 0.5) / 0.5 = 19, so the mapped point lands far off-image.
    EXPECT_GT(std::fabs(ox), 64.0f);
}

TEST(ImageDistortionOpticsCompensationTest, DirectionFlipsTheWarp) {
    const auto undistort = makeOpticsCompensation(32.0f, 32.0f, 45.0f, 1);
    const auto distort = makeOpticsCompensation(32.0f, 32.0f, 45.0f, -1);

    float undistortX = 0.0f;
    float undistortY = 0.0f;
    undistort(48.0f, 32.0f, 64.0f, 64.0f, undistortX, undistortY);
    float distortX = 0.0f;
    float distortY = 0.0f;
    distort(48.0f, 32.0f, 64.0f, 64.0f, distortX, distortY);

    // Undistort pulls samples inward, distort pushes them outward.
    EXPECT_LT(undistortX, 48.0f);
    EXPECT_GT(distortX, 48.0f);
}

TEST(ImageDistortionPolarCoordinatesTest, CenterStaysFixed) {
    ImageF32x4_RGBA src = makeCoordinateImage(64, 64);
    ImageF32x4_RGBA dst;
    const float cx = 32.0f;
    const float cy = 32.0f;
    // A full conversion must still terminate: the corners clamp rather than
    // sampling garbage.
    applyDisplacement(src, dst, makePolarCoordinates(cx, cy, 30.0f, 1.0f));

    const FloatRGBA center = pixelAt(dst, 32, 32);
    EXPECT_NEAR(center.r(), 32.0f, 1e-2f);
    EXPECT_NEAR(center.g(), 32.0f, 1e-2f);
}

TEST(ImageDistortionMakePinchBulgeTest, ZeroAmountIsIdentity) {
    ImageF32x4_RGBA src = makeCoordinateImage(32, 32);
    ImageF32x4_RGBA dst;
    applyDisplacement(src, dst, makePinchBulge(16.0f, 16.0f, 32.0f, 0.0f));

    for (int y = 0; y < 32; ++y) {
        for (int x = 0; x < 32; ++x) {
            const FloatRGBA p = pixelAt(dst, x, y);
            ASSERT_NEAR(p.r(), static_cast<float>(x), 1e-2f) << "at " << x << "," << y;
            ASSERT_NEAR(p.g(), static_cast<float>(y), 1e-2f) << "at " << x << "," << y;
        }
    }
}

TEST(ImageDistortionMakePinchBulgeTest, PositiveAmountBulgesOutward) {
    // amount > 0 uses pow(t, 1 + amount*0.01), so t^>1 < t and the sampled
    // radius shrinks: the image is magnified (bulged outward).
    const auto mapper = makePinchBulge(16.0f, 16.0f, 32.0f, 50.0f);
    float ox = 0.0f;
    float oy = 0.0f;
    mapper(24.0f, 16.0f, 32.0f, 32.0f, ox, oy);
    EXPECT_LT(ox, 24.0f);

    // amount < 0 pinches inward, pushing the sampled radius outward.
    const auto pincher = makePinchBulge(16.0f, 16.0f, 32.0f, -50.0f);
    float px = 0.0f;
    float py = 0.0f;
    pincher(24.0f, 16.0f, 32.0f, 32.0f, px, py);
    EXPECT_GT(px, 24.0f);
}

TEST(ImageDistortionMakePinchBulgeTest, OutsideRadiusIsIdentity) {
    const auto mapper = makePinchBulge(16.0f, 16.0f, 8.0f, 50.0f);
    float ox = 0.0f;
    float oy = 0.0f;
    // (31,16) sits 15px from the center, well past the 8px radius.
    mapper(31.0f, 16.0f, 32.0f, 32.0f, ox, oy);
    EXPECT_NEAR(ox, 31.0f, 1e-3f);
    EXPECT_NEAR(oy, 16.0f, 1e-3f);
}

TEST(ImageDistortionMakeMagnifyTest, PositiveAmountUsesInvertedExponent) {
    // Magnify deliberately flips the sign relative to PinchBulge:
    // pow(t, 1 - amount*0.01). With amount > 0 the exponent drops below 1,
    // t^e > t, so the sampled radius grows and the image shrinks. This pins the
    // inversion the resident GPU shader is written against.
    const auto mapper = makeMagnify(16.0f, 16.0f, 32.0f, 50.0f);
    float ox = 0.0f;
    float oy = 0.0f;
    mapper(24.0f, 16.0f, 32.0f, 32.0f, ox, oy);
    EXPECT_GT(ox, 24.0f);
}

TEST(ImageDistortionMakeMagnifyTest, OutsideRadiusIsIdentity) {
    const auto mapper = makeMagnify(16.0f, 16.0f, 8.0f, 50.0f);
    float ox = 0.0f;
    float oy = 0.0f;
    mapper(31.0f, 16.0f, 32.0f, 32.0f, ox, oy);
    EXPECT_NEAR(ox, 31.0f, 1e-3f);
    EXPECT_NEAR(oy, 16.0f, 1e-3f);
}

TEST(ImageDistortionMakePolarCoordinatesTest, ZeroAmountIsIdentity) {
    ImageF32x4_RGBA src = makeCoordinateImage(32, 32);
    ImageF32x4_RGBA dst;
    applyDisplacement(src, dst, makePolarCoordinates(16.0f, 16.0f, 16.0f, 0.0f));

    for (int y = 0; y < 32; ++y) {
        for (int x = 0; x < 32; ++x) {
            const FloatRGBA p = pixelAt(dst, x, y);
            ASSERT_NEAR(p.r(), static_cast<float>(x), 1e-2f) << "at " << x << "," << y;
            ASSERT_NEAR(p.g(), static_cast<float>(y), 1e-2f) << "at " << x << "," << y;
        }
    }
}

TEST(ImageDistortionMakePolarCoordinatesTest, AngleBecomesHorizontalAxis) {
    // At full conversion the source radius is radius * normalizedAngle, so the
    // four axis directions must land at four distinct radii.
    const auto mapper = makePolarCoordinates(16.0f, 16.0f, 16.0f, 1.0f);

    float eastX = 0.0f;
    float eastY = 0.0f;
    mapper(24.0f, 16.0f, 32.0f, 32.0f, eastX, eastY);   // angle 0
    float westX = 0.0f;
    float westY = 0.0f;
    mapper(8.0f, 16.0f, 32.0f, 32.0f, westX, westY);    // angle pi
    float northX = 0.0f;
    float northY = 0.0f;
    mapper(16.0f, 8.0f, 32.0f, 32.0f, northX, northY);   // angle -pi/2

    const float eastRadius = std::fabs(eastX - 16.0f);
    const float westRadius = std::fabs(westX - 16.0f);
    const float northRadius = std::fabs(northY - 16.0f);

    // angle 0 -> normAngle 0.5, angle pi -> 1.0, angle -pi/2 -> 0.25.
    EXPECT_NEAR(eastRadius, 16.0f * 0.5f, 1e-2f);
    EXPECT_NEAR(westRadius, 16.0f * 1.0f, 1e-2f);
    EXPECT_NEAR(northRadius, 16.0f * 0.25f, 1e-2f);
}
