#include <gtest/gtest.h>

#include <cmath>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <utility>

#include <QSize>
#include <QString>

import Preview.Settings;
import Preview.Quality;

using namespace ArtifactCore;

TEST(PreviewSettingsLodContractTest, ResolutionScaleClampsToSupportedRange)
{
    PreviewSettings settings;

    settings.setResolutionScale(0.0f);
    EXPECT_FLOAT_EQ(settings.resolutionScale(), 0.125f);
    settings.setResolutionScale(-2.0f);
    EXPECT_FLOAT_EQ(settings.resolutionScale(), 0.125f);
    settings.setResolutionScale(2.0f);
    EXPECT_FLOAT_EQ(settings.resolutionScale(), 1.0f);
}

TEST(PreviewSettingsLodContractTest, NonFiniteResolutionScalesStayFinite)
{
    PreviewSettings settings;

    settings.setResolutionScale(std::numeric_limits<float>::quiet_NaN());
    EXPECT_TRUE(std::isfinite(settings.resolutionScale()));
    EXPECT_FLOAT_EQ(settings.resolutionScale(), 1.0f);

    settings.setResolutionScale(std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(settings.resolutionScale(), 1.0f);

    settings.setResolutionScale(-std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(settings.resolutionScale(), 0.125f);
}

TEST(PreviewSettingsLodContractTest, KeepsRepresentativePreviewResolutionScales)
{
    PreviewSettings settings;
    for (const float scale : {1.0f, 0.75f, 0.5f, 0.25f, 0.125f}) {
        settings.setResolutionScale(scale);
        EXPECT_FLOAT_EQ(settings.resolutionScale(), scale);
    }
}

TEST(PreviewSettingsLodContractTest, UnknownScaleFallsBackToFullResolution)
{
    PreviewQuality quality(static_cast<PreviewQuality::Scale>(-1));

    EXPECT_FLOAT_EQ(quality.scaleMultiplier(), 1.0f);
    EXPECT_EQ(quality.calculatePreviewSize(QSize{1919, 1079}),
              (QSize{1919, 1079}));

    quality.setQuality(static_cast<PreviewQuality::Scale>(100));
    EXPECT_FLOAT_EQ(quality.scaleMultiplier(), 1.0f);
    EXPECT_EQ(quality.calculatePreviewSize(QSize{17, 9}), (QSize{17, 9}));
}

TEST(PreviewSettingsLodContractTest, CopiesAndResetsResolutionScale)
{
    PreviewSettings source;
    source.setResolutionScale(0.375f);

    PreviewSettings copy(source);
    EXPECT_FLOAT_EQ(copy.resolutionScale(), 0.375f);
    copy.setResolutionScale(0.25f);
    EXPECT_FLOAT_EQ(source.resolutionScale(), 0.375f);

    copy.reset();
    EXPECT_FLOAT_EQ(copy.resolutionScale(), 1.0f);
}

TEST(PreviewSettingsLodContractTest, PreviewQualityScalesProduceMonotonicOddDimensions)
{
    constexpr QSize sourceSize{1919, 1079};
    const PreviewQuality::Scale scales[]{
        PreviewQuality::Scale::Eighth,
        PreviewQuality::Scale::Quarter,
        PreviewQuality::Scale::Half,
        PreviewQuality::Scale::Full};
    const QSize expectedSizes[]{
        QSize{239, 134},
        QSize{479, 269},
        QSize{959, 539},
        QSize{1919, 1079}};

    int previousPixelCount = 0;
    for (std::size_t index = 0; index < std::size(scales); ++index) {
        PreviewQuality quality(scales[index]);
        const QSize previewSize = quality.calculatePreviewSize(sourceSize);
        EXPECT_EQ(previewSize, expectedSizes[index]);
        EXPECT_GT(previewSize.width(), 0);
        EXPECT_GT(previewSize.height(), 0);
        const int pixelCount = previewSize.width() * previewSize.height();
        EXPECT_GT(pixelCount, previousPixelCount);
        previousPixelCount = pixelCount;
    }
}

TEST(PreviewSettingsLodContractTest, AutoScaleUsesTheDocumentedHalfResolution)
{
    PreviewQuality quality(PreviewQuality::Scale::Auto);

    EXPECT_FLOAT_EQ(quality.scaleMultiplier(), 0.5f);
    EXPECT_EQ(quality.calculatePreviewSize(QSize{1920, 1080}), (QSize{960, 540}));
    EXPECT_EQ(quality.calculatePreviewSize(QSize{1919, 1079}), (QSize{959, 539}));
}

TEST(PreviewSettingsLodContractTest, PositiveSourceDimensionsNeverCollapseToZero)
{
    const PreviewQuality::Scale scales[]{
        PreviewQuality::Scale::Full,
        PreviewQuality::Scale::Half,
        PreviewQuality::Scale::Quarter,
        PreviewQuality::Scale::Eighth,
        PreviewQuality::Scale::Auto};
    const QSize sourceSizes[]{QSize{1, 1}, QSize{1, 7}, QSize{9, 1}};

    for (const PreviewQuality::Scale scale : scales) {
        PreviewQuality quality(scale);
        for (const QSize& sourceSize : sourceSizes) {
            const QSize previewSize = quality.calculatePreviewSize(sourceSize);
            EXPECT_GE(previewSize.width(), 1)
                << "scale=" << static_cast<int>(scale)
                << " source=" << sourceSize.width() << "x" << sourceSize.height();
            EXPECT_GE(previewSize.height(), 1)
                << "scale=" << static_cast<int>(scale)
                << " source=" << sourceSize.width() << "x" << sourceSize.height();
            EXPECT_LE(previewSize.width(), sourceSize.width());
            EXPECT_LE(previewSize.height(), sourceSize.height());
        }
    }
}

TEST(PreviewSettingsLodContractTest,
     TinyPositiveDimensionsUsePerAxisFloorWithOnePixelMinimum)
{
    const PreviewQuality::Scale scales[]{
        PreviewQuality::Scale::Full,
        PreviewQuality::Scale::Half,
        PreviewQuality::Scale::Quarter,
        PreviewQuality::Scale::Eighth,
        PreviewQuality::Scale::Auto};

    for (const PreviewQuality::Scale scale : scales) {
        PreviewQuality quality(scale);
        const float multiplier = quality.scaleMultiplier();
        for (int sourceWidth = 1; sourceWidth <= 9; ++sourceWidth) {
            for (int sourceHeight = 1; sourceHeight <= 9; ++sourceHeight) {
                const auto expectedDimension = [multiplier](const int source) {
                    const int scaled = static_cast<int>(source * multiplier);
                    return scaled > 0 ? scaled : 1;
                };
                const QSize sourceSize{sourceWidth, sourceHeight};
                const QSize expectedSize{expectedDimension(sourceWidth),
                                         expectedDimension(sourceHeight)};

                EXPECT_EQ(quality.calculatePreviewSize(sourceSize), expectedSize)
                    << "scale=" << static_cast<int>(scale)
                    << " source=" << sourceWidth << "x" << sourceHeight;
            }
        }
    }
}

TEST(PreviewSettingsLodContractTest,
     LargePositiveDimensionsStayWithinIntegerAndSourceBounds)
{
    const int sourceWidth = std::numeric_limits<int>::max() - 127;
    const int sourceHeight = std::numeric_limits<int>::max() - 255;
    const QSize sourceSize{sourceWidth, sourceHeight};
    const struct LargeDimensionCase {
        PreviewQuality::Scale scale;
        int divisor;
    } cases[] = {
        {PreviewQuality::Scale::Full, 1},
        {PreviewQuality::Scale::Half, 2},
        {PreviewQuality::Scale::Quarter, 4},
        {PreviewQuality::Scale::Eighth, 8},
        {PreviewQuality::Scale::Auto, 2},
    };

    for (const LargeDimensionCase& testCase : cases) {
        PreviewQuality quality(testCase.scale);
        const QSize previewSize = quality.calculatePreviewSize(sourceSize);
        EXPECT_EQ(previewSize.width(), sourceWidth / testCase.divisor)
            << "scale=" << static_cast<int>(testCase.scale);
        EXPECT_EQ(previewSize.height(), sourceHeight / testCase.divisor)
            << "scale=" << static_cast<int>(testCase.scale);
        EXPECT_GE(previewSize.width(), 1);
        EXPECT_GE(previewSize.height(), 1);
        EXPECT_LE(previewSize.width(), sourceWidth);
        EXPECT_LE(previewSize.height(), sourceHeight);
    }
}

TEST(PreviewSettingsLodContractTest,
     IntMaximumDimensionsScaleWithoutFloatRoundingPastTheIntegerRange)
{
    const int sourceDimension = std::numeric_limits<int>::max();
    const QSize sourceSize{sourceDimension, sourceDimension};
    const struct MaximumDimensionCase {
        PreviewQuality::Scale scale;
        int divisor;
    } cases[] = {
        {PreviewQuality::Scale::Full, 1},
        {PreviewQuality::Scale::Half, 2},
        {PreviewQuality::Scale::Quarter, 4},
        {PreviewQuality::Scale::Eighth, 8},
        {PreviewQuality::Scale::Auto, 2},
    };

    for (const MaximumDimensionCase& testCase : cases) {
        PreviewQuality quality(testCase.scale);
        const QSize previewSize = quality.calculatePreviewSize(sourceSize);
        const int expectedDimension = sourceDimension / testCase.divisor;

        EXPECT_EQ(previewSize.width(), expectedDimension)
            << "scale=" << static_cast<int>(testCase.scale);
        EXPECT_EQ(previewSize.height(), expectedDimension)
            << "scale=" << static_cast<int>(testCase.scale);
        EXPECT_GE(previewSize.width(), 1);
        EXPECT_GE(previewSize.height(), 1);
        EXPECT_LE(previewSize.width(), sourceDimension);
        EXPECT_LE(previewSize.height(), sourceDimension);
    }
}

TEST(PreviewSettingsLodContractTest, EmptySourceDimensionsStayEmptyAtEveryQuality)
{
    const PreviewQuality::Scale scales[]{
        PreviewQuality::Scale::Full,
        PreviewQuality::Scale::Half,
        PreviewQuality::Scale::Quarter,
        PreviewQuality::Scale::Eighth,
        PreviewQuality::Scale::Auto};

    for (const PreviewQuality::Scale scale : scales) {
        PreviewQuality quality(scale);
        const float multiplier = quality.scaleMultiplier();
        const int expectedHeight = static_cast<int>(7.0f * multiplier);
        const int expectedWidth = static_cast<int>(9.0f * multiplier);
        EXPECT_EQ(quality.calculatePreviewSize(QSize{0, 0}), (QSize{0, 0}));
        EXPECT_EQ(quality.calculatePreviewSize(QSize{0, 7}),
                  (QSize{0, expectedHeight}));
        EXPECT_EQ(quality.calculatePreviewSize(QSize{9, 0}),
                  (QSize{expectedWidth, 0}));
    }
}

TEST(PreviewSettingsLodContractTest,
     FrameRateLimitActivatesOnlyForPositiveCapsAndSurvivesPresetChanges)
{
    PreviewQuality quality;
    EXPECT_EQ(quality.maxFrameRate(), 0);
    EXPECT_FALSE(quality.isFrameRateLimited());

    for (const int fps : {-120, -1, 0}) {
        quality.setMaxFrameRate(fps);
        EXPECT_EQ(quality.maxFrameRate(), fps);
        EXPECT_FALSE(quality.isFrameRateLimited());
    }

    for (const int fps : {1, 24, 60, 240}) {
        quality.setMaxFrameRate(fps);
        EXPECT_EQ(quality.maxFrameRate(), fps);
        EXPECT_TRUE(quality.isFrameRateLimited());
    }

    quality.applyPreset(QStringLiteral("Draft"));
    EXPECT_EQ(quality.maxFrameRate(), 240);
    EXPECT_TRUE(quality.isFrameRateLimited());
    quality.applyPreset(QStringLiteral("High"));
    EXPECT_EQ(quality.maxFrameRate(), 240);
    EXPECT_TRUE(quality.isFrameRateLimited());
}

TEST(PreviewSettingsLodContractTest, QualityPresetsSelectExpectedResolutionLevels)
{
    PreviewQuality quality;

    quality.applyPreset(QStringLiteral("High"));
    EXPECT_EQ(quality.quality(), PreviewQuality::Scale::Full);
    EXPECT_FLOAT_EQ(quality.scaleMultiplier(), 1.0f);
    EXPECT_EQ(quality.textureQuality(), PreviewQuality::TextureQuality::High);
    EXPECT_EQ(quality.antiAliasing(), PreviewQuality::AntiAliasing::MSAA_4x);
    EXPECT_EQ(quality.renderMode(), PreviewQuality::RenderMode::Normal);
    EXPECT_TRUE(quality.isHighQuality());
    EXPECT_FALSE(quality.isLowQuality());
    EXPECT_TRUE(quality.isMotionBlurEnabled());
    EXPECT_TRUE(quality.isDepthOfFieldEnabled());
    EXPECT_TRUE(quality.areShadowsEnabled());
    EXPECT_EQ(quality.calculatePreviewSize(QSize{1280, 720}), (QSize{1280, 720}));

    quality.applyPreset(QStringLiteral("Medium"));
    EXPECT_EQ(quality.quality(), PreviewQuality::Scale::Half);
    EXPECT_FLOAT_EQ(quality.scaleMultiplier(), 0.5f);
    EXPECT_EQ(quality.textureQuality(), PreviewQuality::TextureQuality::Medium);
    EXPECT_EQ(quality.antiAliasing(), PreviewQuality::AntiAliasing::FXAA);
    EXPECT_EQ(quality.renderMode(), PreviewQuality::RenderMode::Normal);
    EXPECT_FALSE(quality.isHighQuality());
    EXPECT_FALSE(quality.isLowQuality());
    EXPECT_TRUE(quality.isMotionBlurEnabled());
    EXPECT_FALSE(quality.isDepthOfFieldEnabled());
    EXPECT_TRUE(quality.areShadowsEnabled());
    EXPECT_EQ(quality.calculatePreviewSize(QSize{1280, 720}), (QSize{640, 360}));

    quality.applyPreset(QStringLiteral("Low"));
    EXPECT_EQ(quality.quality(), PreviewQuality::Scale::Quarter);
    EXPECT_FLOAT_EQ(quality.scaleMultiplier(), 0.25f);
    EXPECT_EQ(quality.textureQuality(), PreviewQuality::TextureQuality::Low);
    EXPECT_EQ(quality.antiAliasing(), PreviewQuality::AntiAliasing::None);
    EXPECT_EQ(quality.renderMode(), PreviewQuality::RenderMode::Fast);
    EXPECT_TRUE(quality.isLowQuality());
    EXPECT_FALSE(quality.isHighQuality());
    EXPECT_FALSE(quality.isMotionBlurEnabled());
    EXPECT_FALSE(quality.isDepthOfFieldEnabled());
    EXPECT_FALSE(quality.areShadowsEnabled());
    EXPECT_EQ(quality.calculatePreviewSize(QSize{1280, 720}), (QSize{320, 180}));

    quality.applyPreset(QStringLiteral("Draft"));
    EXPECT_EQ(quality.quality(), PreviewQuality::Scale::Eighth);
    EXPECT_FLOAT_EQ(quality.scaleMultiplier(), 0.125f);
    EXPECT_EQ(quality.textureQuality(), PreviewQuality::TextureQuality::Low);
    EXPECT_EQ(quality.antiAliasing(), PreviewQuality::AntiAliasing::None);
    EXPECT_EQ(quality.renderMode(), PreviewQuality::RenderMode::Draft);
    EXPECT_TRUE(quality.isLowQuality());
    EXPECT_FALSE(quality.isHighQuality());
    EXPECT_FALSE(quality.isMotionBlurEnabled());
    EXPECT_FALSE(quality.isDepthOfFieldEnabled());
    EXPECT_FALSE(quality.areShadowsEnabled());
    EXPECT_EQ(quality.calculatePreviewSize(QSize{1280, 720}), (QSize{160, 90}));

    quality.applyPreset(QStringLiteral("High"));
    EXPECT_EQ(quality.currentPresetName(), QStringLiteral("High"));
    EXPECT_EQ(quality.quality(), PreviewQuality::Scale::Full);
    EXPECT_FLOAT_EQ(quality.scaleMultiplier(), 1.0f);
    EXPECT_EQ(quality.textureQuality(), PreviewQuality::TextureQuality::High);
    EXPECT_EQ(quality.antiAliasing(), PreviewQuality::AntiAliasing::MSAA_4x);
    EXPECT_EQ(quality.renderMode(), PreviewQuality::RenderMode::Normal);
    EXPECT_TRUE(quality.isHighQuality());
    EXPECT_FALSE(quality.isLowQuality());
    EXPECT_TRUE(quality.isMotionBlurEnabled());
    EXPECT_TRUE(quality.isDepthOfFieldEnabled());
    EXPECT_TRUE(quality.areShadowsEnabled());
    EXPECT_EQ(quality.calculatePreviewSize(QSize{1280, 720}), (QSize{1280, 720}));
}

TEST(PreviewSettingsLodContractTest,
     UnknownPresetFallsBackToFullResolutionQuality)
{
    PreviewQuality quality;
    quality.applyPreset(QStringLiteral("Draft"));
    ASSERT_EQ(quality.quality(), PreviewQuality::Scale::Eighth);
    ASSERT_EQ(quality.calculatePreviewSize(QSize{1920, 1080}),
              (QSize{240, 135}));

    quality.applyPreset(QStringLiteral("Ultra"));

    EXPECT_EQ(quality.quality(), PreviewQuality::Scale::Full);
    EXPECT_FLOAT_EQ(quality.scaleMultiplier(), 1.0f);
    EXPECT_EQ(quality.renderMode(), PreviewQuality::RenderMode::Normal);
    EXPECT_EQ(quality.antiAliasing(), PreviewQuality::AntiAliasing::None);
    EXPECT_EQ(quality.textureQuality(), PreviewQuality::TextureQuality::High);
    EXPECT_EQ(quality.calculatePreviewSize(QSize{1920, 1080}),
              (QSize{1920, 1080}));
}

TEST(PreviewSettingsLodContractTest,
     TextureQualityChangesRemainIndependentFromResolutionLOD)
{
    PreviewQuality quality(PreviewQuality::Scale::Full);
    const PreviewQuality defaultHighQuality;
    const PreviewQuality::TextureQuality textureLevels[]{
        PreviewQuality::TextureQuality::Low,
        PreviewQuality::TextureQuality::Medium,
        PreviewQuality::TextureQuality::High};

    for (const PreviewQuality::TextureQuality textureLevel : textureLevels) {
        quality.setTextureQuality(textureLevel);

        EXPECT_EQ(quality.textureQuality(), textureLevel);
        EXPECT_EQ(quality.quality(), PreviewQuality::Scale::Full);
        EXPECT_EQ(quality.calculatePreviewSize(QSize{1280, 720}),
                  (QSize{1280, 720}));
        EXPECT_EQ(quality == defaultHighQuality,
                  textureLevel == PreviewQuality::TextureQuality::High);
    }
}

TEST(PreviewSettingsLodContractTest, CopiesAndMovesPreserveIndependentQualityScale)
{
    PreviewQuality original(PreviewQuality::Scale::Quarter);
    PreviewQuality copied(original);
    original.setQuality(PreviewQuality::Scale::Full);

    EXPECT_EQ(copied.quality(), PreviewQuality::Scale::Quarter);
    EXPECT_EQ(copied.calculatePreviewSize(QSize{800, 600}), (QSize{200, 150}));

    PreviewQuality assigned;
    assigned = original;
    EXPECT_EQ(assigned.quality(), PreviewQuality::Scale::Full);
    EXPECT_EQ(assigned.calculatePreviewSize(QSize{800, 600}), (QSize{800, 600}));

    PreviewQuality moved;
    moved = std::move(copied);
    EXPECT_EQ(moved.quality(), PreviewQuality::Scale::Quarter);
    EXPECT_EQ(moved.calculatePreviewSize(QSize{800, 600}), (QSize{200, 150}));
}

TEST(PreviewSettingsLodContractTest,
     CopiesAndMovesPreserveAllPresetLODFeatures)
{
    PreviewQuality source;
    source.applyPreset(QStringLiteral("Draft"));
    PreviewQuality copied(source);
    source.applyPreset(QStringLiteral("High"));

    EXPECT_EQ(copied.currentPresetName(), QStringLiteral("Draft"));
    EXPECT_EQ(copied.quality(), PreviewQuality::Scale::Eighth);
    EXPECT_EQ(copied.renderMode(), PreviewQuality::RenderMode::Draft);
    EXPECT_EQ(copied.textureQuality(), PreviewQuality::TextureQuality::Low);
    EXPECT_EQ(copied.antiAliasing(), PreviewQuality::AntiAliasing::None);
    EXPECT_FALSE(copied.isMotionBlurEnabled());
    EXPECT_FALSE(copied.isDepthOfFieldEnabled());
    EXPECT_FALSE(copied.areShadowsEnabled());

    PreviewQuality moved;
    moved = std::move(copied);
    EXPECT_EQ(moved.currentPresetName(), QStringLiteral("Draft"));
    EXPECT_EQ(moved.quality(), PreviewQuality::Scale::Eighth);
    EXPECT_EQ(moved.renderMode(), PreviewQuality::RenderMode::Draft);
    EXPECT_EQ(moved.textureQuality(), PreviewQuality::TextureQuality::Low);
    EXPECT_EQ(moved.antiAliasing(), PreviewQuality::AntiAliasing::None);
    EXPECT_FALSE(moved.isMotionBlurEnabled());
    EXPECT_FALSE(moved.isDepthOfFieldEnabled());
    EXPECT_FALSE(moved.areShadowsEnabled());
}
