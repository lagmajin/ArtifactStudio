#include <gtest/gtest.h>
#include <QCryptographicHash>
#include <QFile>
#include <QString>
#include <QTemporaryDir>
#include <QVariant>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

import ImageProcessing;
import Graphics.SurfaceColorContract;
import Artifact.Effect.Abstract;
import Artifact.Effect.Keying.IBKKeyer;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Property.Abstract;
import Utils.String.UniString;

using namespace ArtifactCore::Keying;
using ArtifactCore::SurfaceAlphaMode;
using ArtifactCore::SurfaceChannelOrder;
using Artifact::IBKKeyerEffect;
using ArtifactCore::ImageF32x4RGBAWithCache;
using ArtifactCore::ImageF32x4_RGBA;

namespace {

float alphaAt(const std::array<float, 12>& pixels, std::size_t pixelIndex)
{
    return pixels[pixelIndex * 4 + 3];
}

} // namespace

TEST(IBKKeyerEffectContractTest, IBKGpuMorphologySupportMatchesShaderSamplingWindow)
{
    IBKParams params;
    EXPECT_TRUE(IBKGpuParams::hasGpuSupportedMorphologyRadius(params));

    params.erodePixels = -1;
    params.dilatePixels = -2;
    EXPECT_TRUE(IBKGpuParams::hasGpuSupportedMorphologyRadius(params));
    auto gpuParams = IBKGpuParams::fromParams(params);
    EXPECT_EQ(gpuParams.erodePixels, 0u);
    EXPECT_EQ(gpuParams.dilatePixels, 0u);

    params.erodePixels = 64;
    params.dilatePixels = 64;
    EXPECT_FALSE(IBKGpuParams::hasGpuSupportedMorphologyRadius(params));
    gpuParams = IBKGpuParams::fromParams(params);
    EXPECT_EQ(gpuParams.erodePixels, 64u);
    EXPECT_EQ(gpuParams.dilatePixels, 64u);

    params.erodePixels = IBKGpuParams::MaxGpuMorphologyRadius;
    params.dilatePixels = IBKGpuParams::MaxGpuMorphologyRadius;
    EXPECT_TRUE(IBKGpuParams::hasGpuSupportedMorphologyRadius(params));

    params.erodePixels++;
    EXPECT_FALSE(IBKGpuParams::hasGpuSupportedMorphologyRadius(params));
    params.erodePixels = 1;
    params.dilatePixels++;
    EXPECT_FALSE(IBKGpuParams::hasGpuSupportedMorphologyRadius(params));
}

TEST(IBKKeyerEffectContractTest, IBKGpuParameterConversionSanitizesInvalidValues)
{
    IBKParams params;
    params.screenCorrection = std::numeric_limits<float>::quiet_NaN();
    params.coreMatteClip = std::numeric_limits<float>::infinity();
    params.edgeMatteSoftness = -std::numeric_limits<float>::infinity();
    params.despillStrength = std::numeric_limits<float>::quiet_NaN();
    params.garbageMatteGamma = 9.0f;
    params.detailRecovery = -2.0f;
    params.erodePixels = -8;
    params.dilatePixels = 100;

    const auto gpuParams = IBKGpuParams::fromParams(params);

    EXPECT_FLOAT_EQ(gpuParams.screenCorrection, 1.0f);
    EXPECT_FLOAT_EQ(gpuParams.coreMatteClip, 0.5f);
    EXPECT_FLOAT_EQ(gpuParams.edgeMatteSoftness, 0.2f);
    EXPECT_FLOAT_EQ(gpuParams.despillStrength, 0.5f);
    EXPECT_FLOAT_EQ(gpuParams.garbageMatteGamma, 4.0f);
    EXPECT_FLOAT_EQ(gpuParams.detailRecovery, 0.0f);
    EXPECT_EQ(gpuParams.erodePixels, 0u);
    EXPECT_EQ(gpuParams.dilatePixels, 64u);

    params.screenCorrection = 9.0f;
    const auto upperClampedGpuParams = IBKGpuParams::fromParams(params);
    EXPECT_FLOAT_EQ(upperClampedGpuParams.screenCorrection, 4.0f);
}

