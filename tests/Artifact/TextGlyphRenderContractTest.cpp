#include <gtest/gtest.h>

#include <QByteArray>
#include <QColor>
#include <QGuiApplication>
#include <QImage>
#include <QPointF>
#include <QSize>
#include <QString>
#include <QtCore/qglobal.h>
#include <cstdint>
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
