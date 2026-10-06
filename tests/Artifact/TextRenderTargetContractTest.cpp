#include <gtest/gtest.h>

#include <QColor>
#include <QImage>
#include <QSize>

import Artifact.Render.TextGpuDevice;
import Artifact.Render.TextRenderTarget;

TEST(TextRenderTargetContractTest, RejectsNullDeviceAndNonPositiveDimensions)
{
    Artifact::ArtifactTextRenderTarget target;

    EXPECT_FALSE(target.create(nullptr, 8, 8));
    EXPECT_FALSE(target.isValid());
    EXPECT_FALSE(target.create(nullptr, 0, 8));
    EXPECT_FALSE(target.create(nullptr, 8, 0));
    EXPECT_EQ(target.width(), 0);
    EXPECT_EQ(target.height(), 0);
}

TEST(TextRenderTargetContractTest, ReadbackRejectsMissingContextAndClearsOutput)
{
    Artifact::ArtifactTextRenderTarget target;
    QImage output(2, 2, QImage::Format_RGBA8888);

    EXPECT_FALSE(target.readback(nullptr, output));
    EXPECT_TRUE(output.isNull());
    EXPECT_FALSE(target.readback(nullptr, output));
    EXPECT_TRUE(output.isNull());
}

TEST(TextRenderTargetContractTest, D3D12ClearAndReadbackPreserveEveryPixel)
{
    Artifact::ArtifactTextGpuDevice gpu;
    if (!gpu.initialize()) {
        GTEST_SKIP() << "Diligent D3D12 headless device is unavailable on this host";
        return;
    }
    Artifact::ArtifactTextRenderTarget target;
    ASSERT_TRUE(target.create(gpu.device(), 7, 5));
    ASSERT_TRUE(target.isValid());
    EXPECT_EQ(target.width(), 7);
    EXPECT_EQ(target.height(), 5);

    target.clear(gpu.context(), 1.0f, 0.0f, 1.0f, 0.5f);
    QImage output;
    ASSERT_TRUE(target.readback(gpu.context(), output));
    ASSERT_FALSE(output.isNull());
    ASSERT_EQ(output.size(), QSize(7, 5));
    for (int y = 0; y < output.height(); ++y) {
        for (int x = 0; x < output.width(); ++x) {
            const QColor pixel = output.pixelColor(x, y);
            EXPECT_EQ(pixel.red(), 255) << "at " << x << ',' << y;
            EXPECT_EQ(pixel.green(), 0) << "at " << x << ',' << y;
            EXPECT_EQ(pixel.blue(), 255) << "at " << x << ',' << y;
            EXPECT_NEAR(pixel.alpha(), 128, 1) << "at " << x << ',' << y;
        }
    }
}

TEST(TextRenderTargetContractTest, RecreateResizesTargetAndTransparentClearRemovesOldPixels)
{
    Artifact::ArtifactTextGpuDevice gpu;
    if (!gpu.initialize()) {
        GTEST_SKIP() << "Diligent D3D12 headless device is unavailable on this host";
        return;
    }
    Artifact::ArtifactTextRenderTarget target;
    ASSERT_TRUE(target.create(gpu.device(), 3, 2));
    target.clear(gpu.context(), 1.0f, 1.0f, 1.0f, 1.0f);

    ASSERT_TRUE(target.create(gpu.device(), 5, 4));
    ASSERT_EQ(target.width(), 5);
    ASSERT_EQ(target.height(), 4);
    target.clear(gpu.context(), 0.0f, 0.0f, 0.0f, 0.0f);
    QImage output;
    ASSERT_TRUE(target.readback(gpu.context(), output));
    ASSERT_EQ(output.size(), QSize(5, 4));
    for (int y = 0; y < output.height(); ++y) {
        for (int x = 0; x < output.width(); ++x) {
            EXPECT_EQ(output.pixelColor(x, y), QColor(0, 0, 0, 0))
                << "at " << x << ',' << y;
        }
    }
}
