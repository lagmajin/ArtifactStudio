#include <gtest/gtest.h>

#include <QByteArray>
#include <QColor>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QPointF>
#include <QSize>
#include <QString>
#include <QtCore/qglobal.h>
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <vector>

#include "DiligentVulkanTestDevice.hpp"

import Artifact.Render.TextRenderTarget;
import Artifact.Render.TextGlyphShaderSources;
import Artifact.Render.TextGlyphSubmitter.Contract;
import Color.Float;
import Text.GlyphLayout;
import Text.ShapingBackend;
import Text.Style;
import Text.Animator;

namespace {

class OffscreenGuiApplication final : public testing::Environment {
public:
    void SetUp() override
    {
        qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
        static int argc = 1;
        static char applicationName[] = "ArtifactTextGlyphRenderContractTest";
        static char* argv[] = {applicationName, nullptr};
        application_ = new QGuiApplication(argc, argv);
    }

    void TearDown() override
    {
        delete application_;
        application_ = nullptr;
    }

private:
    QGuiApplication* application_ = nullptr;
};

testing::Environment* const guiApplication =
    testing::AddGlobalTestEnvironment(new OffscreenGuiApplication());

namespace {

constexpr int kBaselineChannelTolerance = 2;
constexpr double kBaselineMaxDifferingFraction = 0.01;
constexpr double kBaselineMaxMeanDifference = 0.5;

QString baselineDirectory()
{
    const QString fromEnvironment =
        qEnvironmentVariable("ARTIFACT_TEXT_ANIMATOR_BASELINE_DIR");
    if (!fromEnvironment.isEmpty()) {
        return fromEnvironment;
    }
#ifdef ARTIFACT_TEXT_ANIMATOR_BASELINE_DIR
    return QString::fromUtf8(ARTIFACT_TEXT_ANIMATOR_BASELINE_DIR);
#else
    return QString();
#endif
}

QString captureDirectory()
{
    const QString fromEnvironment =
        qEnvironmentVariable("ARTIFACT_TEXT_ANIMATOR_CAPTURE_DIR");
    if (!fromEnvironment.isEmpty()) {
        return fromEnvironment;
    }
    return QDir(QDir::tempPath())
        .filePath(QStringLiteral("artifact-text-animator"));
}

bool updateBaselinesRequested()
{
    return qEnvironmentVariable("ARTIFACT_TEXT_ANIMATOR_UPDATE_BASELINES") ==
           QStringLiteral("1");
}

struct ImageDiff {
    double meanAbsoluteDifference = 0.0;
    double differingFraction = 0.0;
    int maxChannelDifference = 0;
    bool sizeMismatch = false;
};

ImageDiff compareImages(const QImage& expected, const QImage& actual)
{
    ImageDiff diff;
    if (expected.size() != actual.size()) {
        diff.sizeMismatch = true;
        return diff;
    }

    long long differingPixels = 0;
    long long channelCount = 0;
    double absoluteSum = 0.0;
    for (int y = 0; y < expected.height(); ++y) {
        for (int x = 0; x < expected.width(); ++x) {
            const QColor left = expected.pixelColor(x, y);
            const QColor right = actual.pixelColor(x, y);
            const int channels[4] = {
                std::abs(left.red() - right.red()),
                std::abs(left.green() - right.green()),
                std::abs(left.blue() - right.blue()),
                std::abs(left.alpha() - right.alpha()),
            };
            int pixelMax = 0;
            for (const int channel : channels) {
                pixelMax = std::max(pixelMax, channel);
                absoluteSum += static_cast<double>(channel);
                ++channelCount;
            }
            diff.maxChannelDifference =
                std::max(diff.maxChannelDifference, pixelMax);
            if (pixelMax > kBaselineChannelTolerance) {
                ++differingPixels;
            }
        }
    }

    const long long pixelCount =
        static_cast<long long>(expected.width()) * expected.height();
    diff.meanAbsoluteDifference =
        channelCount > 0 ? absoluteSum / static_cast<double>(channelCount) : 0.0;
    diff.differingFraction =
        pixelCount > 0 ? static_cast<double>(differingPixels) /
                             static_cast<double>(pixelCount)
                       : 0.0;
    return diff;
}

void saveDiffArtifacts(const QString& name, const QImage& expected,
                       const QImage& actual)
{
    QDir().mkpath(captureDirectory());
    QDir directory(captureDirectory());
    actual.save(directory.filePath(name + QStringLiteral("_actual.png")));
    expected.save(directory.filePath(name + QStringLiteral("_expected.png")));

    QImage diff(expected.size(), QImage::Format_RGBA8888);
    for (int y = 0; y < expected.height(); ++y) {
        for (int x = 0; x < expected.width(); ++x) {
            const QColor left = expected.pixelColor(x, y);
            const QColor right = actual.pixelColor(x, y);
            const int maximum = std::max(
                {std::abs(left.red() - right.red()),
                 std::abs(left.green() - right.green()),
                 std::abs(left.blue() - right.blue()),
                 std::abs(left.alpha() - right.alpha())});
            diff.setPixelColor(
                x, y,
                maximum > kBaselineChannelTolerance
                    ? QColor(Qt::magenta)
                    : QColor(left.red() / 4, left.green() / 4,
                             left.blue() / 4, 255));
        }
    }
    diff.save(directory.filePath(name + QStringLiteral("_diff.png")));
}

enum class BaselineStatus {
    Compared,
    Written,
    Skipped,
};

struct BaselineOutcome {
    BaselineStatus status = BaselineStatus::Skipped;
    QString message;
    ImageDiff diff;
};

BaselineOutcome verifyTextAnimatorBaseline(const QString& name,
                                           const QImage& actual)
{
    BaselineOutcome outcome;
    const QString directory = baselineDirectory();
    if (directory.isEmpty()) {
        outcome.message =
            QStringLiteral("ARTIFACT_TEXT_ANIMATOR_BASELINE_DIR is not configured");
        return outcome;
    }

    QDir().mkpath(directory);
    const QString path =
        QDir(directory).filePath(name + QStringLiteral(".png"));
    if (updateBaselinesRequested()) {
        if (!actual.save(path)) {
            outcome.message =
                QStringLiteral("failed to save baseline ") + path;
            return outcome;
        }
        outcome.status = BaselineStatus::Written;
        return outcome;
    }
    if (!QFileInfo::exists(path)) {
        QDir().mkpath(captureDirectory());
        actual.save(QDir(captureDirectory())
                        .filePath(name + QStringLiteral("_actual.png")));
        outcome.message =
            QStringLiteral("baseline is missing: ") + path +
            QStringLiteral(
                "; set ARTIFACT_TEXT_ANIMATOR_UPDATE_BASELINES=1 to create it "
                "(current render saved to ") +
            captureDirectory() + QStringLiteral(")");
        return outcome;
    }

    const QImage expected =
        QImage(path).convertToFormat(QImage::Format_RGBA8888);
    outcome.diff = compareImages(expected, actual);
    if (outcome.diff.sizeMismatch ||
        outcome.diff.maxChannelDifference > kBaselineChannelTolerance ||
        outcome.diff.differingFraction > kBaselineMaxDifferingFraction ||
        outcome.diff.meanAbsoluteDifference > kBaselineMaxMeanDifference) {
        saveDiffArtifacts(name, expected, actual);
        outcome.message =
            QStringLiteral("image differs from baseline (capture dir: ") +
            captureDirectory() + QStringLiteral(")");
        return outcome;
    }
    outcome.status = BaselineStatus::Compared;
    return outcome;
}

} // namespace

class TextGlyphRenderContractTest : public testing::Test {
protected:
    void SetUp() override
    {
        if (!gpu_.initialize()) {
            GTEST_SKIP() << "Diligent Vulkan headless device is unavailable on this host";
        }

        ASSERT_TRUE(target_.create(gpu_.device(), 128, 96));
        ASSERT_TRUE(Artifact::createArtifactTextGlyphShaders(
            gpu_.device(), pixelShader_, vertexShader_, transformedVertexShader_));
        ASSERT_TRUE(Artifact::createArtifactTextGlyphPipelines(
            gpu_.device(), Diligent::TEX_FORMAT_RGBA8_UNORM,
            vertexShader_, pixelShader_, transformedVertexShader_,
            glyphPipeline_, glyphBinding_, transformedPipeline_,
            transformedBinding_, atlasSampler_));

        const Artifact::ArtifactTextGlyphPipelineProvider provider{
            glyphPipeline_.RawPtr(), glyphBinding_.RawPtr(),
            transformedPipeline_.RawPtr(), transformedBinding_.RawPtr(),
            atlasSampler_.RawPtr()};
        ASSERT_TRUE(submitter_.initialize(
            Diligent::RefCntAutoPtr<Diligent::IRenderDevice>(gpu_.device()),
            Diligent::TEX_FORMAT_RGBA8_UNORM, provider));
    }

