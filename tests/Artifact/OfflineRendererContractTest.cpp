#include <gtest/gtest.h>

#include <QByteArray>
#include <QGuiApplication>

#include <algorithm>
#include <cmath>

import Artifact.Render.IRenderer;
import Color.Float;
import Image.ImageF32x4_RGBA;

namespace {

class OffscreenGuiApplication final : public testing::Environment {
public:
    void SetUp() override
    {
        qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
        static int argc = 1;
        static char applicationName[] = "ArtifactOfflineRendererContractTest";
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

} // namespace

TEST(OfflineRendererContractTest, ClearProducesTransparentReadback)
{
    constexpr int width = 16;
    constexpr int height = 12;
    Artifact::ArtifactIRenderer renderer;
    renderer.initializeHeadless(width, height);
    if (!renderer.isInitialized()) {
        GTEST_SKIP() << "Diligent headless GPU is unavailable on this host";
    }

    renderer.setClearColor(ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 0.0f));
    renderer.clear();
    renderer.flushAndWait();

    const auto image = renderer.readbackToImageF32();
    ASSERT_EQ(image.width(), width);
    ASSERT_EQ(image.height(), height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const auto pixel = image.getPixel(x, y);
            EXPECT_FLOAT_EQ(pixel.r(), 0.0f) << "pixel=" << x << "," << y;
            EXPECT_FLOAT_EQ(pixel.g(), 0.0f) << "pixel=" << x << "," << y;
            EXPECT_FLOAT_EQ(pixel.b(), 0.0f) << "pixel=" << x << "," << y;
            EXPECT_FLOAT_EQ(pixel.a(), 0.0f) << "pixel=" << x << "," << y;
        }
    }

    renderer.destroy();
}

TEST(OfflineRendererContractTest, HeadlessRenderProducesRequestedPixels)
{
    constexpr int width = 32;
    constexpr int height = 24;
    Artifact::ArtifactIRenderer renderer;
    renderer.initializeHeadless(width, height);
    if (!renderer.isInitialized()) {
        GTEST_SKIP() << "Diligent headless GPU is unavailable on this host";
    }

    renderer.setClearColor(ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 0.0f));
    renderer.clear();
    renderer.drawSolidRect(8.0f, 6.0f, 16.0f, 12.0f,
                           ArtifactCore::FloatColor(0.9f, 0.1f, 0.05f, 1.0f));
    renderer.flushAndWait();

    const auto image = renderer.readbackToImageF32();
    ASSERT_EQ(image.width(), width);
    ASSERT_EQ(image.height(), height);

    int renderedPixelCount = 0;
    int minX = width;
    int minY = height;
    int maxX = -1;
    int maxY = -1;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const auto pixel = image.getPixel(x, y);
            if (std::isfinite(pixel.r()) && std::isfinite(pixel.g()) &&
                std::isfinite(pixel.b()) && std::isfinite(pixel.a()) &&
                pixel.r() > 0.5f && pixel.r() > pixel.g() * 2.0f &&
                pixel.a() > 0.5f) {
                ++renderedPixelCount;
                minX = std::min(minX, x);
                minY = std::min(minY, y);
                maxX = std::max(maxX, x);
                maxY = std::max(maxY, y);
            }
        }
    }
    EXPECT_GE(renderedPixelCount, 140)
        << "offline render/readback should contain the submitted rectangle";
    EXPECT_LE(renderedPixelCount, 240)
        << "submitted rectangle should not cover most of the render target";
    EXPECT_NEAR(minX, 8, 1);
    EXPECT_NEAR(minY, 6, 1);
    EXPECT_NEAR(maxX, 23, 1);
    EXPECT_NEAR(maxY, 17, 1);

    renderer.destroy();
}
