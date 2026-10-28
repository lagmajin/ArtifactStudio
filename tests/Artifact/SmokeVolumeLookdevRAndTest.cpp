#include <gtest/gtest.h>

#include <QDir>
#include <QImage>
#include <QMatrix4x4>
#include <QRectF>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include "DiligentHeadlessRenderer.hpp"

import Color.Float;
import Image.ImageF32x4_RGBA;
import Artifact.Render.IRenderer;
import Physics.Fluid;

TEST(ArtifactSmokeVolumeLookdevRAndTest,
     DensityRampAndShadowAblationPreservesTheSameVolume)
{
    constexpr int outputWidth = 768;
    constexpr int outputHeight = 432;
    constexpr int fieldWidth = 128;
    constexpr int fieldHeight = 128;
    bool frameOk = false;
    const int requestedFrame = qEnvironmentVariableIntValue(
        "ARTIFACT_SMOKE_LOOKDEV_FRAME_RND", &frameOk);
    const std::int64_t frame = frameOk
        ? std::clamp(requestedFrame, 1, 180) : 60;
    QString outputDirectory = qEnvironmentVariable(
        "ARTIFACT_SMOKE_LOOK_AB_OUTPUT_DIR_RND");
    if (outputDirectory.isEmpty())
        outputDirectory = QDir::current().filePath(
            QStringLiteral("smoke_volume_lookdev_captures"));
    ASSERT_TRUE(QDir().mkpath(outputDirectory));

    ArtifactTest::HeadlessRenderer renderer;
    if (!renderer.initialize(outputWidth, outputHeight))
        GTEST_SKIP() << "Diligent headless renderer is unavailable";
    renderer.setClearColor(ArtifactCore::FloatColor(0.0f, 0.0f, 0.0f, 0.0f));

    bool curlStrengthOk = false;
    const float requestedCurlStrength = qEnvironmentVariable(
        "ARTIFACT_SMOKE_LOOKDEV_CURL_STRENGTH_RND")
        .toFloat(&curlStrengthOk);
    const float curlStrength = curlStrengthOk
        ? std::clamp(requestedCurlStrength, 0.0f, 2.0f) : 0.0f;
    bool curlOctavesOk = false;
    const int requestedCurlOctaves = qEnvironmentVariableIntValue(
        "ARTIFACT_SMOKE_LOOKDEV_CURL_OCTAVES_RND", &curlOctavesOk);
    const int curlOctaves = curlOctavesOk
        ? std::clamp(requestedCurlOctaves, 1, 4) : 1;

    std::array<float, fieldWidth * fieldHeight> flame{};
    const std::array<float, fieldWidth * fieldHeight> noFlame{};
    std::array<float, fieldWidth * fieldHeight> flameScratch{};
    ArtifactCore::FluidSolver2D solver(fieldWidth, fieldHeight);
    solver.setViscosity(0.00001f);
    solver.setDiffusion(0.00001f);
    solver.setBuoyancy(0.05f);
    solver.setThermalBuoyancy(0.0f);
    solver.setTemperatureDissipationPerSecond(1.5f);
    solver.setVorticity(0.1f);
    solver.setSolverIterations(20);
    solver.setCurlNoise(curlStrength, 0.0f, 18.0f, 7.0f, curlOctaves);
    float maximumDensity = 0.0f;
    double densityMass = 0.0;
    constexpr int emitterCount = 7;
    constexpr float emitterRadius = 0.20f;
    constexpr float emitterDensity = 0.04f;
    constexpr float emitterY = 0.90f;
    constexpr float frameDt = 1.0f / 30.0f;
    const float sourceRadius = emitterRadius /
        std::sqrt(static_cast<float>(emitterCount));
    const float placementRadius = std::max(0.0f, emitterRadius - sourceRadius);
    constexpr float goldenAngle = 2.39996322972865332f;
    const auto simulationStart = std::chrono::steady_clock::now();
    for (int frameIndex = 0; frameIndex < frame; ++frameIndex) {
        const float elapsedSeconds = (frameIndex + 1) * frameDt;
        const float swayPhase = elapsedSeconds * (6.28318530718f * 0.22f);
        for (int source = 0; source < emitterCount; ++source) {
            const float radial = std::sqrt((source + 0.5f) / emitterCount);
            const float angle = source * goldenAngle;
            const float offset = placementRadius * radial;
            const ArtifactCore::SmokeEmitter2D emitter{
                0.5f + std::cos(angle) * offset,
                emitterY + std::sin(angle) * offset,
                sourceRadius, emitterDensity, std::sin(swayPhase) * 0.04f,
                -0.75f, 0.0f, 1.0f};
            ArtifactCore::injectSmokeEmitter2D(solver, emitter, 1.0f);
        }
        solver.update(frameDt);
        std::fill(flameScratch.begin(), flameScratch.end(), 0.0f);
        const float flameRetention = std::exp(-frameDt / 0.65f);
        for (int y = 1; y < fieldHeight - 1; ++y) {
            for (int x = 1; x < fieldWidth - 1; ++x) {
                const std::size_t index = static_cast<std::size_t>(
                    y * fieldWidth + x);
                float velocityX = 0.0f;
                float velocityY = 0.0f;
                solver.getVelocity(x, y, velocityX, velocityY);
                const float sourceX = std::clamp(
                    static_cast<float>(x) - velocityX * frameDt * fieldWidth,
                    0.0f, static_cast<float>(fieldWidth - 1));
                const float sourceY = std::clamp(
                    static_cast<float>(y) - velocityY * frameDt * fieldHeight,
                    0.0f, static_cast<float>(fieldHeight - 1));
                const float xBlend = sourceX - std::floor(sourceX);
                const float yBlend = sourceY - std::floor(sourceY);
                const int x0 = std::clamp(static_cast<int>(std::floor(sourceX)),
                                          0, fieldWidth - 1);
                const int y0 = std::clamp(static_cast<int>(std::floor(sourceY)),
                                          0, fieldHeight - 1);
                const int x1 = std::min(x0 + 1, fieldWidth - 1);
                const int y1 = std::min(y0 + 1, fieldHeight - 1);
                const auto sample = [&](int sx, int sy) {
                    return flame[static_cast<std::size_t>(sy * fieldWidth + sx)];
                };
                const float a = sample(x0, y0) * (1.0f - xBlend) +
                    sample(x1, y0) * xBlend;
                const float b = sample(x0, y1) * (1.0f - xBlend) +
                    sample(x1, y1) * xBlend;
                flameScratch[index] = std::max(0.0f,
                    (a * (1.0f - yBlend) + b * yBlend) * flameRetention);
            }
        }
        flame.swap(flameScratch);
        const float flameSource = std::clamp((1.0f - 0.35f) * 0.6f,
                                             0.0f, 1.0f);
        for (int source = 0; source < emitterCount; ++source) {
            const float radial = std::sqrt((source + 0.5f) / emitterCount);
            const float angle = source * goldenAngle;
            const float offset = placementRadius * radial;
            const float sourceX = 0.5f + std::cos(angle) * offset;
            const float sourceY = emitterY + std::sin(angle) * offset;
            const int centerX = 1 + static_cast<int>(std::round(
                sourceX * (fieldWidth - 3)));
            const int centerY = 1 + static_cast<int>(std::round(
                sourceY * (fieldHeight - 3)));
            const float radiusX = sourceRadius * (fieldWidth - 2);
            const float radiusY = sourceRadius * (fieldHeight - 2);
            for (int dy = -static_cast<int>(std::ceil(radiusY));
                 dy <= static_cast<int>(std::ceil(radiusY)); ++dy) {
                for (int dx = -static_cast<int>(std::ceil(radiusX));
                     dx <= static_cast<int>(std::ceil(radiusX)); ++dx) {
                    const int x = centerX + dx;
                    const int y = centerY + dy;
                    if (x <= 0 || x >= fieldWidth - 1 ||
                        y <= 0 || y >= fieldHeight - 1) continue;
                    const float nx = dx / std::max(0.5f, radiusX);
                    const float ny = dy / std::max(0.5f, radiusY);
                    const float value = flameSource *
                        std::clamp(1.0f - nx * nx - ny * ny, 0.0f, 1.0f);
                    const std::size_t index = static_cast<std::size_t>(
                        y * fieldWidth + x);
                    flame[index] = std::max(flame[index], value);
                }
            }
        }
        const int outflowRows = std::clamp(
            static_cast<int>(std::ceil(0.35f * fieldHeight)),
            1, fieldHeight / 2);
        for (int y = 1; y <= outflowRows; ++y) {
            const float heightWeight = 1.0f -
                static_cast<float>(y - 1) / outflowRows;
            const float retention = std::exp(-100.0f * heightWeight * frameDt);
            for (int x = 1; x < fieldWidth - 1; ++x) {
                const float value = solver.getDensity(x, y);
                if (value > 0.0f)
                    solver.addDensity(x, y, value * (retention - 1.0f));
            }
        }
        solver.scaleDensity(std::exp(-2.5f * frameDt));
    }
    const double simulationMs = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - simulationStart).count();
    const float* density = solver.densityFieldData();
    ASSERT_NE(density, nullptr);
    for (std::size_t index = 0; index < solver.fieldElementCount(); ++index) {
        maximumDensity = std::max(maximumDensity, density[index]);
        densityMass += std::max(0.0f, density[index]);
    }
    ASSERT_GT(maximumDensity, 0.0f);
    ASSERT_GT(densityMass, 0.0);
    const std::size_t flameActiveCells = static_cast<std::size_t>(
        std::count_if(flame.begin(), flame.end(),
                      [](float value) { return value > 0.035f; }));
    ASSERT_GT(flameActiveCells, 0u);

    Artifact::SmokeVolumeLook baseline;
    Artifact::SmokeVolumeLook ramp = baseline;
    ramp.denseColor = ArtifactCore::FloatColor(0.20f, 0.24f, 0.29f, 1.0f);
    ramp.densityRampStart = maximumDensity * 0.14f;
    ramp.densityRampEnd = maximumDensity * 0.58f;
    ramp.densityRampStrength = 1.0f;
    Artifact::SmokeVolumeLook shaded = ramp;
    shaded.scatteringBrightness = 1.15f;
    shaded.shadowDensity = 1.35f;
    Artifact::SmokeVolumeLook fineDetail = baseline;
    fineDetail.detailFrequency = 0.90f;
    fineDetail.detailContrast = 0.70f;
    const std::array<const char*, 4> names{
        "baseline", "density_ramp", "density_ramp_shadow", "fine_detail"};
    const std::array<Artifact::SmokeVolumeLook, 4> looks{
        baseline, ramp, shaded, fineDetail};
    std::array<ArtifactCore::ImageF32x4_RGBA, 4> images;
    std::array<double, 4> alphaMasses{};
    std::array<double, 4> meanLumaGradients{};
    QMatrix4x4 identity;
    identity.setToIdentity();

    ArtifactCore::ImageF32x4_RGBA flameOffImage;
    ArtifactCore::ImageF32x4_RGBA flameOnImage;
    for (int variant = 0; variant < 2; ++variant) {
        renderer.clear();
        const auto renderStart = std::chrono::steady_clock::now();
        ASSERT_TRUE(renderer.drawSmokeVolumeField(
            density, variant == 0 ? noFlame.data() : flame.data(),
            fieldWidth, fieldHeight,
            QRectF(0.0, 0.0, outputWidth, outputHeight), identity,
            ArtifactCore::FloatColor(0.42f, 0.72f, 1.0f, 1.0f), 1.0f,
            static_cast<float>(frame) / 30.0f, 0.9f, ramp));
        renderer.flushAndWait();
        auto& image = variant == 0 ? flameOffImage : flameOnImage;
        image = renderer.readbackToImageF32();
        const double renderMs = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - renderStart).count();
        const QString path = QDir(outputDirectory).filePath(
            QStringLiteral("smoke_flame_%1_frame%2.png")
                .arg(variant == 0 ? QStringLiteral("off") : QStringLiteral("on"))
                .arg(frame));
        ASSERT_TRUE(image.toQImage().save(path, "PNG")) << path.toStdString();
        GTEST_LOG_(INFO) << "flame=" << (variant ? "on" : "off")
            << " activeCells=" << (variant ? flameActiveCells : 0)
            << " gpuRenderMs=" << renderMs << " image=" << path.toStdString();
    }
    double flameOnWarmPixels = 0.0;
    for (int y = 0; y < outputHeight; ++y) {
        for (int x = 0; x < outputWidth; ++x) {
            const auto pixel = flameOnImage.getPixel(x, y);
            if (pixel.r() > pixel.b() * 1.15f && pixel.a() > 0.02f)
                flameOnWarmPixels += 1.0;
        }
    }
    EXPECT_GT(flameOnWarmPixels, 0.0);

    renderer.clear();
    ASSERT_TRUE(renderer.drawSmokeVolumeField(
        density, flame.data(), fieldWidth, fieldHeight,
        QRectF(0.0, 0.0, outputWidth, outputHeight), identity,
        ArtifactCore::FloatColor(0.42f, 0.72f, 1.0f, 1.0f), 1.0f,
        static_cast<float>(frame) / 30.0f, 0.9f, baseline));
    renderer.flushAndWait();
    (void)renderer.readbackToImageF32();

    for (std::size_t lookIndex = 0; lookIndex < looks.size(); ++lookIndex) {
        renderer.clear();
        const auto renderStart = std::chrono::steady_clock::now();
        ASSERT_TRUE(renderer.drawSmokeVolumeField(
            density, flame.data(), fieldWidth, fieldHeight,
            QRectF(0.0, 0.0, outputWidth, outputHeight), identity,
            ArtifactCore::FloatColor(0.42f, 0.72f, 1.0f, 1.0f), 1.0f,
            static_cast<float>(frame) / 30.0f, 0.9f, looks[lookIndex]));
        renderer.flushAndWait();
        images[lookIndex] = renderer.readbackToImageF32();
        const double renderMs = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - renderStart).count();
        ASSERT_EQ(images[lookIndex].width(), outputWidth);
        ASSERT_EQ(images[lookIndex].height(), outputHeight);
        const QString path = QDir(outputDirectory).filePath(
            QStringLiteral("smoke_%1_frame%2.png")
                .arg(QString::fromLatin1(names[lookIndex]))
                .arg(frame));
        ASSERT_TRUE(images[lookIndex].toQImage().save(path, "PNG"))
            << path.toStdString();

        double alphaMass = 0.0;
        double lumaMass = 0.0;
        double lumaGradient = 0.0;
        std::size_t gradientSamples = 0;
        for (int y = 0; y < outputHeight; ++y) {
            for (int x = 0; x < outputWidth; ++x) {
                const auto pixel = images[lookIndex].getPixel(x, y);
                const double luma = 0.2126 * pixel.r() +
                    0.7152 * pixel.g() + 0.0722 * pixel.b();
                alphaMass += pixel.a();
                lumaMass += luma * pixel.a();
                if (x + 1 < outputWidth) {
                    const auto right = images[lookIndex].getPixel(x + 1, y);
                    lumaGradient += std::abs(luma - (0.2126 * right.r() +
                        0.7152 * right.g() + 0.0722 * right.b()));
                    ++gradientSamples;
                }
            }
        }
        alphaMasses[lookIndex] = alphaMass;
        meanLumaGradients[lookIndex] = gradientSamples
            ? lumaGradient / gradientSamples : 0.0;
        GTEST_LOG_(INFO) << "look=" << names[lookIndex] << " frame=" << frame
            << " smokeCurl=" << curlStrength
            << " curlOctaves=" << curlOctaves
            << " densityMax=" << maximumDensity << " densityMass=" << densityMass
            << " simulationMs=" << simulationMs
            << " alphaMass=" << alphaMass << " alphaWeightedLuma=" << lumaMass
            << " meanHorizontalLumaGradient=" << meanLumaGradients[lookIndex]
            << " gpuRenderMs=" << renderMs << " image=" << path.toStdString();
    }
    if (frame == 60) {
        EXPECT_GT(meanLumaGradients[3], meanLumaGradients[0] * 1.15)
            << "Fine detail frequency should add measurable image structure";
        EXPECT_LE(alphaMasses[3], alphaMasses[0] * 1.02)
            << "Detail frequency should not materially inflate smoke opacity";
    }

    for (int y = 0; y < outputHeight; ++y) {
        for (int x = 0; x < outputWidth; ++x) {
            const float expectedAlpha = images[0].getPixel(x, y).a();
            for (std::size_t lookIndex = 1; lookIndex < 3; ++lookIndex)
                EXPECT_NEAR(images[lookIndex].getPixel(x, y).a(),
                            expectedAlpha, 1.0e-5f);
        }
    }
}