    void TearDown() override
    {
        submitter_.destroy();
        target_.destroy();
        gpu_.destroy();
    }

    ArtifactCore::GlyphItem makeGlyph() const
    {
        ArtifactCore::GlyphItem glyph{};
        glyph.charCode = U'A';
        glyph.index = 0;
        glyph.clusterText = QStringLiteral("A");
        glyph.basePosition = QPointF(16.0, 64.0);
        return glyph;
    }

    QImage render(const std::vector<ArtifactCore::GlyphItem>& glyphs,
                  const ArtifactCore::TextStyle& style,
                  float opacity = 1.0f)
    {
        target_.clear(gpu_.context(), 0.0f, 0.0f, 0.0f, 0.0f);
        EXPECT_TRUE(submitter_.submit(
            gpu_.context(), target_.renderTargetView(), glyphs, style,
            ArtifactCore::FloatColor(1.0f, 0.0f, 0.0f, 1.0f), opacity));
        submitter_.flush(gpu_.context());
        gpu_.flushAndWait();
        QImage image;
        EXPECT_TRUE(target_.readback(gpu_.context(), image));
        return image;
    }

    QImage render(const ArtifactCore::GlyphItem& glyph, float opacity = 1.0f)
    {
        ArtifactCore::TextStyle style;
        style.fontSize = 32.0f;
        style.pixelSize = 32.0f;
        const std::vector<ArtifactCore::GlyphItem> glyphs{glyph};
        return render(glyphs, style, opacity);
    }