TEST(IBKKeyerEffectContractTest, IBKPublicPropertyRangesMatchSetterClamps)
{
    IBKKeyerEffect effect;
    effect.setPropertyValue(ArtifactCore::UniString("screenCorrection"), QVariant(9.0));
    effect.setPropertyValue(ArtifactCore::UniString("coreMatteClip"), QVariant(-0.5));
    effect.setPropertyValue(ArtifactCore::UniString("edgeMatteSoftness"), QVariant(0.0));
    effect.setPropertyValue(ArtifactCore::UniString("despillStrength"), QVariant(1.5));
    effect.setPropertyValue(ArtifactCore::UniString("garbageMatteGamma"), QVariant(9.0));
    effect.setPropertyValue(ArtifactCore::UniString("detailRecovery"), QVariant(-1.0));
    effect.setPropertyValue(ArtifactCore::UniString("erodePixels"), QVariant(99));
    effect.setPropertyValue(ArtifactCore::UniString("dilatePixels"), QVariant(-3));
    effect.setPropertyValue(ArtifactCore::UniString("previewMode"), QVariant(99));

    struct ExpectedProperty {
        QString name;
        double value;
        double minimum;
        double maximum;
    };
    const std::array expected{
        ExpectedProperty{QStringLiteral("screenCorrection"), 4.0, 0.0, 4.0},
        ExpectedProperty{QStringLiteral("coreMatteClip"), 0.0, 0.0, 1.0},
        ExpectedProperty{QStringLiteral("edgeMatteSoftness"), 1.0e-5, 1.0e-5, 1.0},
        ExpectedProperty{QStringLiteral("despillStrength"), 1.0, 0.0, 1.0},
        ExpectedProperty{QStringLiteral("garbageMatteGamma"), 4.0, 1.0e-5, 4.0},
        ExpectedProperty{QStringLiteral("detailRecovery"), 0.0, 0.0, 1.0},
        ExpectedProperty{QStringLiteral("erodePixels"), 64.0, 0.0, 64.0},
        ExpectedProperty{QStringLiteral("dilatePixels"), 0.0, 0.0, 64.0},
        ExpectedProperty{QStringLiteral("previewMode"), 2.0, 0.0, 2.0},
    };
    const auto properties = effect.getProperties();
    for (const auto& expectedProperty : expected) {
        const ArtifactCore::AbstractProperty* actual = nullptr;
        for (const auto& property : properties) {
            if (property.getName() == expectedProperty.name) {
                actual = &property;
                break;
            }
        }
        ASSERT_NE(actual, nullptr) << expectedProperty.name.toStdString();
        EXPECT_NEAR(actual->getValue().toDouble(), expectedProperty.value, 1.0e-6)
            << expectedProperty.name.toStdString();
        const auto metadata = actual->metadata();
        EXPECT_DOUBLE_EQ(metadata.hardMin.toDouble(), expectedProperty.minimum)
            << expectedProperty.name.toStdString();
        EXPECT_DOUBLE_EQ(metadata.hardMax.toDouble(), expectedProperty.maximum)
            << expectedProperty.name.toStdString();
    }
}

