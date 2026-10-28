#include <gtest/gtest.h>

#include <DeviceContext.h>
#include <RenderDevice.h>
#include <TextureView.h>
#include <QColor>
#include <QDir>
#include <QImage>
#include <QSize>
#include <QString>
#include <QtGlobal>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>

#include "DiligentHeadlessRenderer.hpp"

import Core.Simulation.Pyro;
import Render.VolumeRenderer;
import Artifact.Render.IRenderer;

TEST(PyroSmokeLookdevRAndTest,
     MatchedPhysicalSweepReportsFieldAndRenderedDistribution)
{
    using namespace ArtifactCore;
    using namespace ArtifactCore::RayTrace;

    struct Condition {
        const char* name;
        float sourceY;
        float buoyancyScale;
        float turbulenceStrength;
        float turbulenceScaleFraction;
        std::uint8_t turbulenceOctaves;
        int simulationFrames;
        int emissionFrames;
    };
    constexpr std::array<Condition, 10> conditions{{
        {"baseline", -0.30f, 1.0f, 0.0f, 0.36f, 1, 48, 24},
        {"low_buoyancy", -0.30f, 0.5f, 0.0f, 0.36f, 1, 48, 24},
        {"lower_source", -0.42f, 1.0f, 0.0f, 0.36f, 1, 48, 24},
        {"turbulence_single_standard", -0.30f, 1.0f, 3.0f, 0.36f, 1, 48, 24},
        {"turbulence_multi_fine", -0.30f, 1.0f, 3.0f, 0.24f, 2, 48, 24},
        {"turbulence_multi_standard", -0.30f, 1.0f, 3.0f, 0.36f, 2, 48, 24},
        {"turbulence_multi_coarse", -0.30f, 1.0f, 3.0f, 0.48f, 2, 48, 24},
        {"combined_balanced_turbulence", -0.42f, 0.5f, 3.0f, 0.36f, 2, 48, 24},
        // Match the profile's two-second source duration so the late frame
        // shows a sustained plume instead of a short-pulse remnant.
        {"sustained_baseline", -0.30f, 1.0f, 0.0f, 0.36f, 1, 120, 120},
        {"sustained_turbulence", -0.30f, 1.0f, 3.0f, 0.36f, 2, 120, 120}}};
    constexpr std::array<int, 2> gridSizes{24, 48};
    constexpr int outputSize = 384;
    const QString outputDirectory =
        qEnvironmentVariable("ARTIFACT_PYRO_LOOKDEV_OUTPUT_DIR");
    if (!outputDirectory.isEmpty())
        ASSERT_TRUE(QDir().mkpath(outputDirectory));

    ArtifactTest::HeadlessRenderer renderer;
    if (!renderer.initialize(outputSize, outputSize))
        GTEST_SKIP() << "Diligent headless renderer is unavailable";
    GpuVolumeRenderer volumeRenderer;
    ASSERT_TRUE(volumeRenderer.initialize(renderer.device(),
                                          renderer.immediateContext()));

    for (const int gridSize : gridSizes) {
        const int substepsPerFrame = gridSize / gridSizes.front();
        const double stepSeconds =
            1.0 / (60.0 * static_cast<double>(substepsPerFrame));
        const float voxelSize = 1.2f / static_cast<float>(gridSize);
        const PyroDomain domain{
            .min = {-0.6f, -0.6f, -0.6f},
            .max = {0.6f, 0.6f, 0.6f},
            .voxelSize = voxelSize,
            .fixedTimeStep = stepSeconds,
            .boundaryMode = PyroBoundaryMode::Open};
        const auto requestedResolution = domain.resolution();
        ASSERT_EQ(requestedResolution.width, gridSize);
        ASSERT_EQ(requestedResolution.height, gridSize);
        ASSERT_EQ(requestedResolution.depth, gridSize);

        for (const auto& condition : conditions) {
            const int totalSteps =
                condition.simulationFrames * substepsPerFrame;
            const int emissionSteps =
                condition.emissionFrames * substepsPerFrame;
            auto profile = makePyroEffectProfile(
                PyroEffectKind::Smoke, domain.resolution());
            profile.simulation.pressureIterations =
                gridSize == 24 ? 8.0f : 36.0f;
            profile.simulation.buoyancy *=
                static_cast<float>(gridSize) / 24.0f *
                condition.buoyancyScale;
            profile.simulation.smokeTurbulence =
                condition.turbulenceStrength *
                static_cast<float>(gridSize) / 24.0f;
            profile.simulation.smokeTurbulenceScale =
                static_cast<float>(gridSize) *
                condition.turbulenceScaleFraction;
            profile.simulation.smokeTurbulenceOctaves =
                condition.turbulenceOctaves;
            profile.source.position = {0.0f, condition.sourceY, 0.0f};

            PyroSimulation simulation(domain);
            simulation.setSettings(profile.simulation);
            simulation.setCacheInterval(0);
            auto source = profile.source;
            source.position = {0.0f, condition.sourceY, 0.0f};
            const auto simulationStart = std::chrono::steady_clock::now();
            for (int step = 0; step < totalSteps; ++step) {
                source.enabled = step < emissionSteps;
                simulation.setSources(
                    std::span<const PyroSourceState>(&source, 1));
                simulation.step(stepSeconds);
            }
            const double simulationMilliseconds =
                std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - simulationStart)
                    .count();

            const auto resolution = simulation.fields().resolution();
            const auto& fields = simulation.fields();
            const auto& density = fields.densityStorage();
            const auto& temperature = fields.temperatureStorage();
            const auto& reaction = fields.reactionStorage();
            ASSERT_EQ(density.size(), resolution.cellCount());
            double densityMass = 0.0;
            double densityMomentY = 0.0;
            std::array<double, 3> densityThirdMass{};
            std::array<double, 6> densityBoundaryMass{};
            double densityMinY01 = 1.0;
            double densityMaxY01 = 0.0;
            const int boundaryBand =
                std::max(1, resolution.width / 24);
            const auto velocity = fields.velocityView();
            double divergenceSquaredSum = 0.0;
            std::size_t divergenceSamples = 0;
            for (int z = 0; z < resolution.depth; ++z) {
                for (int y = 0; y < resolution.height; ++y) {
                    const std::size_t third = std::min<std::size_t>(
                        2, 3 * static_cast<std::size_t>(y) /
                            static_cast<std::size_t>(resolution.height));
                    for (int x = 0; x < resolution.width; ++x) {
                        const auto index = static_cast<std::size_t>(x) +
                            static_cast<std::size_t>(resolution.width) *
                            (static_cast<std::size_t>(y) +
                             static_cast<std::size_t>(resolution.height) *
                                 static_cast<std::size_t>(z));
                        const double cellMass =
                            static_cast<double>(
                                std::max(0.0f, density[index])) * voxelSize *
                            voxelSize * voxelSize;
                        densityMass += cellMass;
                        densityMomentY += cellMass * (y + 0.5);
                        densityThirdMass[third] += cellMass;
                        if (density[index] > 1.0e-4f) {
                            const double y01 =
                                (y + 0.5) / static_cast<double>(resolution.height);
                            densityMinY01 = std::min(densityMinY01, y01);
                            densityMaxY01 = std::max(densityMaxY01, y01);
                        }
                        if (x < boundaryBand) densityBoundaryMass[0] += cellMass;
                        if (x >= resolution.width - boundaryBand)
                            densityBoundaryMass[1] += cellMass;
                        if (y < boundaryBand) densityBoundaryMass[2] += cellMass;
                        if (y >= resolution.height - boundaryBand)
                            densityBoundaryMass[3] += cellMass;
                        if (z < boundaryBand) densityBoundaryMass[4] += cellMass;
                        if (z >= resolution.depth - boundaryBand)
                            densityBoundaryMass[5] += cellMass;
                        if (x > 0 && x + 1 < resolution.width &&
                            y > 0 && y + 1 < resolution.height &&
                            z > 0 && z + 1 < resolution.depth) {
                            const float divergence = fields.divergence(
                                velocity,
                                {static_cast<float>(x), static_cast<float>(y),
                                 static_cast<float>(z)},
                                domain.boundaryMode);
                            divergenceSquaredSum += divergence * divergence;
                            ++divergenceSamples;
                        }
                    }
                }
            }
            ASSERT_GT(densityMass, 0.0);
            ASSERT_GT(divergenceSamples, 0u);

            const VolumeResolution volumeResolution{
                resolution.width, resolution.height, resolution.depth};
            const VolumeFieldSet volumeFields{
                .density = {const_cast<float*>(density.data()),
                            volumeResolution},
                .temperature = {const_cast<float*>(temperature.data()),
                                volumeResolution},
                .reaction = {const_cast<float*>(reaction.data()),
                             volumeResolution}};
            GpuVolumeRenderSettings renderSettings;
            renderSettings.outputWidth = outputSize;
            renderSettings.outputHeight = outputSize;
            renderSettings.raySteps = gridSize * 2;
            renderSettings.densityScale = 1.0f;
            renderSettings.temperatureScale = 1.0f;
            renderSettings.opacity = 1.0f;
            const auto renderStart = std::chrono::steady_clock::now();
            ASSERT_TRUE(volumeRenderer.render(renderer.immediateContext(),
                volumeFields, TransferFunction::makeSmoke(), renderSettings));
            renderer.flushAndWait();
            const QImage image = renderer.readbackTextureViewToImage(
                volumeRenderer.outputSRV());
            const double renderMilliseconds =
                std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - renderStart)
                    .count();
            ASSERT_EQ(image.size(), QSize(outputSize, outputSize));

            double alphaMass = 0.0;
            double alphaMomentY = 0.0;
            std::array<double, 3> alphaThirdMass{};
            double alphaWeightedLumaMass = 0.0;
            double alphaWeightedLumaMomentY = 0.0;
            std::array<double, 3> alphaWeightedLumaThirdMass{};
            std::array<std::size_t, 3> visiblePixels{};
            double alphaGradientSquaredSum = 0.0;
            std::size_t alphaGradientSamples = 0;
            double alphaHighPassSquaredSum = 0.0;
            std::size_t alphaHighPassSamples = 0;
            for (int y = 0; y < image.height(); ++y) {
                const std::size_t third = std::min<std::size_t>(
                    2, 3 * static_cast<std::size_t>(y) /
                        static_cast<std::size_t>(image.height()));
                for (int x = 0; x < image.width(); ++x) {
                    const QColor pixel = image.pixelColor(x, y);
                    const double alpha = pixel.alphaF();
                    const double weightedLuma = alpha * (
                        0.2126 * pixel.redF() +
                        0.7152 * pixel.greenF() +
                        0.0722 * pixel.blueF());
                    alphaMass += alpha;
                    alphaMomentY += alpha * (image.height() - y - 0.5);
                    alphaThirdMass[third] += alpha;
                    alphaWeightedLumaMass += weightedLuma;
                    alphaWeightedLumaMomentY += weightedLuma *
                        (image.height() - y - 0.5);
                    alphaWeightedLumaThirdMass[third] += weightedLuma;
                    if (alpha > 0.04) ++visiblePixels[third];
                    if (x + 1 < image.width()) {
                        const double dx = image.pixelColor(x + 1, y).alphaF() -
                            alpha;
                        alphaGradientSquaredSum += dx * dx;
                        ++alphaGradientSamples;
                    }
                    if (y + 1 < image.height()) {
                        const double dy = image.pixelColor(x, y + 1).alphaF() -
                            alpha;
                        alphaGradientSquaredSum += dy * dy;
                        ++alphaGradientSamples;
                    }
                    if (x > 0 && x + 1 < image.width() &&
                        y > 0 && y + 1 < image.height()) {
                        const double laplacian = 4.0 * alpha -
                            image.pixelColor(x - 1, y).alphaF() -
                            image.pixelColor(x + 1, y).alphaF() -
                            image.pixelColor(x, y - 1).alphaF() -
                            image.pixelColor(x, y + 1).alphaF();
                        alphaHighPassSquaredSum += laplacian * laplacian;
                        ++alphaHighPassSamples;
                    }
                }
            }
            ASSERT_GT(alphaMass, 0.0);
            ASSERT_GT(alphaWeightedLumaMass, 0.0);
            GTEST_LOG_(INFO)
                << "Pyro lookdev grid=" << gridSize << '^' << 3
                << " actualGrid=" << resolution.width << 'x'
                << resolution.height << 'x' << resolution.depth
                << " condition=" << condition.name
                << " simulationFrames=" << condition.simulationFrames
                << " emissionFrames=" << condition.emissionFrames
                << " simulatedSeconds=" << totalSteps * stepSeconds
                << " buoyancy=" << profile.simulation.buoyancy
                << " sourceY=" << condition.sourceY
                << " pressureIterations="
                << profile.simulation.pressureIterations
                << " projectedDivergenceRms="
                << std::sqrt(divergenceSquaredSum /
                    static_cast<double>(divergenceSamples))
                << " smokeTurbulence="
                << profile.simulation.smokeTurbulence
                << " smokeTurbulenceScale="
                << profile.simulation.smokeTurbulenceScale
                << " smokeTurbulenceOctaves="
                << static_cast<int>(
                    profile.simulation.smokeTurbulenceOctaves)
                << " scaleFractionOfDomain="
                << condition.turbulenceScaleFraction
                << " densityCentroidY01="
                << densityMomentY / densityMass / resolution.height
                << " densityThirdShares(bottom/middle/top)="
                << densityThirdMass[0] / densityMass << '/'
                << densityThirdMass[1] / densityMass << '/'
                << densityThirdMass[2] / densityMass
                << " densityYBounds01=" << densityMinY01 << '/'
                << densityMaxY01
                << " boundaryDensityShares(xMin/xMax/yMin/yMax/zMin/zMax)="
                << densityBoundaryMass[0] / densityMass << '/'
                << densityBoundaryMass[1] / densityMass << '/'
                << densityBoundaryMass[2] / densityMass << '/'
                << densityBoundaryMass[3] / densityMass << '/'
                << densityBoundaryMass[4] / densityMass << '/'
                << densityBoundaryMass[5] / densityMass
                << " outputAlphaCentroidY01="
                << alphaMomentY / alphaMass / image.height()
                << " outputAlphaThirdShares(top/middle/bottom)="
                << alphaThirdMass[0] / alphaMass << '/'
                << alphaThirdMass[1] / alphaMass << '/'
                << alphaThirdMass[2] / alphaMass
                << " alphaWeightedLumaCentroidY01="
                << alphaWeightedLumaMomentY / alphaWeightedLumaMass /
                    image.height()
                << " alphaWeightedLumaThirdShares(top/middle/bottom)="
                << alphaWeightedLumaThirdMass[0] / alphaWeightedLumaMass
                << '/' << alphaWeightedLumaThirdMass[1] /
                    alphaWeightedLumaMass
                << '/' << alphaWeightedLumaThirdMass[2] /
                    alphaWeightedLumaMass
                << " visiblePixels(top/middle/bottom)="
                << visiblePixels[0] << '/' << visiblePixels[1] << '/'
                << visiblePixels[2]
                << " renderedAlphaGradientRms="
                << (alphaGradientSamples > 0
                    ? std::sqrt(alphaGradientSquaredSum /
                        static_cast<double>(alphaGradientSamples)) : 0.0)
                << " renderedAlphaHighPassRms="
                << (alphaHighPassSamples > 0
                    ? std::sqrt(alphaHighPassSquaredSum /
                        static_cast<double>(alphaHighPassSamples)) : 0.0)
                << " fieldMemoryMiB="
                << simulation.estimateMemory().totalBytes /
                    (1024.0 * 1024.0)
                << " simulationMs/renderFlushReadbackMs="
                << simulationMilliseconds << '/' << renderMilliseconds;
            if (!outputDirectory.isEmpty()) {
                const QString filename = QStringLiteral("pyro-%1-%2.png")
                    .arg(gridSize)
                    .arg(QString::fromLatin1(condition.name));
                ASSERT_TRUE(image.save(QDir(outputDirectory).filePath(filename),
                                       "PNG"));
            }
        }
    }
}