    static int countVisiblePixels(const QImage& image)
    {
        int count = 0;
        for (int y = 0; y < image.height(); ++y) {
            for (int x = 0; x < image.width(); ++x) {
                if (image.pixelColor(x, y).alpha() != 0) {
                    ++count;
                }
            }
        }
        return count;
    }

    static std::uint64_t sumAlpha(const QImage& image)
    {
        std::uint64_t sum = 0;
        for (int y = 0; y < image.height(); ++y) {
            for (int x = 0; x < image.width(); ++x) {
                sum += static_cast<std::uint64_t>(image.pixelColor(x, y).alpha());
            }
        }
        return sum;
    }

    static double alphaWeightedCentroidX(const QImage& image)
    {
        double weightedX = 0.0;
        std::uint64_t totalAlpha = 0;
        for (int y = 0; y < image.height(); ++y) {
            for (int x = 0; x < image.width(); ++x) {
                const auto alpha = static_cast<std::uint64_t>(
                    image.pixelColor(x, y).alpha());
                weightedX += static_cast<double>(x) * alpha;
                totalAlpha += alpha;
            }
        }
        return totalAlpha > 0
                   ? weightedX / static_cast<double>(totalAlpha)
                   : 0.0;
    }

    ArtifactTest::VulkanGpuDevice gpu_;
    Artifact::ArtifactTextRenderTarget target_;
    Diligent::RefCntAutoPtr<Diligent::IShader> pixelShader_;
    Diligent::RefCntAutoPtr<Diligent::IShader> vertexShader_;
    Diligent::RefCntAutoPtr<Diligent::IShader> transformedVertexShader_;
    Diligent::RefCntAutoPtr<Diligent::IPipelineState> glyphPipeline_;
    Diligent::RefCntAutoPtr<Diligent::IPipelineState> transformedPipeline_;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> glyphBinding_;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> transformedBinding_;
    Diligent::RefCntAutoPtr<Diligent::ISampler> atlasSampler_;
    Artifact::ArtifactTextGlyphSubmitter submitter_;
};

} // namespace

TEST_F(TextGlyphRenderContractTest, RasterizesOpaqueGlyphPixelsWithRequestedColor)
{
    const QImage image = render(makeGlyph());

    ASSERT_EQ(image.size(), QSize(128, 96));
    const int visiblePixels = countVisiblePixels(image);
    ASSERT_GT(visiblePixels, 0);
    int redPixels = 0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const QColor pixel = image.pixelColor(x, y);
            if (pixel.alpha() == 0) {
                continue;
            }
            EXPECT_GT(pixel.red(), pixel.green());
            EXPECT_GT(pixel.red(), pixel.blue());
            ++redPixels;
        }
    }
    EXPECT_EQ(redPixels, visiblePixels);
}