TEST(IBKKeyerEffectContractTest, IBKEffectUsesSpatiallyVaryingCleanPlate)
{
    const std::array<float, 12> foreground{
        0.0f, 0.72f, 0.0f, 1.0f,
        1.0f, 0.0f, 0.0f, 1.0f,
        0.0f, 0.90f, 0.0f, 1.0f};
    const std::array<float, 12> cleanPlate{
        0.0f, 0.72f, 0.0f, 1.0f,
        0.0f, 0.85f, 0.0f, 1.0f,
        0.0f, 0.90f, 0.0f, 1.0f};

    ImageF32x4_RGBA sourceImage;
    ImageF32x4_RGBA plateImage;
    sourceImage.setFromRGBA32F(foreground.data(), 3, 1);
    plateImage.setFromRGBA32F(cleanPlate.data(), 3, 1);

    IBKKeyerEffect effect;
    effect.setCleanPlate(plateImage);
    effect.setPropertyValue(ArtifactCore::UniString("erodePixels"), QVariant(0));
    effect.setPropertyValue(ArtifactCore::UniString("dilatePixels"), QVariant(0));
    ImageF32x4RGBAWithCache output;
    effect.applyCPUOnly(ImageF32x4RGBAWithCache(sourceImage), output);

    ASSERT_EQ(output.image().width(), 3);
    EXPECT_LT(output.image().getPixel(0, 0).a(), 0.01f);
    EXPECT_GT(output.image().getPixel(1, 0).a(), 0.7f);
    EXPECT_LT(output.image().getPixel(2, 0).a(), 0.01f);
}

TEST(IBKKeyerEffectContractTest, IBKMatchesAllForegroundAndCleanPlateChannelOrders)
{
    const std::array<float, 8> foregroundRgba{
        0.0f, 0.8f, 0.0f, 1.0f,
        0.8f, 0.2f, 0.1f, 1.0f};
    const std::array<float, 8> cleanPlateRgba{
        0.0f, 0.8f, 0.0f, 1.0f,
        0.0f, 0.7f, 0.0f, 1.0f};
    const auto toBgra = [](const std::array<float, 8>& rgba) {
        std::array<float, 8> bgra{};
        for (std::size_t pixel = 0; pixel < 2; ++pixel) {
            bgra[pixel * 4] = rgba[pixel * 4 + 2];
            bgra[pixel * 4 + 1] = rgba[pixel * 4 + 1];
            bgra[pixel * 4 + 2] = rgba[pixel * 4];
            bgra[pixel * 4 + 3] = rgba[pixel * 4 + 3];
        }
        return bgra;
    };
    const auto foregroundBgra = toBgra(foregroundRgba);
    const auto cleanPlateBgra = toBgra(cleanPlateRgba);
    const std::array<SurfaceChannelOrder, 2> orders{
        SurfaceChannelOrder::RGBA, SurfaceChannelOrder::BGRA};
    std::array<float, 8> referenceOutput{};
    IBKParams params;
    params.erodePixels = 0;
    params.dilatePixels = 0;
    params.despillStrength = 0.0f;

    for (const auto foregroundOrder : orders) {
        for (const auto cleanPlateOrder : orders) {
            const float* foreground = foregroundOrder == SurfaceChannelOrder::RGBA
                ? foregroundRgba.data() : foregroundBgra.data();
            const float* cleanPlate = cleanPlateOrder == SurfaceChannelOrder::RGBA
                ? cleanPlateRgba.data() : cleanPlateBgra.data();
            std::array<float, 8> output{};
            ASSERT_TRUE(processIBK(
                IBKBuffers{foreground, cleanPlate, output.data(), 2, 1,
                           foregroundOrder, cleanPlateOrder}, params));

            const int rIndex = foregroundOrder == SurfaceChannelOrder::RGBA ? 0 : 2;
            const int gIndex = 1;
            const int bIndex = foregroundOrder == SurfaceChannelOrder::RGBA ? 2 : 0;
            for (std::size_t pixel = 0; pixel < 2; ++pixel) {
                const std::array<float, 4> logicalOutput{
                    output[pixel * 4 + rIndex], output[pixel * 4 + gIndex],
                    output[pixel * 4 + bIndex], output[pixel * 4 + 3]};
                if (foregroundOrder == SurfaceChannelOrder::RGBA &&
                    cleanPlateOrder == SurfaceChannelOrder::RGBA) {
                    std::copy(logicalOutput.begin(), logicalOutput.end(),
                              referenceOutput.begin() + pixel * 4);
                } else {
                    for (std::size_t channel = 0; channel < 4; ++channel) {
                        EXPECT_NEAR(logicalOutput[channel],
                                    referenceOutput[pixel * 4 + channel],
                                    1.0e-6f);
                    }
                }
            }
        }
    }
    EXPECT_LT(referenceOutput[3], 0.01f);
    EXPECT_GT(referenceOutput[7], 0.7f);
}

