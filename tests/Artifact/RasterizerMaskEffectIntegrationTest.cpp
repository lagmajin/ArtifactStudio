#include <gtest/gtest.h>

#include <array>
#include <cstddef>

#include <QCoreApplication>
#include <QPointF>
#include <QString>

import Artifact.Effect.Rasterizer.Blur;
import Artifact.Layer.Abstract;
import Artifact.Layer.Factory;
import Artifact.Layer.InitParams;
import Artifact.Mask.LayerMask;
import Artifact.Mask.Path;
import Artifact.Render.CompositionViewDrawing;
import Image.ImageF32x4_RGBA;
import Memory.SharedPtr;

namespace {

class RasterizerMaskEffectEnvironment final : public testing::Environment {
public:
    void SetUp() override
    {
        static int argc = 1;
        static char applicationName[] = "RasterizerMaskEffectIntegrationTest";
        static char* argv[] = {applicationName, nullptr};
        application_ = new QCoreApplication(argc, argv);
    }

    void TearDown() override
    {
        delete application_;
        application_ = nullptr;
    }

private:
    QCoreApplication* application_ = nullptr;
};

RasterizerMaskEffectEnvironment* const environment =
    new RasterizerMaskEffectEnvironment();
const bool environmentRegistered = [] {
    testing::AddGlobalTestEnvironment(environment);
    return true;
}();

} // namespace

TEST(RasterizerMaskEffectIntegrationTest, RasterizerBlurRunsAfterLayerMask)
{
    constexpr int width = 32;
    constexpr int height = 16;
    std::array<float, width * height * 4> pixels{};
    for (std::size_t i = 0; i < pixels.size(); i += 4) {
        pixels[i] = 0.4f;
        pixels[i + 1] = 0.4f;
        pixels[i + 2] = 0.4f;
        pixels[i + 3] = 1.0f;
    }

    Artifact::ArtifactSolidLayerInitParams params(QStringLiteral("MaskBlurFixture"));
    params.setWidth(width);
    params.setHeight(height);
    Artifact::ArtifactLayerFactory factory;
    const auto created = factory.createLayer(params);
    ASSERT_TRUE(created.success);
    ASSERT_NE(created.layer.get(), nullptr);

    Artifact::MaskPath path;
    path.setClosed(true);
    path.addVertex({QPointF(0.0, 0.0), QPointF(), QPointF()});
    path.addVertex({QPointF(16.0, 0.0), QPointF(), QPointF()});
    path.addVertex({QPointF(16.0, 16.0), QPointF(), QPointF()});
    path.addVertex({QPointF(0.0, 16.0), QPointF(), QPointF()});
    Artifact::LayerMask mask;
    mask.addMaskPath(path);
    created.layer->addMask(mask);

    auto blur = ArtifactCore::makeShared<Artifact::BlurEffect>();
    blur->setRadius(4.0f);
    blur->setStrength(1.0f);
    created.layer->addEffect(blur);

    ArtifactCore::ImageF32x4_RGBA surface;
    surface.setFromRGBA32F(pixels.data(), width, height);
    Artifact::applyRasterizerEffectsAndMasksToSurface(
        created.layer.get(), surface, Artifact::DetailLevel::High);

    const auto interior = surface.getPixel(8, 8);
    const auto boundary = surface.getPixel(17, 8);
    const auto outside = surface.getPixel(25, 8);
    EXPECT_GT(interior.a(), boundary.a());
    EXPECT_GT(boundary.a(), outside.a());
    EXPECT_GT(boundary.a(), 0.0f);
}