TEST_F(TextGlyphRenderContractTest, IdenticalGlyphSubmissionProducesIdenticalPixels)
{
    const ArtifactCore::GlyphItem glyph = makeGlyph();
    const QImage first = render(glyph);
    const QImage second = render(glyph);

    ASSERT_GT(countVisiblePixels(first), 0);
    EXPECT_EQ(first, second);
}

TEST_F(TextGlyphRenderContractTest, ZeroOpacityLeavesTransparentTarget)
{
    const QImage image = render(makeGlyph(), 0.0f);

    ASSERT_FALSE(image.isNull());
    EXPECT_EQ(countVisiblePixels(image), 0);
}

TEST_F(TextGlyphRenderContractTest, NonFiniteGlyphTransformIsRejectedWithoutPixels)
{
    ArtifactCore::GlyphItem glyph = makeGlyph();
    glyph.offsetPosition.setX(std::numeric_limits<double>::quiet_NaN());
    const std::vector<ArtifactCore::GlyphItem> glyphs{glyph};
    const ArtifactCore::TextStyle style;
    target_.clear(gpu_.context(), 0.0f, 0.0f, 0.0f, 0.0f);

    EXPECT_FALSE(submitter_.submit(
        gpu_.context(), target_.renderTargetView(), glyphs, style,
        ArtifactCore::FloatColor(1.0f, 1.0f, 1.0f, 1.0f)));
    gpu_.flushAndWait();
    QImage image;
    ASSERT_TRUE(target_.readback(gpu_.context(), image));
    EXPECT_EQ(countVisiblePixels(image), 0);
}

TEST_F(TextGlyphRenderContractTest, TextAnimatorTransformIsRasterizedByGpuSubmitter)
{
    const ArtifactCore::GlyphItem baselineGlyph = makeGlyph();
    const QImage baseline = render(baselineGlyph);

    std::vector<ArtifactCore::GlyphItem> animatedGlyphs{baselineGlyph};
    ArtifactCore::AnimatorSelectorSet animator;
    animator.range.units = ArtifactCore::SelectorUnits::Index;
    animator.range.start = 0.0f;
    animator.range.end = 0.0f;
    animator.range.shape = ArtifactCore::SelectorShape::Square;
    animator.properties.position = QPointF(12.0, -4.0);
    animator.properties.scale = 0.8f;
    animator.properties.rotation = 24.0f;
    animator.properties.opacity = 0.65f;
    const std::vector<ArtifactCore::AnimatorSelectorSet> animators{animator};
    ArtifactCore::TextAnimatorEngine::applyAnimatorSets(
        animatedGlyphs, animators, 0.0f, QStringLiteral("A"));

    ASSERT_EQ(animatedGlyphs.size(), 1u);
    EXPECT_FLOAT_EQ(animatedGlyphs[0].offsetPosition.x(), 12.0f);
    EXPECT_FLOAT_EQ(animatedGlyphs[0].offsetPosition.y(), -4.0f);
    EXPECT_FLOAT_EQ(animatedGlyphs[0].offsetRotation, 24.0f);
    EXPECT_FLOAT_EQ(animatedGlyphs[0].offsetScale, 0.8f);
    EXPECT_FLOAT_EQ(animatedGlyphs[0].offsetOpacity, 0.65f);
    const QImage animated = render(animatedGlyphs[0]);

    ASSERT_GT(countVisiblePixels(baseline), 0);
    ASSERT_GT(countVisiblePixels(animated), 0);
    EXPECT_LT(sumAlpha(animated), sumAlpha(baseline));
    EXPECT_NE(baseline, animated);
}