TEST(IBKKeyerEffectContractTest, IBKClampsForegroundAlphaBeforeWritingOutput)
{
    const std::array<float, 4> cleanPlate{0.0f, 1.0f, 0.0f, 1.0f};
    IBKParams params;
    params.coreMatteClip = 0.0f;
    params.edgeMatteSoftness = 1.0f;
    params.despillStrength = 0.0f;
    params.detailRecovery = 1.0f;
    params.erodePixels = 0;
    params.dilatePixels = 0;

    std::array<float, 4> foreground{1.0f, 0.0f, 0.0f, 2.0f};
    std::array<float, 4> output{};
    ASSERT_TRUE(processIBK(
        IBKBuffers{foreground.data(), cleanPlate.data(), output.data(), 1, 1},
        params));
    EXPECT_GT(output[3], 0.9f);
    EXPECT_LE(output[3], 1.0f);

    foreground[3] = -1.0f;
    ASSERT_TRUE(processIBK(
        IBKBuffers{foreground.data(), cleanPlate.data(), output.data(), 1, 1},
        params));
    EXPECT_FLOAT_EQ(output[3], 0.0f);
    EXPECT_FLOAT_EQ(output[0], 0.0f);
    EXPECT_FLOAT_EQ(output[1], 0.0f);
    EXPECT_FLOAT_EQ(output[2], 0.0f);
}

TEST(IBKKeyerEffectContractTest, IBKCleanPlateGenerationUsesTemporalMedian)
{
    const std::array<float, 4> first{0.0f, 1.0f, 0.0f, 1.0f};
    const std::array<float, 4> subject{1.0f, 0.0f, 0.2f, 1.0f};
    const std::array<float, 4> last{0.4f, 0.6f, 0.4f, 1.0f};
    std::vector<ImageF32x4_RGBA> frames(3);
    frames[0].setFromRGBA32F(first.data(), 1, 1);
    frames[1].setFromRGBA32F(subject.data(), 1, 1);
    frames[2].setFromRGBA32F(last.data(), 1, 1);

    ImageF32x4_RGBA cleanPlate;
    ASSERT_TRUE(autoGenerateCleanPlate(frames, cleanPlate));

    const auto pixel = cleanPlate.getPixel(0, 0);
    EXPECT_FLOAT_EQ(pixel.r(), 0.4f);
    EXPECT_FLOAT_EQ(pixel.g(), 0.6f);
    EXPECT_FLOAT_EQ(pixel.b(), 0.2f);
}

TEST(IBKKeyerEffectContractTest, IBKCleanPlateGenerationAveragesTheMiddlePairForEvenFrameCounts)
{
    const std::array<float, 4> first{0.0f, 1.0f, 0.0f, 0.2f};
    const std::array<float, 4> second{0.4f, 0.6f, 0.2f, 0.8f};
    std::vector<ImageF32x4_RGBA> frames(2);
    frames[0].setFromRGBA32F(first.data(), 1, 1);
    frames[1].setFromRGBA32F(second.data(), 1, 1);

    ImageF32x4_RGBA cleanPlate;
    ASSERT_TRUE(autoGenerateCleanPlate(frames, cleanPlate));

    const auto pixel = cleanPlate.getPixel(0, 0);
    EXPECT_FLOAT_EQ(pixel.r(), 0.2f);
    EXPECT_FLOAT_EQ(pixel.g(), 0.8f);
    EXPECT_FLOAT_EQ(pixel.b(), 0.1f);
    EXPECT_FLOAT_EQ(pixel.a(), 0.5f);
}

