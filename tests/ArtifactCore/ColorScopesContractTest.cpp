#include <gtest/gtest.h>

#include <QColor>
#include <QGuiApplication>
#include <QImage>
#include <QSize>

#include <cstdint>
#include <vector>

import Color.Grading.ColorScopes;

namespace {

QGuiApplication* ensureGuiApplication()
{
    static int argc = 1;
    static char appName[] = "ArtifactCoreColorScopesContractTest";
    static char* argv[] = {appName, nullptr};
    static QGuiApplication app(argc, argv);
    return &app;
}

void expectBackground(const QImage& image, int x, int y)
{
    const QColor pixel = image.pixelColor(x, y);
    EXPECT_EQ(pixel.red(), 0);
    EXPECT_EQ(pixel.green(), 0);
    EXPECT_EQ(pixel.blue(), 0);
    EXPECT_EQ(pixel.alpha(), 220);
}

} // namespace

TEST(ColorScopesContractTest, NullImageProducesSizedTransparentBackgrounds)
{
    const QImage nullImage;
    const QImage waveform = ArtifactCore::ColorScopeRenderer::renderWaveform(
        nullImage, 5, 7);
    const QImage vectorscope = ArtifactCore::ColorScopeRenderer::renderVectorscope(
        nullImage, 9);
    const QImage histogram = ArtifactCore::ColorScopeRenderer::renderHistogram(
        nullImage, 5, 7);

    ASSERT_EQ(waveform.size(), QSize(5, 7));
    ASSERT_EQ(vectorscope.size(), QSize(9, 9));
    ASSERT_EQ(histogram.size(), QSize(5, 7));
    EXPECT_EQ(waveform.format(), QImage::Format_RGBA8888);
    EXPECT_EQ(vectorscope.format(), QImage::Format_RGBA8888);
    EXPECT_EQ(histogram.format(), QImage::Format_RGBA8888);
    expectBackground(waveform, 2, 3);
    expectBackground(vectorscope, 1, 1);
    expectBackground(histogram, 2, 3);
}

TEST(ColorScopesContractTest, CpuWaveformAndHistogramReflectSourcePixels)
{
    QImage blackPixel(1, 1, QImage::Format_RGBA8888);
    blackPixel.fill(Qt::black);
    const QImage waveform = ArtifactCore::ColorScopeRenderer::renderWaveform(
        blackPixel, 4, 9);
    ASSERT_EQ(waveform.size(), QSize(4, 9));
    EXPECT_EQ(waveform.pixelColor(0, 8), QColor(255, 255, 255, 255));

    QImage blackAndWhite(2, 1, QImage::Format_RGBA8888);
    blackAndWhite.setPixelColor(0, 0, Qt::black);
    blackAndWhite.setPixelColor(1, 0, Qt::white);
    const QImage histogram = ArtifactCore::ColorScopeRenderer::renderHistogram(
        blackAndWhite, 256, 16);
    ASSERT_EQ(histogram.size(), QSize(256, 16));
    const QColor whiteBin = histogram.pixelColor(255, 14);
    EXPECT_GT(whiteBin.red(), 0);
    EXPECT_GT(whiteBin.green(), 0);
    EXPECT_GT(whiteBin.blue(), 0);
    EXPECT_GT(whiteBin.alpha(), 220);
}

TEST(ColorScopesContractTest, WaveformBinsNormalizeCountsAndPreserveEmptyBins)
{
    constexpr int width = 3;
    constexpr int height = 5;
    std::vector<std::uint32_t> bins(width * height, 0);
    bins[4 * width] = 4;
    bins[4 * width + 1] = 1;

    const QImage scope = ArtifactCore::ColorScopeRenderer::renderWaveformFromBins(
        bins.data(), width, height);
    ASSERT_EQ(scope.size(), QSize(width, height));
    EXPECT_EQ(scope.format(), QImage::Format_ARGB32_Premultiplied);

    const QColor peak = scope.pixelColor(0, 4);
    EXPECT_EQ(peak, QColor(255, 255, 255, 255));
    expectBackground(scope, 2, 4);
}

TEST(ColorScopesContractTest, VectorscopeBinsRenderPeakAtItsCoordinate)
{
    ensureGuiApplication();
    constexpr int size = 64;
    std::vector<std::uint32_t> bins(size * size, 0);
    bins[1 * size + 1] = 7;

    const QImage scope = ArtifactCore::ColorScopeRenderer::renderVectorscopeFromBins(
        bins.data(), size);
    ASSERT_EQ(scope.size(), QSize(size, size));
    EXPECT_EQ(scope.format(), QImage::Format_ARGB32_Premultiplied);
    EXPECT_EQ(scope.pixelColor(1, 1), QColor(255, 255, 255, 255));
}

TEST(ColorScopesContractTest, ParadeBinsStayInTheirChannelPanes)
{
    constexpr int width = 2;
    constexpr int height = 5;
    constexpr int paneSize = width * height;
    std::vector<std::uint32_t> bins(3 * paneSize, 0);
    bins[4 * width + 1] = 5;
    bins[paneSize + 4 * width + 1] = 5;
    bins[2 * paneSize + 4 * width + 1] = 5;

    const QImage scope = ArtifactCore::ColorScopeRenderer::renderParadeFromBins(
        bins.data(), width, height);
    ASSERT_EQ(scope.size(), QSize(width * 3, height));
    EXPECT_EQ(scope.format(), QImage::Format_ARGB32_Premultiplied);
    EXPECT_EQ(scope.pixelColor(1, 4), QColor(255, 80, 80, 255));
    EXPECT_EQ(scope.pixelColor(width + 1, 4), QColor(80, 255, 80, 255));
    EXPECT_EQ(scope.pixelColor(width * 2 + 1, 4), QColor(80, 80, 255, 255));
}

TEST(ColorScopesContractTest, NullBinBuffersReturnBackgroundWithoutReadingData)
{
    const QImage waveform = ArtifactCore::ColorScopeRenderer::renderWaveformFromBins(
        nullptr, 3, 5);
    const QImage vectorscope = ArtifactCore::ColorScopeRenderer::renderVectorscopeFromBins(
        nullptr, 9);
    const QImage parade = ArtifactCore::ColorScopeRenderer::renderParadeFromBins(
        nullptr, 3, 5);

    ASSERT_EQ(waveform.size(), QSize(3, 5));
    ASSERT_EQ(vectorscope.size(), QSize(9, 9));
    ASSERT_EQ(parade.size(), QSize(9, 5));
    expectBackground(waveform, 1, 4);
    expectBackground(vectorscope, 1, 1);
    expectBackground(parade, 0, 4);
}