TEST_F(TextGlyphRenderContractTest, ShapedTextAnimatorChangesRenderedImage)
{
    ArtifactCore::TextStyle style;
    style.fontSize = 32.0f;
    style.pixelSize = 32.0f;
    ArtifactCore::TextShapingRequest request;
    request.text = QStringLiteral("AB");
    request.style = style;
    ArtifactCore::QtShapingBackend backend;
    const auto shaped = backend.shape(request);
    ASSERT_EQ(shaped.glyphs.size(), 2u);

    const QImage baseline = render(shaped.glyphs, style);
    auto animatedGlyphs = shaped.glyphs;
    ArtifactCore::AnimatorSelectorSet animator;
    animator.range.units = ArtifactCore::SelectorUnits::Index;
    animator.range.start = 0.0f;
    animator.range.end = 0.0f;
    animator.range.shape = ArtifactCore::SelectorShape::Square;
    animator.properties.position = QPointF(18.0, 0.0);
    const std::vector<ArtifactCore::AnimatorSelectorSet> animators{animator};
    ArtifactCore::TextAnimatorEngine::applyAnimatorSets(
        animatedGlyphs, animators, 0.0f, request.text);
    ASSERT_EQ(animatedGlyphs.size(), 2u);
    EXPECT_EQ(animatedGlyphs[0].offsetPosition, QPointF(18.0, 0.0));
    EXPECT_EQ(animatedGlyphs[1].offsetPosition, QPointF(0.0, 0.0));
    const QImage animated = render(animatedGlyphs, style);

    ASSERT_GT(countVisiblePixels(baseline), 0);
    ASSERT_GT(countVisiblePixels(animated), 0);
    EXPECT_NE(baseline, animated);
    EXPECT_GT(alphaWeightedCentroidX(animated),
             alphaWeightedCentroidX(baseline) + 2.0);
}

TEST_F(TextGlyphRenderContractTest, StackedAnimatorsAreAppliedBeforeGpuRasterization)
{
    ArtifactCore::TextStyle style;
    style.fontSize = 32.0f;
    style.pixelSize = 32.0f;
    ArtifactCore::TextShapingRequest request;
    request.text = QStringLiteral("AB");
    request.style = style;
    ArtifactCore::QtShapingBackend backend;
    const auto shaped = backend.shape(request);
    ASSERT_EQ(shaped.glyphs.size(), 2u);

    const QImage baseline = render(shaped.glyphs, style);
    auto animatedGlyphs = shaped.glyphs;
    ArtifactCore::AnimatorSelectorSet positionAnimator;
    positionAnimator.range.units = ArtifactCore::SelectorUnits::Index;
    positionAnimator.range.start = 0.0f;
    positionAnimator.range.end = 0.0f;
    positionAnimator.range.shape = ArtifactCore::SelectorShape::Square;
    positionAnimator.properties.position = QPointF(18.0, 0.0);

    ArtifactCore::AnimatorSelectorSet opacityAnimator;
    opacityAnimator.range = positionAnimator.range;
    opacityAnimator.properties.opacity = 0.5f;

    const std::vector<ArtifactCore::AnimatorSelectorSet> animators{
        positionAnimator, opacityAnimator};
    ArtifactCore::TextAnimatorEngine::applyAnimatorSets(
        animatedGlyphs, animators, 0.0f, request.text);

    ASSERT_EQ(animatedGlyphs.size(), 2u);
    EXPECT_EQ(animatedGlyphs[0].offsetPosition, QPointF(18.0, 0.0));
    EXPECT_FLOAT_EQ(animatedGlyphs[0].offsetOpacity, 0.5f);
    EXPECT_EQ(animatedGlyphs[1].offsetPosition, QPointF(0.0, 0.0));
    EXPECT_FLOAT_EQ(animatedGlyphs[1].offsetOpacity, 1.0f);

    const QImage stacked = render(animatedGlyphs, style);
    ASSERT_GT(countVisiblePixels(baseline), 0);
    ASSERT_GT(countVisiblePixels(stacked), 0);
    EXPECT_LT(sumAlpha(stacked), sumAlpha(baseline));
    EXPECT_NE(baseline, stacked);
}