TEST(IBKKeyerEffectContractTest, IBKCleanPlateGenerationNormalizesRgbaAndBgraIngress)
{
    const std::array<float, 4> rgbaFrame{0.1f, 0.2f, 0.3f, 1.0f};
    const std::array<float, 4> bgraFrame{0.5f, 0.6f, 0.7f, 1.0f};
    std::vector<ImageF32x4_RGBA> frames(2);
    frames[0].setFromRGBA32F(rgbaFrame.data(), 1, 1);
    frames[1].setFromBGRA32F(bgraFrame.data(), 1, 1);

    ImageF32x4_RGBA cleanPlate;
    ASSERT_TRUE(autoGenerateCleanPlate(frames, cleanPlate));

    const auto pixel = cleanPlate.getPixel(0, 0);
    EXPECT_NEAR(pixel.r(), 0.4f, 1.0e-6f);
    EXPECT_NEAR(pixel.g(), 0.4f, 1.0e-6f);
    EXPECT_NEAR(pixel.b(), 0.4f, 1.0e-6f);
    EXPECT_FLOAT_EQ(pixel.a(), 1.0f);
    EXPECT_EQ(cleanPlate.colorDescriptor().channelOrder, SurfaceChannelOrder::BGRA);
}

TEST(IBKKeyerEffectContractTest, IBKCleanPlateGenerationReplacesNonFiniteSamplesWithZero)
{
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();
    const std::array<float, 4> source{nan, 0.4f, infinity, nan};
    std::vector<ImageF32x4_RGBA> frames(1);
    frames[0].setFromRGBA32F(source.data(), 1, 1);

    ImageF32x4_RGBA cleanPlate;
    ASSERT_TRUE(autoGenerateCleanPlate(frames, cleanPlate));

    const auto pixel = cleanPlate.getPixel(0, 0);
    EXPECT_FLOAT_EQ(pixel.r(), 0.0f);
    EXPECT_FLOAT_EQ(pixel.g(), 0.4f);
    EXPECT_FLOAT_EQ(pixel.b(), 0.0f);
    EXPECT_FLOAT_EQ(pixel.a(), 0.0f);
}

TEST(IBKKeyerEffectContractTest, IBKCleanPlateGenerationRejectsEmptyAndMismatchedFramesWithoutChangingOutput)
{
    const std::array<float, 4> sentinel{0.3f, 0.2f, 0.1f, 0.7f};
    const std::array<float, 4> first{0.0f, 1.0f, 0.0f, 1.0f};
    const std::array<float, 8> differentSize{
        0.0f, 1.0f, 0.0f, 1.0f,
        0.1f, 0.8f, 0.0f, 1.0f};
    ImageF32x4_RGBA output;
    output.setFromRGBA32F(sentinel.data(), 1, 1);
    std::vector<ImageF32x4_RGBA> frames;

    EXPECT_FALSE(autoGenerateCleanPlate(frames, output));
    auto pixel = output.getPixel(0, 0);
    EXPECT_FLOAT_EQ(pixel.r(), sentinel[0]);
    EXPECT_FLOAT_EQ(pixel.g(), sentinel[1]);
    EXPECT_FLOAT_EQ(pixel.b(), sentinel[2]);
    EXPECT_FLOAT_EQ(pixel.a(), sentinel[3]);

    frames.resize(2);
    frames[0].setFromRGBA32F(first.data(), 1, 1);
    frames[1].setFromRGBA32F(differentSize.data(), 2, 1);
    EXPECT_FALSE(autoGenerateCleanPlate(frames, output));
    pixel = output.getPixel(0, 0);
    EXPECT_FLOAT_EQ(pixel.r(), sentinel[0]);
    EXPECT_FLOAT_EQ(pixel.g(), sentinel[1]);
    EXPECT_FLOAT_EQ(pixel.b(), sentinel[2]);
    EXPECT_FLOAT_EQ(pixel.a(), sentinel[3]);
}

