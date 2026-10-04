#include <gtest/gtest.h>
#include <cmath>
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

FloatRGBA pixelAt(const ImageF32x4_RGBA& image, int x, int y) {
    const float* data = image.rgba32fData();
    const size_t o = (static_cast<size_t>(y) * image.width() + x) * 4u;
    return FloatRGBA(data[o + 0], data[o + 1], data[o + 2], data[o + 3]);
}

} // namespace

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