TEST_F(TextGlyphRenderContractTest, SingleAnimatorTransformMatchesBaseline)
{
    std::vector<ArtifactCore::GlyphItem> glyphs{makeGlyph()};
    ArtifactCore::AnimatorSelectorSet animator;
    animator.range.units = ArtifactCore::SelectorUnits::Index;
    animator.range.start = 0.0f;
    animator.range.end = 0.0f;
    animator.range.shape = ArtifactCore::SelectorShape::Square;
    animator.properties.position = QPointF(12.0, -4.0);
    animator.properties.scale = 0.8f;
    animator.properties.rotation = 24.0f;
    animator.properties.opacity = 0.65f;
    const std::vector<ArtifactCore::AnimatorSelectorSet> animators{animator};
    ArtifactCore::TextAnimatorEngine::applyAnimatorSets(
        glyphs, animators, 0.0f, QStringLiteral("A"));

    const QImage image = render(glyphs[0]);
    ASSERT_GT(countVisiblePixels(image), 0);

    const BaselineOutcome outcome = verifyTextAnimatorBaseline(
        QStringLiteral("animator_single_transform"), image);
    if (outcome.status == BaselineStatus::Skipped) {
        GTEST_SKIP() << outcome.message.toStdString();
    }
    EXPECT_NE(outcome.status, BaselineStatus::Skipped)
        << outcome.message.toStdString();
}

TEST_F(TextGlyphRenderContractTest, StackedAnimatorsOnShapedTextMatchBaseline)
{
    ArtifactCore::TextStyle style;
    style.fontSize = 32.0f;
    style.pixelSize = 32.0f;
    ArtifactCore::TextShapingRequest request;
    request.text = QStringLiteral("AB");
    request.style = style;
    ArtifactCore::QtShapingBackend backend;
    const auto shaped = backend.shape(request);
    ASSERT_EQ(shaped.glyphs.size(), 2u);

    auto animatedGlyphs = shaped.glyphs;
    ArtifactCore::AnimatorSelectorSet positionAnimator;
    positionAnimator.range.units = ArtifactCore::SelectorUnits::Index;
    positionAnimator.range.start = 0.0f;
    positionAnimator.range.end = 0.0f;
    positionAnimator.range.shape = ArtifactCore::SelectorShape::Square;
    positionAnimator.properties.position = QPointF(10.0, -6.0);

    ArtifactCore::AnimatorSelectorSet rotationAnimator;
    rotationAnimator.range = positionAnimator.range;
    rotationAnimator.range.start = 1.0f;
    rotationAnimator.range.end = 1.0f;
    rotationAnimator.properties.rotation = -12.0f;
    rotationAnimator.properties.opacity = 0.6f;

    const std::vector<ArtifactCore::AnimatorSelectorSet> animators{
        positionAnimator, rotationAnimator};
    ArtifactCore::TextAnimatorEngine::applyAnimatorSets(
        animatedGlyphs, animators, 0.0f, request.text);
    ASSERT_EQ(animatedGlyphs.size(), 2u);

    const QImage image = render(animatedGlyphs, style);
    ASSERT_GT(countVisiblePixels(image), 0);

    const BaselineOutcome outcome = verifyTextAnimatorBaseline(
        QStringLiteral("animator_stacked_shaped_ab"), image);
    if (outcome.status == BaselineStatus::Skipped) {
        GTEST_SKIP() << outcome.message.toStdString();
    }
    EXPECT_NE(outcome.status, BaselineStatus::Skipped)
        << outcome.message.toStdString();
}