TEST(IBKKeyerEffectContractTest, IBKCleanPlateGenerationEnforcesThe4096FrameLimit)
{
    const std::array<float, 4> source{0.2f, 0.3f, 0.4f, 1.0f};
    ImageF32x4_RGBA frame;
    frame.setFromRGBA32F(source.data(), 1, 1);
    std::vector<ImageF32x4_RGBA> frames(4096, frame);
    ImageF32x4_RGBA cleanPlate;

    ASSERT_TRUE(autoGenerateCleanPlate(frames, cleanPlate));
    const auto acceptedPixel = cleanPlate.getPixel(0, 0);
    EXPECT_FLOAT_EQ(acceptedPixel.r(), source[0]);
    EXPECT_FLOAT_EQ(acceptedPixel.g(), source[1]);
    EXPECT_FLOAT_EQ(acceptedPixel.b(), source[2]);
    EXPECT_FLOAT_EQ(acceptedPixel.a(), source[3]);

    frames.emplace_back(frame);
    EXPECT_FALSE(autoGenerateCleanPlate(frames, cleanPlate));
    const auto preservedPixel = cleanPlate.getPixel(0, 0);
    EXPECT_FLOAT_EQ(preservedPixel.r(), acceptedPixel.r());
    EXPECT_FLOAT_EQ(preservedPixel.g(), acceptedPixel.g());
    EXPECT_FLOAT_EQ(preservedPixel.b(), acceptedPixel.b());
    EXPECT_FLOAT_EQ(preservedPixel.a(), acceptedPixel.a());
}

TEST(IBKKeyerEffectContractTest, EmptyCleanPlatePathClearsTheLoadedPlate)
{
    const std::array<float, 4> foreground{0.0f, 0.85f, 0.0f, 1.0f};
    ImageF32x4_RGBA sourceImage;
    ImageF32x4_RGBA plateImage;
    sourceImage.setFromRGBA32F(foreground.data(), 1, 1);
    plateImage.setFromRGBA32F(foreground.data(), 1, 1);

    IBKKeyerEffect effect;
    effect.setCleanPlate(plateImage);
    effect.setPropertyValue(ArtifactCore::UniString("cleanPlatePath"), QVariant(QString()));
    ImageF32x4RGBAWithCache output;
    effect.applyCPUOnly(ImageF32x4RGBAWithCache(sourceImage), output);

    EXPECT_FLOAT_EQ(output.image().getPixel(0, 0).a(), 1.0f);
    EXPECT_FLOAT_EQ(output.image().getPixel(0, 0).g(), 0.85f);
}

TEST(IBKKeyerEffectContractTest, IBKLoadsCleanPlateFromTheConfiguredImagePath)
{
    const QString fixturePath =
        qEnvironmentVariable("ARTIFACT_KEYING_REAL_FIXTURE");
    ASSERT_FALSE(fixturePath.isEmpty())
        << "CMake must provide the bundled clean-plate fixture path";

    ImageF32x4_RGBA sourceImage;
    ASSERT_TRUE(sourceImage.load(fixturePath));
    ASSERT_EQ(sourceImage.width(), 540);
    ASSERT_EQ(sourceImage.height(), 960);

    IBKKeyerEffect effect;
    effect.setPropertyValue(ArtifactCore::UniString("erodePixels"), QVariant(0));
    effect.setPropertyValue(ArtifactCore::UniString("dilatePixels"), QVariant(0));
    effect.setPropertyValue(ArtifactCore::UniString("cleanPlatePath"),
                            QVariant(fixturePath));

    ImageF32x4RGBAWithCache output;
    effect.applyCPUOnly(ImageF32x4RGBAWithCache(sourceImage), output);

    ASSERT_EQ(output.image().width(), sourceImage.width());
    ASSERT_EQ(output.image().height(), sourceImage.height());
    EXPECT_LT(output.image().getPixel(270, 480).a(), 0.01f)
        << "a successfully loaded identical clean plate should remove the frame";
}