TEST_F(TextGlyphRenderContractTest, IntenseStackedAnimationFramesStayDeterministic)
{
    std::vector<ArtifactCore::GlyphItem> glyphs(3);
    for (int index = 0; index < 3; ++index) {
        glyphs[static_cast<size_t>(index)] = makeGlyph();
        glyphs[static_cast<size_t>(index)].charCode =
            static_cast<char32_t>(U'A' + index);
        glyphs[static_cast<size_t>(index)].index = index;
        glyphs[static_cast<size_t>(index)].clusterIndex = index;
        glyphs[static_cast<size_t>(index)].clusterText = QStringLiteral("A");
        glyphs[static_cast<size_t>(index)].basePosition =
            QPointF(16.0 + 32.0 * index, 64.0);
    }

    ArtifactCore::AnimatorSelectorSet positionSet;
    positionSet.range.units = ArtifactCore::SelectorUnits::Index;
    positionSet.range.start = 0.0f;
    positionSet.range.end = 2.0f;
    positionSet.range.shape = ArtifactCore::SelectorShape::Square;
    positionSet.properties.position = QPointF(6.0, -4.0);

    ArtifactCore::AnimatorSelectorSet rotationSet;
    rotationSet.range.units = ArtifactCore::SelectorUnits::Index;
    rotationSet.range.start = 1.0f;
    rotationSet.range.end = 1.0f;
    rotationSet.range.shape = ArtifactCore::SelectorShape::Square;
    rotationSet.properties.rotation = 18.0f;

    ArtifactCore::AnimatorSelectorSet opacitySet;
    opacitySet.range.shape = ArtifactCore::SelectorShape::Square;
    opacitySet.expression.enabled = true;
    opacitySet.expression.expression = QStringLiteral("textIndex / textTotal");
    opacitySet.properties.opacity = 0.3f;

    ArtifactCore::AnimatorSelectorSet wigglySet;
    wigglySet.range.shape = ArtifactCore::SelectorShape::Square;
    wigglySet.wiggly.enabled = true;
    wigglySet.wiggly.wigglesPerSecond = 4.0f;
    wigglySet.wiggly.correlation = 0.0f;
    wigglySet.wiggly.seed = 99;
    wigglySet.properties.position = QPointF(3.0, -2.0);

    const std::vector<ArtifactCore::AnimatorSelectorSet> sets{
        positionSet, rotationSet, opacitySet, wigglySet};

    ArtifactCore::TextStyle style;
    style.fontSize = 32.0f;
    style.pixelSize = 32.0f;

    std::vector<QImage> frames;
    for (const float time : {0.0f, 0.75f, 1.5f}) {
        auto animated = glyphs;
        ArtifactCore::TextAnimatorEngine::applyAnimatorSets(
            animated, sets, time, QStringLiteral("ABC"));
        ASSERT_EQ(animated.size(), glyphs.size());
        for (const auto& glyph : animated) {
            EXPECT_TRUE(std::isfinite(glyph.offsetPosition.x()));
            EXPECT_TRUE(std::isfinite(glyph.offsetPosition.y()));
            EXPECT_TRUE(std::isfinite(glyph.offsetRotation));
            EXPECT_TRUE(std::isfinite(glyph.offsetOpacity));
        }

        const QImage first = render(animated, style);
        const QImage repeat = render(animated, style);
        ASSERT_GT(countVisiblePixels(first), 0) << "time=" << time;
        EXPECT_EQ(first, repeat) << "time=" << time;
        frames.push_back(first);
    }

    ASSERT_EQ(frames.size(), 3u);
    EXPECT_NE(frames[0], frames[1]);
    EXPECT_NE(frames[1], frames[2]);
}

TEST_F(TextGlyphRenderContractTest,
       WigglyAnimatorTimeSampleIsDeterministicAndMatchesBaseline)
{
    std::vector<ArtifactCore::GlyphItem> glyphs(2);
    glyphs[0] = makeGlyph();
    glyphs[1] = makeGlyph();
    glyphs[1].charCode = U'B';
    glyphs[1].index = 1;
    glyphs[1].clusterIndex = 1;
    glyphs[1].clusterText = QStringLiteral("B");
    glyphs[1].basePosition = QPointF(48.0, 64.0);

    ArtifactCore::AnimatorSelectorSet animator;
    animator.range.shape = ArtifactCore::SelectorShape::Square;
    animator.wiggly.enabled = true;
    animator.wiggly.wigglesPerSecond = 2.0f;
    animator.wiggly.correlation = 0.0f;
    animator.wiggly.phase = 0.0f;
    animator.wiggly.seed = 12345;
    animator.properties.position = QPointF(10.0, -8.0);
    animator.properties.rotation = 20.0f;
    animator.properties.opacity = 0.8f;

    const std::vector<ArtifactCore::AnimatorSelectorSet> animators{animator};
    auto animatedGlyphs = glyphs;
    ArtifactCore::TextAnimatorEngine::applyAnimatorSets(
        animatedGlyphs, animators, 1.75f, QStringLiteral("AB"));
    ASSERT_EQ(animatedGlyphs.size(), 2u);

    ArtifactCore::TextStyle style;
    style.fontSize = 32.0f;
    style.pixelSize = 32.0f;
    const QImage first = render(animatedGlyphs, style);
    const QImage second = render(animatedGlyphs, style);
    ASSERT_GT(countVisiblePixels(first), 0);
    EXPECT_EQ(first, second);

    const BaselineOutcome outcome = verifyTextAnimatorBaseline(
        QStringLiteral("animator_wiggly_time_175"), first);
    if (outcome.status == BaselineStatus::Skipped) {
        GTEST_SKIP() << outcome.message.toStdString();
    }
    EXPECT_NE(outcome.status, BaselineStatus::Skipped)
        << outcome.message.toStdString();
}