TEST(IBKKeyerEffectContractTest, InvalidCleanPlatePathKeepsThePreviouslyLoadedPlate)
{
    QTemporaryDir temporaryDirectory;
    ASSERT_TRUE(temporaryDirectory.isValid());

    const std::array<float, 4> foreground{0.0f, 0.85f, 0.0f, 1.0f};
    ImageF32x4_RGBA sourceImage;
    ImageF32x4_RGBA plateImage;
    sourceImage.setFromRGBA32F(foreground.data(), 1, 1);
    plateImage.setFromRGBA32F(foreground.data(), 1, 1);

    IBKKeyerEffect effect;
    effect.setCleanPlate(plateImage);
    effect.setPropertyValue(
        ArtifactCore::UniString("cleanPlatePath"),
        QVariant(temporaryDirectory.filePath(QStringLiteral("missing.png"))));

    ImageF32x4RGBAWithCache output;
    effect.applyCPUOnly(ImageF32x4RGBAWithCache(sourceImage), output);

    EXPECT_LT(output.image().getPixel(0, 0).a(), 0.01f)
        << "a failed path update should preserve the previously loaded plate";
}

TEST(IBKKeyerEffectContractTest, IBKPassesThroughWhenCleanPlateDimensionsDoNotMatch)
{
    const std::array<float, 8> foreground{
        0.15f, 0.7f, 0.2f, 0.6f,
        0.8f, 0.1f, 0.05f, 1.0f};
    const std::array<float, 4> cleanPlate{0.0f, 0.8f, 0.0f, 1.0f};
    ImageF32x4_RGBA sourceImage;
    ImageF32x4_RGBA plateImage;
    sourceImage.setFromRGBA32F(foreground.data(), 2, 1);
    plateImage.setFromRGBA32F(cleanPlate.data(), 1, 1);

    IBKKeyerEffect effect;
    effect.setCleanPlate(plateImage);
    ImageF32x4RGBAWithCache output;
    effect.applyCPUOnly(ImageF32x4RGBAWithCache(sourceImage), output);

    ASSERT_EQ(output.image().width(), 2);
    ASSERT_EQ(output.image().height(), 1);
    for (int x = 0; x < 2; ++x) {
        const auto actual = output.image().getPixel(x, 0);
        const std::size_t offset = static_cast<std::size_t>(x) * 4;
        EXPECT_FLOAT_EQ(actual.r(), foreground[offset]);
        EXPECT_FLOAT_EQ(actual.g(), foreground[offset + 1]);
        EXPECT_FLOAT_EQ(actual.b(), foreground[offset + 2]);
        EXPECT_FLOAT_EQ(actual.a(), foreground[offset + 3]);
    }
}

TEST(IBKKeyerEffectContractTest, IBKMattePreviewDisplaysTheFinalAlphaAsOpaqueGrayscale)
{
    const std::array<float, 4> foreground{1.0f, 0.0f, 0.0f, 1.0f};
    const std::array<float, 4> cleanPlate{0.0f, 0.85f, 0.0f, 1.0f};
    ImageF32x4_RGBA sourceImage;
    ImageF32x4_RGBA plateImage;
    sourceImage.setFromRGBA32F(foreground.data(), 1, 1);
    plateImage.setFromRGBA32F(cleanPlate.data(), 1, 1);

    IBKKeyerEffect effect;
    effect.setCleanPlate(plateImage);
    effect.setPropertyValue(ArtifactCore::UniString("erodePixels"), QVariant(0));
    effect.setPropertyValue(ArtifactCore::UniString("dilatePixels"), QVariant(0));
    ImageF32x4RGBAWithCache finalOutput;
    effect.applyCPUOnly(ImageF32x4RGBAWithCache(sourceImage), finalOutput);
    effect.setPropertyValue(ArtifactCore::UniString("previewMode"), QVariant(1));
    ImageF32x4RGBAWithCache mattePreview;
    effect.applyCPUOnly(ImageF32x4RGBAWithCache(sourceImage), mattePreview);

    const float matte = finalOutput.image().getPixel(0, 0).a();
    ASSERT_GT(matte, 0.0f);
    const auto preview = mattePreview.image().getPixel(0, 0);
    EXPECT_NEAR(preview.r(), matte, 1.0e-6f);
    EXPECT_NEAR(preview.g(), matte, 1.0e-6f);
    EXPECT_NEAR(preview.b(), matte, 1.0e-6f);
    EXPECT_FLOAT_EQ(preview.a(), 1.0f);
}
