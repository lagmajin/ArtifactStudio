#include <gtest/gtest.h>

#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QMatrix4x4>
#include <QRectF>
#include <QSize>
#include <QString>
#include <array>
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cmath>
#include <cstdint>
#include <numeric>
#include <numbers>
#include <utility>
#include <vector>

import Artifact.Layer.FluidRuntimeState;
import Artifact.Render.IRenderer;
import Container.NamedVector;
import Core.Simulation.Pyro;
import Graphics.ParticleData;
import Render.VolumeRenderer;

namespace {
struct SmokeProjectionStageCapture final {
  int stage = 0;
  int width = 0;
  int height = 0;
  int solverIterations = 0;
  std::vector<float> density;
  std::vector<float> velocityX;
  std::vector<float> velocityY;
};

std::vector<SmokeProjectionStageCapture>* gSmokeProjectionCaptures = nullptr;
}

extern "C" void artifactSmokeRuntimeRndObserve(
    const int stage, const int width, const int height,
    const int solverIterations, const float* density,
    const float* velocityX, const float* velocityY) {
  if (!gSmokeProjectionCaptures || width <= 0 || height <= 0 ||
      !density || !velocityX || !velocityY) {
    return;
  }
  SmokeProjectionStageCapture capture;
  capture.stage = stage;
  capture.width = width;
  capture.height = height;
  capture.solverIterations = solverIterations;
  const auto count = static_cast<std::size_t>(width) *
                     static_cast<std::size_t>(height);
  capture.density.assign(density, density + count);
  capture.velocityX.assign(velocityX, velocityX + count);
  capture.velocityY.assign(velocityY, velocityY + count);
  gSmokeProjectionCaptures->push_back(std::move(capture));
}

namespace {

constexpr int kGridWidth = 96;
constexpr int kGridHeight = 72;
constexpr int kImageScale = 8;
constexpr int kFrameCount = 24;
constexpr int kSmokeRenderSamplesPerAxis = 64;
constexpr QRgb kBackground = qRgba(12, 16, 24, 255);

std::pair<int, int> expectedSmokeRenderSampleCounts(
    const int width, const int height) {
  const int safeWidth = std::max(1, width);
  const int safeHeight = std::max(1, height);
  const int longestAxis = std::max(safeWidth, safeHeight);
  if (longestAxis <= kSmokeRenderSamplesPerAxis)
    return {safeWidth, safeHeight};
  const float scale = static_cast<float>(kSmokeRenderSamplesPerAxis - 1) /
      static_cast<float>(longestAxis - 1);
  const int sampleCountX = safeWidth <= kSmokeRenderSamplesPerAxis
      ? safeWidth
      : std::clamp(static_cast<int>(std::round(
            static_cast<float>(safeWidth - 1) * scale)) + 1,
            1, kSmokeRenderSamplesPerAxis);
  const int sampleCountY = safeHeight <= kSmokeRenderSamplesPerAxis
      ? safeHeight
      : std::clamp(static_cast<int>(std::round(
            static_cast<float>(safeHeight - 1) * scale)) + 1,
            1, kSmokeRenderSamplesPerAxis);
  return {sampleCountX, sampleCountY};
}

float smokeValueNoise(float x, float y);

QImage rasterizeParticles(
    const ArtifactCore::ParticleRenderData& renderData,
    const float alphaScale = 1.0f) {
  QImage image(kGridWidth * kImageScale, kGridHeight * kImageScale,
               QImage::Format_ARGB32);
  image.fill(kBackground);

  for (const auto& particle : renderData.particles) {
    const int centerX = static_cast<int>(
        std::lround(particle.px * kImageScale));
    const int centerY = static_cast<int>(
        std::lround(particle.py * kImageScale));
    const int radius = std::max(
        1, static_cast<int>(std::lround(
               std::max(0.375f, particle.size * 10.0f) * kImageScale)));
    const float radiusSquared = static_cast<float>(radius * radius);
    const int minX = std::max(0, centerX - radius);
    const int maxX = std::min(image.width() - 1, centerX + radius);
    const int minY = std::max(0, centerY - radius);
    const int maxY = std::min(image.height() - 1, centerY + radius);

    for (int y = minY; y <= maxY; ++y) {
      auto* row = reinterpret_cast<QRgb*>(image.scanLine(y));
      for (int x = minX; x <= maxX; ++x) {
        const float dx = static_cast<float>(x - centerX);
        const float dy = static_cast<float>(y - centerY);
        const float distanceSquared = dx * dx + dy * dy;
        if (distanceSquared > radiusSquared) continue;

        const float normalizedDistance =
            std::sqrt(distanceSquared) / radius;
        const float edge = std::clamp(
            (normalizedDistance - 0.8f) / 0.2f, 0.0f, 1.0f);
        const float falloff = 1.0f - edge * edge * (3.0f - 2.0f * edge);
        const float opacity = std::clamp(
            particle.a * alphaScale * falloff, 0.0f, 1.0f);
        const QRgb destination = row[x];
        const float inverseOpacity = 1.0f - opacity;
        const int red = static_cast<int>(
            particle.r * 255.0f * opacity + qRed(destination) * inverseOpacity);
        const int green = static_cast<int>(
            particle.g * 255.0f * opacity + qGreen(destination) * inverseOpacity);
        const int blue = static_cast<int>(
            particle.b * 255.0f * opacity + qBlue(destination) * inverseOpacity);
        row[x] = qRgba(red, green, blue, 255);
      }
    }
  }
  return image;
}

QImage rasterizeSolverDensity(
    const std::vector<float>& field, const int fieldWidth,
    const int fieldHeight, const float extinction, const float lightX,
    const float lightY, const float shadowStrength,
    const float colorR, const float colorG, const float colorB,
    const float detailStrength = 0.0f) {
  constexpr int kWidth = kGridWidth * kImageScale;
  constexpr int kHeight = kGridHeight * kImageScale;
  const auto sample = [&](const float x, const float y) {
    const float clampedX = std::clamp(x, 0.0f,
                                      static_cast<float>(fieldWidth - 1));
    const float clampedY = std::clamp(y, 0.0f,
                                      static_cast<float>(fieldHeight - 1));
    const int x0 = static_cast<int>(clampedX);
    const int y0 = static_cast<int>(clampedY);
    const int x1 = std::min(x0 + 1, fieldWidth - 1);
    const int y1 = std::min(y0 + 1, fieldHeight - 1);
    const float tx = clampedX - static_cast<float>(x0);
    const float ty = clampedY - static_cast<float>(y0);
    const auto at = [&](const int px, const int py) {
      return field[static_cast<std::size_t>(py * fieldWidth + px)];
    };
    return std::lerp(std::lerp(at(x0, y0), at(x1, y0), tx),
                     std::lerp(at(x0, y1), at(x1, y1), tx), ty);
  };
  const float lightLength = std::max(0.001f, std::sqrt(lightX * lightX +
                                                       lightY * lightY));
  const float normalizedLightX = lightX / lightLength;
  const float normalizedLightY = lightY / lightLength;
  QImage image(kWidth, kHeight, QImage::Format_ARGB32);
  image.fill(kBackground);
  for (int y = 0; y < kHeight; ++y) {
    auto* row = reinterpret_cast<QRgb*>(image.scanLine(y));
    for (int x = 0; x < kWidth; ++x) {
      const float gridX = (static_cast<float>(x) + 0.5f) *
          static_cast<float>(fieldWidth - 1) / static_cast<float>(kWidth);
      const float gridY = (static_cast<float>(y) + 0.5f) *
          static_cast<float>(fieldHeight - 1) / static_cast<float>(kHeight);
      float sampleX = gridX;
      float sampleY = gridY;
      if (detailStrength > 0.0f) {
        const float broadWarpX = smokeValueNoise(
            gridX * 0.15f + 17.0f, gridY * 0.15f - 31.0f);
        const float broadWarpY = smokeValueNoise(
            gridX * 0.15f - 53.0f, gridY * 0.15f + 11.0f);
        const float fineWarpX = smokeValueNoise(
            gridX * 0.42f + 7.0f, gridY * 0.42f + 29.0f);
        const float fineWarpY = smokeValueNoise(
            gridX * 0.42f - 19.0f, gridY * 0.42f - 3.0f);
        sampleX += detailStrength * (2.8f * broadWarpX + 0.7f * fineWarpX);
        sampleY += detailStrength * (2.8f * broadWarpY + 0.7f * fineWarpY);
      }
      float rho = sample(sampleX, sampleY);
      if (rho < 0.002f) continue;
      if (detailStrength > 0.0f) {
        const float detail = 0.55f * smokeValueNoise(
            sampleX * 0.18f, sampleY * 0.18f) +
            0.30f * smokeValueNoise(sampleX * 0.36f, sampleY * 0.36f) +
            0.15f * smokeValueNoise(sampleX * 0.72f, sampleY * 0.72f);
        rho *= std::max(0.05f, 1.0f + detailStrength * 0.25f * detail);
      }
      float opticalDepth = 0.0f;
      constexpr int kShadowSteps = 10;
      constexpr float kShadowStep = 0.55f;
      for (int step = 1; step <= kShadowSteps; ++step) {
        const float distance = static_cast<float>(step) * kShadowStep;
        opticalDepth += sample(gridX + normalizedLightX * distance,
                               gridY + normalizedLightY * distance) *
                        kShadowStep;
      }
      const float alpha = 1.0f - std::exp(-rho * extinction);
      const float illumination = 0.42f + 0.72f *
          std::exp(-opticalDepth * shadowStrength);
      row[x] = qRgb(
          static_cast<int>(std::clamp(colorR * 255.0f * alpha * illumination,
                                      0.0f, 255.0f)),
          static_cast<int>(std::clamp(colorG * 255.0f * alpha * illumination,
                                      0.0f, 255.0f)),
          static_cast<int>(std::clamp(colorB * 255.0f * alpha * illumination,
                                      0.0f, 255.0f)));
    }
  }
  return image;
}

struct SmokeRun {
  QImage image;
  int particleCount = 0;
  int drawCount = 0;
  int reusableCapacity = 0;
  double meanParticleX = 0.0;
  double meanParticleY = 0.0;
  double meanParticleR = 0.0;
  double meanParticleG = 0.0;
  double meanParticleB = 0.0;
  double totalParticleAlpha = 0.0;
};

SmokeRun runProductionSmokeRuntime(const float emitterX,
                                   const bool stir = false,
                                   const bool preEvaluate = false,
                                   const double framesPerSecond = 30.0,
                                   const int frameCount = kFrameCount,
                                   const float vorticity = 0.1f,
                                   const float emitterSwirl = 0.15f,
                                   const int emitterCount = 1,
                                   const float alphaScale = 1.0f) {
  Artifact::LayerFluidRuntimeState state;
  Artifact::ArtifactIRenderer renderer;
  Artifact::LayerSmokeRuntimeSettings settings;
  settings.gridWidth = kGridWidth;
  settings.gridHeight = kGridHeight;
  settings.emitterX = emitterX;
  settings.emitterY = 0.84f;
  settings.emitterRadius = 0.08f;
  settings.emitterCount = emitterCount;
  settings.emitterDensity = 0.6f;
  settings.emitterVelocityY = -1.2f;
  settings.vorticity = vorticity;
  settings.emitterSwirl = emitterSwirl;
  settings.colorR = 0.12f;
  settings.colorG = 0.76f;
  settings.colorB = 0.28f;
  settings.stirActive = stir;
  settings.stirX = 0.62f;
  settings.stirY = 0.56f;
  settings.stirRadius = 0.16f;
  settings.stirVelocityX = -2.5f;
  settings.stirVelocityY = -0.8f;
  settings.stirStrength = 1.8f;

  QMatrix4x4 transform;
  transform.setToIdentity();
  for (int frame = 0; frame <= frameCount; ++frame) {
    settings.stirRevision = static_cast<std::uint64_t>(frame + 1);
    if (preEvaluate) {
      Artifact::advanceLayerSmokeRuntime(
          state, frame, framesPerSecond, settings);
    }
    Artifact::renderLayerSmokeRuntime(
        state, &renderer, QRectF(0.0, 0.0, kGridWidth, kGridHeight),
        transform, 1.0f, frame, framesPerSecond, settings);
  }

  const auto& data = renderer.lastParticleData();
  double particleXTotal = 0.0;
  double particleYTotal = 0.0;
  double particleRTotal = 0.0;
  double particleGTotal = 0.0;
  double particleBTotal = 0.0;
  double particleAlphaTotal = 0.0;
  for (const auto& particle : data.particles) {
    particleXTotal += particle.px;
    particleYTotal += particle.py;
    particleRTotal += particle.r;
    particleGTotal += particle.g;
    particleBTotal += particle.b;
    particleAlphaTotal += particle.a;
  }
  const double meanParticleX = data.particles.empty()
      ? static_cast<double>(kGridWidth)
      : particleXTotal / static_cast<double>(data.particles.size());
  const double meanParticleY = data.particles.empty()
      ? static_cast<double>(kGridHeight)
      : particleYTotal / static_cast<double>(data.particles.size());
  const double particleCount = static_cast<double>(data.particles.size());
  const double meanParticleR = particleCount > 0.0 ? particleRTotal / particleCount : 0.0;
  const double meanParticleG = particleCount > 0.0 ? particleGTotal / particleCount : 0.0;
  const double meanParticleB = particleCount > 0.0 ? particleBTotal / particleCount : 0.0;
  return {rasterizeParticles(data, alphaScale),
          static_cast<int>(data.particles.size()), renderer.particleDrawCount(),
          static_cast<int>(state.fluidRenderData_.particles.capacity()),
          meanParticleX, meanParticleY, meanParticleR, meanParticleG,
          meanParticleB, particleAlphaTotal};
}

int countDifferentPixels(const QImage& left, const QImage& right) {
  if (left.size() != right.size()) return -1;
  int changed = 0;
  for (int y = 0; y < left.height(); ++y) {
    for (int x = 0; x < left.width(); ++x) {
      if (left.pixel(x, y) != right.pixel(x, y)) ++changed;
    }
  }
  return changed;
}

struct SmokeImageMetrics {
  int visiblePixels = 0;
  int saturatedPixels = 0;
  std::uint64_t greenExcess = 0;
};

float smokeValueNoise(const float x, const float y) {
  const int x0 = static_cast<int>(std::floor(x));
  const int y0 = static_cast<int>(std::floor(y));
  const auto hashValue = [](const int ix, const int iy) {
    std::uint32_t hash = static_cast<std::uint32_t>(ix) * 0x9E3779B9u ^
                         static_cast<std::uint32_t>(iy) * 0x85EBCA6Bu;
    hash ^= hash >> 16u;
    hash *= 0x7FEB352Du;
    hash ^= hash >> 15u;
    return static_cast<float>(hash & 0xFFFFu) * (2.0f / 65535.0f) - 1.0f;
  };
  const float fractionX = x - static_cast<float>(x0);
  const float fractionY = y - static_cast<float>(y0);
  const float smoothX = fractionX * fractionX * (3.0f - 2.0f * fractionX);
  const float smoothY = fractionY * fractionY * (3.0f - 2.0f * fractionY);
  return std::lerp(
      std::lerp(hashValue(x0, y0), hashValue(x0 + 1, y0), smoothX),
      std::lerp(hashValue(x0, y0 + 1), hashValue(x0 + 1, y0 + 1), smoothX),
      smoothY);
}

SmokeImageMetrics measureSmokeImage(const QImage& image) {
  SmokeImageMetrics metrics;
  for (int y = 0; y < image.height(); ++y) {
    for (int x = 0; x < image.width(); ++x) {
      const QRgb pixel = image.pixel(x, y);
      const int green = qGreen(pixel);
      if (green > qGreen(kBackground) + 2) ++metrics.visiblePixels;
      if (green > 250) ++metrics.saturatedPixels;
      metrics.greenExcess += static_cast<std::uint64_t>(
          std::max(0, green - qGreen(kBackground)));
    }
  }
  return metrics;
}

struct SmokeResolutionCapture final {
  int width = 0;
  int height = 0;
  std::vector<float> density;
  QImage image;
  float maximumDensity = 0.0f;
  double integratedDensity = 0.0;
  double gradientEnergy = 0.0;
  double simulationMicroseconds = 0.0;
  double renderingMicroseconds = 0.0;
};

struct SmokeFieldFrame final {
  std::vector<float> density;
  std::vector<float> velocityX;
  std::vector<float> velocityY;
};

std::uint32_t smokeNoiseHash(std::uint32_t value) {
  value ^= value >> 16U;
  value *= 0x7FEB352DU;
  value ^= value >> 15U;
  value *= 0x846CA68BU;
  value ^= value >> 16U;
  return value;
}

float smokeSeededValueNoise(const float x, const float y,
                            const std::uint32_t seed) {
  const int x0 = static_cast<int>(std::floor(x));
  const int y0 = static_cast<int>(std::floor(y));
  const float fx = x - static_cast<float>(x0);
  const float fy = y - static_cast<float>(y0);
  const float sx = fx * fx * (3.0f - 2.0f * fx);
  const float sy = fy * fy * (3.0f - 2.0f * fy);
  const auto hashValue = [seed](const int px, const int py) {
    std::uint32_t h = seed ^ static_cast<std::uint32_t>(px) * 0x9E3779B9U ^
        static_cast<std::uint32_t>(py) * 0x85EBCA6BU;
    h = smokeNoiseHash(h);
    return static_cast<float>(h & 0x00FFFFFFU) /
        static_cast<float>(0x007FFFFFU) - 1.0f;
  };
  return std::lerp(
      std::lerp(hashValue(x0, y0), hashValue(x0 + 1, y0), sx),
      std::lerp(hashValue(x0, y0 + 1), hashValue(x0 + 1, y0 + 1), sx), sy);
}

void injectTestSmokeTemperature(
    ArtifactCore::NamedVector<float>& temperature, const int width,
    const int height, const Artifact::LayerSmokeRuntimeSettings& settings) {
  const int centerX = 1 + static_cast<int>(
      std::round(std::clamp(settings.emitterX, 0.0f, 1.0f) * (width - 3)));
  const int centerY = 1 + static_cast<int>(
      std::round(std::clamp(settings.emitterY, 0.0f, 1.0f) * (height - 3)));
  const float radiusX = std::max(settings.emitterRadius * (width - 2), 0.5f);
  const float radiusY = std::max(settings.emitterRadius * (height - 2), 0.5f);
  const int extentX = static_cast<int>(std::ceil(radiusX));
  const int extentY = static_cast<int>(std::ceil(radiusY));
  for (int y = std::max(1, centerY - extentY);
       y <= std::min(height - 2, centerY + extentY); ++y) {
    for (int x = std::max(1, centerX - extentX);
         x <= std::min(width - 2, centerX + extentX); ++x) {
      const float dx = static_cast<float>(x - centerX) / radiusX;
      const float dy = static_cast<float>(y - centerY) / radiusY;
      const float radiusSquared = dx * dx + dy * dy;
      if (radiusSquared > 1.0f) continue;
      temperature[static_cast<std::size_t>(x + y * width)] +=
          (1.0f - radiusSquared) * settings.emitterDensity;
    }
  }
}

TEST(ArtifactSmokeRuntimeStandaloneTest,
     ComparesThreeDimensionalSourceVelocityFieldsForSmokeRollup) {
  using namespace ArtifactCore;
  using namespace ArtifactCore::RayTrace;
  PyroDomain domain;
  domain.min = {-0.75f, 0.0f, -0.5f};
  domain.max = {0.75f, 1.125f, 0.5f};
  domain.voxelSize = 1.0f / 32.0f;
  domain.fixedTimeStep = 1.0 / 60.0;
  domain.boundaryMode = PyroBoundaryMode::Open;
  domain.memoryBudgetBytes = 32u * 1024u * 1024u;
  const auto pyroResolution = domain.resolution();
  const VolumeResolution resolution{pyroResolution.width,
      pyroResolution.height, pyroResolution.depth};
  PyroSimulationSettings settings;
  settings.dissipation = 0.035f;
  settings.coolingRate = 0.18f;
  settings.buoyancy = 0.7f;
  settings.vorticity = 0.45f;
  settings.pressureIterations = 16.0f;
  PyroSimulation pulse(domain);
  pulse.setSettings(settings);
  PyroSourceState source;
  source.position = {0.0f, 0.18f, 0.0f};
  source.extent = {0.12f, 0.09f, 0.12f};
  source.velocity = {0.0f, 240.0f, 0.0f};
  source.density = 4.0f;
  source.temperature = 1.5f;
  source.enabled = true;
  for (int step = 0; step < 12; ++step) {
    pulse.setSources(std::span<const PyroSourceState>(&source, 1));
    pulse.step(1.0 / 60.0);
  }

  const auto initialDensity = pulse.fields().densityStorage();
  const auto initialTemperature = pulse.fields().temperatureStorage();
  const auto initialFuel = pulse.fields().fuelStorage();
  const auto initialVelocity = pulse.fields().velocityStorage();
  const double initialMass = std::accumulate(
      initialDensity.begin(), initialDensity.end(), 0.0);
  ASSERT_GT(initialMass, 0.0);

  struct Branch final {
    const char* label;
    PyroSimulation simulation;
    explicit Branch(const char* branchLabel, const PyroDomain& branchDomain)
        : label(branchLabel), simulation(branchDomain) {}
  };
  std::array<Branch, 15> branches{{
      Branch("source-velocity", domain),
      Branch("curl-augmented", domain),
      Branch("curl-strong", domain),
      Branch("velocity-attenuated", domain),
      Branch("fine-timestep", domain),
      Branch("gate-smoke-interior", domain),
      Branch("gate-density-interface", domain),
      Branch("gate-ambient-interface-seed17", domain),
      Branch("gate-ambient-interface-seed29", domain),
      Branch("gate-ambient-interface-seed71", domain),
      Branch("source-wide", domain),
      Branch("source-tall", domain),
      Branch("source-low-jet", domain),
      Branch("source-low-jet-high-buoyancy", domain),
      Branch("source-low-jet-hot", domain)}};
  for (auto& branch : branches) {
    branch.simulation.setSettings(settings);
    branch.simulation.fields().densityStorage() = initialDensity;
    branch.simulation.fields().temperatureStorage() = initialTemperature;
    branch.simulation.fields().fuelStorage() = initialFuel;
    branch.simulation.fields().velocityStorage() = initialVelocity;
  }
  branches[4].simulation.setFixedTimeStep(1.0 / 120.0);
  branches[13].simulation.setSettings(PyroSimulationSettings{
      .sourceDensity = settings.sourceDensity,
      .sourceTemperature = settings.sourceTemperature,
      .sourceFuel = settings.sourceFuel,
      .dissipation = settings.dissipation,
      .coolingRate = settings.coolingRate,
      .buoyancy = 5.6f,
      .vorticity = settings.vorticity,
      .pressureIterations = settings.pressureIterations,
      .advectionClamp = settings.advectionClamp});
  const auto integratedSourceWeight = [&](const PyroVec3 extent) {
    const int centerX = static_cast<int>(
        (source.position.x - domain.min.x) / domain.voxelSize);
    const int centerY = static_cast<int>(
        (source.position.y - domain.min.y) / domain.voxelSize);
    const int centerZ = static_cast<int>(
        (source.position.z - domain.min.z) / domain.voxelSize);
    const int radiusX = static_cast<int>(std::ceil(extent.x / domain.voxelSize));
    const int radiusY = static_cast<int>(std::ceil(extent.y / domain.voxelSize));
    const int radiusZ = static_cast<int>(std::ceil(extent.z / domain.voxelSize));
    const float extentMax = std::max({extent.x, extent.y, extent.z, 1.0e-6f});
    double sum = 0.0;
    for (int z = std::max(0, centerZ - radiusZ);
         z <= std::min(resolution.depth - 1, centerZ + radiusZ); ++z)
      for (int y = std::max(0, centerY - radiusY);
           y <= std::min(resolution.height - 1, centerY + radiusY); ++y)
        for (int x = std::max(0, centerX - radiusX);
             x <= std::min(resolution.width - 1, centerX + radiusX); ++x) {
          const float dx = (static_cast<float>(x) + 0.5f) * domain.voxelSize +
              domain.min.x - source.position.x;
          const float dy = (static_cast<float>(y) + 0.5f) * domain.voxelSize +
              domain.min.y - source.position.y;
          const float dz = (static_cast<float>(z) + 0.5f) * domain.voxelSize +
              domain.min.z - source.position.z;
          sum += std::clamp(1.0f - std::sqrt(dx * dx + dy * dy + dz * dz) /
              extentMax, 0.0f, 1.0f);
        }
    return sum;
  };
  const double baseSourceWeight = integratedSourceWeight(source.extent);
  const auto sourceForBranch = [&](const std::size_t branchIndex) {
    auto branchSource = source;
    if (branchIndex == 10) {
      branchSource.extent = {0.20f, 0.09f, 0.20f};
      branchSource.density *= static_cast<float>(baseSourceWeight /
          integratedSourceWeight(branchSource.extent));
    } else if (branchIndex == 11) {
      branchSource.extent = {0.07f, 0.18f, 0.07f};
      branchSource.density *= static_cast<float>(baseSourceWeight /
          integratedSourceWeight(branchSource.extent));
    } else if (branchIndex >= 12) {
      branchSource.velocity.y = 30.0f;
    }
    if (branchIndex == 14) {
      branchSource.temperature *= 12.0f;
    }
    return branchSource;
  };
  for (std::size_t branchIndex = 10; branchIndex < branches.size();
       ++branchIndex) {
    PyroSimulation variant(domain);
    variant.setSettings(branches[branchIndex].simulation.settings());
    const auto variantSource = sourceForBranch(branchIndex);
    for (int step = 0; step < 12; ++step) {
      variant.setSources(
          std::span<const PyroSourceState>(&variantSource, 1));
      variant.step(1.0 / 60.0);
    }
    branches[branchIndex].simulation.fields().densityStorage() =
        variant.fields().densityStorage();
    branches[branchIndex].simulation.fields().temperatureStorage() =
        variant.fields().temperatureStorage();
    branches[branchIndex].simulation.fields().fuelStorage() =
        variant.fields().fuelStorage();
    branches[branchIndex].simulation.fields().velocityStorage() =
        variant.fields().velocityStorage();
  }

  const auto measureVelocity = [&](const PyroSimulation& simulation) {
    const auto density = simulation.fields().densityView();
    const auto velocity = simulation.fields().velocityView();
    double weightedSpeedSquared = 0.0;
    double weight = 0.0;
    for (std::size_t i = 0; i < density.values.size(); ++i) {
      const double rho = std::max(0.0f, density.values[i]);
      const auto v = velocity.values[i];
      weightedSpeedSquared += rho * (v.x * v.x + v.y * v.y + v.z * v.z);
      weight += rho;
    }
    double divergenceSquared = 0.0;
    std::size_t divergenceCount = 0;
    for (int z = 1; z + 1 < resolution.depth; ++z)
      for (int y = 1; y + 1 < resolution.height; ++y)
        for (int x = 1; x + 1 < resolution.width; ++x) {
          const auto xp = velocity.at(x + 1, y, z);
          const auto xm = velocity.at(x - 1, y, z);
          const auto yp = velocity.at(x, y + 1, z);
          const auto ym = velocity.at(x, y - 1, z);
          const auto zp = velocity.at(x, y, z + 1);
          const auto zm = velocity.at(x, y, z - 1);
          const double div = 0.5 * ((xp.x - xm.x) + (yp.y - ym.y) +
                                     (zp.z - zm.z));
          divergenceSquared += div * div;
          ++divergenceCount;
        }
    return std::array<double, 2>{weight > 0.0
        ? std::sqrt(weightedSpeedSquared / weight) : 0.0,
        divergenceCount > 0
            ? std::sqrt(divergenceSquared / divergenceCount) : 0.0};
  };

  auto& curlBranch = branches[1].simulation;
  auto curlVelocity = curlBranch.fields().velocityView();
  auto strongCurlVelocity = branches[2].simulation.fields().velocityView();
  const float inverseTwoVoxel = 0.5f / domain.voxelSize;
  const auto potential = [&](const int sampleX, const int sampleY,
                             const int sampleZ) {
    const int x = std::clamp(sampleX, 0, resolution.width - 1);
    const int y = std::clamp(sampleY, 0, resolution.height - 1);
    const int z = std::clamp(sampleZ, 0, resolution.depth - 1);
    const float wx = domain.min.x + (static_cast<float>(x) + 0.5f) *
        domain.voxelSize;
    const float wy = domain.min.y + (static_cast<float>(y) + 0.5f) *
        domain.voxelSize;
    const float wz = domain.min.z + (static_cast<float>(z) + 0.5f) *
        domain.voxelSize;
    const double rho = std::max(0.0f,
        curlBranch.fields().densityView().at(x, y, z));
    const float gate = static_cast<float>(rho / (rho + 0.06));
    constexpr float twoPi = 6.28318530718f;
    constexpr std::array<float, 3> wavelengths{0.32f, 0.16f, 0.08f};
    constexpr std::array<float, 3> amplitudes{0.055f, 0.026f, 0.012f};
    PyroVec3 result{};
    for (std::size_t band = 0; band < wavelengths.size(); ++band) {
      const float k = twoPi / wavelengths[band];
      const float phase = static_cast<float>(band) * 1.71f + 0.37f;
      result.x += amplitudes[band] * std::sin(k * (0.31f * wx + wy +
          0.19f * wz) + phase);
      result.y += amplitudes[band] * std::sin(k * (0.23f * wx +
          0.37f * wy + wz) - phase * 0.7f);
      result.z += amplitudes[band] * std::sin(k * (wx - 0.29f * wy +
          0.41f * wz) + phase * 0.43f);
    }
    result.x *= gate;
    result.y *= gate;
    result.z *= gate;
    return result;
  };
  for (int z = 0; z < resolution.depth; ++z)
    for (int y = 0; y < resolution.height; ++y)
      for (int x = 0; x < resolution.width; ++x) {
        const auto axp = potential(x + 1, y, z);
        const auto axm = potential(x - 1, y, z);
        const auto ayp = potential(x, y + 1, z);
        const auto aym = potential(x, y - 1, z);
        const auto azp = potential(x, y, z + 1);
        const auto azm = potential(x, y, z - 1);
        const PyroVec3 curl{
            (ayp.z - aym.z - azp.y + azm.y) * inverseTwoVoxel,
            (azp.x - azm.x - axp.z + axm.z) * inverseTwoVoxel,
            (axp.y - axm.y - ayp.x + aym.x) * inverseTwoVoxel};
        auto& velocity = curlVelocity.at(x, y, z);
        velocity.x += curl.x;
        velocity.y += curl.y;
        velocity.z += curl.z;
        strongCurlVelocity.at(x, y, z).x += 2.0f * curl.x;
        strongCurlVelocity.at(x, y, z).y += 2.0f * curl.y;
        strongCurlVelocity.at(x, y, z).z += 2.0f * curl.z;
      }
  auto attenuatedVelocity = branches[3].simulation.fields().velocityView();
  for (auto& velocity : attenuatedVelocity.values) {
    velocity.x *= 0.5f;
    velocity.y *= 0.5f;
    velocity.z *= 0.5f;
  }
  GTEST_LOG_(INFO) << "3d-source pulse mass=" << initialMass
      << " initial velocityRms source/curl/curlStrong/half/fine="
      << measureVelocity(branches[0].simulation)[0] << '/'
      << measureVelocity(branches[1].simulation)[0] << '/'
      << measureVelocity(branches[2].simulation)[0] << '/'
      << measureVelocity(branches[3].simulation)[0] << '/'
      << measureVelocity(branches[4].simulation)[0] << " divergence="
      << measureVelocity(branches[0].simulation)[1] << '/'
      << measureVelocity(branches[1].simulation)[1] << '/'
      << measureVelocity(branches[2].simulation)[1] << '/'
      << measureVelocity(branches[3].simulation)[1] << '/'
      << measureVelocity(branches[4].simulation)[1] << " fineDt="
      << branches[4].simulation.fixedTimeStep();

  const auto applyCoherentTurbulence = [&](PyroSimulation& simulation,
      const double timeSeconds, const float pulseLength,
      const std::array<float, 4>& wavelengths, const int bandCount,
      const int gateMode, const std::uint32_t noiseSeed) {
    auto velocity = simulation.fields().velocityView();
    const auto density = simulation.fields().densityView();
    const float pulse = std::max(0.01f, pulseLength);
    const auto valueNoise = [](const float x, const float y, const float z,
                               const std::uint32_t channel) {
      const int x0 = static_cast<int>(std::floor(x));
      const int y0 = static_cast<int>(std::floor(y));
      const int z0 = static_cast<int>(std::floor(z));
      const float fx = x - static_cast<float>(x0);
      const float fy = y - static_cast<float>(y0);
      const float fz = z - static_cast<float>(z0);
      const auto fade = [](const float t) {
        return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
      };
      const float tx = fade(fx);
      const float ty = fade(fy);
      const float tz = fade(fz);
      const auto hash = [channel](const int ix, const int iy, const int iz) {
        std::uint32_t value = 0x9e3779b9u ^ channel;
        value ^= static_cast<std::uint32_t>(ix) * 0x85ebca6bu;
        value = (value ^ (value >> 13)) * 0xc2b2ae35u;
        value ^= static_cast<std::uint32_t>(iy) * 0x27d4eb2fu;
        value = (value ^ (value >> 15)) * 0x165667b1u;
        value ^= static_cast<std::uint32_t>(iz) * 0xd3a2646cu;
        value ^= value >> 16;
        return static_cast<float>(value & 0x00ffffffu) /
            8388607.5f - 1.0f;
      };
      const auto lerp = [](const float a, const float b, const float t) {
        return std::lerp(a, b, t);
      };
      const float z0y0 = lerp(lerp(hash(x0, y0, z0),
          hash(x0 + 1, y0, z0), tx), lerp(hash(x0, y0 + 1, z0),
          hash(x0 + 1, y0 + 1, z0), tx), ty);
      const float z0y1 = lerp(lerp(hash(x0, y0, z0 + 1),
          hash(x0 + 1, y0, z0 + 1), tx), lerp(hash(x0, y0 + 1, z0 + 1),
          hash(x0 + 1, y0 + 1, z0 + 1), tx), ty);
      return lerp(z0y0, z0y1, tz);
    };
    const auto densityGradientMagnitude = [&](const int x, const int y,
                                               const int z) {
      const float dx = density.at(std::min(x + 1, resolution.width - 1), y, z) -
          density.at(std::max(x - 1, 0), y, z);
      const float dy = density.at(x, std::min(y + 1, resolution.height - 1), z) -
          density.at(x, std::max(y - 1, 0), z);
      const float dz = density.at(x, y, std::min(z + 1, resolution.depth - 1)) -
          density.at(x, y, std::max(z - 1, 0));
      return 0.5f * std::sqrt(dx * dx + dy * dy + dz * dz);
    };
    const auto gateAt = [&](const int x, const int y, const int z) {
      const float rho = std::max(0.0f, density.at(x, y, z));
      if (gateMode == 1) {
        const float inSmoke = std::clamp((rho - 0.005f) / 0.025f,
                                         0.0f, 1.0f);
        const float belowCore = std::clamp((0.75f - rho) / 0.25f,
                                           0.0f, 1.0f);
        const float interface = std::clamp(
            densityGradientMagnitude(x, y, z) / 0.055f, 0.0f, 1.0f);
        return inSmoke * belowCore * interface;
      }
      if (gateMode == 2) {
        const float ambient = std::clamp((0.08f - rho) / 0.08f,
                                         0.0f, 1.0f);
        const float nearInterface = std::clamp(
            densityGradientMagnitude(x, y, z) / 0.055f, 0.0f, 1.0f);
        return ambient * nearInterface;
      }
      return std::clamp(rho / 0.06f, 0.0f, 1.0f);
    };
    const auto potentialAt = [&](int sampleX, int sampleY, int sampleZ) {
      const int x = std::clamp(sampleX, 0, resolution.width - 1);
      const int y = std::clamp(sampleY, 0, resolution.height - 1);
      const int z = std::clamp(sampleZ, 0, resolution.depth - 1);
      const float wx = domain.min.x + (static_cast<float>(x) + 0.5f) *
          domain.voxelSize;
      const float wy = domain.min.y + (static_cast<float>(y) + 0.5f) *
          domain.voxelSize;
      const float wz = domain.min.z + (static_cast<float>(z) + 0.5f) *
          domain.voxelSize;
      const float gate = gateAt(x, y, z);
      PyroVec3 result{};
      for (int band = 0; band < bandCount; ++band) {
        const float wavelength = wavelengths[static_cast<std::size_t>(band)];
        const float inverseScale = 1.0f / wavelength;
        const float drift = static_cast<float>(timeSeconds) / pulse *
            (0.31f + 0.17f * static_cast<float>(band));
        const float amplitude = wavelength /
            static_cast<float>(band + 1);
        result.x += amplitude * valueNoise(wx * inverseScale + drift,
            wy * inverseScale - drift * 0.43f,
            wz * inverseScale + drift * 0.19f,
            (0x1234u + band * 3u) ^ noiseSeed);
        result.y += amplitude * valueNoise(wx * inverseScale - drift * 0.23f,
            wy * inverseScale + drift * 0.61f,
            wz * inverseScale - drift,
            (0x5678u + band * 3u) ^ noiseSeed);
        result.z += amplitude * valueNoise(wx * inverseScale + drift * 0.47f,
            wy * inverseScale + drift * 0.13f,
            wz * inverseScale + drift * 0.73f,
            (0x9abcu + band * 3u) ^ noiseSeed);
      }
      result.x *= gate;
      result.y *= gate;
      result.z *= gate;
      return result;
    };
    std::vector<PyroVec3> vectorPotential(resolution.cellCount());
    for (int z = 0; z < resolution.depth; ++z)
      for (int y = 0; y < resolution.height; ++y)
        for (int x = 0; x < resolution.width; ++x)
          vectorPotential[resolution.indexOf(x, y, z)] =
              potentialAt(x, y, z);
    const auto potential = [&](const int x, const int y, const int z) {
      return vectorPotential[resolution.indexOf(
          std::clamp(x, 0, resolution.width - 1),
          std::clamp(y, 0, resolution.height - 1),
          std::clamp(z, 0, resolution.depth - 1))];
    };
    std::vector<PyroVec3> acceleration(resolution.cellCount());
    double weightedSquared = 0.0;
    double supportWeight = 0.0;
    const float inverseTwoVoxel = 0.5f / domain.voxelSize;
    for (int z = 0; z < resolution.depth; ++z)
      for (int y = 0; y < resolution.height; ++y)
        for (int x = 0; x < resolution.width; ++x) {
          const auto axp = potential(x + 1, y, z);
          const auto axm = potential(x - 1, y, z);
          const auto ayp = potential(x, y + 1, z);
          const auto aym = potential(x, y - 1, z);
          const auto azp = potential(x, y, z + 1);
          const auto azm = potential(x, y, z - 1);
          const PyroVec3 curl{
              (ayp.z - aym.z - azp.y + azm.y) * inverseTwoVoxel,
              (azp.x - azm.x - axp.z + axm.z) * inverseTwoVoxel,
              (axp.y - axm.y - ayp.x + aym.x) * inverseTwoVoxel};
          const auto cell = resolution.indexOf(x, y, z);
          acceleration[cell] = curl;
          const double support = gateAt(x, y, z) > 0.01f ? 1.0 : 0.0;
          weightedSquared += support * (curl.x * curl.x + curl.y * curl.y +
                                         curl.z * curl.z);
          supportWeight += support;
        }
    const double rawRms = supportWeight > 0.0
        ? std::sqrt(weightedSquared / supportWeight) : 0.0;
    const double referenceSpeed = measureVelocity(branches[0].simulation)[0];
    const float scale = rawRms > 1.0e-12
        ? static_cast<float>((0.45 * referenceSpeed / 1.2) / rawRms) : 0.0f;
    for (std::size_t i = 0; i < acceleration.size(); ++i) {
      velocity.values[i].x += acceleration[i].x * scale / 60.0f;
      velocity.values[i].y += acceleration[i].y * scale / 60.0f;
      velocity.values[i].z += acceleration[i].z * scale / 60.0f;
    }
    return std::array<double, 2>{rawRms, supportWeight};
  };

  const auto outputDirectory = qEnvironmentVariable(
      "ARTIFACT_SMOKE_RUNTIME_OUTPUT_DIR");
  const auto render = [&](float* density, const bool sideView) {
    CPUVolumeRenderer renderer;
    VolumeAABB bounds;
    bounds.min = {domain.min.x, domain.min.y, domain.min.z};
    bounds.max = {domain.max.x, domain.max.y, domain.max.z};
    renderer.bounds = bounds;
    VolumeFieldSet fields;
    fields.density = {density, resolution};
    renderer.setVolumeData(bounds, fields);
    renderer.settings.stepSize = 0.018f;
    renderer.settings.maxSteps = 256;
    renderer.settings.densityScale = 7.2f;
    renderer.settings.useTemperatureEmission = false;
    renderer.settings.backgroundColor = {0.015f, 0.02f, 0.03f};
    renderer.settings.lightIntensity = 3.0f;
    renderer.settings.phaseAnisotropy = 0.25f;
    renderer.settings.multipleScatteringStrength = 0.18f;
    renderer.settings.shadowIntensity = 0.25f;
    renderer.settings.shadowStepSize = 0.04f;
    renderer.settings.shadowMaxSteps = 96;
    renderer.settings.shadowDensityScale = 0.08f;
    const Point3 lookAt{0.0f, 0.56f, 0.0f};
    const Point3 lookFrom = sideView ? Point3{2.4f, 0.56f, 0.0f}
                                     : Point3{0.0f, 0.56f, 2.4f};
    renderer.camera = Camera(1.0f, 36.0f, 2.4f, 0.0f, lookFrom,
        lookAt, {0.0f, 1.0f, 0.0f});
    return renderer.render(160, 160);
  };
  const auto savePng = [](const ImageBuffer& buffer, const QString& path) {
    QImage image(buffer.width, buffer.height, QImage::Format_ARGB32);
    for (int y = 0; y < buffer.height; ++y) {
      auto* row = reinterpret_cast<QRgb*>(image.scanLine(y));
      for (int x = 0; x < buffer.width; ++x) {
        const auto offset = static_cast<std::size_t>(
            (y * buffer.width + x) * 3);
        const auto encode = [](const std::uint8_t channel) {
          const float linear = static_cast<float>(channel) / 255.0f;
          return static_cast<int>(std::lround(
              std::pow(linear, 1.0f / 2.2f) * 255.0f));
        };
        row[x] = qRgba(encode(buffer.pixels[offset]),
            encode(buffer.pixels[offset + 1]),
            encode(buffer.pixels[offset + 2]), 255);
      }
    }
    return image.save(path, "PNG");
  };
  const auto measureDensity = [&](const PyroSimulation& simulation) {
    const auto density = simulation.fields().densityView();
    double mass = 0.0;
    double weightedY = 0.0;
    double weightedZ = 0.0;
    for (int z = 0; z < resolution.depth; ++z)
      for (int y = 0; y < resolution.height; ++y)
        for (int x = 0; x < resolution.width; ++x) {
          const double rho = std::max(0.0f, density.at(x, y, z));
          mass += rho;
          weightedY += rho * y;
          weightedZ += rho * z;
        }
    return std::array<double, 3>{mass,
        mass > 0.0 ? weightedY / mass : 0.0,
        mass > 0.0 ? weightedZ / mass : 0.0};
  };
  struct PreviousImage final { std::vector<float> pixels; };
  const auto measureImage = [](const ImageBuffer& image,
                               const PreviousImage& previous) {
    std::size_t occupied = 0;
    double luminance = 0.0;
    double delta = 0.0;
    for (int y = 0; y < image.height; ++y)
      for (int x = 0; x < image.width; ++x) {
        const auto offset = static_cast<std::size_t>(
            (y * image.width + x) * 3);
        const double luma = 0.2126 * image.pixels[offset] +
            0.7152 * image.pixels[offset + 1] +
            0.0722 * image.pixels[offset + 2];
        if (luma > 10.0) ++occupied;
        luminance += luma;
        if (previous.pixels.size() == image.pixels.size())
          delta += std::abs(static_cast<double>(image.pixels[offset]) -
              previous.pixels[offset]) +
              std::abs(static_cast<double>(image.pixels[offset + 1]) -
              previous.pixels[offset + 1]) +
              std::abs(static_cast<double>(image.pixels[offset + 2]) -
              previous.pixels[offset + 2]);
      }
    const double meanDelta = previous.pixels.size() == image.pixels.size()
        ? delta / (image.width * image.height * 3.0) : 0.0;
    return std::array<double, 3>{static_cast<double>(occupied), luminance,
                                  meanDelta};
  };
  const auto captureDirectory = QDir(outputDirectory).filePath(
      QStringLiteral("3d-source-rollup-rnd"));
  if (!outputDirectory.isEmpty())
    ASSERT_TRUE(QDir().mkpath(captureDirectory));
  constexpr std::array<int, 4> captureSteps{0, 24, 48, 72};
  std::array<PreviousImage, 15> previousFront;
  std::array<PreviousImage, 15> previousSide;
  std::size_t nextCapture = 0;
  constexpr std::array<float, 4> largeWavelengths{0.75f, 0.48f, 0.30f, 0.0f};
  for (int step = 0; step <= captureSteps.back(); ++step) {
    if (step == captureSteps[nextCapture]) {
      for (std::size_t branchIndex = 0; branchIndex < branches.size();
           ++branchIndex) {
        auto& branch = branches[branchIndex];
        const auto densityMetrics = measureDensity(branch.simulation);
        const auto velocityMetrics = measureVelocity(branch.simulation);
        const auto front = render(
            branch.simulation.fields().densityStorage().data(), false);
        const auto side = render(
            branch.simulation.fields().densityStorage().data(), true);
        const auto frontMetrics = measureImage(front,
                                               previousFront[branchIndex]);
        const auto sideMetrics = measureImage(side,
                                              previousSide[branchIndex]);
        GTEST_LOG_(INFO) << "3d-source branch=" << branch.label << " step="
            << step << " mass=" << densityMetrics[0] << " centroidY/Z="
            << densityMetrics[1] << '/' << densityMetrics[2]
            << " velocityRms/divergenceRms=" << velocityMetrics[0] << '/'
            << velocityMetrics[1] << " frontOccupied/luma/delta="
            << frontMetrics[0] << '/' << frontMetrics[1] << '/'
            << frontMetrics[2] << " sideOccupied/luma/delta="
            << sideMetrics[0] << '/' << sideMetrics[1] << '/'
            << sideMetrics[2];
        const bool compareMassMatched = step > 0 &&
            (branchIndex == 0 || branchIndex == 6 || branchIndex >= 7);
        if (compareMassMatched) {
          const double controlMass = measureDensity(
              branches[0].simulation)[0];
          const double currentMass = densityMetrics[0];
          const float massScale = currentMass > 0.0
              ? static_cast<float>(controlMass / currentMass) : 1.0f;
          std::vector<float> massMatchedDensity(
              branch.simulation.fields().densityStorage().begin(),
              branch.simulation.fields().densityStorage().end());
          for (auto& value : massMatchedDensity) value *= massScale;
          const auto matchedFront = render(massMatchedDensity.data(), false);
          const auto matchedSide = render(massMatchedDensity.data(), true);
          const auto matchedFrontMetrics = measureImage(
              matchedFront, PreviousImage{});
          const auto matchedSideMetrics = measureImage(
              matchedSide, PreviousImage{});
          GTEST_LOG_(INFO) << "3d-source mass-matched branch=" << branch.label
              << " scale=" << massScale << " frontOccupied/luma="
              << matchedFrontMetrics[0] << '/' << matchedFrontMetrics[1]
              << " sideOccupied/luma=" << matchedSideMetrics[0] << '/'
              << matchedSideMetrics[1];
          if (!outputDirectory.isEmpty()) {
            const auto stem = QStringLiteral("%1_step_%2_mass_matched")
                .arg(QString::fromLatin1(branch.label))
                .arg(step, 3, 10, QLatin1Char('0'));
            EXPECT_TRUE(savePng(matchedFront, QDir(captureDirectory).filePath(
                stem + QStringLiteral("_front.png"))));
            EXPECT_TRUE(savePng(matchedSide, QDir(captureDirectory).filePath(
                stem + QStringLiteral("_side.png"))));
          }
        }
        if (!outputDirectory.isEmpty()) {
          const auto stem = QStringLiteral("%1_step_%2")
              .arg(QString::fromLatin1(branch.label))
              .arg(step, 3, 10, QLatin1Char('0'));
          EXPECT_TRUE(savePng(front, QDir(captureDirectory).filePath(
              stem + QStringLiteral("_front.png"))));
          EXPECT_TRUE(savePng(side, QDir(captureDirectory).filePath(
              stem + QStringLiteral("_side.png"))));
        }
        previousFront[branchIndex].pixels.assign(front.pixels.begin(),
                                                 front.pixels.end());
        previousSide[branchIndex].pixels.assign(side.pixels.begin(),
                                                side.pixels.end());
      }
      if (nextCapture + 1 < captureSteps.size()) ++nextCapture;
    }
    if (step == captureSteps.back()) break;
    for (std::size_t branchIndex = 0; branchIndex < branches.size();
         ++branchIndex) {
      const auto branchSource = sourceForBranch(branchIndex);
      branches[branchIndex].simulation.setSources(
          std::span<const PyroSourceState>(&branchSource, 1));
    }
    const auto smokeForce = applyCoherentTurbulence(branches[5].simulation,
        static_cast<double>(step) / 60.0, 0.45f, largeWavelengths, 3, 0,
        0x17u);
    const auto interfaceForce = applyCoherentTurbulence(
        branches[6].simulation, static_cast<double>(step) / 60.0,
        0.45f, largeWavelengths, 3, 1, 0x17u);
    const auto ambientForce = applyCoherentTurbulence(
        branches[7].simulation, static_cast<double>(step) / 60.0,
        0.45f, largeWavelengths, 3, 2, 0x17u);
    const auto ambientSeed29Force = applyCoherentTurbulence(
        branches[8].simulation, static_cast<double>(step) / 60.0,
        0.45f, largeWavelengths, 3, 2, 0x29u);
    const auto ambientSeed71Force = applyCoherentTurbulence(
        branches[9].simulation, static_cast<double>(step) / 60.0,
        0.45f, largeWavelengths, 3, 2, 0x71u);
    if (step == 0) {
      GTEST_LOG_(INFO) << "3d-source gate force rawRms/support interior/interface/ambient="
          << smokeForce[0] << '/' << smokeForce[1] << ' '
          << interfaceForce[0] << '/' << interfaceForce[1] << ' '
          << ambientForce[0] << '/' << ambientForce[1] << " seeds29/71="
          << ambientSeed29Force[0] << '/' << ambientSeed29Force[1] << ' '
          << ambientSeed71Force[0] << '/' << ambientSeed71Force[1];
      EXPECT_GT(smokeForce[1], 0.0);
      EXPECT_GT(interfaceForce[1], 0.0);
      EXPECT_GT(ambientForce[1], 0.0);
      EXPECT_GT(ambientSeed29Force[1], 0.0);
      EXPECT_GT(ambientSeed71Force[1], 0.0);
    }
    for (auto& branch : branches) branch.simulation.step(1.0 / 60.0);
  }
  for (const auto& branch : branches) {
    EXPECT_GT(measureDensity(branch.simulation)[0], 0.0);
    EXPECT_TRUE(std::isfinite(measureVelocity(branch.simulation)[1]));
  }
}

TEST(ArtifactSmokeRuntimeStandaloneTest,
     RechecksPyroSourceShapeAcrossSeedsAtHigherResolution) {
  using namespace ArtifactCore;
  using namespace ArtifactCore::RayTrace;
  PyroDomain domain;
  domain.min = {-0.75f, 0.0f, -0.5f};
  domain.max = {0.75f, 1.125f, 0.5f};
  domain.voxelSize = 1.0f / 48.0f;
  domain.fixedTimeStep = 1.0 / 60.0;
  domain.boundaryMode = PyroBoundaryMode::Open;
  domain.memoryBudgetBytes = 64u * 1024u * 1024u;
  const auto pyroResolution = domain.resolution();
  const VolumeResolution resolution{pyroResolution.width,
      pyroResolution.height, pyroResolution.depth};
  PyroSimulationSettings settings;
  settings.dissipation = 0.035f;
  settings.coolingRate = 0.18f;
  settings.buoyancy = 0.7f;
  settings.vorticity = 0.0f;
  settings.pressureIterations = 96.0f;
  PyroSourceState source;
  source.position = {0.0f, 0.18f, 0.0f};
  source.extent = {0.12f, 0.09f, 0.12f};
  source.velocity = {0.0f, 180.0f, 0.0f};
  source.density = 4.0f;
  source.temperature = 1.5f;
  source.enabled = true;

  const auto sourceWeight = [&](const PyroVec3 extent) {
    const int cx = static_cast<int>(
        (source.position.x - domain.min.x) / domain.voxelSize);
    const int cy = static_cast<int>(
        (source.position.y - domain.min.y) / domain.voxelSize);
    const int cz = static_cast<int>(
        (source.position.z - domain.min.z) / domain.voxelSize);
    const int rx = static_cast<int>(std::ceil(extent.x / domain.voxelSize));
    const int ry = static_cast<int>(std::ceil(extent.y / domain.voxelSize));
    const int rz = static_cast<int>(std::ceil(extent.z / domain.voxelSize));
    const float extentMax = std::max({extent.x, extent.y, extent.z, 1.0e-6f});
    double sum = 0.0;
    for (int z = std::max(0, cz - rz);
         z <= std::min(resolution.depth - 1, cz + rz); ++z)
      for (int y = std::max(0, cy - ry);
           y <= std::min(resolution.height - 1, cy + ry); ++y)
        for (int x = std::max(0, cx - rx);
             x <= std::min(resolution.width - 1, cx + rx); ++x) {
          const float dx = (static_cast<float>(x) + 0.5f) * domain.voxelSize +
              domain.min.x - source.position.x;
          const float dy = (static_cast<float>(y) + 0.5f) * domain.voxelSize +
              domain.min.y - source.position.y;
          const float dz = (static_cast<float>(z) + 0.5f) * domain.voxelSize +
              domain.min.z - source.position.z;
          sum += std::clamp(1.0f - std::sqrt(dx * dx + dy * dy + dz * dz) /
              extentMax, 0.0f, 1.0f);
        }
    return sum;
  };
  const double baseWeight = sourceWeight(source.extent);

  struct Branch final {
    const char* label;
    std::uint32_t seed;
    float vorticityStrength;
    float shearFrequencyScale;
    bool multiscaleShear;
    PyroSimulation simulation;
    PyroSourceState source;
    Branch(const char* name, const std::uint32_t branchSeed,
           const PyroDomain& branchDomain, const PyroSourceState& branchSource,
           const float strength = 0.0f, const float frequencyScale = 1.0f,
           const bool useMultiscaleShear = false)
        : label(name), seed(branchSeed),
          vorticityStrength(strength),
          shearFrequencyScale(frequencyScale),
          multiscaleShear(useMultiscaleShear),
          simulation(branchDomain),
          source(branchSource) {}
  };
  std::array<Branch, 14> branches{{
      Branch("highres-control-seed17", 17u, domain, source),
      Branch("highres-wide-seed17", 17u, domain, source),
      Branch("highres-tall-seed17", 17u, domain, source),
      Branch("highres-control-seed71", 71u, domain, source),
      Branch("highres-wide-seed71", 71u, domain, source),
      Branch("highres-tall-seed71", 71u, domain, source),
      Branch("highres-wide-curlconfinement3-seed17", 17u, domain,
          source, 3.0f),
      Branch("highres-wide-curlconfinement8-seed17", 17u, domain,
          source, 8.0f),
      Branch("highres-wide-curlconfinement3-seed71", 71u, domain,
          source, 3.0f),
      Branch("highres-wide-curlconfinement8-seed71", 71u, domain,
          source, 8.0f),
      Branch("highres-wide-curlconfinement8-highfreq-seed17", 17u, domain,
          source, 8.0f, 2.75f),
      Branch("highres-wide-curlconfinement8-highfreq-seed71", 71u, domain,
          source, 8.0f, 2.75f),
      Branch("highres-wide-curlconfinement8-multiscale-seed17", 17u, domain,
          source, 8.0f, 1.0f, true),
      Branch("highres-wide-curlconfinement8-multiscale-seed71", 71u, domain,
          source, 8.0f, 1.0f, true)}};
  for (std::size_t i = 0; i < branches.size(); ++i) {
    auto& branch = branches[i];
    auto branchSettings = settings;
    const std::size_t shapeIndex = i < 6 ? i % 3 : 1;
    branchSettings.vorticity = branch.vorticityStrength;
    branch.simulation.setSettings(branchSettings);
    if (shapeIndex == 1) branch.source.extent = {0.20f, 0.09f, 0.20f};
    if (shapeIndex == 2) branch.source.extent = {0.07f, 0.18f, 0.07f};
    if (shapeIndex != 0)
      branch.source.density *= static_cast<float>(baseWeight /
          sourceWeight(branch.source.extent));
  }

  const auto measure = [&](const PyroSimulation& simulation,
                           const double densityScale = 1.0) {
    const auto density = simulation.fields().densityView();
    const auto velocity = simulation.fields().velocityView();
    double mass = 0.0;
    double weightedY = 0.0;
    double divergenceSquared = 0.0;
    std::size_t divergenceCount = 0;
    std::size_t occupiedCells = 0;
    std::size_t surfaceFaces = 0;
    std::vector<std::size_t> sliceOccupancy(
        static_cast<std::size_t>(resolution.height), 0);
    for (int z = 1; z + 1 < resolution.depth; ++z)
      for (int y = 1; y + 1 < resolution.height; ++y)
        for (int x = 1; x + 1 < resolution.width; ++x) {
          const float rho = static_cast<float>(
              std::max(0.0f, density.at(x, y, z)) * densityScale);
          mass += rho;
          weightedY += rho * y;
          const bool occupied = rho > 0.05f;
          if (occupied) {
            ++occupiedCells;
            ++sliceOccupancy[static_cast<std::size_t>(y)];
            if (x + 1 < resolution.width - 1 &&
                !(density.at(x + 1, y, z) * densityScale > 0.05f))
              ++surfaceFaces;
            if (y + 1 < resolution.height - 1 &&
                !(density.at(x, y + 1, z) * densityScale > 0.05f))
              ++surfaceFaces;
            if (z + 1 < resolution.depth - 1 &&
                !(density.at(x, y, z + 1) * densityScale > 0.05f))
              ++surfaceFaces;
          }
          const auto xp = velocity.at(x + 1, y, z);
          const auto xm = velocity.at(x - 1, y, z);
          const auto yp = velocity.at(x, y + 1, z);
          const auto ym = velocity.at(x, y - 1, z);
          const auto zp = velocity.at(x, y, z + 1);
          const auto zm = velocity.at(x, y, z - 1);
          const double div = 0.5 * (xp.x - xm.x + yp.y - ym.y +
                                    zp.z - zm.z);
          divergenceSquared += div * div;
          ++divergenceCount;
        }
    std::size_t slicePeaks = 0;
    for (int y = 2; y + 2 < resolution.height; ++y) {
      const auto previous = sliceOccupancy[static_cast<std::size_t>(y - 1)];
      const auto current = sliceOccupancy[static_cast<std::size_t>(y)];
      const auto next = sliceOccupancy[static_cast<std::size_t>(y + 1)];
      if (current > previous * 1.05 && current > next * 1.05)
        ++slicePeaks;
    }
    const double normalizedSurface = occupiedCells > 0
        ? static_cast<double>(surfaceFaces) /
            std::pow(static_cast<double>(occupiedCells), 2.0 / 3.0)
        : 0.0;
    return std::array<double, 6>{mass, mass > 0.0 ? weightedY / mass : 0.0,
        divergenceCount > 0
            ? std::sqrt(divergenceSquared / divergenceCount) : 0.0,
        static_cast<double>(occupiedCells), normalizedSurface,
        static_cast<double>(slicePeaks)};
  };

  const auto outputDirectory = qEnvironmentVariable(
      "ARTIFACT_SMOKE_RUNTIME_OUTPUT_DIR");
  const auto captureDirectory = QDir(outputDirectory).filePath(
      QStringLiteral("3d-source-shape-highres-rnd"));
  if (!outputDirectory.isEmpty())
    ASSERT_TRUE(QDir().mkpath(captureDirectory));
  constexpr int simulationSteps = 60;
  for (int step = 0; step < simulationSteps; ++step) {
    for (auto& branch : branches) {
      auto density = branch.simulation.fields().densityView();
      auto velocity = branch.simulation.fields().velocityView();
      const float phase = branch.seed == 17u ? 0.7f : 2.4f;
      for (int z = 1; z + 1 < resolution.depth; ++z)
        for (int y = 1; y + 1 < resolution.height; ++y) {
          const float gate = std::clamp(
              density.at(resolution.width / 2, y, resolution.depth / 2) /
                  0.2f, 0.0f, 1.0f);
          const float frequencyScale = branch.shearFrequencyScale;
          float wave = std::sin(static_cast<float>(y) *
              0.31f * frequencyScale + phase);
          float waveZ = std::cos(static_cast<float>(y) *
              0.23f * frequencyScale + phase * 1.7f);
          if (branch.multiscaleShear) {
            constexpr float kInvSqrtTwo = 0.70710678f;
            wave = (wave + kInvSqrtTwo * std::sin(
                static_cast<float>(y) * 0.86f + phase * 0.71f)) /
                (1.0f + kInvSqrtTwo);
            waveZ = (waveZ + kInvSqrtTwo * std::cos(
                static_cast<float>(y) * 0.67f + phase * 1.19f)) /
                (1.0f + kInvSqrtTwo);
          }
          for (int x = 1; x + 1 < resolution.width; ++x) {
            auto& v = velocity.at(x, y, z);
            v.x += 4.0f * wave * gate / 60.0f;
            v.z += 4.0f * waveZ * gate / 60.0f;
          }
        }
      branch.simulation.setSources(
          std::span<const PyroSourceState>(&branch.source, 1));
      branch.simulation.step(1.0 / 60.0);
    }
    if (step == 29 || step == simulationSteps - 1) {
      for (const auto& branch : branches) {
        const auto metrics = measure(branch.simulation);
        GTEST_LOG_(INFO) << "3d-source highres branch=" << branch.label
            << " step=" << step + 1
            << " mass/Ycentroid/divergenceRms/occupied/surfaceComplexity/slicePeaks="
            << metrics[0] << '/' << metrics[1] << '/' << metrics[2] << '/'
            << metrics[3] << '/' << metrics[4] << '/' << metrics[5];
      }
    }
  }

  const auto render = [&](const float* density, const bool sideView) {
    CPUVolumeRenderer renderer;
    VolumeAABB bounds;
    bounds.min = {domain.min.x, domain.min.y, domain.min.z};
    bounds.max = {domain.max.x, domain.max.y, domain.max.z};
    VolumeFieldSet fields;
    fields.density = {const_cast<float*>(density), resolution};
    renderer.setVolumeData(bounds, fields);
    renderer.settings.stepSize = 0.012f;
    renderer.settings.maxSteps = 320;
    renderer.settings.densityScale = 7.2f;
    renderer.settings.useTemperatureEmission = false;
    renderer.settings.backgroundColor = {0.015f, 0.02f, 0.03f};
    renderer.settings.lightIntensity = 3.0f;
    renderer.settings.phaseAnisotropy = 0.25f;
    renderer.settings.multipleScatteringStrength = 0.18f;
    renderer.settings.shadowIntensity = 0.0f;
    const Point3 lookAt{0.0f, 0.56f, 0.0f};
    const Point3 lookFrom = sideView ? Point3{2.4f, 0.56f, 0.0f}
                                     : Point3{0.0f, 0.56f, 2.4f};
    renderer.camera = Camera(1.0f, 36.0f, 2.4f, 0.0f, lookFrom,
        lookAt, {0.0f, 1.0f, 0.0f});
    return renderer.render(144, 144);
  };
  const auto savePng = [](const ImageBuffer& buffer, const QString& path) {
    QImage image(buffer.width, buffer.height, QImage::Format_ARGB32);
    for (int y = 0; y < buffer.height; ++y) {
      auto* row = reinterpret_cast<QRgb*>(image.scanLine(y));
      for (int x = 0; x < buffer.width; ++x) {
        const auto i = static_cast<std::size_t>((y * buffer.width + x) * 3);
        row[x] = qRgba(buffer.pixels[i], buffer.pixels[i + 1],
                       buffer.pixels[i + 2], 255);
      }
    }
    return image.save(path, "PNG");
  };
  const double controlMass = measure(branches[0].simulation)[0];
  const auto imageStats = [](const ImageBuffer& image) {
    std::size_t occupied = 0;
    double luma = 0.0;
    for (int y = 0; y < image.height; ++y)
      for (int x = 0; x < image.width; ++x) {
        const auto i = static_cast<std::size_t>((y * image.width + x) * 3);
        const float r = image.pixels[i];
        const float g = image.pixels[i + 1];
        const float b = image.pixels[i + 2];
        if (std::max({r, g, b}) > 10.0f) ++occupied;
        luma += 0.2126 * r + 0.7152 * g + 0.0722 * b;
      }
    return std::array<double, 2>{static_cast<double>(occupied), luma};
  };
  for (const auto& branch : branches) {
    const auto metrics = measure(branch.simulation);
    const auto matchedMetrics = measure(branch.simulation,
        metrics[0] > 0.0 ? controlMass / metrics[0] : 1.0);
    EXPECT_GT(metrics[0], 0.0);
    EXPECT_TRUE(std::isfinite(metrics[2]));
    EXPECT_TRUE(std::isfinite(matchedMetrics[4]));
    auto matchedDensity = branch.simulation.fields().densityStorage();
    const float scale = metrics[0] > 0.0
        ? static_cast<float>(controlMass / metrics[0]) : 1.0f;
    for (auto& value : matchedDensity) value *= scale;
    const auto front = render(matchedDensity.data(), false);
    const auto side = render(matchedDensity.data(), true);
    const auto frontStats = imageStats(front);
    const auto sideStats = imageStats(side);
    GTEST_LOG_(INFO) << "3d-source highres mass-matched image=" << branch.label
        << " frontOccupied/luma=" << frontStats[0] << '/' << frontStats[1]
        << " sideOccupied/luma=" << sideStats[0] << '/' << sideStats[1]
        << " normalizedSurface/slicePeaks=" << matchedMetrics[4] << '/'
        << matchedMetrics[5];
    const auto stem = QString::fromLatin1(branch.label);
    EXPECT_TRUE(savePng(front, QDir(captureDirectory).filePath(
        stem + QStringLiteral("_front.png"))));
    EXPECT_TRUE(savePng(side, QDir(captureDirectory).filePath(
        stem + QStringLiteral("_side.png"))));
  }
}

TEST(ArtifactSmokeRuntimeStandaloneTest,
     MeasuresSingleStepPyroProjectionResidualByIterationCount) {
  using namespace ArtifactCore;
  using namespace ArtifactCore::RayTrace;
  PyroDomain domain;
  domain.min = {-0.75f, 0.0f, -0.5f};
  domain.max = {0.75f, 1.125f, 0.5f};
  domain.voxelSize = 1.0f / 48.0f;
  domain.fixedTimeStep = 1.0 / 60.0;
  domain.boundaryMode = PyroBoundaryMode::Open;
  domain.memoryBudgetBytes = 64u * 1024u * 1024u;
  const auto domainResolution = domain.resolution();
  const VolumeResolution resolution{domainResolution.width,
      domainResolution.height, domainResolution.depth};
  constexpr std::array<int, 4> iterationCounts{12, 24, 48, 96};
  constexpr float twoPi = 6.28318530718f;
  for (const int iterations : iterationCounts) {
    PyroSimulation simulation(domain);
    PyroSimulationSettings settings;
    settings.dissipation = 0.0f;
    settings.coolingRate = 0.0f;
    settings.buoyancy = 0.0f;
    settings.vorticity = 0.0f;
    settings.pressureIterations = static_cast<float>(iterations);
    simulation.setSettings(settings);
    auto density = simulation.fields().densityView();
    auto velocity = simulation.fields().velocityView();
    for (int z = 0; z < resolution.depth; ++z)
      for (int y = 0; y < resolution.height; ++y)
        for (int x = 0; x < resolution.width; ++x) {
          const float nx = (static_cast<float>(x) + 0.5f) /
              static_cast<float>(resolution.width);
          const float ny = (static_cast<float>(y) + 0.5f) /
              static_cast<float>(resolution.height);
          const float nz = (static_cast<float>(z) + 0.5f) /
              static_cast<float>(resolution.depth);
          const auto index = resolution.indexOf(x, y, z);
          const float radiusSquared = (nx - 0.5f) * (nx - 0.5f) +
              (ny - 0.5f) * (ny - 0.5f) + (nz - 0.5f) * (nz - 0.5f);
          density.values[index] = std::exp(-radiusSquared * 28.0f);
          velocity.values[index] = {
              8.0f * std::sin(twoPi * nx),
              5.0f * std::sin(twoPi * ny),
              4.0f * std::sin(twoPi * nz)};
        }
    const auto divergenceRms = [&] {
      double sumSquared = 0.0;
      std::size_t count = 0;
      const auto field = simulation.fields().velocityView();
      for (int z = 1; z + 1 < resolution.depth; ++z)
        for (int y = 1; y + 1 < resolution.height; ++y)
          for (int x = 1; x + 1 < resolution.width; ++x) {
            const auto xp = field.at(x + 1, y, z);
            const auto xm = field.at(x - 1, y, z);
            const auto yp = field.at(x, y + 1, z);
            const auto ym = field.at(x, y - 1, z);
            const auto zp = field.at(x, y, z + 1);
            const auto zm = field.at(x, y, z - 1);
            const double divergence = 0.5 * (xp.x - xm.x + yp.y - ym.y +
                                             zp.z - zm.z);
            sumSquared += divergence * divergence;
            ++count;
          }
      return count > 0 ? std::sqrt(sumSquared / count) : 0.0;
    };
    const double before = divergenceRms();
    simulation.step(1.0 / 60.0);
    const double after = divergenceRms();
    GTEST_LOG_(INFO) << "Pyro one-step projection iterations=" << iterations
        << " divergenceRms before/after=" << before << '/' << after;
    EXPECT_TRUE(std::isfinite(after));
    EXPECT_GT(before, 0.0);
  }
}

void advectTestSmokeTemperature(
    ArtifactCore::NamedVector<float>& temperature,
    ArtifactCore::NamedVector<float>& nextTemperature,
    const ArtifactCore::FluidSolver2D& solver, const float dt,
    const float coolingRate) {
  const int width = solver.width();
  const int height = solver.height();
  const auto index = [width](const int x, const int y) {
    return static_cast<std::size_t>(x + y * width);
  };
  const float dtx = dt * static_cast<float>(width - 2);
  const float dty = dt * static_cast<float>(height - 2);
  const float retention = std::exp(-std::max(0.0f, coolingRate) * dt);
  std::fill(nextTemperature.begin(), nextTemperature.end(), 0.0f);
  for (int y = 1; y < height - 1; ++y) {
    for (int x = 1; x < width - 1; ++x) {
      float velocityX = 0.0f;
      float velocityY = 0.0f;
      solver.getVelocity(x, y, velocityX, velocityY);
      const float backX = std::clamp(
          static_cast<float>(x) - dtx * velocityX, 0.5f,
          static_cast<float>(width) - 1.5f);
      const float backY = std::clamp(
          static_cast<float>(y) - dty * velocityY, 0.5f,
          static_cast<float>(height) - 1.5f);
      const int x0 = static_cast<int>(std::floor(backX));
      const int y0 = static_cast<int>(std::floor(backY));
      const int x1 = x0 + 1;
      const int y1 = y0 + 1;
      const float tx = backX - static_cast<float>(x0);
      const float ty = backY - static_cast<float>(y0);
      const float top = std::lerp(temperature[index(x0, y0)],
                                  temperature[index(x1, y0)], tx);
      const float bottom = std::lerp(temperature[index(x0, y1)],
                                     temperature[index(x1, y1)], tx);
      nextTemperature[index(x, y)] = std::lerp(top, bottom, ty) * retention;
    }
  }
  std::copy(nextTemperature.begin(), nextTemperature.end(), temperature.begin());
}

void applyTestSmokeTemperatureCurlNoise(
    ArtifactCore::FluidSolver2D& solver,
    const ArtifactCore::NamedVector<float>& temperature,
    ArtifactCore::NamedVector<float>& streamFunction, const float dt,
    const float simulationTime, const std::uint32_t seed) {
  const int width = solver.width();
  const int height = solver.height();
  const auto index = [width](const int x, const int y) {
    return static_cast<std::size_t>(x + y * width);
  };
  struct Mode final {
    float wavelengthX;
    float wavelengthY;
    float temporalRate;
    float strength;
    std::uint32_t seedOffset;
  };
  constexpr std::array<Mode, 3> modes{{
      {25.0f, 21.0f, 0.65f, 5.0f, 0xB5297A4Du},
      {19.0f, 28.0f, -0.47f, 3.15f, 0x68E31DA4u},
      {10.0f, 13.0f, 0.91f, 2.5f, 0x1B56C4E9u},
  }};
  std::fill(streamFunction.begin(), streamFunction.end(), 0.0f);
  for (const Mode& mode : modes) {
    for (int y = 1; y < height - 1; ++y) {
      for (int x = 1; x < width - 1; ++x) {
        const auto cell = index(x, y);
        const float envelope = std::clamp(temperature[cell] * 3.0f,
                                          0.0f, 1.0f);
        if (envelope <= 0.0f) continue;
        const float noise = smokeSeededValueNoise(
            static_cast<float>(x) / mode.wavelengthX +
                simulationTime * mode.temporalRate * 0.11f,
            static_cast<float>(y) / mode.wavelengthY -
                simulationTime * mode.temporalRate * 0.07f,
            seed ^ mode.seedOffset);
        streamFunction[cell] += mode.strength * envelope * noise;
      }
    }
  }
  for (int y = 2; y < height - 2; ++y) {
    for (int x = 2; x < width - 2; ++x) {
      const float derivativeX = 0.5f * (
          streamFunction[index(x + 1, y)] -
          streamFunction[index(x - 1, y)]);
      const float derivativeY = 0.5f * (
          streamFunction[index(x, y + 1)] -
          streamFunction[index(x, y - 1)]);
      solver.addVelocity(x, y, derivativeY * dt, -derivativeX * dt);
    }
  }
}

SmokeFieldFrame captureSmokeFieldFrame(
    Artifact::LayerFluidRuntimeState& state, const int width, const int height) {
  SmokeFieldFrame field;
  const std::size_t count = static_cast<std::size_t>(width * height);
  field.density.resize(count);
  field.velocityX.resize(count);
  field.velocityY.resize(count);
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const auto index = static_cast<std::size_t>(y * width + x);
      field.density[index] = state.fluidSolver_->getDensity(x, y);
      state.fluidSolver_->getVelocity(x, y, field.velocityX[index],
                                      field.velocityY[index]);
    }
  }
  return field;
}

SmokeResolutionCapture captureSmokeDensityAtResolution(
    const int width, const int height, const int frameCount,
    const int solverIterations = 1, const float vorticity = 1.5f) {
  SmokeResolutionCapture capture;
  capture.width = width;
  capture.height = height;
  Artifact::LayerFluidRuntimeState state;
  Artifact::LayerSmokeRuntimeSettings settings;
  settings.gridWidth = width;
  settings.gridHeight = height;
  settings.solverIterations = solverIterations;
  settings.emitterX = 0.5f;
  settings.emitterY = 0.78f;
  settings.emitterRadius = 0.08f;
  settings.emitterDensity = 0.8f;
  settings.emitterVelocityY = -1.2f;
  settings.vorticity = vorticity;
  settings.emitterSwirl = 0.6f;

  const auto simulationStart = std::chrono::steady_clock::now();
  for (int frame = 0; frame <= frameCount; ++frame) {
    Artifact::advanceLayerSmokeRuntime(state, frame, 30.0, settings);
  }
  const auto simulationEnd = std::chrono::steady_clock::now();
  capture.simulationMicroseconds = std::chrono::duration<double, std::micro>(
      simulationEnd - simulationStart).count();
  capture.density.resize(static_cast<std::size_t>(width * height));
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const float value = state.fluidSolver_->getDensity(x, y);
      capture.density[static_cast<std::size_t>(y * width + x)] = value;
      capture.maximumDensity = std::max(capture.maximumDensity, value);
      capture.integratedDensity += value;
      if (x > 0) {
        capture.gradientEnergy += std::abs(value - state.fluidSolver_->getDensity(
            x - 1, y));
      }
      if (y > 0) {
        capture.gradientEnergy += std::abs(value - state.fluidSolver_->getDensity(
            x, y - 1));
      }
    }
  }
  capture.image = rasterizeSolverDensity(
      capture.density, width, height, 1.1f, -0.6f, -0.8f, 0.18f,
      0.82f, 0.84f, 0.88f);
  for (int warmup = 0; warmup < 2; ++warmup) {
    capture.image = rasterizeSolverDensity(
        capture.density, width, height, 1.1f, -0.6f, -0.8f, 0.18f,
        0.82f, 0.84f, 0.88f);
  }
  constexpr int kRenderIterations = 5;
  const auto renderingStart = std::chrono::steady_clock::now();
  for (int iteration = 0; iteration < kRenderIterations; ++iteration) {
    capture.image = rasterizeSolverDensity(
        capture.density, width, height, 1.1f, -0.6f, -0.8f, 0.18f,
        0.82f, 0.84f, 0.88f);
  }
  const auto renderingEnd = std::chrono::steady_clock::now();
  capture.renderingMicroseconds = std::chrono::duration<double, std::micro>(
      renderingEnd - renderingStart).count() / kRenderIterations;
  return capture;
}

} // namespace

TEST(ArtifactSmokeRuntimeStandaloneTest,
     ProductionRuntimeSubmitsVisibleStableParticleImages) {
  const SmokeRun first = runProductionSmokeRuntime(0.5f);
  const SmokeRun repeated = runProductionSmokeRuntime(0.5f);
  const SmokeRun shifted = runProductionSmokeRuntime(0.25f);
  const SmokeRun stirred = runProductionSmokeRuntime(0.5f, true);
  const SmokeRun stirredRepeat = runProductionSmokeRuntime(0.5f, true);
  const SmokeRun preEvaluated = runProductionSmokeRuntime(0.5f, false, true);

  ASSERT_FALSE(first.image.isNull());
  EXPECT_EQ(first.image.size(), QSize(kGridWidth * kImageScale,
                                      kGridHeight * kImageScale));
  EXPECT_GT(first.particleCount, 20);
  EXPECT_EQ(first.drawCount, kFrameCount + 1);
  EXPECT_GE(first.reusableCapacity, first.particleCount)
      << "particle submission storage should be recycled between frames";
  EXPECT_NEAR(first.meanParticleR, 0.12, 1.0e-5);
  EXPECT_NEAR(first.meanParticleG, 0.76, 1.0e-5);
  EXPECT_NEAR(first.meanParticleB, 0.28, 1.0e-5);
  EXPECT_LT(first.meanParticleY, 0.84 * kGridHeight)
      << "the particle cloud should rise above the source";
  EXPECT_LT(shifted.meanParticleX, first.meanParticleX)
      << "moving the source left should move the particle cloud left";
  EXPECT_EQ(countDifferentPixels(first.image, repeated.image), 0)
      << "the production runtime should produce deterministic particle output";
  EXPECT_EQ(countDifferentPixels(first.image, preEvaluated.image), 0)
      << "advancing smoke during frame evaluation must not advance it again "
         "when the renderer consumes the same frame";
  EXPECT_GT(countDifferentPixels(first.image, shifted.image), 1000)
      << "moving the production emitter should move the image output";
  EXPECT_GT(countDifferentPixels(first.image, stirred.image), 1000)
      << "the production runtime should apply viewport stir input";
  EXPECT_EQ(countDifferentPixels(stirred.image, stirredRepeat.image), 0)
      << "a fixed ordered stir input sequence should remain deterministic";

  int visiblePixels = 0;
  int topEdgeSmokePixels = 0;
  for (int y = 0; y < first.image.height(); ++y) {
    for (int x = 0; x < first.image.width(); ++x) {
      if (first.image.pixel(x, y) != kBackground) {
        ++visiblePixels;
        if (y == 0) ++topEdgeSmokePixels;
      }
    }
  }
  EXPECT_GT(visiblePixels, 500);
  EXPECT_EQ(topEdgeSmokePixels, 0);

  const QString outputDirectory =
      qEnvironmentVariable("ARTIFACT_SMOKE_RUNTIME_OUTPUT_DIR");
  ASSERT_FALSE(outputDirectory.isEmpty())
      << "Set ARTIFACT_SMOKE_RUNTIME_OUTPUT_DIR to an absolute directory";
  ASSERT_TRUE(QDir().mkpath(outputDirectory.toUtf8().constData()));
  const QString basePath =
      QDir(outputDirectory).filePath(QStringLiteral("smoke_runtime.png"));
  const QString repeatPath = QDir(outputDirectory).filePath(
      QStringLiteral("smoke_runtime_repeat.png"));
  const QString shiftedPath = QDir(outputDirectory).filePath(
      QStringLiteral("smoke_runtime_shifted.png"));
  const QString stirredPath = QDir(outputDirectory).filePath(
      QStringLiteral("smoke_runtime_stirred.png"));
  ASSERT_TRUE(first.image.save(basePath, "PNG")) << basePath.toStdString();
  ASSERT_TRUE(repeated.image.save(repeatPath, "PNG"))
      << repeatPath.toStdString();
  ASSERT_TRUE(shifted.image.save(shiftedPath, "PNG"))
      << shiftedPath.toStdString();
  ASSERT_TRUE(stirred.image.save(stirredPath, "PNG"))
      << stirredPath.toStdString();
  EXPECT_GT(QFileInfo(basePath).size(), 1024);
}

TEST(ArtifactSmokeRuntimeStandaloneTest,
     NullRendererFrameAdvanceCanBeRenderedWithoutDoubleStepping) {
  Artifact::LayerSmokeRuntimeSettings settings;
  settings.gridWidth = kGridWidth;
  settings.gridHeight = kGridHeight;
  settings.emitterDensity = 0.6f;
  settings.emitterVelocityY = -1.2f;
  QMatrix4x4 transform;
  transform.setToIdentity();

  Artifact::LayerFluidRuntimeState afterNullRenderer;
  Artifact::renderLayerSmokeRuntime(
      afterNullRenderer, nullptr,
      QRectF(0.0, 0.0, kGridWidth, kGridHeight), transform, 1.0f,
      0, 30.0, settings);
  Artifact::ArtifactIRenderer resumedRenderer;
  Artifact::renderLayerSmokeRuntime(
      afterNullRenderer, &resumedRenderer,
      QRectF(0.0, 0.0, kGridWidth, kGridHeight), transform, 1.0f,
      0, 30.0, settings);

  Artifact::LayerFluidRuntimeState freshState;
  Artifact::ArtifactIRenderer freshRenderer;
  Artifact::renderLayerSmokeRuntime(
      freshState, &freshRenderer,
      QRectF(0.0, 0.0, kGridWidth, kGridHeight), transform, 1.0f,
      0, 30.0, settings);

  EXPECT_EQ(resumedRenderer.particleDrawCount(), 1);
  EXPECT_EQ(countDifferentPixels(
                rasterizeParticles(resumedRenderer.lastParticleData()),
                rasterizeParticles(freshRenderer.lastParticleData())),
            0);
}

TEST(ArtifactSmokeRuntimeStandaloneTest,
     JitteredParticlesSampleDensityAndVelocityAwayFromIntegerGridPoints) {
  Artifact::LayerSmokeRuntimeSettings settings;
  settings.gridWidth = kGridWidth;
  settings.gridHeight = kGridHeight;
  settings.emitterX = 0.5f;
  settings.emitterY = 0.84f;
  settings.emitterRadius = 0.08f;
  settings.emitterDensity = 0.6f;
  settings.emitterVelocityY = -1.2f;
  settings.emitterSwirl = 0.15f;

  Artifact::LayerFluidRuntimeState state;
  Artifact::advanceLayerSmokeRuntime(state, 24, 30.0, settings);
  QMatrix4x4 transform;
  transform.setToIdentity();
  const auto& data = Artifact::prepareLayerSmokeRenderData(
      state, QRectF(0.0, 0.0, kGridWidth, kGridHeight), transform, 1.0f,
      24, settings);

  double densityDifference = 0.0;
  double velocityDifference = 0.0;
  int comparedParticles = 0;
  for (const auto& particle : data.particles) {
    const float sampleGridX = particle.px * static_cast<float>(kGridWidth - 1) /
                              static_cast<float>(kGridWidth);
    const float sampleGridY = particle.py * static_cast<float>(kGridHeight - 1) /
                              static_cast<float>(kGridHeight);
    const auto [sampleCountX, sampleCountY] =
        expectedSmokeRenderSampleCounts(kGridWidth, kGridHeight);
    const float sampleStepX = static_cast<float>(kGridWidth - 1) /
                              static_cast<float>(sampleCountX - 1);
    const float sampleStepY = static_cast<float>(kGridHeight - 1) /
                              static_cast<float>(sampleCountY - 1);
    const int sampleX = std::clamp(static_cast<int>(std::lround(
        sampleGridX / sampleStepX)), 0, sampleCountX - 1);
    const int sampleY = std::clamp(static_cast<int>(std::lround(
        sampleGridY / sampleStepY)), 0, sampleCountY - 1);
    const float baseGridX = static_cast<float>(sampleX) * sampleStepX;
    const float baseGridY = static_cast<float>(sampleY) * sampleStepY;
    const int gx = std::clamp(static_cast<int>(std::lround(baseGridX)),
                              0, kGridWidth - 1);
    const int gy = std::clamp(static_cast<int>(std::lround(baseGridY)),
                              0, kGridHeight - 1);
    std::uint32_t sampleHash = static_cast<std::uint32_t>(sampleX) * 0x9E3779B9u ^
                               static_cast<std::uint32_t>(sampleY) * 0x85EBCA6Bu;
    sampleHash ^= sampleHash >> 16u;
    sampleHash *= 0x7FEB352Du;
    sampleHash ^= sampleHash >> 15u;
    const float jitterX = static_cast<float>(sampleHash & 0xFFFFu) *
                              (2.0f / 65535.0f) - 1.0f;
    const float jitterY = static_cast<float>(sampleHash >> 16u) *
                              (2.0f / 65535.0f) - 1.0f;
    const float expectedGridX = std::clamp(
        baseGridX + jitterX * sampleStepX * 0.30f,
        0.0f, static_cast<float>(kGridWidth - 1));
    const float expectedGridY = std::clamp(
        baseGridY + jitterY * sampleStepY * 0.30f,
        0.0f, static_cast<float>(kGridHeight - 1));
    const float actualGridX = particle.px * static_cast<float>(kGridWidth - 1) /
                              static_cast<float>(kGridWidth);
    const float actualGridY = particle.py * static_cast<float>(kGridHeight - 1) /
                              static_cast<float>(kGridHeight);
    ASSERT_NEAR(actualGridX, expectedGridX, 2.0e-4f);
    ASSERT_NEAR(actualGridY, expectedGridY, 2.0e-4f);

    const float sampledDensity = std::pow(particle.a / 0.18f, 2.0f);
    const float integerDensity = state.fluidSolver_->getDensity(gx, gy);
    densityDifference += std::abs(sampledDensity - integerDensity);
    float integerVelocityX = 0.0f;
    float integerVelocityY = 0.0f;
    state.fluidSolver_->getVelocity(gx, gy, integerVelocityX, integerVelocityY);
    const float sampledVelocityX = particle.vx / 24.0f;
    const float sampledVelocityY = particle.vy / 24.0f;
    velocityDifference += std::abs(sampledVelocityX - integerVelocityX) +
                          std::abs(sampledVelocityY - integerVelocityY);
    ++comparedParticles;
  }

  ASSERT_GT(comparedParticles, 20);
  EXPECT_GT(densityDifference / comparedParticles, 1.0e-5)
      << "particle alpha should encode density sampled at the jittered position";
  EXPECT_GT(velocityDifference / comparedParticles, 1.0e-5)
      << "particle velocity should come from the jittered fractional position";
}

TEST(ArtifactSmokeRuntimeStandaloneTest,
     GpuParticlePreparationSkipsUnusedVelocitySampling) {
  Artifact::LayerSmokeRuntimeSettings settings;
  settings.gridWidth = kGridWidth;
  settings.gridHeight = kGridHeight;
  settings.solverIterations = 1;
  settings.emitterX = 0.5f;
  settings.emitterY = 0.78f;
  settings.emitterRadius = 0.08f;
  settings.emitterDensity = 0.8f;
  settings.emitterVelocityY = -1.2f;

  Artifact::LayerFluidRuntimeState cpuState;
  Artifact::LayerFluidRuntimeState gpuState;
  for (int frame = 0; frame <= kFrameCount; ++frame) {
    Artifact::advanceLayerSmokeRuntime(cpuState, frame, 30.0, settings);
    Artifact::advanceLayerSmokeRuntime(gpuState, frame, 30.0, settings);
  }

  QMatrix4x4 transform;
  transform.setToIdentity();
  const QRectF bounds(0.0, 0.0, kGridWidth, kGridHeight);
  const auto& cpuData = Artifact::prepareLayerSmokeRenderData(
      cpuState, bounds, transform, 1.0f, kFrameCount, settings);
  Artifact::ArtifactIRenderer gpuRenderer;
  Artifact::renderLayerSmokeRuntime(
      gpuState, &gpuRenderer, bounds, transform, 1.0f, kFrameCount,
      30.0, settings);
  const auto& gpuData = gpuRenderer.lastParticleData();

  ASSERT_GT(cpuData.particles.size(), 20u);
  ASSERT_EQ(cpuData.particles.size(), gpuData.particles.size());
  bool cpuHasNonzeroVelocity = false;
  for (std::size_t index = 0; index < cpuData.particles.size(); ++index) {
    const auto& cpuParticle = cpuData.particles[index];
    const auto& gpuParticle = gpuData.particles[index];
    EXPECT_FLOAT_EQ(cpuParticle.px, gpuParticle.px);
    EXPECT_FLOAT_EQ(cpuParticle.py, gpuParticle.py);
    EXPECT_FLOAT_EQ(cpuParticle.a, gpuParticle.a);
    EXPECT_FLOAT_EQ(gpuParticle.vx, 0.0f);
    EXPECT_FLOAT_EQ(gpuParticle.vy, 0.0f);
    cpuHasNonzeroVelocity = cpuHasNonzeroVelocity ||
        std::abs(cpuParticle.vx) > 1.0e-5f ||
        std::abs(cpuParticle.vy) > 1.0e-5f;
  }
  EXPECT_TRUE(cpuHasNonzeroVelocity)
      << "the CPU surface path must retain velocity for particle orientation";
}

TEST(ArtifactSmokeRuntimeStandaloneTest,
     CappedSampleAlphaGainTradeoffRemainsVisibleAndUnsaturated) {
  const SmokeRun baseline = runProductionSmokeRuntime(
      0.5f, false, false, 30.0, 44, 0.1f, 0.15f, 1, 1.0f);
  const SmokeRun gain120 = runProductionSmokeRuntime(
      0.5f, false, false, 30.0, 44, 0.1f, 0.15f, 1, 1.20f);
  const SmokeRun gain135 = runProductionSmokeRuntime(
      0.5f, false, false, 30.0, 44, 0.1f, 0.15f, 1, 1.35f);
  const SmokeImageMetrics baselineMetrics = measureSmokeImage(baseline.image);
  const SmokeImageMetrics gain120Metrics = measureSmokeImage(gain120.image);
  const SmokeImageMetrics gain135Metrics = measureSmokeImage(gain135.image);

  EXPECT_GT(baselineMetrics.visiblePixels, 500);
  EXPECT_GT(gain120Metrics.visiblePixels, 500);
  EXPECT_GT(gain135Metrics.visiblePixels, 500);
  EXPECT_EQ(baselineMetrics.saturatedPixels, 0);
  EXPECT_EQ(gain120Metrics.saturatedPixels, 0);
  EXPECT_EQ(gain135Metrics.saturatedPixels, 0);
  EXPECT_GT(gain120Metrics.greenExcess, baselineMetrics.greenExcess);
  EXPECT_GT(gain135Metrics.greenExcess, gain120Metrics.greenExcess);
  GTEST_LOG_(INFO) << "Capped-grid alpha gain (visible / green excess / "
      << "saturated): 1.00=" << baselineMetrics.visiblePixels << '/'
      << baselineMetrics.greenExcess << '/' << baselineMetrics.saturatedPixels
      << "; 1.20=" << gain120Metrics.visiblePixels << '/'
      << gain120Metrics.greenExcess << '/' << gain120Metrics.saturatedPixels
      << "; 1.35=" << gain135Metrics.visiblePixels << '/'
      << gain135Metrics.greenExcess << '/' << gain135Metrics.saturatedPixels;

  const QString outputDirectory =
      qEnvironmentVariable("ARTIFACT_SMOKE_RUNTIME_OUTPUT_DIR");
  if (!outputDirectory.isEmpty()) {
    ASSERT_TRUE(QDir().mkpath(outputDirectory.toUtf8().constData()));
    EXPECT_TRUE(baseline.image.save(QDir(outputDirectory).filePath(
        QStringLiteral("smoke_alpha_gain_1p00.png")), "PNG"));
    EXPECT_TRUE(gain120.image.save(QDir(outputDirectory).filePath(
        QStringLiteral("smoke_alpha_gain_1p20.png")), "PNG"));
    EXPECT_TRUE(gain135.image.save(QDir(outputDirectory).filePath(
        QStringLiteral("smoke_alpha_gain_1p35.png")), "PNG"));
  }
}

TEST(ArtifactSmokeRuntimeStandaloneTest,
     ReportsCappedGridReconstructionParameterTradeoffs) {
  constexpr int kReconstructionFrame = 44;
  struct Variant {
    const char* name;
    float jitter;
    float sizeScale;
    float alphaScale;
  };
  constexpr std::array<Variant, 6> variants{{
      {"baseline", 0.30f, 1.00f, 1.00f},
      {"lower-jitter", 0.10f, 1.00f, 1.00f},
      {"larger-kernel", 0.30f, 1.18f, 1.00f},
      {"larger-kernel-compensated-1p125", 0.30f, 1.125f, 0.79f},
      {"larger-kernel-compensated-1p18", 0.30f, 1.18f, 0.72f},
      {"larger-kernel-compensated-1p25", 0.30f, 1.25f, 0.64f},
  }};
  std::array<SmokeImageMetrics, variants.size()> metrics{};
  std::array<ArtifactCore::ParticleRenderData, variants.size()>
      reconstructionStreams{};
  std::vector<float> solverDensityField;
  for (std::size_t variantIndex = 0; variantIndex < variants.size();
       ++variantIndex) {
    const auto& variant = variants[variantIndex];
    Artifact::LayerSmokeRuntimeSettings settings;
    settings.gridWidth = kGridWidth;
    settings.gridHeight = kGridHeight;
    settings.solverIterations = 1;
    settings.emitterX = 0.5f;
    settings.emitterY = 0.78f;
    settings.emitterRadius = 0.08f;
    settings.emitterDensity = 0.8f;
    settings.emitterVelocityY = -1.2f;
    settings.vorticity = 1.5f;
    settings.emitterSwirl = 0.6f;

    Artifact::LayerFluidRuntimeState state;
    for (int frame = 0; frame <= kReconstructionFrame; ++frame) {
      Artifact::advanceLayerSmokeRuntime(state, frame, 30.0, settings);
    }
    QMatrix4x4 transform;
    transform.setToIdentity();
    const QRectF bounds(0.0, 0.0, kGridWidth, kGridHeight);
    const auto& renderData = Artifact::prepareLayerSmokeRenderData(
        state, bounds, transform, 1.0f, kReconstructionFrame, settings);
    if (variantIndex == 0) {
      solverDensityField.resize(
          static_cast<std::size_t>(kGridWidth * kGridHeight));
      for (int y = 0; y < kGridHeight; ++y) {
        for (int x = 0; x < kGridWidth; ++x) {
          solverDensityField[static_cast<std::size_t>(y * kGridWidth + x)] =
              state.fluidSolver_->getDensity(x, y);
        }
      }
    }
    auto adjustedData = renderData;
    adjustedData.particles.clear();
    adjustedData.particles.reserve(renderData.particles.size());
    for (auto particle : renderData.particles) {
      const float gridX = particle.px /
          static_cast<float>(kGridWidth) * static_cast<float>(kGridWidth - 1);
      const float gridY = particle.py /
          static_cast<float>(kGridHeight) * static_cast<float>(kGridHeight - 1);
      const auto [sampleCountX, sampleCountY] =
          expectedSmokeRenderSampleCounts(kGridWidth, kGridHeight);
      const float sampleStepX = static_cast<float>(kGridWidth - 1) /
                                static_cast<float>(sampleCountX - 1);
      const float sampleStepY = static_cast<float>(kGridHeight - 1) /
                                static_cast<float>(sampleCountY - 1);
      const float latticeX = gridX / sampleStepX;
      const float latticeY = gridY / sampleStepY;
      const int sampleX = std::clamp(
          static_cast<int>(std::lround(latticeX)), 0, sampleCountX - 1);
      const int sampleY = std::clamp(
          static_cast<int>(std::lround(latticeY)), 0, sampleCountY - 1);
      const float baseGridX = static_cast<float>(sampleX) * sampleStepX;
      const float baseGridY = static_cast<float>(sampleY) * sampleStepY;
      const float actualGridX = particle.px *
          static_cast<float>(kGridWidth - 1) / static_cast<float>(kGridWidth);
      const float actualGridY = particle.py *
          static_cast<float>(kGridHeight - 1) / static_cast<float>(kGridHeight);
      std::uint32_t sampleHash =
          static_cast<std::uint32_t>(sampleX) * 0x9E3779B9u ^
          static_cast<std::uint32_t>(sampleY) * 0x85EBCA6Bu;
      sampleHash ^= sampleHash >> 16u;
      sampleHash *= 0x7FEB352Du;
      sampleHash ^= sampleHash >> 15u;
      const float originalJitterX =
          static_cast<float>(sampleHash & 0xFFFFu) * (2.0f / 65535.0f) - 1.0f;
      const float originalJitterY =
          static_cast<float>(sampleHash >> 16u) * (2.0f / 65535.0f) - 1.0f;
      particle.px += (variant.jitter - 0.30f) * originalJitterX * sampleStepX;
      particle.py += (variant.jitter - 0.30f) * originalJitterY * sampleStepY;
      EXPECT_NEAR(actualGridX, std::clamp(baseGridX +
          originalJitterX * sampleStepX * 0.30f, 0.0f,
          static_cast<float>(kGridWidth - 1)), 0.01f);
      EXPECT_NEAR(actualGridY, std::clamp(baseGridY +
          originalJitterY * sampleStepY * 0.30f, 0.0f,
          static_cast<float>(kGridHeight - 1)), 0.01f);
      particle.size *= variant.sizeScale / 1.25f;
      particle.a *= variant.alphaScale / 0.64f;
      adjustedData.particles.push_back(particle);
    }
    metrics[variantIndex] = measureSmokeImage(rasterizeParticles(adjustedData));
    GTEST_LOG_(INFO) << "Smoke reconstruction " << variant.name
        << ": particles=" << adjustedData.particles.size()
        << " visible=" << metrics[variantIndex].visiblePixels
        << " greenExcess=" << metrics[variantIndex].greenExcess
        << " saturated=" << metrics[variantIndex].saturatedPixels;
    const QString outputDirectory =
        qEnvironmentVariable("ARTIFACT_SMOKE_RUNTIME_OUTPUT_DIR");
    if (!outputDirectory.isEmpty()) {
      ASSERT_TRUE(QDir().mkpath(outputDirectory.toUtf8().constData()));
      const QString fileName = QStringLiteral("smoke_reconstruction_%1.png")
          .arg(QString::fromLatin1(variant.name));
      EXPECT_TRUE(rasterizeParticles(adjustedData).save(
          QDir(outputDirectory).filePath(fileName), "PNG"));
    }
    reconstructionStreams[variantIndex] = std::move(adjustedData);
  }
  EXPECT_EQ(metrics[0].saturatedPixels, 0);
  EXPECT_EQ(metrics[1].saturatedPixels, 0);
  EXPECT_EQ(metrics[2].saturatedPixels, 0);
  EXPECT_EQ(metrics[3].saturatedPixels, 0);
  EXPECT_EQ(metrics[4].saturatedPixels, 0);
  EXPECT_EQ(metrics[5].saturatedPixels, 0);
  EXPECT_NEAR(metrics[1].greenExcess,
              metrics[0].greenExcess, metrics[0].greenExcess * 0.01);
  EXPECT_GT(metrics[2].visiblePixels, metrics[0].visiblePixels);
  EXPECT_GT(metrics[2].greenExcess,
            metrics[0].greenExcess * 1.20);

  struct RasterCost final {
    double meanMicroseconds = 0.0;
    std::uint64_t boundingBoxPixelVisits = 0;
    std::uint64_t sink = 0;
  };
  const auto measureRasterCost = [](const ArtifactCore::ParticleRenderData& data) {
    RasterCost cost;
    const int imageWidth = kGridWidth * kImageScale;
    const int imageHeight = kGridHeight * kImageScale;
    for (const auto& particle : data.particles) {
      const int centerX = static_cast<int>(
          std::lround(particle.px * kImageScale));
      const int centerY = static_cast<int>(
          std::lround(particle.py * kImageScale));
      const int radius = std::max(1, static_cast<int>(std::lround(
          std::max(0.375f, particle.size * 10.0f) * kImageScale)));
      const int minX = std::max(0, centerX - radius);
      const int maxX = std::min(imageWidth - 1, centerX + radius);
      const int minY = std::max(0, centerY - radius);
      const int maxY = std::min(imageHeight - 1, centerY + radius);
      if (minX <= maxX && minY <= maxY) {
        cost.boundingBoxPixelVisits += static_cast<std::uint64_t>(maxX - minX + 1) *
                                       static_cast<std::uint64_t>(maxY - minY + 1);
      }
    }

    constexpr int kWarmupIterations = 4;
    constexpr int kMeasuredIterations = 32;
    for (int iteration = 0; iteration < kWarmupIterations; ++iteration) {
      const QImage image = rasterizeParticles(data);
      cost.sink += static_cast<std::uint64_t>(
          qGreen(image.pixel(iteration, iteration)));
    }
    const auto start = std::chrono::steady_clock::now();
    for (int iteration = 0; iteration < kMeasuredIterations; ++iteration) {
      const QImage image = rasterizeParticles(data);
      cost.sink += static_cast<std::uint64_t>(qGreen(image.pixel(
          (iteration * 37) % imageWidth, (iteration * 19) % imageHeight)));
    }
    const auto end = std::chrono::steady_clock::now();
    cost.meanMicroseconds = std::chrono::duration<double, std::micro>(
        end - start).count() / static_cast<double>(kMeasuredIterations);
    return cost;
  };
  const RasterCost baselineRasterCost = measureRasterCost(
      reconstructionStreams[0]);
  const RasterCost compensated1125RasterCost = measureRasterCost(
      reconstructionStreams[3]);
  const RasterCost compensated118RasterCost = measureRasterCost(
      reconstructionStreams[4]);
  const RasterCost compensated125RasterCost = measureRasterCost(
      reconstructionStreams[5]);
  GTEST_LOG_(INFO) << "Smoke software reconstruction cost proxy (768x576, "
      << "mean us / bounding-box pixel visits per frame): baseline="
      << baselineRasterCost.meanMicroseconds << '/'
      << baselineRasterCost.boundingBoxPixelVisits << "; compensated="
      << compensated1125RasterCost.meanMicroseconds << '/'
      << compensated1125RasterCost.boundingBoxPixelVisits << ", "
      << compensated118RasterCost.meanMicroseconds << '/'
      << compensated118RasterCost.boundingBoxPixelVisits << ", "
      << compensated125RasterCost.meanMicroseconds << '/'
      << compensated125RasterCost.boundingBoxPixelVisits << "; sink="
      << baselineRasterCost.sink + compensated1125RasterCost.sink +
             compensated118RasterCost.sink + compensated125RasterCost.sink;
  EXPECT_GT(compensated125RasterCost.boundingBoxPixelVisits,
            baselineRasterCost.boundingBoxPixelVisits);

  const QImage reconstructedUnlit = rasterizeSolverDensity(
      solverDensityField, kGridWidth, kGridHeight, 1.1f, -0.6f, -0.8f, 0.0f,
      0.82f, 0.84f, 0.88f);
  const QImage reconstructedLit = rasterizeSolverDensity(
      solverDensityField, kGridWidth, kGridHeight, 1.1f, -0.6f, -0.8f, 0.18f,
      0.82f, 0.84f, 0.88f);
  const QImage reconstructedDense = rasterizeSolverDensity(
      solverDensityField, kGridWidth, kGridHeight, 2.0f, -0.6f, -0.8f, 0.18f,
      0.82f, 0.84f, 0.88f);
  const SmokeImageMetrics reconstructedUnlitMetrics =
      measureSmokeImage(reconstructedUnlit);
  const SmokeImageMetrics reconstructedLitMetrics =
      measureSmokeImage(reconstructedLit);
  const SmokeImageMetrics reconstructedDenseMetrics =
      measureSmokeImage(reconstructedDense);
  GTEST_LOG_(INFO) << "Smoke density reconstruction (visible / green excess / "
      << "saturated): unlit=" << reconstructedUnlitMetrics.visiblePixels << '/'
      << reconstructedUnlitMetrics.greenExcess << '/'
      << reconstructedUnlitMetrics.saturatedPixels << "; lit="
      << reconstructedLitMetrics.visiblePixels << '/'
      << reconstructedLitMetrics.greenExcess << '/'
      << reconstructedLitMetrics.saturatedPixels << "; denser="
      << reconstructedDenseMetrics.visiblePixels << '/'
      << reconstructedDenseMetrics.greenExcess << '/'
      << reconstructedDenseMetrics.saturatedPixels;
  EXPECT_GT(reconstructedUnlitMetrics.visiblePixels, 500);
  EXPECT_GT(reconstructedLitMetrics.visiblePixels, 500);
  EXPECT_GT(reconstructedDenseMetrics.visiblePixels, 500);
  EXPECT_EQ(reconstructedUnlitMetrics.saturatedPixels, 0);
  EXPECT_EQ(reconstructedLitMetrics.saturatedPixels, 0);
  EXPECT_EQ(reconstructedDenseMetrics.saturatedPixels, 0);
  EXPECT_GT(countDifferentPixels(reconstructedUnlit, reconstructedLit), 1000);
  EXPECT_GT(countDifferentPixels(reconstructedLit, reconstructedDense), 1000);
  const QString outputDirectory =
      qEnvironmentVariable("ARTIFACT_SMOKE_RUNTIME_OUTPUT_DIR");
  if (!outputDirectory.isEmpty()) {
    EXPECT_TRUE(reconstructedUnlit.save(QDir(outputDirectory).filePath(
        QStringLiteral("smoke_density_unlit.png")), "PNG"));
    EXPECT_TRUE(reconstructedLit.save(QDir(outputDirectory).filePath(
        QStringLiteral("smoke_density_lit_shadow.png")), "PNG"));
    EXPECT_TRUE(reconstructedDense.save(QDir(outputDirectory).filePath(
        QStringLiteral("smoke_density_dense_lit_shadow.png")), "PNG"));
  }
}

TEST(ArtifactSmokeRuntimeStandaloneTest,
     ComparesSmokeParticleSamplingResolutionAtMatchedExposure) {
  Artifact::LayerSmokeRuntimeSettings settings;
  settings.gridWidth = kGridWidth;
  settings.gridHeight = kGridHeight;
  settings.solverIterations = 1;
  settings.emitterX = 0.5f;
  settings.emitterY = 0.78f;
  settings.emitterRadius = 0.08f;
  settings.emitterDensity = 0.8f;
  settings.emitterVelocityY = -1.2f;
  settings.vorticity = 1.5f;
  settings.emitterSwirl = 0.6f;
  settings.colorR = 0.12f;
  settings.colorG = 0.76f;
  settings.colorB = 0.28f;

  Artifact::LayerFluidRuntimeState state;
  constexpr int finalFrame = 44;
  for (int frame = 0; frame <= finalFrame; ++frame)
    Artifact::advanceLayerSmokeRuntime(state, frame, 30.0, settings);
  ASSERT_NE(state.fluidSolver_, nullptr);

  QMatrix4x4 transform;
  transform.setToIdentity();
  const QRectF bounds(0.0, 0.0, kGridWidth, kGridHeight);
  const auto& productionData = Artifact::prepareLayerSmokeRenderData(
      state, bounds, transform, 1.0f, finalFrame, settings);
  const QImage baseline = rasterizeParticles(productionData);

  const auto makeResampledParticles = [&](const int sampleCountX,
                                           const int sampleCountY) {
    ArtifactCore::ParticleRenderData data;
    data.options.blend = ArtifactCore::ParticleBlendPolicy::Alpha;
    data.options.billboard = ArtifactCore::ParticleBillboardPolicy::ScreenAligned;
    const float stepX = static_cast<float>(kGridWidth - 1) /
        static_cast<float>(std::max(1, sampleCountX - 1));
    const float stepY = static_cast<float>(kGridHeight - 1) /
        static_cast<float>(std::max(1, sampleCountY - 1));
    const float spacingX = static_cast<float>(bounds.width()) /
        static_cast<float>(std::max(1, sampleCountX - 1));
    const float spacingY = static_cast<float>(bounds.height()) /
        static_cast<float>(std::max(1, sampleCountY - 1));
    const float sampleSpacing = std::max(spacingX, spacingY);
    data.particles.reserve(static_cast<std::size_t>(sampleCountX * sampleCountY));
    for (int sy = 0; sy < sampleCountY; ++sy) {
      for (int sx = 0; sx < sampleCountX; ++sx) {
        std::uint32_t hash = static_cast<std::uint32_t>(sx) * 0x9E3779B9u ^
            static_cast<std::uint32_t>(sy) * 0x85EBCA6Bu;
        hash ^= hash >> 16u;
        hash *= 0x7FEB352Du;
        hash ^= hash >> 15u;
        const float jitterX = static_cast<float>(hash & 0xFFFFu) *
            (2.0f / 65535.0f) - 1.0f;
        const float jitterY = static_cast<float>(hash >> 16u) *
            (2.0f / 65535.0f) - 1.0f;
        const float gx = std::clamp(static_cast<float>(sx) * stepX +
            jitterX * stepX * 0.30f, 0.0f,
            static_cast<float>(kGridWidth - 1));
        const float gy = std::clamp(static_cast<float>(sy) * stepY +
            jitterY * stepY * 0.30f, 0.0f,
            static_cast<float>(kGridHeight - 1));
        const int x0 = static_cast<int>(std::floor(gx));
        const int y0 = static_cast<int>(std::floor(gy));
        const int x1 = std::min(x0 + 1, kGridWidth - 1);
        const int y1 = std::min(y0 + 1, kGridHeight - 1);
        const float tx = gx - static_cast<float>(x0);
        const float ty = gy - static_cast<float>(y0);
        const auto densityAt = [&](const int x, const int y) {
          return state.fluidSolver_->getDensity(x, y);
        };
        const float density = std::lerp(
            std::lerp(densityAt(x0, y0), densityAt(x1, y0), tx),
            std::lerp(densityAt(x0, y1), densityAt(x1, y1), tx), ty);
        if (density < 0.025f) continue;
        ArtifactCore::ParticleVertex particle{};
        particle.px = static_cast<float>(bounds.left()) +
            gx / static_cast<float>(kGridWidth - 1) *
                static_cast<float>(bounds.width());
        particle.py = static_cast<float>(bounds.top()) +
            gy / static_cast<float>(kGridHeight - 1) *
                static_cast<float>(bounds.height());
        particle.ppx = particle.px;
        particle.ppy = particle.py;
        particle.r = settings.colorR;
        particle.g = settings.colorG;
        particle.b = settings.colorB;
        particle.a = std::clamp(std::sqrt(density) * 0.18f,
            0.01f, 0.18f) * 0.79f;
        particle.size = sampleSpacing * (0.118f + density * 0.02f) * 1.125f;
        data.particles.push_back(particle);
      }
    }
    return data;
  };

  struct Variant final {
    const char* name;
    int samplesX;
    int samplesY;
    ArtifactCore::ParticleRenderData data;
    QImage image;
    SmokeImageMetrics metrics;
    double alphaScale = 1.0;
  };
  std::array<Variant, 3> variants{{
      {"current-40x40", 40, 40, productionData, baseline,
       measureSmokeImage(baseline), 1.0},
      {"resample-64x48", 64, 48, makeResampledParticles(64, 48), {}, {}, 1.0},
      {"resample-96x72", 96, 72, makeResampledParticles(96, 72), {}, {}, 1.0},
  }};
  ASSERT_GT(variants[0].data.particles.size(), 20u);
  EXPECT_EQ(variants[0].data.particles.size(),
            productionData.particles.size());

  for (std::size_t index = 1; index < variants.size(); ++index) {
    float low = 0.0f;
    float high = 2.0f;
    for (int iteration = 0; iteration < 18; ++iteration) {
      const float mid = 0.5f * (low + high);
      const auto candidate = measureSmokeImage(
          rasterizeParticles(variants[index].data, mid));
      if (candidate.greenExcess < variants[0].metrics.greenExcess) low = mid;
      else high = mid;
    }
    variants[index].alphaScale = 0.5f * (low + high);
    variants[index].image = rasterizeParticles(
        variants[index].data, static_cast<float>(variants[index].alphaScale));
    variants[index].metrics = measureSmokeImage(variants[index].image);
  }

  const auto highPassRms = [](const QImage& image) {
    double squared = 0.0;
    std::size_t count = 0;
    for (int y = 1; y + 1 < image.height(); ++y)
      for (int x = 1; x + 1 < image.width(); ++x) {
        const double center = qGreen(image.pixel(x, y));
        const double cross = 0.25 * (qGreen(image.pixel(x - 1, y)) +
            qGreen(image.pixel(x + 1, y)) + qGreen(image.pixel(x, y - 1)) +
            qGreen(image.pixel(x, y + 1)));
        const double residual = center - cross;
        squared += residual * residual;
        ++count;
      }
    return count ? std::sqrt(squared / count) : 0.0;
  };
  const QString outputDirectory =
      qEnvironmentVariable("ARTIFACT_SMOKE_RUNTIME_OUTPUT_DIR");
  for (auto& variant : variants) {
    const double hp = highPassRms(variant.image);
    GTEST_LOG_(INFO) << "Smoke reconstruction sampling " << variant.name
        << " particles=" << variant.data.particles.size()
        << " alphaScale=" << variant.alphaScale
        << " visible=" << variant.metrics.visiblePixels
        << " greenExcess=" << variant.metrics.greenExcess
        << " hp-rms=" << hp
        << " saturated=" << variant.metrics.saturatedPixels;
    EXPECT_EQ(variant.metrics.saturatedPixels, 0);
    EXPECT_NEAR(static_cast<double>(variant.metrics.greenExcess),
        static_cast<double>(variants[0].metrics.greenExcess),
        static_cast<double>(variants[0].metrics.greenExcess) * 0.01);
    if (!outputDirectory.isEmpty()) {
      ASSERT_TRUE(QDir().mkpath(outputDirectory.toUtf8().constData()));
      EXPECT_TRUE(variant.image.save(QDir(outputDirectory).filePath(
          QStringLiteral("smoke_sampling_%1.png").arg(
              QString::fromLatin1(variant.name))), "PNG"));
    }
  }
}

TEST(ArtifactSmokeRuntimeStandaloneTest,
     ReportsDensityResolutionQualityAndCostTradeoff) {
  constexpr int kFrameCountForResolution = 44;
  const std::array<SmokeResolutionCapture, 3> captures{{
      captureSmokeDensityAtResolution(96, 72, kFrameCountForResolution),
      captureSmokeDensityAtResolution(128, 96, kFrameCountForResolution),
      captureSmokeDensityAtResolution(192, 144, kFrameCountForResolution),
  }};
  for (const auto& capture : captures) {
    const SmokeImageMetrics imageMetrics = measureSmokeImage(capture.image);
    GTEST_LOG_(INFO) << "Smoke density resolution " << capture.width << 'x'
        << capture.height << ": simulation="
        << capture.simulationMicroseconds / kFrameCountForResolution
        << " us/frame, densityMax=" << capture.maximumDensity
        << " integratedDensity=" << capture.integratedDensity
        << " gradientEnergy=" << capture.gradientEnergy
        << " render=" << capture.renderingMicroseconds << " us, visible="
        << imageMetrics.visiblePixels << " greenExcess="
        << imageMetrics.greenExcess << " saturated="
        << imageMetrics.saturatedPixels;
    EXPECT_GT(capture.maximumDensity, 0.0f);
    EXPECT_GT(imageMetrics.visiblePixels, 500);
    EXPECT_EQ(imageMetrics.saturatedPixels, 0);
  }
  EXPECT_GT(countDifferentPixels(captures[0].image, captures[1].image), 1000);
  EXPECT_GT(countDifferentPixels(captures[1].image, captures[2].image), 1000);

  const QImage detailLight = rasterizeSolverDensity(
      captures[0].density, captures[0].width, captures[0].height,
      1.1f, -0.6f, -0.8f, 0.18f, 0.82f, 0.84f, 0.88f, 0.45f);
  const QImage detailStrong = rasterizeSolverDensity(
      captures[0].density, captures[0].width, captures[0].height,
      1.1f, -0.6f, -0.8f, 0.18f, 0.82f, 0.84f, 0.88f, 0.9f);
  const SmokeImageMetrics baseMetrics = measureSmokeImage(captures[0].image);
  const SmokeImageMetrics lightDetailMetrics = measureSmokeImage(detailLight);
  const SmokeImageMetrics strongDetailMetrics = measureSmokeImage(detailStrong);
  GTEST_LOG_(INFO) << "Smoke procedural density detail (visible / green excess): "
      << "base=" << baseMetrics.visiblePixels << '/'
      << baseMetrics.greenExcess << "; light="
      << lightDetailMetrics.visiblePixels << '/'
      << lightDetailMetrics.greenExcess << "; strong="
      << strongDetailMetrics.visiblePixels << '/'
      << strongDetailMetrics.greenExcess;
  EXPECT_GT(countDifferentPixels(captures[0].image, detailLight), 1000);
  EXPECT_GT(countDifferentPixels(detailLight, detailStrong), 1000);
  EXPECT_EQ(lightDetailMetrics.saturatedPixels, 0);
  EXPECT_EQ(strongDetailMetrics.saturatedPixels, 0);

  const QString outputDirectory =
      qEnvironmentVariable("ARTIFACT_SMOKE_RUNTIME_OUTPUT_DIR");
  if (!outputDirectory.isEmpty()) {
    ASSERT_TRUE(QDir().mkpath(outputDirectory.toUtf8().constData()));
    for (const auto& capture : captures) {
      const QString name = QStringLiteral("smoke_density_%1x%2.png")
          .arg(capture.width).arg(capture.height);
      EXPECT_TRUE(capture.image.save(
          QDir(outputDirectory).filePath(name), "PNG"));
    }
    EXPECT_TRUE(detailLight.save(QDir(outputDirectory).filePath(
        QStringLiteral("smoke_density_detail_light.png")), "PNG"));
    EXPECT_TRUE(detailStrong.save(QDir(outputDirectory).filePath(
        QStringLiteral("smoke_density_detail_strong.png")), "PNG"));
  }
}

TEST(ArtifactSmokeRuntimeStandaloneTest,
     ReportsProductionIterationVorticityDetailTradeoff) {
  constexpr int kFrameCount = 22;
  constexpr int kProductionSolverIterations = 20;
  constexpr std::array<float, 3> vorticityStrengths{0.1f, 0.8f, 1.5f};
  std::array<SmokeResolutionCapture, vorticityStrengths.size()> captures;
  for (std::size_t index = 0; index < captures.size(); ++index) {
    captures[index] = captureSmokeDensityAtResolution(
        96, 72, kFrameCount, kProductionSolverIterations,
        vorticityStrengths[index]);
    const auto metrics = measureSmokeImage(captures[index].image);
    GTEST_LOG_(INFO) << "Smoke product-iteration vorticity="
        << vorticityStrengths[index] << " simulation="
        << captures[index].simulationMicroseconds / kFrameCount
        << " us/frame mass=" << captures[index].integratedDensity
        << " gradientEnergy=" << captures[index].gradientEnergy
        << " visible=" << metrics.visiblePixels
        << " greenExcess=" << metrics.greenExcess;
    EXPECT_GT(captures[index].maximumDensity, 0.0f);
    EXPECT_GT(metrics.visiblePixels, 500);
    EXPECT_EQ(metrics.saturatedPixels, 0);
  }
  EXPECT_GT(countDifferentPixels(captures[0].image, captures[1].image), 1000);
  EXPECT_GT(countDifferentPixels(captures[1].image, captures[2].image), 1000);

  const QString outputDirectory =
      qEnvironmentVariable("ARTIFACT_SMOKE_RUNTIME_OUTPUT_DIR");
  if (!outputDirectory.isEmpty()) {
    ASSERT_TRUE(QDir().mkpath(outputDirectory.toUtf8().constData()));
    for (std::size_t index = 0; index < captures.size(); ++index) {
      const QString filename = QStringLiteral("smoke_product_vorticity_%1.png")
          .arg(vorticityStrengths[index], 0, 'f', 1);
      EXPECT_TRUE(captures[index].image.save(
          QDir(outputDirectory).filePath(filename), "PNG"));
    }
  }
}

TEST(ArtifactSmokeRuntimeStandaloneTest,
     ComparesCommonFieldReconstructionAcrossDensityResolutions) {
  constexpr int kFrameCountForResolution = 44;
  const auto low = captureSmokeDensityAtResolution(
      96, 72, kFrameCountForResolution);
  const auto high = captureSmokeDensityAtResolution(
      192, 144, kFrameCountForResolution);
  std::vector<float> upsampled(
      static_cast<std::size_t>(high.width * high.height));
  for (int y = 0; y < high.height; ++y) {
    for (int x = 0; x < high.width; ++x) {
      const float sourceX = static_cast<float>(x) *
          static_cast<float>(low.width - 1) /
          static_cast<float>(high.width - 1);
      const float sourceY = static_cast<float>(y) *
          static_cast<float>(low.height - 1) /
          static_cast<float>(high.height - 1);
      const int x0 = static_cast<int>(sourceX);
      const int y0 = static_cast<int>(sourceY);
      const int x1 = std::min(x0 + 1, low.width - 1);
      const int y1 = std::min(y0 + 1, low.height - 1);
      const float tx = sourceX - static_cast<float>(x0);
      const float ty = sourceY - static_cast<float>(y0);
      const auto at = [&](const int sx, const int sy) {
        return low.density[static_cast<std::size_t>(sy * low.width + sx)];
      };
      upsampled[static_cast<std::size_t>(y * high.width + x)] = std::lerp(
          std::lerp(at(x0, y0), at(x1, y0), tx),
          std::lerp(at(x0, y1), at(x1, y1), tx), ty);
    }
  }
  const QImage commonFieldHighResolution = rasterizeSolverDensity(
      upsampled, high.width, high.height, 1.1f, -0.6f, -0.8f, 0.18f,
      0.82f, 0.84f, 0.88f);
  const auto lowMetrics = measureSmokeImage(low.image);
  const auto reconstructedMetrics = measureSmokeImage(commonFieldHighResolution);
  const auto simulatedHighMetrics = measureSmokeImage(high.image);
  GTEST_LOG_(INFO) << "Smoke common-field reconstruction (visible / green excess): "
      << "native 96x72=" << lowMetrics.visiblePixels << '/'
      << lowMetrics.greenExcess << "; resampled 96x72 -> 192x144="
      << reconstructedMetrics.visiblePixels << '/'
      << reconstructedMetrics.greenExcess << "; independently simulated 192x144="
      << simulatedHighMetrics.visiblePixels << '/'
      << simulatedHighMetrics.greenExcess;
  EXPECT_EQ(commonFieldHighResolution.size(), low.image.size());
  EXPECT_GT(reconstructedMetrics.visiblePixels, 500);
  EXPECT_GT(simulatedHighMetrics.visiblePixels, 500);
  EXPECT_EQ(reconstructedMetrics.saturatedPixels, 0);
  EXPECT_EQ(simulatedHighMetrics.saturatedPixels, 0);
  EXPECT_GT(countDifferentPixels(low.image, commonFieldHighResolution), 100);
  EXPECT_GT(countDifferentPixels(commonFieldHighResolution, high.image), 100);

  const QString outputDirectory =
      qEnvironmentVariable("ARTIFACT_SMOKE_RUNTIME_OUTPUT_DIR");
  if (!outputDirectory.isEmpty()) {
    ASSERT_TRUE(QDir().mkpath(outputDirectory.toUtf8().constData()));
    EXPECT_TRUE(low.image.save(QDir(outputDirectory).filePath(
        QStringLiteral("smoke_common_field_native_96x72.png")), "PNG"));
    EXPECT_TRUE(commonFieldHighResolution.save(QDir(outputDirectory).filePath(
        QStringLiteral("smoke_common_field_resampled_192x144.png")), "PNG"));
    EXPECT_TRUE(high.image.save(QDir(outputDirectory).filePath(
        QStringLiteral("smoke_common_field_simulated_192x144.png")), "PNG"));
  }
}

TEST(ArtifactSmokeRuntimeStandaloneTest,
     ReportsVelocityAdvectedSmokeDetailAcrossFramesAndSeeds) {
  constexpr int kWidth = 96;
  constexpr int kHeight = 72;
  constexpr int kLastFrame = 22;
  constexpr int kFirstCapturedFrame = 20;
  constexpr float kDeltaTime = 1.0f / 30.0f;
  constexpr float kWavelength = 2.5f;
  constexpr float kDetailAmplitude = 1.0f;
  constexpr std::array<std::uint32_t, 3> seeds{17U, 29U, 71U};

  struct FrameResult {
    QImage baseImage;
    QImage detailedImage;
    QImage normalSourceImage;
    QImage tangentSourceImage;
    double baseMass = 0.0;
    double detailedMass = 0.0;
    double baseCentroidX = 0.0;
    double baseCentroidY = 0.0;
    double detailedCentroidX = 0.0;
    double normalSourceMass = 0.0;
    double tangentSourceMass = 0.0;
    double normalSourceCentroidX = 0.0;
    double tangentSourceCentroidX = 0.0;
    double normalSourceCentroidY = 0.0;
    double tangentSourceCentroidY = 0.0;
    std::vector<float> velocityX;
    std::vector<float> velocityY;
  };
  const auto runSeed = [](const std::uint32_t seed) {
    std::array<FrameResult, kLastFrame - kFirstCapturedFrame + 1> frames{};
    Artifact::LayerFluidRuntimeState state;
    Artifact::LayerSmokeRuntimeSettings settings;
    settings.gridWidth = kWidth;
    settings.gridHeight = kHeight;
    settings.solverIterations = 20;
    settings.emitterX = 0.5f;
    settings.emitterY = 0.78f;
    settings.emitterRadius = 0.08f;
    settings.emitterDensity = 0.8f;
    settings.emitterVelocityY = -1.2f;
    settings.vorticity = 1.5f;
    settings.emitterSwirl = 0.6f;
    std::vector<float> detail(static_cast<std::size_t>(kWidth * kHeight), 0.0f);
    std::vector<float> advected(static_cast<std::size_t>(kWidth * kHeight), 0.0f);
    std::vector<float> corrected(static_cast<std::size_t>(kWidth * kHeight), 0.0f);
    for (int frame = 0; frame <= kLastFrame; ++frame) {
      Artifact::advanceLayerSmokeRuntime(state, frame, 30.0, settings);
      const SmokeFieldFrame field = captureSmokeFieldFrame(state, kWidth, kHeight);
      if (frame == 0) {
        for (int y = 1; y < kHeight - 1; ++y) {
          for (int x = 1; x < kWidth - 1; ++x) {
            const auto index = static_cast<std::size_t>(y * kWidth + x);
            const float envelope = field.density[index] /
                (field.density[index] + 0.035f);
            detail[index] = smokeSeededValueNoise(
                static_cast<float>(x) / kWavelength,
                static_cast<float>(y) / kWavelength, seed) * envelope;
          }
        }
      } else {
        const auto sampleDetail = [](const std::vector<float>& values,
                                     const float x, const float y) {
          const float clampedX = std::clamp(x, 1.0f,
                                             static_cast<float>(kWidth - 2));
          const float clampedY = std::clamp(y, 1.0f,
                                             static_cast<float>(kHeight - 2));
          const int x0 = static_cast<int>(clampedX);
          const int y0 = static_cast<int>(clampedY);
          const int x1 = std::min(x0 + 1, kWidth - 2);
          const int y1 = std::min(y0 + 1, kHeight - 2);
          const float tx = clampedX - static_cast<float>(x0);
          const float ty = clampedY - static_cast<float>(y0);
          const auto at = [&values](const int sx, const int sy) {
            return values[static_cast<std::size_t>(sy * kWidth + sx)];
          };
          return std::lerp(std::lerp(at(x0, y0), at(x1, y0), tx),
                           std::lerp(at(x0, y1), at(x1, y1), tx), ty);
        };
        for (int y = 1; y < kHeight - 1; ++y) {
          for (int x = 1; x < kWidth - 1; ++x) {
            const auto index = static_cast<std::size_t>(y * kWidth + x);
            const float stepX = field.velocityX[index] * kDeltaTime *
                static_cast<float>(kWidth - 2);
            const float stepY = field.velocityY[index] * kDeltaTime *
                static_cast<float>(kHeight - 2);
            const float sourceX = static_cast<float>(x) - stepX;
            const float sourceY = static_cast<float>(y) - stepY;
            advected[index] = sampleDetail(detail, sourceX, sourceY);
          }
        }
        for (int y = 1; y < kHeight - 1; ++y) {
          for (int x = 1; x < kWidth - 1; ++x) {
            const auto index = static_cast<std::size_t>(y * kWidth + x);
            const float stepX = field.velocityX[index] * kDeltaTime *
                static_cast<float>(kWidth - 2);
            const float stepY = field.velocityY[index] * kDeltaTime *
                static_cast<float>(kHeight - 2);
            const float reverseEstimate = sampleDetail(
                advected, static_cast<float>(x) + stepX,
                static_cast<float>(y) + stepY);
            const float maccormack = advected[index] +
                0.5f * (detail[index] - reverseEstimate);
            const float sourceX = std::clamp(static_cast<float>(x) - stepX,
                1.0f, static_cast<float>(kWidth - 2));
            const float sourceY = std::clamp(static_cast<float>(y) - stepY,
                1.0f, static_cast<float>(kHeight - 2));
            const int x0 = static_cast<int>(sourceX);
            const int y0 = static_cast<int>(sourceY);
            const int x1 = std::min(x0 + 1, kWidth - 2);
            const int y1 = std::min(y0 + 1, kHeight - 2);
            const std::array<float, 4> neighborhood{
                detail[static_cast<std::size_t>(y0 * kWidth + x0)],
                detail[static_cast<std::size_t>(y0 * kWidth + x1)],
                detail[static_cast<std::size_t>(y1 * kWidth + x0)],
                detail[static_cast<std::size_t>(y1 * kWidth + x1)]};
            const auto [minimum, maximum] = std::minmax_element(
                neighborhood.begin(), neighborhood.end());
            corrected[index] = std::clamp(
                maccormack, *minimum, *maximum) * 0.985f;
          }
        }
        detail.swap(corrected);
      }

      double densityMass = 0.0;
      double weightedDetail = 0.0;
      for (std::size_t index = 0; index < detail.size(); ++index) {
        densityMass += field.density[index];
        weightedDetail += field.density[index] * detail[index];
      }
      const float detailMean = densityMass > 0.0
          ? static_cast<float>(weightedDetail / densityMass) : 0.0f;
      std::vector<float> detailedDensity(field.density.size());
      std::vector<float> normalDirectionalDetail(field.density.size(), 0.0f);
      std::vector<float> tangentDirectionalDetail(field.density.size(), 0.0f);
      double detailedMass = 0.0;
      double baseMomentX = 0.0;
      double baseMomentY = 0.0;
      double detailedMomentX = 0.0;
      double normalWeightedDetail = 0.0;
      double tangentWeightedDetail = 0.0;
      const auto interfaceNoise = [&](const float x, const float y) {
        const float drift = static_cast<float>(frame) * 0.0125f;
        const float low = smokeSeededValueNoise(
            (x + drift) / 6.0f, (y - drift * 0.7f) / 6.0f, seed);
        const float high = smokeSeededValueNoise(
            (x - drift * 0.5f) / 3.5f,
            (y + drift) / 3.5f, seed ^ 0xa511e9b3U);
        return 0.65f * low + 0.35f * high;
      };
      for (int y = 1; y < kHeight - 1; ++y) {
        for (int x = 1; x < kWidth - 1; ++x) {
          const auto index = static_cast<std::size_t>(y * kWidth + x);
          const float density = field.density[index];
          if (density <= 1.0e-4f) continue;
          const float gradientX = 0.5f * (field.density[index + 1] -
              field.density[index - 1]);
          const float gradientY = 0.5f *
              (field.density[index + kWidth] - field.density[index - kWidth]);
          const float gradientLength = std::sqrt(
              gradientX * gradientX + gradientY * gradientY);
          if (gradientLength <= 1.0e-7f) continue;
          const float normalX = gradientX / gradientLength;
          const float normalY = gradientY / gradientLength;
          const float noiseGradientX = 0.5f *
              (interfaceNoise(static_cast<float>(x + 1), static_cast<float>(y)) -
               interfaceNoise(static_cast<float>(x - 1), static_cast<float>(y)));
          const float noiseGradientY = 0.5f *
              (interfaceNoise(static_cast<float>(x), static_cast<float>(y + 1)) -
               interfaceNoise(static_cast<float>(x), static_cast<float>(y - 1)));
          const float relativeGradient = gradientLength / (density + 0.035f);
          const float interfaceWeight = std::clamp(
              relativeGradient / (relativeGradient + 0.5f), 0.0f, 1.0f);
          const float normalSource = interfaceWeight *
              (noiseGradientX * normalX + noiseGradientY * normalY);
          const float tangentSource = interfaceWeight *
              (-noiseGradientX * normalY + noiseGradientY * normalX);
          normalDirectionalDetail[index] = normalSource;
          tangentDirectionalDetail[index] = tangentSource;
          normalWeightedDetail += density * normalSource;
          tangentWeightedDetail += density * tangentSource;
        }
      }
      const float normalDetailMean = densityMass > 0.0
          ? static_cast<float>(normalWeightedDetail / densityMass) : 0.0f;
      const float tangentDetailMean = densityMass > 0.0
          ? static_cast<float>(tangentWeightedDetail / densityMass) : 0.0f;
      std::vector<float> normalSourceDensity(field.density.size());
      std::vector<float> tangentSourceDensity(field.density.size());
      double normalSourceMass = 0.0;
      double tangentSourceMass = 0.0;
      double normalSourceMomentX = 0.0;
      double tangentSourceMomentX = 0.0;
      double normalSourceMomentY = 0.0;
      double tangentSourceMomentY = 0.0;
      for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
          const auto index = static_cast<std::size_t>(y * kWidth + x);
          const float modulation = std::clamp(
              detail[index] - detailMean, -0.9f, 0.9f);
          detailedDensity[index] = field.density[index] *
              (1.0f + kDetailAmplitude * modulation);
          detailedMass += detailedDensity[index];
          constexpr float kInterfaceDetailAmplitude = 4.0f;
          const float normalModulation = std::clamp(
              normalDirectionalDetail[index] - normalDetailMean, -0.2f, 0.2f);
          const float tangentModulation = std::clamp(
              tangentDirectionalDetail[index] - tangentDetailMean, -0.2f, 0.2f);
          normalSourceDensity[index] = field.density[index] *
              (1.0f + kInterfaceDetailAmplitude * normalModulation);
          tangentSourceDensity[index] = field.density[index] *
              (1.0f + kInterfaceDetailAmplitude * tangentModulation);
          normalSourceMass += normalSourceDensity[index];
          tangentSourceMass += tangentSourceDensity[index];
          normalSourceMomentX += normalSourceDensity[index] * x;
          tangentSourceMomentX += tangentSourceDensity[index] * x;
          normalSourceMomentY += normalSourceDensity[index] * y;
          tangentSourceMomentY += tangentSourceDensity[index] * y;
          baseMomentX += field.density[index] * x;
          baseMomentY += field.density[index] * y;
          detailedMomentX += detailedDensity[index] * x;
        }
      }
      if (frame >= kFirstCapturedFrame) {
        const auto resultIndex = static_cast<std::size_t>(
            frame - kFirstCapturedFrame);
        auto& result = frames[resultIndex];
        result.baseImage = rasterizeSolverDensity(
            field.density, kWidth, kHeight, 1.1f, -0.6f, -0.8f,
            0.18f, 0.82f, 0.84f, 0.88f);
        result.detailedImage = rasterizeSolverDensity(
            detailedDensity, kWidth, kHeight, 1.1f, -0.6f, -0.8f,
            0.18f, 0.82f, 0.84f, 0.88f);
        result.normalSourceImage = rasterizeSolverDensity(
            normalSourceDensity, kWidth, kHeight, 1.1f, -0.6f, -0.8f,
            0.18f, 0.82f, 0.84f, 0.88f);
        result.tangentSourceImage = rasterizeSolverDensity(
            tangentSourceDensity, kWidth, kHeight, 1.1f, -0.6f, -0.8f,
            0.18f, 0.82f, 0.84f, 0.88f);
        result.baseMass = densityMass;
        result.detailedMass = detailedMass;
        result.normalSourceMass = normalSourceMass;
        result.tangentSourceMass = tangentSourceMass;
        result.normalSourceCentroidX = normalSourceMomentX / normalSourceMass;
        result.tangentSourceCentroidX = tangentSourceMomentX / tangentSourceMass;
        result.normalSourceCentroidY = normalSourceMomentY / normalSourceMass;
        result.tangentSourceCentroidY = tangentSourceMomentY / tangentSourceMass;
        result.baseCentroidX = densityMass > 0.0 ? baseMomentX / densityMass : 0.0;
        result.baseCentroidY = densityMass > 0.0 ? baseMomentY / densityMass : 0.0;
        result.detailedCentroidX = detailedMass > 0.0
            ? detailedMomentX / detailedMass : 0.0;
        result.velocityX = field.velocityX;
        result.velocityY = field.velocityY;
      }
    }
    return frames;
  };

  const QString outputDirectory =
      qEnvironmentVariable("ARTIFACT_SMOKE_RUNTIME_OUTPUT_DIR");
  const auto highPassEnergy = [](const QImage& image) {
    double energy = 0.0;
    std::size_t samples = 0;
    for (int y = 1; y < image.height() - 1; ++y) {
      for (int x = 1; x < image.width() - 1; ++x) {
        const float center = static_cast<float>(qGreen(image.pixel(x, y)));
        const float neighbors = static_cast<float>(
            qGreen(image.pixel(x - 1, y)) + qGreen(image.pixel(x + 1, y)) +
            qGreen(image.pixel(x, y - 1)) + qGreen(image.pixel(x, y + 1))) *
            0.25f;
        const double residual = static_cast<double>(center - neighbors) / 255.0;
        energy += residual * residual;
        ++samples;
      }
    }
    return energy / static_cast<double>(samples);
  };
  const auto registeredHighPassTemporalResidual = [](
      const QImage& previous, const QImage& current,
      const double centroidDeltaX, const double centroidDeltaY) {
    const int shiftX = static_cast<int>(std::lround(centroidDeltaX * kImageScale));
    const int shiftY = static_cast<int>(std::lround(centroidDeltaY * kImageScale));
    double squaredResidual = 0.0;
    std::size_t samples = 0;
    for (int y = 1; y < previous.height() - 1; ++y) {
      for (int x = 1; x < previous.width() - 1; ++x) {
        const int currentX = x + shiftX;
        const int currentY = y + shiftY;
        if (currentX <= 0 || currentX >= current.width() - 1 ||
            currentY <= 0 || currentY >= current.height() - 1)
          continue;
        const auto highPassAt = [](const QImage& image, const int px,
                                   const int py) {
          const float center = static_cast<float>(qGreen(image.pixel(px, py)));
          const float neighbors = 0.25f * static_cast<float>(
              qGreen(image.pixel(px - 1, py)) + qGreen(image.pixel(px + 1, py)) +
              qGreen(image.pixel(px, py - 1)) + qGreen(image.pixel(px, py + 1)));
          return center - neighbors;
        };
        const double residual = (highPassAt(previous, x, y) -
            highPassAt(current, currentX, currentY)) / 255.0;
        squaredResidual += residual * residual;
        ++samples;
      }
    }
    return samples > 0
        ? std::sqrt(squaredResidual / static_cast<double>(samples)) : 0.0;
  };
  const auto flowRegisteredHighPassTemporalResidual = [](
      const QImage& previous, const QImage& current,
      const std::vector<float>& currentVelocityX,
      const std::vector<float>& currentVelocityY, const float deltaTime,
      const float maximumNormalizedY) {
    const auto highPassAt = [](const QImage& image, const int px,
                               const int py) {
      const float center = static_cast<float>(qGreen(image.pixel(px, py)));
      const float neighbors = 0.25f * static_cast<float>(
          qGreen(image.pixel(px - 1, py)) + qGreen(image.pixel(px + 1, py)) +
          qGreen(image.pixel(px, py - 1)) + qGreen(image.pixel(px, py + 1)));
      return center - neighbors;
    };
    const auto samplePreviousHighPass = [&](const float px, const float py) {
      const float clampedX = std::clamp(px, 1.0f,
          static_cast<float>(previous.width() - 2));
      const float clampedY = std::clamp(py, 1.0f,
          static_cast<float>(previous.height() - 2));
      const int x0 = static_cast<int>(std::floor(clampedX));
      const int y0 = static_cast<int>(std::floor(clampedY));
      const int x1 = std::min(x0 + 1, previous.width() - 2);
      const int y1 = std::min(y0 + 1, previous.height() - 2);
      const float tx = clampedX - static_cast<float>(x0);
      const float ty = clampedY - static_cast<float>(y0);
      return std::lerp(
          std::lerp(highPassAt(previous, x0, y0),
                    highPassAt(previous, x1, y0), tx),
          std::lerp(highPassAt(previous, x0, y1),
                    highPassAt(previous, x1, y1), tx), ty);
    };
    const auto velocityAt = [](const std::vector<float>& field,
                               const float gx, const float gy) {
      const float clampedX = std::clamp(gx, 0.0f,
          static_cast<float>(kWidth - 1));
      const float clampedY = std::clamp(gy, 0.0f,
          static_cast<float>(kHeight - 1));
      const int x0 = static_cast<int>(std::floor(clampedX));
      const int y0 = static_cast<int>(std::floor(clampedY));
      const int x1 = std::min(x0 + 1, kWidth - 1);
      const int y1 = std::min(y0 + 1, kHeight - 1);
      const float tx = clampedX - static_cast<float>(x0);
      const float ty = clampedY - static_cast<float>(y0);
      const auto at = [&](const int x, const int y) {
        return field[static_cast<std::size_t>(y * kWidth + x)];
      };
      return std::lerp(std::lerp(at(x0, y0), at(x1, y0), tx),
                       std::lerp(at(x0, y1), at(x1, y1), tx), ty);
    };
    const float maximumGridY = maximumNormalizedY *
        static_cast<float>(kHeight - 1);
    double squaredResidual = 0.0;
    std::size_t samples = 0;
    for (int y = 1; y < current.height() - 1; ++y) {
      for (int x = 1; x < current.width() - 1; ++x) {
        const float gridX = (static_cast<float>(x) + 0.5f) *
            static_cast<float>(kWidth - 1) / current.width();
        const float gridY = (static_cast<float>(y) + 0.5f) *
            static_cast<float>(kHeight - 1) / current.height();
        if (gridY > maximumGridY) continue;
        const float sourceGridX = gridX - velocityAt(currentVelocityX,
            gridX, gridY) * deltaTime * static_cast<float>(kWidth - 2);
        const float sourceGridY = gridY - velocityAt(currentVelocityY,
            gridX, gridY) * deltaTime * static_cast<float>(kHeight - 2);
        if (sourceGridX < 0.5f || sourceGridX > kWidth - 1.5f ||
            sourceGridY < 0.5f || sourceGridY > kHeight - 1.5f)
          continue;
        if (sourceGridY > maximumGridY) continue;
        const float sourcePixelX = sourceGridX *
            static_cast<float>(previous.width()) / (kWidth - 1) - 0.5f;
        const float sourcePixelY = sourceGridY *
            static_cast<float>(previous.height()) / (kHeight - 1) - 0.5f;
        const double residual = (samplePreviousHighPass(sourcePixelX, sourcePixelY) -
            highPassAt(current, x, y)) / 255.0;
        squaredResidual += residual * residual;
        ++samples;
      }
    }
    return samples > 0
        ? std::sqrt(squaredResidual / static_cast<double>(samples)) : 0.0;
  };
  for (const std::uint32_t seed : seeds) {
    const auto frames = runSeed(seed);
    for (std::size_t index = 0; index < frames.size(); ++index) {
      const auto& frame = frames[index];
      ASSERT_FALSE(frame.baseImage.isNull());
      ASSERT_FALSE(frame.detailedImage.isNull());
      ASSERT_FALSE(frame.normalSourceImage.isNull());
      ASSERT_FALSE(frame.tangentSourceImage.isNull());
      const auto baseMetrics = measureSmokeImage(frame.baseImage);
      const auto detailMetrics = measureSmokeImage(frame.detailedImage);
      EXPECT_GT(baseMetrics.visiblePixels, 500);
      EXPECT_GT(detailMetrics.visiblePixels, 500);
      EXPECT_EQ(detailMetrics.saturatedPixels, 0);
      EXPECT_LT(std::abs(frame.detailedMass - frame.baseMass) /
                    std::max(1.0, frame.baseMass), 0.002);
      EXPECT_LT(std::abs(frame.detailedCentroidX - frame.baseCentroidX), 0.25);
      EXPECT_GT(countDifferentPixels(frame.baseImage, frame.detailedImage), 100);
      const double normalMassError = std::abs(
          frame.normalSourceMass - frame.baseMass) /
          std::max(1.0, frame.baseMass);
      const double tangentMassError = std::abs(
          frame.tangentSourceMass - frame.baseMass) /
          std::max(1.0, frame.baseMass);
      EXPECT_LT(normalMassError, 0.002);
      EXPECT_LT(tangentMassError, 0.002);
      const int normalChangedPixels = countDifferentPixels(
          frame.baseImage, frame.normalSourceImage);
      const int tangentChangedPixels = countDifferentPixels(
          frame.baseImage, frame.tangentSourceImage);
      EXPECT_GT(normalChangedPixels, 20);
      EXPECT_GT(tangentChangedPixels, 20);
      EXPECT_LT(std::abs(frame.normalSourceCentroidX - frame.baseCentroidX), 0.5);
      EXPECT_LT(std::abs(frame.tangentSourceCentroidX - frame.baseCentroidX), 0.5);
      GTEST_LOG_(INFO) << "Smoke velocity-advected detail seed=" << seed
          << " frame=" << (kFirstCapturedFrame + index)
          << " visible=" << baseMetrics.visiblePixels << '/'
          << detailMetrics.visiblePixels << " mass=" << frame.baseMass << '/'
          << frame.detailedMass << " centroidX=" << frame.baseCentroidX << '/'
          << frame.detailedCentroidX << " laplacianEnergy="
          << highPassEnergy(frame.baseImage) << '/'
          << highPassEnergy(frame.detailedImage)
          << " interfaceNormal/Tangent highPass="
          << highPassEnergy(frame.normalSourceImage) << '/'
          << highPassEnergy(frame.tangentSourceImage)
          << " massError=" << normalMassError << '/' << tangentMassError
          << " centroidX=" << frame.baseCentroidX << '/'
          << frame.normalSourceCentroidX << '/'
          << frame.tangentSourceCentroidX << " changedPixels="
          << normalChangedPixels << '/' << tangentChangedPixels;
      if (!outputDirectory.isEmpty()) {
        ASSERT_TRUE(QDir().mkpath(outputDirectory.toUtf8().constData()));
        const QString suffix = QStringLiteral("seed_%1_frame_%2.png")
            .arg(seed).arg(kFirstCapturedFrame + index);
        EXPECT_TRUE(frame.baseImage.save(QDir(outputDirectory).filePath(
            QStringLiteral("smoke_advected_base_") + suffix), "PNG"));
        EXPECT_TRUE(frame.detailedImage.save(QDir(outputDirectory).filePath(
            QStringLiteral("smoke_advected_detail_") + suffix), "PNG"));
        EXPECT_TRUE(frame.normalSourceImage.save(QDir(outputDirectory).filePath(
            QStringLiteral("smoke_interface_normal_detail_") + suffix), "PNG"));
        EXPECT_TRUE(frame.tangentSourceImage.save(QDir(outputDirectory).filePath(
            QStringLiteral("smoke_interface_tangent_detail_") + suffix), "PNG"));
      }
    }
    for (std::size_t index = 1; index < frames.size(); ++index) {
      EXPECT_GT(countDifferentPixels(frames[index - 1].detailedImage,
                                     frames[index].detailedImage), 50);
      const auto& previous = frames[index - 1];
      const auto& current = frames[index];
      const double baseResidual = registeredHighPassTemporalResidual(
          previous.baseImage, current.baseImage,
          current.baseCentroidX - previous.baseCentroidX,
          current.baseCentroidY - previous.baseCentroidY);
      const double normalResidual = registeredHighPassTemporalResidual(
          previous.normalSourceImage, current.normalSourceImage,
          current.normalSourceCentroidX - previous.normalSourceCentroidX,
          current.normalSourceCentroidY - previous.normalSourceCentroidY);
      const double tangentResidual = registeredHighPassTemporalResidual(
          previous.tangentSourceImage, current.tangentSourceImage,
          current.tangentSourceCentroidX - previous.tangentSourceCentroidX,
          current.tangentSourceCentroidY - previous.tangentSourceCentroidY);
      const double baseFlowResidual = flowRegisteredHighPassTemporalResidual(
          previous.baseImage, current.baseImage, current.velocityX,
          current.velocityY, kDeltaTime, 1.0f);
      const double normalFlowResidual = flowRegisteredHighPassTemporalResidual(
          previous.normalSourceImage, current.normalSourceImage,
          current.velocityX, current.velocityY, kDeltaTime, 1.0f);
      const double tangentFlowResidual = flowRegisteredHighPassTemporalResidual(
          previous.tangentSourceImage, current.tangentSourceImage,
          current.velocityX, current.velocityY, kDeltaTime, 1.0f);
      constexpr float kSourceExcludedMaxNormalizedY = 0.68f;
      const double baseSourceExcludedResidual =
          flowRegisteredHighPassTemporalResidual(previous.baseImage,
              current.baseImage, current.velocityX, current.velocityY,
              kDeltaTime, kSourceExcludedMaxNormalizedY);
      const double normalSourceExcludedResidual =
          flowRegisteredHighPassTemporalResidual(previous.normalSourceImage,
              current.normalSourceImage, current.velocityX,
              current.velocityY, kDeltaTime, kSourceExcludedMaxNormalizedY);
      const double tangentSourceExcludedResidual =
          flowRegisteredHighPassTemporalResidual(previous.tangentSourceImage,
              current.tangentSourceImage, current.velocityX,
              current.velocityY, kDeltaTime, kSourceExcludedMaxNormalizedY);
      EXPECT_GT(baseResidual, 0.0);
      EXPECT_GT(normalResidual, 0.0);
      EXPECT_GT(tangentResidual, 0.0);
      EXPECT_GT(baseFlowResidual, 0.0);
      EXPECT_GT(normalFlowResidual, 0.0);
      EXPECT_GT(tangentFlowResidual, 0.0);
      EXPECT_GT(baseSourceExcludedResidual, 0.0);
      EXPECT_GT(normalSourceExcludedResidual, 0.0);
      EXPECT_GT(tangentSourceExcludedResidual, 0.0);
      GTEST_LOG_(INFO) << "Smoke centroid-registered high-pass temporal residual seed="
          << seed << " frames=" << (kFirstCapturedFrame + index - 1) << "->"
          << (kFirstCapturedFrame + index) << " base/normal/tangent="
          << baseResidual << '/' << normalResidual << '/' << tangentResidual
          << " flow-backtrace=" << baseFlowResidual << '/'
          << normalFlowResidual << '/' << tangentFlowResidual
          << " source-excluded-y<0.68=" << baseSourceExcludedResidual << '/'
          << normalSourceExcludedResidual << '/'
          << tangentSourceExcludedResidual;
    }
  }
}

TEST(ArtifactSmokeRuntimeStandaloneTest,
     ReportsSteadyStateRenderDataPreparationCost) {
  constexpr int kWarmupIterations = 12;
  constexpr int kMeasuredIterations = 100;
  struct Measurement {
    int width;
    int height;
    int particleCount;
    int reservedCapacity;
    double meanMicroseconds;
    double p95Microseconds;
    double integerFetchMicroseconds;
    double bilinearFetchMicroseconds;
    double densityOnlyFetchMicroseconds;
    double velocityFreeSubmitMicroseconds;
  };

  struct KernelMeasurement {
    double meanMicroseconds;
    double p95Microseconds;
    double sink;
  };

  const auto measure = [](const int width, const int height) {
    Artifact::LayerSmokeRuntimeSettings settings;
    settings.gridWidth = width;
    settings.gridHeight = height;
    settings.solverIterations = 1;
    settings.vorticity = 0.0f;
    settings.emitterX = 0.5f;
    settings.emitterY = 0.78f;
    settings.emitterRadius = 0.08f;
    settings.emitterDensity = 0.8f;
    settings.emitterVelocityY = -1.2f;

    Artifact::LayerFluidRuntimeState state;
    for (int frame = 0; frame <= kFrameCount; ++frame) {
      Artifact::advanceLayerSmokeRuntime(state, frame, 30.0, settings);
    }
    QMatrix4x4 transform;
    transform.setToIdentity();
    const QRectF bounds(0.0, 0.0, width, height);
    for (int i = 0; i < kWarmupIterations; ++i) {
      Artifact::prepareLayerSmokeRenderData(
          state, bounds, transform, 1.0f, 0, settings);
    }
    const int reservedCapacity = static_cast<int>(
        state.fluidRenderData_.particles.capacity());
    const auto [sampleCountX, sampleCountY] =
        expectedSmokeRenderSampleCounts(width, height);
    const int expectedSampleCount = sampleCountX * sampleCountY;
    EXPECT_EQ(reservedCapacity, expectedSampleCount);

    std::array<double, kMeasuredIterations> samples{};
    int particleCount = 0;
    for (auto& sample : samples) {
      const auto start = std::chrono::steady_clock::now();
      const auto& data = Artifact::prepareLayerSmokeRenderData(
          state, bounds, transform, 1.0f, 0, settings);
      const auto end = std::chrono::steady_clock::now();
      sample = std::chrono::duration<double, std::micro>(end - start).count();
      particleCount = static_cast<int>(data.particles.size());
      EXPECT_EQ(data.particles.capacity(),
                static_cast<std::size_t>(reservedCapacity));
    }
    std::sort(samples.begin(), samples.end());
    double totalMicroseconds = 0.0;
    for (const double sample : samples) totalMicroseconds += sample;

    const auto measureFieldKernel = [&]<bool Bilinear, bool SampleVelocity>() {
      const auto runKernel = [&]() {
        double valueSum = 0.0;
        const auto [sampleCountX, sampleCountY] =
            expectedSmokeRenderSampleCounts(width, height);
        const float sampleStepX = static_cast<float>(width - 1) /
                                  static_cast<float>(sampleCountX - 1);
        const float sampleStepY = static_cast<float>(height - 1) /
                                  static_cast<float>(sampleCountY - 1);
        for (int sampleY = 0; sampleY < sampleCountY; ++sampleY) {
          const float baseGridY = sampleY * sampleStepY;
          for (int sampleXIndex = 0; sampleXIndex < sampleCountX;
               ++sampleXIndex) {
            const float baseGridX = sampleXIndex * sampleStepX;
            const int gx = std::clamp(static_cast<int>(std::lround(baseGridX)),
                                      0, width - 1);
            const int gy = std::clamp(static_cast<int>(std::lround(baseGridY)),
                                      0, height - 1);
            std::uint32_t hash =
                static_cast<std::uint32_t>(sampleXIndex) * 0x9E3779B9u ^
                static_cast<std::uint32_t>(sampleY) * 0x85EBCA6Bu;
            hash ^= hash >> 16u;
            hash *= 0x7FEB352Du;
            hash ^= hash >> 15u;
            const float jitterX = static_cast<float>(hash & 0xFFFFu) *
                                      (2.0f / 65535.0f) - 1.0f;
            const float jitterY = static_cast<float>(hash >> 16u) *
                                      (2.0f / 65535.0f) - 1.0f;
            const float sampleX = std::clamp(
                baseGridX + jitterX * sampleStepX * 0.30f,
                0.0f, static_cast<float>(width - 1));
            const float sampleY = std::clamp(
                baseGridY + jitterY * sampleStepY * 0.30f,
                0.0f, static_cast<float>(height - 1));
            if constexpr (Bilinear) {
              const int x0 = static_cast<int>(std::floor(sampleX));
              const int y0 = static_cast<int>(std::floor(sampleY));
              const int x1 = std::min(x0 + 1, width - 1);
              const int y1 = std::min(y0 + 1, height - 1);
              const float tx = sampleX - x0;
              const float ty = sampleY - y0;
              const float d00 = state.fluidSolver_->getDensity(x0, y0);
              const float d10 = state.fluidSolver_->getDensity(x1, y0);
              const float d01 = state.fluidSolver_->getDensity(x0, y1);
              const float d11 = state.fluidSolver_->getDensity(x1, y1);
              const auto interpolate = [tx, ty](const float a, const float b,
                  const float c, const float d) {
                return std::lerp(std::lerp(a, b, tx), std::lerp(c, d, tx), ty);
              };
              valueSum += interpolate(d00, d10, d01, d11);
              if constexpr (SampleVelocity) {
                float vx00 = 0.0f, vy00 = 0.0f;
                float vx10 = 0.0f, vy10 = 0.0f;
                float vx01 = 0.0f, vy01 = 0.0f;
                float vx11 = 0.0f, vy11 = 0.0f;
                state.fluidSolver_->getVelocity(x0, y0, vx00, vy00);
                state.fluidSolver_->getVelocity(x1, y0, vx10, vy10);
                state.fluidSolver_->getVelocity(x0, y1, vx01, vy01);
                state.fluidSolver_->getVelocity(x1, y1, vx11, vy11);
                valueSum += interpolate(vx00, vx10, vx01, vx11) +
                            interpolate(vy00, vy10, vy01, vy11);
              }
              valueSum += (sampleX + sampleY) * 1.0e-6f;
            } else {
              valueSum += state.fluidSolver_->getDensity(gx, gy);
              if constexpr (SampleVelocity) {
                float vx = 0.0f, vy = 0.0f;
                state.fluidSolver_->getVelocity(gx, gy, vx, vy);
                valueSum += vx + vy;
              }
              valueSum += (sampleX + sampleY) * 1.0e-6f;
            }
          }
        }
        return valueSum;
      };

      for (int i = 0; i < kWarmupIterations; ++i) {
        static_cast<void>(runKernel());
      }
      std::array<double, kMeasuredIterations> kernelSamples{};
      double sink = 0.0;
      for (auto& sample : kernelSamples) {
        const auto start = std::chrono::steady_clock::now();
        sink += runKernel();
        const auto end = std::chrono::steady_clock::now();
        sample = std::chrono::duration<double, std::micro>(end - start).count();
      }
      std::sort(kernelSamples.begin(), kernelSamples.end());
      double kernelTotal = 0.0;
      for (const double sample : kernelSamples) kernelTotal += sample;
      return KernelMeasurement{
          kernelTotal / static_cast<double>(kernelSamples.size()),
          kernelSamples[(kernelSamples.size() * 95) / 100], sink};
    };
    const KernelMeasurement integerFetch =
        measureFieldKernel.template operator()<false, true>();
    const KernelMeasurement bilinearFetch =
        measureFieldKernel.template operator()<true, true>();
    const KernelMeasurement densityOnlyFetch =
        measureFieldKernel.template operator()<true, false>();

    Artifact::ArtifactIRenderer gpuRenderer;
    for (int i = 0; i < kWarmupIterations; ++i) {
      Artifact::renderLayerSmokeRuntime(
          state, &gpuRenderer, bounds, transform, 1.0f, kFrameCount,
          30.0, settings);
    }
    std::array<double, kMeasuredIterations> gpuSamples{};
    for (auto& sample : gpuSamples) {
      const auto start = std::chrono::steady_clock::now();
      Artifact::renderLayerSmokeRuntime(
          state, &gpuRenderer, bounds, transform, 1.0f, kFrameCount,
          30.0, settings);
      const auto end = std::chrono::steady_clock::now();
      sample = std::chrono::duration<double, std::micro>(end - start).count();
    }
    std::sort(gpuSamples.begin(), gpuSamples.end());
    double gpuTotalMicroseconds = 0.0;
    for (const double sample : gpuSamples) gpuTotalMicroseconds += sample;
    GTEST_LOG_(INFO) << "Smoke field sampling at " << width << 'x' << height
        << " (integer mean/p95=" << integerFetch.meanMicroseconds << '/'
        << integerFetch.p95Microseconds << " us; bilinear mean/p95="
        << bilinearFetch.meanMicroseconds << '/' << bilinearFetch.p95Microseconds
        << " us; density-only bilinear=" << densityOnlyFetch.meanMicroseconds
        << " us; velocity-free runtime submit mean/p95="
        << gpuTotalMicroseconds / static_cast<double>(gpuSamples.size()) << '/'
        << gpuSamples[(gpuSamples.size() * 95) / 100] << " us; sinks="
        << integerFetch.sink << '/' << bilinearFetch.sink << '/'
        << densityOnlyFetch.sink << ')';
    return Measurement{width, height, particleCount, reservedCapacity,
        totalMicroseconds / static_cast<double>(samples.size()),
        samples[(samples.size() * 95) / 100],
        integerFetch.meanMicroseconds, bilinearFetch.meanMicroseconds,
        densityOnlyFetch.meanMicroseconds,
        gpuTotalMicroseconds / static_cast<double>(gpuSamples.size())};
  };

  const Measurement smallGrid = measure(32, 32);
  const Measurement peakSampleGrid = measure(79, 79);
  const Measurement defaultGrid = measure(128, 128);
  const Measurement highGrid = measure(512, 512);
  const std::array<const Measurement*, 4> results{
      &smallGrid, &peakSampleGrid, &defaultGrid, &highGrid};
  for (const Measurement* result : results) {
    ASSERT_GT(result->particleCount, 0);
    GTEST_LOG_(INFO) << "Smoke render-data preparation (steady-state, us): "
        << result->width << 'x' << result->height << " particles="
        << result->particleCount << " reserved=" << result->reservedCapacity
        << " mean=" << result->meanMicroseconds
        << " p95=" << result->p95Microseconds << " field-integer/bilinear="
        << result->integerFetchMicroseconds << '/'
        << result->bilinearFetchMicroseconds << " us density-only="
        << result->densityOnlyFetchMicroseconds << " us velocity-free-submit="
        << result->velocityFreeSubmitMicroseconds << " us";
  }
}

TEST(ArtifactSmokeRuntimeStandaloneTest,
     EmissionAndDensityDissipationScaleWithElapsedTime) {
  const QString outputDirectory =
      qEnvironmentVariable("ARTIFACT_SMOKE_RUNTIME_OUTPUT_DIR");
  if (!outputDirectory.isEmpty()) {
    ASSERT_TRUE(QDir().mkpath(outputDirectory.toUtf8().constData()));
  }
  const auto compareDuration = [&](const double seconds,
                                   const QString& label) {
    const SmokeRun at10 = runProductionSmokeRuntime(
        0.5f, false, false, 10.0,
        static_cast<int>(std::round(seconds * 10.0)) - 1);
    const SmokeRun at15 = runProductionSmokeRuntime(
        0.5f, false, false, 15.0,
        static_cast<int>(std::round(seconds * 15.0)) - 1);
    const SmokeRun at24 = runProductionSmokeRuntime(
        0.5f, false, false, 24.0,
        static_cast<int>(std::round(seconds * 24.0)) - 1);
    const SmokeRun at30 = runProductionSmokeRuntime(
        0.5f, false, false, 30.0,
        static_cast<int>(std::round(seconds * 30.0)) - 1);
    const SmokeRun at60 = runProductionSmokeRuntime(
        0.5f, false, false, 60.0,
        static_cast<int>(std::round(seconds * 60.0)) - 1);

    ASSERT_GT(at10.particleCount, 20) << label.toStdString();
    ASSERT_GT(at15.particleCount, 20) << label.toStdString();
    ASSERT_GT(at24.particleCount, 20) << label.toStdString();
    ASSERT_GT(at30.particleCount, 20) << label.toStdString();
    ASSERT_GT(at60.particleCount, 20) << label.toStdString();
    EXPECT_NEAR(at10.meanParticleY, at30.meanParticleY, 4.0)
        << label.toStdString();
    EXPECT_NEAR(at15.meanParticleY, at30.meanParticleY, 4.0)
        << label.toStdString();
    EXPECT_NEAR(at10.totalParticleAlpha, at30.totalParticleAlpha,
                at30.totalParticleAlpha * 0.2)
        << label.toStdString();
    EXPECT_NEAR(at15.totalParticleAlpha, at30.totalParticleAlpha,
                at30.totalParticleAlpha * 0.2)
        << label.toStdString();
    GTEST_LOG_(INFO) << "Smoke elapsed-time comparison " << label.toStdString()
        << " particleMeanY 10/15/24/30/60=" << at10.meanParticleY << '/'
        << at15.meanParticleY << '/'
        << at24.meanParticleY << '/'
        << at30.meanParticleY << '/' << at60.meanParticleY
        << " totalAlpha10/15=" << at10.totalParticleAlpha << '/'
        << at15.totalParticleAlpha
        << " totalAlpha24/30/60="
        << at24.totalParticleAlpha << '/'
        << at30.totalParticleAlpha << '/'
        << at60.totalParticleAlpha;
    GTEST_LOG_(INFO) << "Smoke elapsed-time comparison " << label.toStdString()
        << " particleMeanY 24/30/60=" << at24.meanParticleY << '/'
        << at30.meanParticleY << '/' << at60.meanParticleY
        << " totalAlpha=" << at24.totalParticleAlpha << '/'
        << at30.totalParticleAlpha << '/' << at60.totalParticleAlpha;
    EXPECT_NEAR(at24.meanParticleY, at30.meanParticleY, 4.0)
        << label.toStdString();
    EXPECT_NEAR(at30.meanParticleY, at60.meanParticleY, 4.0)
        << label.toStdString();
    EXPECT_NEAR(at24.totalParticleAlpha, at30.totalParticleAlpha,
                at30.totalParticleAlpha * 0.1)
        << label.toStdString();
    EXPECT_NEAR(at30.totalParticleAlpha, at60.totalParticleAlpha,
                at30.totalParticleAlpha * 0.1)
        << label.toStdString();

    if (!outputDirectory.isEmpty()) {
      EXPECT_TRUE(at10.image.save(QDir(outputDirectory).filePath(
          QStringLiteral("smoke_%1_10fps.png").arg(label)), "PNG"));
      EXPECT_TRUE(at15.image.save(QDir(outputDirectory).filePath(
          QStringLiteral("smoke_%1_15fps.png").arg(label)), "PNG"));
      EXPECT_TRUE(at24.image.save(QDir(outputDirectory).filePath(
          QStringLiteral("smoke_%1_24fps.png").arg(label)), "PNG"));
      EXPECT_TRUE(at30.image.save(QDir(outputDirectory).filePath(
          QStringLiteral("smoke_%1_30fps.png").arg(label)), "PNG"));
      EXPECT_TRUE(at60.image.save(QDir(outputDirectory).filePath(
          QStringLiteral("smoke_%1_60fps.png").arg(label)), "PNG"));
    }
  };

  compareDuration(1.5, QStringLiteral("1p5s"));
  compareDuration(3.0, QStringLiteral("3s"));
}

TEST(ArtifactSmokeRuntimeStandaloneTest,
     MeasuresSmokeStateAcrossFrameGapsAndSeeks) {
  Artifact::LayerSmokeRuntimeSettings settings;
  settings.gridWidth = kGridWidth;
  settings.gridHeight = kGridHeight;
  settings.emitterX = 0.5f;
  settings.emitterY = 0.84f;
  settings.emitterRadius = 0.08f;
  settings.emitterDensity = 0.6f;
  settings.emitterVelocityY = -1.2f;
  settings.vorticity = 0.1f;
  constexpr double frameRate = 30.0;
  const auto advanceThrough = [&](Artifact::LayerFluidRuntimeState& state,
                                  const int frame) {
    for (int current = 0; current <= frame; ++current) {
      Artifact::advanceLayerSmokeRuntime(
          state, current, frameRate, settings);
    }
  };
  const auto summarize = [](const Artifact::LayerFluidRuntimeState& state) {
    const auto snapshot = state.fluidSolver_->snapshot();
    double mass = 0.0;
    double weightedY = 0.0;
    double squaredDensity = 0.0;
    float peak = 0.0f;
    float maxVelocity = 0.0f;
    for (int y = 1; y < snapshot.height - 1; ++y) {
      for (int x = 1; x < snapshot.width - 1; ++x) {
        const auto cell = static_cast<std::size_t>(x + y * snapshot.width);
        const float density = snapshot.density[cell];
        mass += density;
        weightedY += density * static_cast<double>(y);
        squaredDensity += static_cast<double>(density) * density;
        peak = std::max(peak, density);
        maxVelocity = std::max(maxVelocity, std::hypot(
            snapshot.velocityX[cell], snapshot.velocityY[cell]));
      }
    }
    const double centerY = mass > 0.0 ? weightedY / mass : 0.0;
    return std::array<double, 5>{mass, centerY, peak, maxVelocity,
        std::sqrt(squaredDensity / static_cast<double>(
            (snapshot.width - 2) * (snapshot.height - 2)))};
  };
  const auto densityRmse = [](const ArtifactCore::FluidSnapshot2D& a,
                              const ArtifactCore::FluidSnapshot2D& b) {
    EXPECT_EQ(a.width, b.width);
    EXPECT_EQ(a.height, b.height);
    double squaredDifference = 0.0;
    for (std::size_t cell = 0; cell < a.density.size(); ++cell) {
      const double difference = a.density[cell] - b.density[cell];
      squaredDifference += difference * difference;
    }
    return std::sqrt(squaredDifference /
                     static_cast<double>(a.density.size()));
  };

  Artifact::LayerFluidRuntimeState sequentialAt8;
  advanceThrough(sequentialAt8, 8);
  Artifact::LayerFluidRuntimeState gap8;
  Artifact::advanceLayerSmokeRuntime(gap8, 0, frameRate, settings);
  Artifact::advanceLayerSmokeRuntime(gap8, 8, frameRate, settings);
  EXPECT_NEAR(densityRmse(sequentialAt8.fluidSolver_->snapshot(),
                          gap8.fluidSolver_->snapshot()), 0.0, 1.0e-7);

  Artifact::LayerFluidRuntimeState sequentialAt9;
  advanceThrough(sequentialAt9, 9);
  Artifact::LayerFluidRuntimeState gap9;
  Artifact::advanceLayerSmokeRuntime(gap9, 0, frameRate, settings);
  Artifact::advanceLayerSmokeRuntime(gap9, 9, frameRate, settings);

  Artifact::LayerFluidRuntimeState sequentialAt10;
  advanceThrough(sequentialAt10, 10);
  Artifact::LayerFluidRuntimeState gap10;
  Artifact::advanceLayerSmokeRuntime(gap10, 0, frameRate, settings);
  Artifact::advanceLayerSmokeRuntime(gap10, 10, frameRate, settings);

  Artifact::LayerFluidRuntimeState sequentialAt11;
  advanceThrough(sequentialAt11, 11);
  Artifact::LayerFluidRuntimeState gap11;
  Artifact::advanceLayerSmokeRuntime(gap11, 0, frameRate, settings);
  Artifact::advanceLayerSmokeRuntime(gap11, 11, frameRate, settings);

  Artifact::LayerFluidRuntimeState sequentialAt20;
  advanceThrough(sequentialAt20, 20);
  Artifact::LayerFluidRuntimeState directAt20;
  Artifact::advanceLayerSmokeRuntime(directAt20, 20, frameRate, settings);
  Artifact::LayerFluidRuntimeState backwardSeek;
  advanceThrough(backwardSeek, 20);
  Artifact::advanceLayerSmokeRuntime(backwardSeek, 10, frameRate, settings);

  const auto report = [&](const char* label,
                          const Artifact::LayerFluidRuntimeState& reference,
                          const Artifact::LayerFluidRuntimeState& candidate) {
    const auto referenceSummary = summarize(reference);
    const auto candidateSummary = summarize(candidate);
    const double rmse = densityRmse(reference.fluidSolver_->snapshot(),
                                    candidate.fluidSolver_->snapshot());
    GTEST_LOG_(INFO) << "smoke frame-gap " << label << " densityRmse=" << rmse
        << " mass/reference=" << referenceSummary[0]
        << " mass/candidate=" << candidateSummary[0]
        << " centerY/reference=" << referenceSummary[1]
        << " centerY/candidate=" << candidateSummary[1]
        << " peak/reference=" << referenceSummary[2]
        << " peak/candidate=" << candidateSummary[2]
        << " maxVelocity/reference=" << referenceSummary[3]
        << " maxVelocity/candidate=" << candidateSummary[3]
        << " densityRms/reference=" << referenceSummary[4]
        << " densityRms/candidate=" << candidateSummary[4];
  };
  report("gap9-vs-sequential9", sequentialAt9, gap9);
  report("gap10-vs-sequential10", sequentialAt10, gap10);
  report("gap11-vs-sequential11", sequentialAt11, gap11);
  report("direct20-vs-sequential20", sequentialAt20, directAt20);
  report("backward20to10-vs-sequential10", sequentialAt10, backwardSeek);
  EXPECT_LE(densityRmse(sequentialAt9.fluidSolver_->snapshot(),
                        gap9.fluidSolver_->snapshot()), 1.0e-7);
  EXPECT_LE(densityRmse(sequentialAt10.fluidSolver_->snapshot(),
                        gap10.fluidSolver_->snapshot()), 1.0e-7);
  EXPECT_LE(densityRmse(sequentialAt11.fluidSolver_->snapshot(),
                        gap11.fluidSolver_->snapshot()), 1.0e-7);
  EXPECT_LE(densityRmse(sequentialAt10.fluidSolver_->snapshot(),
                        backwardSeek.fluidSolver_->snapshot()), 1.0e-7);
  EXPECT_LE(densityRmse(sequentialAt20.fluidSolver_->snapshot(),
                        directAt20.fluidSolver_->snapshot()), 1.0e-7);
  EXPECT_EQ(sequentialAt20.smokeCheckpoints_.size(), 3U);
  EXPECT_EQ(backwardSeek.smokeCheckpoints_.size(), 2U);

  Artifact::LayerFluidRuntimeState changedSettings;
  advanceThrough(changedSettings, 20);
  ASSERT_EQ(changedSettings.smokeCheckpoints_.size(), 3U);
  settings.emitterX = 0.65f;
  Artifact::advanceLayerSmokeRuntime(
      changedSettings, 20, frameRate, settings);
  ASSERT_EQ(changedSettings.smokeCheckpoints_.size(), 1U);
  EXPECT_EQ(changedSettings.smokeCheckpoints_[0].frame, 20);
}

TEST(ArtifactSmokeRuntimeStandaloneTest,
     MeasuresSmokeCheckpointCopyAndSparseReplayCost) {
  using ArtifactCore::FluidSolver2D;
  constexpr std::array<std::pair<int, int>, 3> resolutions{{
      {96, 72}, {128, 96}, {192, 144}}};
  constexpr int kWarmupIterations = 8;
  constexpr int kMeasureIterations = 64;
  for (const auto [width, height] : resolutions) {
    FluidSolver2D source(width, height);
    ArtifactCore::SmokeEmitter2D emitter;
    emitter.x = 0.5f;
    emitter.y = 0.84f;
    emitter.radius = 0.08f;
    emitter.density = 0.6f;
    emitter.velocityY = -1.2f;
    for (int frame = 0; frame < 120; ++frame) {
      ArtifactCore::injectSmokeEmitter2D(source, emitter, 1.0f);
      source.update(1.0f / 30.0f);
    }
    for (int iteration = 0; iteration < kWarmupIterations; ++iteration) {
      auto warm = source.snapshot();
      ASSERT_TRUE(source.restoreSnapshot(warm));
    }
    const auto copyStart = std::chrono::steady_clock::now();
    for (int iteration = 0; iteration < kMeasureIterations; ++iteration) {
      auto snapshot = source.snapshot();
      ASSERT_TRUE(source.restoreSnapshot(snapshot));
    }
    const double copyMicroseconds =
        std::chrono::duration<double, std::micro>(
            std::chrono::steady_clock::now() - copyStart).count() /
        static_cast<double>(kMeasureIterations);
    const std::size_t bytesPerSnapshot =
        source.snapshot().density.size() * sizeof(float) * 7;
    FluidSolver2D sequential(width, height);
    for (int frame = 0; frame <= 117; ++frame) {
      ArtifactCore::injectSmokeEmitter2D(sequential, emitter, 1.0f);
      sequential.update(1.0f / 30.0f);
    }
    const auto expected = sequential.snapshot();

    for (const int interval : {8, 16, 32}) {
      FluidSolver2D replay(width, height);
      std::vector<ArtifactCore::FluidSnapshot2D> checkpoints;
      checkpoints.reserve(121 / interval + 1);
      const auto replayStart = std::chrono::steady_clock::now();
      constexpr int target = 117;
      for (int frame = 0; frame <= target; ++frame) {
        if (frame % interval == 0) checkpoints.push_back(replay.snapshot());
        ArtifactCore::injectSmokeEmitter2D(replay, emitter, 1.0f);
        replay.update(1.0f / 30.0f);
      }
      const auto checkpoint = checkpoints.back();
      ASSERT_TRUE(replay.restoreSnapshot(checkpoint));
      // Checkpoints are captured before the update for their loop frame.
      const int checkpointFrame = static_cast<int>(
          (checkpoints.size() - 1) * static_cast<std::size_t>(interval)) - 1;
      const auto checkpointEnd = std::chrono::steady_clock::now();
      const auto checkpointBytes = bytesPerSnapshot * checkpoints.size();
      const int replayFrames = target - checkpointFrame;
      for (int frame = checkpointFrame + 1; frame <= target; ++frame) {
        ArtifactCore::injectSmokeEmitter2D(replay, emitter, 1.0f);
        replay.update(1.0f / 30.0f);
      }
      const auto replayed = replay.snapshot();
      double squaredDensityError = 0.0;
      for (std::size_t cell = 0; cell < expected.density.size(); ++cell) {
        const double difference = expected.density[cell] - replayed.density[cell];
        squaredDensityError += difference * difference;
      }
      const double densityRmse = std::sqrt(
          squaredDensityError / static_cast<double>(expected.density.size()));
      EXPECT_LE(densityRmse, 1.0e-7);
      const double replayMicroseconds =
          std::chrono::duration<double, std::micro>(
              std::chrono::steady_clock::now() - checkpointEnd).count();
      GTEST_LOG_(INFO) << "smoke checkpoint resolution=" << width << 'x'
          << height << " interval=" << interval
          << " bytesPerSnapshot=" << bytesPerSnapshot
          << " checkpointCount=" << checkpoints.size()
          << " checkpointBytes=" << checkpointBytes
          << " copyRestoreUs=" << copyMicroseconds
          << " replayFrames=" << replayFrames
          << " replayUs=" << replayMicroseconds
          << " densityRmse=" << densityRmse;
    }
  }
}

TEST(ArtifactSmokeRuntimeStandaloneTest,
     VorticityStrengthChangesTheSmokePlume) {
  const SmokeRun low = runProductionSmokeRuntime(
      0.5f, false, false, 30.0, 44, 0.1f);
  const SmokeRun medium = runProductionSmokeRuntime(
      0.5f, false, false, 30.0, 44, 0.4f);
  const SmokeRun high = runProductionSmokeRuntime(
      0.5f, false, false, 30.0, 44, 0.8f);

  EXPECT_GT(countDifferentPixels(low.image, medium.image), 100);
  EXPECT_GT(countDifferentPixels(medium.image, high.image), 100);

  const QString outputDirectory =
      qEnvironmentVariable("ARTIFACT_SMOKE_RUNTIME_OUTPUT_DIR");
  if (!outputDirectory.isEmpty()) {
    ASSERT_TRUE(QDir().mkpath(outputDirectory.toUtf8().constData()));
    EXPECT_TRUE(low.image.save(QDir(outputDirectory).filePath(
        QStringLiteral("smoke_vorticity_0p1.png")), "PNG"));
    EXPECT_TRUE(medium.image.save(QDir(outputDirectory).filePath(
        QStringLiteral("smoke_vorticity_0p4.png")), "PNG"));
    EXPECT_TRUE(high.image.save(QDir(outputDirectory).filePath(
        QStringLiteral("smoke_vorticity_0p8.png")), "PNG"));
  }
}

TEST(ArtifactSmokeRuntimeStandaloneTest,
     MeasuresResolutionScalingForProductionVorticityConfinement) {
  constexpr int referenceWidth = 96;
  constexpr int finalFrame = 28;
  constexpr float referenceVorticity = 0.1f;
  constexpr std::array<std::pair<int, int>, 3> resolutions{{
      {96, 72}, {128, 96}, {192, 144}}};
  constexpr std::array<float, 2> emitterOffsets{-0.02f, 0.02f};
  const auto outputDirectory = qEnvironmentVariable(
      "ARTIFACT_SMOKE_RUNTIME_OUTPUT_DIR");
  const auto measure = [](const ArtifactCore::FluidSnapshot2D& field) {
    double mass = 0.0;
    double weightedX = 0.0;
    double weightedY = 0.0;
    double squaredGradient = 0.0;
    for (int y = 1; y < field.height - 1; ++y) {
      for (int x = 1; x < field.width - 1; ++x) {
        const auto cell = static_cast<std::size_t>(x + y * field.width);
        const double density = field.density[cell];
        mass += density;
        weightedX += density * static_cast<double>(x) / (field.width - 2);
        weightedY += density * static_cast<double>(y) / (field.height - 2);
        const double dx = (field.density[cell + 1] -
                           field.density[cell - 1]) *
            static_cast<double>(field.width - 2) * 0.5;
        const double dy = (field.density[cell + field.width] -
                           field.density[cell - field.width]) *
            static_cast<double>(field.height - 2) * 0.5;
        squaredGradient += dx * dx + dy * dy;
      }
    }
    const double cellArea = 1.0 /
        static_cast<double>((field.width - 2) * (field.height - 2));
    return std::array<double, 4>{
        mass * cellArea,
        mass > 0.0 ? weightedX / mass : 0.0,
        mass > 0.0 ? weightedY / mass : 0.0,
        std::sqrt(squaredGradient * cellArea)};
  };
  const auto resampleToReference = [](const ArtifactCore::FluidSnapshot2D& field) {
    std::vector<float> result(referenceWidth * 72);
    for (int y = 0; y < 72; ++y) {
      for (int x = 0; x < referenceWidth; ++x) {
        const float sourceX = 1.0f + static_cast<float>(x) *
            static_cast<float>(field.width - 2) /
            static_cast<float>(referenceWidth - 1);
        const float sourceY = 1.0f + static_cast<float>(y) *
            static_cast<float>(field.height - 2) / 71.0f;
        const int x0 = std::clamp(static_cast<int>(std::floor(sourceX)),
                                  0, field.width - 1);
        const int y0 = std::clamp(static_cast<int>(std::floor(sourceY)),
                                  0, field.height - 1);
        const int x1 = std::min(field.width - 1, x0 + 1);
        const int y1 = std::min(field.height - 1, y0 + 1);
        const float tx = sourceX - static_cast<float>(x0);
        const float ty = sourceY - static_cast<float>(y0);
        const auto at = [&field](const int sx, const int sy) {
          return field.density[static_cast<std::size_t>(sx + sy * field.width)];
        };
        const float top = at(x0, y0) * (1.0f - tx) + at(x1, y0) * tx;
        const float bottom = at(x0, y1) * (1.0f - tx) + at(x1, y1) * tx;
        result[static_cast<std::size_t>(x + y * referenceWidth)] =
            top * (1.0f - ty) + bottom * ty;
      }
    }
    return result;
  };
  const auto densityRmse = [](const std::vector<float>& a,
                              const std::vector<float>& b) {
    EXPECT_EQ(a.size(), b.size());
    if (a.size() != b.size()) return 0.0;
    double squaredError = 0.0;
    for (std::size_t cell = 0; cell < a.size(); ++cell) {
      const double difference = a[cell] - b[cell];
      squaredError += difference * difference;
    }
    return std::sqrt(squaredError / static_cast<double>(a.size()));
  };
  struct Result final {
    std::array<double, 4> metrics{};
    std::vector<float> normalizedDensity;
  };
  std::array<std::array<std::array<Result, 2>, 3>, 3> results;

  for (std::size_t resolutionIndex = 0;
       resolutionIndex < resolutions.size(); ++resolutionIndex) {
    const auto [width, height] = resolutions[resolutionIndex];
    const float gridScale = static_cast<float>(width) / referenceWidth;
    for (std::size_t seedIndex = 0;
         seedIndex < emitterOffsets.size(); ++seedIndex) {
      for (int variant = 0; variant < 3; ++variant) {
        Artifact::LayerFluidRuntimeState state;
        Artifact::LayerSmokeRuntimeSettings settings;
        settings.gridWidth = width;
        settings.gridHeight = height;
        settings.solverIterations = 8;
        settings.emitterX = 0.5f + emitterOffsets[seedIndex];
        settings.emitterY = 0.84f;
        settings.emitterRadius = 0.08f;
        settings.emitterDensity = 0.6f;
        settings.emitterVelocityY = -1.2f;
        settings.emitterSwirl = seedIndex == 0 ? 0.15f : -0.15f;
        const float vorticityScale = variant == 0 ? 1.0f
            : variant == 1 ? gridScale : gridScale * gridScale;
        settings.vorticity = referenceVorticity * vorticityScale;
        for (int frame = 0; frame <= finalFrame; ++frame) {
          Artifact::advanceLayerSmokeRuntime(state, frame, 30.0, settings);
        }
        const auto field = state.fluidSolver_->snapshot();
        auto& result = results[static_cast<std::size_t>(variant)]
            [resolutionIndex][seedIndex];
        result.metrics = measure(field);
        result.normalizedDensity = resampleToReference(field);
        const double resampledMass = std::accumulate(
            result.normalizedDensity.begin(),
            result.normalizedDensity.end(), 0.0);
        if (resampledMass > 0.0) {
          const float normalization = static_cast<float>(
              result.metrics[0] *
              static_cast<double>(result.normalizedDensity.size()) /
              resampledMass);
          for (auto& density : result.normalizedDensity) density *= normalization;
        }
        GTEST_LOG_(INFO) << "vorticity-resolution variant="
            << (variant == 0 ? "constant"
                : variant == 1 ? "grid-linear" : "grid-squared")
            << " resolution=" << width << 'x' << height
            << " seed=" << seedIndex
            << " vorticity=" << settings.vorticity
            << " normalizedMass=" << result.metrics[0]
            << " centroid=" << result.metrics[1] << ',' << result.metrics[2]
            << " physicalGradientRms=" << result.metrics[3];
      }
    }
  }

  for (std::size_t seedIndex = 0; seedIndex < emitterOffsets.size(); ++seedIndex) {
    const double constantRmse = densityRmse(
        results[0][0][seedIndex].normalizedDensity,
        results[0][2][seedIndex].normalizedDensity);
    const double linearRmse = densityRmse(
        results[1][0][seedIndex].normalizedDensity,
        results[1][2][seedIndex].normalizedDensity);
    const double squaredRmse = densityRmse(
        results[2][0][seedIndex].normalizedDensity,
        results[2][2][seedIndex].normalizedDensity);
    GTEST_LOG_(INFO) << "vorticity-resolution cross-grid seed=" << seedIndex
        << " constantRmse=" << constantRmse
        << " gridLinearRmse=" << linearRmse
        << " gridSquaredRmse=" << squaredRmse;
    EXPECT_GT(constantRmse, 0.0);
    EXPECT_GT(linearRmse, 0.0);
    EXPECT_GT(squaredRmse, 0.0);
  }

  if (!outputDirectory.isEmpty()) {
    ASSERT_TRUE(QDir().mkpath(outputDirectory));
    for (std::size_t resolutionIndex = 0;
         resolutionIndex < resolutions.size(); ++resolutionIndex) {
      for (std::size_t seedIndex = 0;
           seedIndex < emitterOffsets.size(); ++seedIndex) {
        for (int variant = 0; variant < 3; ++variant) {
          const auto& result = results[static_cast<std::size_t>(variant)]
              [resolutionIndex][seedIndex];
          const auto [width, height] = resolutions[resolutionIndex];
          QImage image(referenceWidth * kImageScale, 72 * kImageScale,
                       QImage::Format_ARGB32);
          image.fill(kBackground);
          const float peak = *std::max_element(
              result.normalizedDensity.begin(),
              result.normalizedDensity.end());
          for (int y = 0; y < 72; ++y) {
            for (int x = 0; x < referenceWidth; ++x) {
              const float density = result.normalizedDensity[
                  static_cast<std::size_t>(x + y * referenceWidth)];
              const auto value = static_cast<int>(std::lround(
                  255.0f * std::clamp(density / std::max(peak, 1.0e-6f),
                                      0.0f, 1.0f)));
              for (int sy = 0; sy < kImageScale; ++sy) {
                auto* row = reinterpret_cast<QRgb*>(
                    image.scanLine(y * kImageScale + sy));
                for (int sx = 0; sx < kImageScale; ++sx) {
                  row[x * kImageScale + sx] = qRgba(value, value, value, 255);
                }
              }
            }
          }
          const QString name = QStringLiteral(
              "vorticity_%1_%2_seed%3.png")
              .arg(variant == 0 ? QStringLiteral("constant")
                    : variant == 1 ? QStringLiteral("grid_linear")
                                    : QStringLiteral("grid_squared"))
              .arg(width)
              .arg(seedIndex);
          EXPECT_TRUE(image.save(QDir(outputDirectory).filePath(name), "PNG"));
        }
      }
    }
  }
}

TEST(ArtifactSmokeRuntimeStandaloneTest,
     SeparatesSmokeResolutionDriftByInjectionAndSolverStage) {
  constexpr std::array<std::pair<int, int>, 3> resolutions{{
      {96, 72}, {128, 96}, {192, 144}}};
  constexpr int solverIterations = 8;
  constexpr float dt = 1.0f / 30.0f;
  const auto measure = [](const ArtifactCore::FluidSnapshot2D& field) {
    double mass = 0.0;
    double weightedX = 0.0;
    double weightedY = 0.0;
    double secondX = 0.0;
    double secondY = 0.0;
    double squaredGradient = 0.0;
    for (int y = 1; y < field.height - 1; ++y) {
      for (int x = 1; x < field.width - 1; ++x) {
        const auto index = static_cast<std::size_t>(x + y * field.width);
        const double density = field.density[index];
        const double nx = static_cast<double>(x - 1) /
            static_cast<double>(field.width - 3);
        const double ny = static_cast<double>(y - 1) /
            static_cast<double>(field.height - 3);
        mass += density;
        weightedX += density * nx;
        weightedY += density * ny;
        secondX += density * nx * nx;
        secondY += density * ny * ny;
        const double dx = (field.density[index + 1] -
                           field.density[index - 1]) *
            static_cast<double>(field.width - 3) * 0.5;
        const double dy = (field.density[index + field.width] -
                           field.density[index - field.width]) *
            static_cast<double>(field.height - 3) * 0.5;
        squaredGradient += dx * dx + dy * dy;
      }
    }
    const double cellArea = 1.0 /
        static_cast<double>((field.width - 2) * (field.height - 2));
    return std::array<double, 6>{
        mass * cellArea,
        mass > 0.0 ? weightedX / mass : 0.0,
        mass > 0.0 ? weightedY / mass : 0.0,
        mass > 0.0 ? std::sqrt(std::max(0.0,
            secondX / mass - (weightedX / mass) * (weightedX / mass))) : 0.0,
        mass > 0.0 ? std::sqrt(std::max(0.0,
            secondY / mass - (weightedY / mass) * (weightedY / mass))) : 0.0,
        std::sqrt(squaredGradient * cellArea)};
  };
  constexpr std::array<const char*, 6> stageNames{
      "injection", "advection-only", "buoyancy-only", "full-solver",
      "buoyancy-no-projection", "buoyancy-project"};
  for (const auto& [width, height] : resolutions) {
    for (int stage = 0; stage < 6; ++stage) {
      ArtifactCore::FluidSolver2D solver(width, height);
      solver.setSolverIterations(solverIterations);
      solver.setAdaptiveIterations(false);
      solver.setDiffusion(0.0f);
      solver.setViscosity(0.0f);
      solver.setVorticity(0.0f);
      solver.setBuoyancy(stage == 2 || stage == 3 ? 0.7f : 0.0f);
      ArtifactCore::injectSmokeEmitter2D(
          solver,
          ArtifactCore::SmokeEmitter2D{
              .x = 0.5f, .y = 0.84f, .radius = 0.08f, .density = 0.6f,
              .velocityX = 0.0f, .velocityY = -1.2f, .swirl = 0.15f},
          1.0f);
      if (stage != 0) {
        if (stage < 4) {
          solver.update(dt);
        } else {
          auto field = solver.snapshot();
          const auto initialVelocityX = field.velocityX;
          const auto initialVelocityY = field.velocityY;
          for (std::size_t index = 0; index < field.density.size(); ++index) {
            field.velocityY[index] -= field.density[index] * 0.7f * dt;
          }
          if (stage == 5) {
            ArtifactCore::FluidSolver2D projectionSolver(width, height);
            projectionSolver.setSolverIterations(solverIterations);
            projectionSolver.setAdaptiveIterations(false);
            projectionSolver.setBuoyancy(0.0f);
            projectionSolver.setVorticity(0.0f);
            projectionSolver.setDiffusion(0.0f);
            projectionSolver.setViscosity(0.0f);
            ASSERT_TRUE(projectionSolver.restoreSnapshot(field));
            projectionSolver.update(dt);
            field = projectionSolver.snapshot();
          }
          double beforeSquared = 0.0;
          double afterSquared = 0.0;
          double densityWeightedAfterY = 0.0;
          double densitySum = 0.0;
          for (std::size_t index = 0; index < field.density.size(); ++index) {
            const double deltaX = field.velocityX[index] - initialVelocityX[index];
            const double deltaY = field.velocityY[index] - initialVelocityY[index];
            beforeSquared += deltaX * deltaX + deltaY * deltaY;
            const double sourceY = -field.density[index] * 0.7 * dt;
            afterSquared += sourceY * sourceY;
            densityWeightedAfterY += field.density[index] * deltaY;
            densitySum += field.density[index];
          }
          GTEST_LOG_(INFO) << "smoke-buoyancy-velocity stage=" << stageNames[stage]
              << " resolution=" << width << 'x' << height
              << " velocityDeltaRms=" << std::sqrt(beforeSquared /
                     static_cast<double>(field.density.size()))
              << " rawBuoyancyRms=" << std::sqrt(afterSquared /
                     static_cast<double>(field.density.size()))
              << " densityWeightedVerticalDelta=" <<
                     (densitySum > 0.0 ? densityWeightedAfterY / densitySum : 0.0);
        }
      }
      const auto metrics = measure(solver.snapshot());
      GTEST_LOG_(INFO) << "smoke-stage-resolution stage=" << stageNames[stage]
          << " resolution=" << width << 'x' << height
          << " normalizedMass=" << metrics[0]
          << " centroid=" << metrics[1] << ',' << metrics[2]
          << " sigma=" << metrics[3] << ',' << metrics[4]
          << " physicalGradientRms=" << metrics[5];
    }
  }

  // Seed one identical smooth density/velocity field in normalized space,
  // then test transport without source injection or buoyancy. This separates
  // the solver's resolution response from emitter rasterization.
  for (const auto& [width, height] : resolutions) {
    ArtifactCore::FluidSolver2D solver(width, height);
    solver.setSolverIterations(solverIterations);
    solver.setAdaptiveIterations(false);
    solver.setDiffusion(0.0f);
    solver.setViscosity(0.0f);
    solver.setVorticity(0.0f);
    solver.setBuoyancy(0.0f);
    auto field = solver.snapshot();
    const double cx = 0.5;
    const double cy = 0.66;
    for (int y = 1; y < height - 1; ++y) {
      for (int x = 1; x < width - 1; ++x) {
        const double nx = static_cast<double>(x - 1) / (width - 3);
        const double ny = static_cast<double>(y - 1) / (height - 3);
        const double dx = (nx - cx) / 0.11;
        const double dy = (ny - cy) / 0.16;
        const auto index = static_cast<std::size_t>(x + y * width);
        field.density[index] = static_cast<float>(
            0.8 * std::exp(-0.5 * (dx * dx + dy * dy)));
        const double vortexRadius2 = (nx - cx) * (nx - cx) +
            (ny - cy) * (ny - cy);
        const double envelope = std::exp(-vortexRadius2 / (2.0 * 0.13 * 0.13));
        field.velocityX[index] = static_cast<float>(-(ny - cy) * 1.5 * envelope);
        field.velocityY[index] = static_cast<float>((nx - cx) * 1.5 * envelope);
      }
    }
    ASSERT_TRUE(solver.restoreSnapshot(field));
    const auto initial = measure(solver.snapshot());
    solver.update(dt);
    const auto after = measure(solver.snapshot());
    GTEST_LOG_(INFO) << "smoke-normalized-field-transport resolution="
        << width << 'x' << height
        << " mass=" << initial[0] << "->" << after[0]
        << " centroidY=" << initial[2] << "->" << after[2]
        << " sigma=" << after[3] << ',' << after[4]
        << " gradientRms=" << after[5];
  }
}

TEST(ArtifactSmokeRuntimeStandaloneTest,
     TracksSmokeMorphologyDriftAcrossFramesAndResolutions) {
  constexpr std::array<std::pair<int, int>, 3> resolutions{{
      {96, 72}, {128, 96}, {192, 144}}};
  constexpr int finalFrame = 28;
  constexpr float emitterX = 0.5f;
  constexpr float emitterY = 0.84f;
  constexpr float emitterRadius = 0.08f;
  constexpr float emitterDensity = 0.6f;
  constexpr float emitterVelocityY = -1.2f;
  constexpr float buoyancy = 0.7f;
  constexpr float vorticity = 0.1f;
  constexpr int solverIterations = 8;
  struct Measurement final {
    double mass = 0.0;
    double centroidX = 0.0;
    double centroidY = 0.0;
    double sigmaX = 0.0;
    double sigmaY = 0.0;
    double gradientRms = 0.0;
  };
  const auto measure = [](const ArtifactCore::FluidSnapshot2D& field) {
    Measurement result;
    double weightedX2 = 0.0;
    double weightedY2 = 0.0;
    double squaredGradient = 0.0;
    double densitySum = 0.0;
    for (int y = 1; y < field.height - 1; ++y) {
      for (int x = 1; x < field.width - 1; ++x) {
        const auto index = static_cast<std::size_t>(x + y * field.width);
        const double density = field.density[index];
        const double nx = static_cast<double>(x - 1) / (field.width - 3);
        const double ny = static_cast<double>(y - 1) / (field.height - 3);
        densitySum += density;
        result.centroidX += density * nx;
        result.centroidY += density * ny;
        weightedX2 += density * nx * nx;
        weightedY2 += density * ny * ny;
        const double dx = (field.density[index + 1] -
                           field.density[index - 1]) *
            static_cast<double>(field.width - 3) * 0.5;
        const double dy = (field.density[index + field.width] -
                           field.density[index - field.width]) *
            static_cast<double>(field.height - 3) * 0.5;
        squaredGradient += dx * dx + dy * dy;
      }
    }
    const double area = static_cast<double>(
        (field.width - 2) * (field.height - 2));
    result.mass = densitySum / area;
    if (densitySum > 0.0) {
      result.centroidX /= densitySum;
      result.centroidY /= densitySum;
      result.sigmaX = std::sqrt(std::max(0.0,
          weightedX2 / densitySum - result.centroidX * result.centroidX));
      result.sigmaY = std::sqrt(std::max(0.0,
          weightedY2 / densitySum - result.centroidY * result.centroidY));
    }
    result.gradientRms = std::sqrt(squaredGradient / area);
    return result;
  };
  std::array<Artifact::LayerFluidRuntimeState, 3> states;
  std::array<Artifact::LayerSmokeRuntimeSettings, 3> settings;
  for (std::size_t grid = 0; grid < resolutions.size(); ++grid) {
    settings[grid].gridWidth = resolutions[grid].first;
    settings[grid].gridHeight = resolutions[grid].second;
    settings[grid].solverIterations = solverIterations;
    settings[grid].emitterX = emitterX;
    settings[grid].emitterY = emitterY;
    settings[grid].emitterRadius = emitterRadius;
    settings[grid].emitterDensity = emitterDensity;
    settings[grid].emitterVelocityY = emitterVelocityY;
    settings[grid].emitterSwirl = 0.15f;
    settings[grid].buoyancy = buoyancy;
    settings[grid].vorticity = vorticity;
  }
  std::array<Measurement, 3> previous{};
  for (int frame = 0; frame <= finalFrame; ++frame) {
    for (std::size_t grid = 0; grid < resolutions.size(); ++grid) {
      Artifact::advanceLayerSmokeRuntime(
          states[grid], frame, 30.0, settings[grid]);
      const auto current = measure(states[grid].fluidSolver_->snapshot());
      const auto [width, height] = resolutions[grid];
      if (frame == 0 || frame == 1 || frame == 2 || frame == 4 ||
          frame == 8 || frame == 16 || frame == 28) {
        GTEST_LOG_(INFO) << "smoke-framewise-resolution frame=" << frame
            << " grid=" << width << 'x' << height
            << " mass=" << current.mass
            << " centroid=" << current.centroidX << ',' << current.centroidY
            << " sigma=" << current.sigmaX << ',' << current.sigmaY
            << " gradientRms=" << current.gradientRms
            << " deltaCentroidY=" << current.centroidY - previous[grid].centroidY
            << " deltaMass=" << current.mass - previous[grid].mass;
      }
      previous[grid] = current;
    }
  }
}

TEST(ArtifactSmokeRuntimeStandaloneTest,
     MeasuresResolutionDriftForStationaryContinuousSmokeSource) {
  constexpr std::array<std::pair<int, int>, 3> resolutions{{
      {96, 72}, {128, 96}, {192, 144}}};
  constexpr int finalFrame = 28;
  constexpr int solverIterations = 8;
  constexpr float emitterX = 0.5f;
  constexpr float emitterY = 0.84f;
  constexpr float emitterRadius = 0.08f;
  constexpr float emitterDensity = 0.6f;
  for (const auto& [width, height] : resolutions) {
    Artifact::LayerFluidRuntimeState state;
    Artifact::LayerSmokeRuntimeSettings settings;
    settings.gridWidth = width;
    settings.gridHeight = height;
    settings.solverIterations = solverIterations;
    settings.emitterX = emitterX;
    settings.emitterY = emitterY;
    settings.emitterRadius = emitterRadius;
    settings.emitterDensity = emitterDensity;
    settings.emitterVelocityX = 0.0f;
    settings.emitterVelocityY = 0.0f;
    settings.emitterSwirl = 0.0f;
    settings.buoyancy = 0.0f;
    settings.vorticity = 0.0f;
    for (int frame = 0; frame <= finalFrame; ++frame) {
      Artifact::advanceLayerSmokeRuntime(state, frame, 30.0, settings);
    }
    const auto field = state.fluidSolver_->snapshot();
    double mass = 0.0;
    double weightedY = 0.0;
    double weightedY2 = 0.0;
    for (int y = 1; y < height - 1; ++y) {
      for (int x = 1; x < width - 1; ++x) {
        const double density = field.density[
            static_cast<std::size_t>(x + y * width)];
        const double ny = static_cast<double>(y - 1) / (height - 3);
        mass += density;
        weightedY += density * ny;
        weightedY2 += density * ny * ny;
      }
    }
    const double densityMass = mass / static_cast<double>(
        (width - 2) * (height - 2));
    const double centroidY = mass > 0.0 ? weightedY / mass : 0.0;
    const double sigmaY = mass > 0.0
        ? std::sqrt(std::max(0.0,
            weightedY2 / mass - centroidY * centroidY)) : 0.0;
    GTEST_LOG_(INFO) << "smoke-stationary-source grid=" << width << 'x'
        << height << " normalizedMass=" << densityMass
        << " centroidY=" << centroidY << " sigmaY=" << sigmaY;
  }
}

TEST(ArtifactSmokeRuntimeStandaloneTest,
     MeasuresProductionSmokeSolverStageEvolutionAcrossResolutions) {
  constexpr std::array<std::pair<int, int>, 3> resolutions{{
      {96, 72}, {128, 96}, {192, 144}}};
  constexpr std::array<int, 4> reportedFrames{0, 1, 4, 28};
  constexpr std::array<const char*, 5> stageNames{
      "pre-update", "after-projection-1", "after-velocity-advection",
      "after-projection-2", "after-density-advection"};
  constexpr int solverIterations = 8;
  struct Metrics final {
    double mass = 0.0;
    double centroidY = 0.0;
    double velocityRms = 0.0;
    double divergenceRms = 0.0;
  };
  const auto measure = [](const SmokeProjectionStageCapture& field) {
    Metrics result;
    double densitySum = 0.0;
    double weightedY = 0.0;
    double velocityEnergy = 0.0;
    double divergenceEnergy = 0.0;
    std::size_t velocitySamples = 0;
    for (int y = 1; y < field.height - 1; ++y) {
      for (int x = 1; x < field.width - 1; ++x) {
        const auto index = static_cast<std::size_t>(x + y * field.width);
        const double density = field.density[index];
        densitySum += density;
        weightedY += density * static_cast<double>(y - 1) /
                     static_cast<double>(field.height - 3);
        const double vx = field.velocityX[index];
        const double vy = field.velocityY[index];
        velocityEnergy += vx * vx + vy * vy;
        ++velocitySamples;
        if (x >= 2 && x < field.width - 2 &&
            y >= 2 && y < field.height - 2) {
          const double divergence = 0.5 * (
              (field.velocityX[index + 1] - field.velocityX[index - 1]) *
                  (field.width - 3) +
              (field.velocityY[index + field.width] -
               field.velocityY[index - field.width]) * (field.height - 3));
          divergenceEnergy += divergence * divergence;
        }
      }
    }
    const double area = static_cast<double>(
        (field.width - 2) * (field.height - 2));
    result.mass = densitySum / area;
    result.centroidY = densitySum > 0.0 ? weightedY / densitySum : 0.0;
    result.velocityRms = velocitySamples > 0
        ? std::sqrt(velocityEnergy / static_cast<double>(velocitySamples)) : 0.0;
    const double divergenceSamples = static_cast<double>(
        (field.width - 5) * (field.height - 5));
    result.divergenceRms = divergenceSamples > 0
        ? std::sqrt(divergenceEnergy / divergenceSamples) : 0.0;
    return result;
  };

  for (const auto& [width, height] : resolutions) {
    Artifact::LayerFluidRuntimeState state;
    Artifact::LayerSmokeRuntimeSettings settings;
    settings.gridWidth = width;
    settings.gridHeight = height;
    settings.solverIterations = solverIterations;
    settings.emitterX = 0.5f;
    settings.emitterY = 0.84f;
    settings.emitterRadius = 0.08f;
    settings.emitterDensity = 0.6f;
    settings.emitterVelocityY = -1.2f;
    settings.emitterSwirl = 0.15f;
    settings.buoyancy = 0.7f;
    settings.vorticity = 0.1f;
    std::vector<SmokeProjectionStageCapture> captures;
    for (int frame = 0; frame <= 28; ++frame) {
      captures.clear();
      gSmokeProjectionCaptures = &captures;
      Artifact::advanceLayerSmokeRuntime(state, frame, 30.0, settings);
      gSmokeProjectionCaptures = nullptr;
      ASSERT_EQ(captures.size(), 4U);
      const bool report = std::find(reportedFrames.begin(), reportedFrames.end(),
                                    frame) != reportedFrames.end();
      if (report) {
        for (const auto& capture : captures) {
          const int blocks = width * height / 4608;
          const int expectedIterations = std::min(
              40, solverIterations + std::max(0, blocks - 1) * 4);
          EXPECT_EQ(capture.solverIterations, expectedIterations);
          const auto metrics = measure(capture);
          GTEST_LOG_(INFO) << "production-smoke-stage frame=" << frame
              << " grid=" << width << 'x' << height
              << " stage=" << stageNames[static_cast<std::size_t>(capture.stage)]
              << " effectiveIterations=" << capture.solverIterations
              << " mass=" << metrics.mass
              << " centroidY=" << metrics.centroidY
              << " velocityRms=" << metrics.velocityRms
              << " divergenceRms=" << metrics.divergenceRms;
        }
      }
    }
  }
}

TEST(ArtifactSmokeRuntimeStandaloneTest,
     MeasuresProductionProjectionResponseToIterationCount) {
  constexpr std::array<std::pair<int, int>, 3> resolutions{{
      {96, 72}, {128, 96}, {192, 144}}};
  constexpr std::array<int, 4> iterationCounts{8, 20, 40, 80};
  constexpr float dt = 1.0f / 30.0f;
  const auto divergenceRms = [](const SmokeProjectionStageCapture& field) {
    double squared = 0.0;
    std::size_t samples = 0;
    for (int y = 2; y < field.height - 2; ++y) {
      for (int x = 2; x < field.width - 2; ++x) {
        const auto index = static_cast<std::size_t>(x + y * field.width);
        const double divergence = 0.5 * (
            (field.velocityX[index + 1] - field.velocityX[index - 1]) *
                (field.width - 3) +
            (field.velocityY[index + field.width] -
             field.velocityY[index - field.width]) * (field.height - 3));
        squared += divergence * divergence;
        ++samples;
      }
    }
    return samples > 0 ? std::sqrt(squared / static_cast<double>(samples)) : 0.0;
  };
  for (const auto& [width, height] : resolutions) {
    ArtifactCore::FluidSolver2D seedSolver(width, height);
    auto seed = seedSolver.snapshot();
    seed.viscosity = 0.0f;
    seed.diffusion = 0.0f;
    seed.buoyancy = 0.0f;
    seed.vorticity = 0.0f;
    seed.adaptiveIterations = false;
    seed.solverIterations = 8;
    for (int y = 1; y < height - 1; ++y) {
      for (int x = 1; x < width - 1; ++x) {
        const double nx = static_cast<double>(x - 1) / (width - 3);
        const double ny = static_cast<double>(y - 1) / (height - 3);
        const double dx = nx - 0.5;
        const double dy = ny - 0.65;
        const double radius2 = dx * dx + dy * dy;
        const double envelope = std::exp(-radius2 / (2.0 * 0.18 * 0.18));
        const auto index = static_cast<std::size_t>(x + y * width);
        seed.density[index] = static_cast<float>(
            0.8 * std::exp(-0.5 *
                (dx * dx / (0.12 * 0.12) + dy * dy / (0.15 * 0.15))));
        seed.velocityX[index] = static_cast<float>(
            dx * 0.8 * envelope - dy * 1.2 * envelope);
        seed.velocityY[index] = static_cast<float>(
            dy * 0.8 * envelope + dx * 1.2 * envelope);
      }
    }
    SmokeProjectionStageCapture before;
    before.width = width;
    before.height = height;
    before.density.assign(seed.density.begin(), seed.density.end());
    before.velocityX.assign(seed.velocityX.begin(), seed.velocityX.end());
    before.velocityY.assign(seed.velocityY.begin(), seed.velocityY.end());
    const double initialDivergence = divergenceRms(before);
    for (const int iterations : iterationCounts) {
      ArtifactCore::FluidSolver2D solver(width, height);
      ASSERT_TRUE(solver.restoreSnapshot(seed));
      solver.setSolverIterations(iterations);
      solver.setAdaptiveIterations(false);
      solver.setViscosity(0.0f);
      solver.setDiffusion(0.0f);
      solver.setBuoyancy(0.0f);
      solver.setVorticity(0.0f);
      std::vector<SmokeProjectionStageCapture> captures;
      gSmokeProjectionCaptures = &captures;
      solver.update(dt);
      gSmokeProjectionCaptures = nullptr;
      ASSERT_EQ(captures.size(), 4U);
      GTEST_LOG_(INFO) << "production-projection-iterations grid=" << width
          << 'x' << height << " iterations=" << iterations
          << " divergenceBefore=" << initialDivergence
          << " afterProjection1=" << divergenceRms(captures[0])
          << " afterVelocityAdvection=" << divergenceRms(captures[1])
          << " afterProjection2=" << divergenceRms(captures[2]);
    }
  }
}

TEST(ArtifactSmokeRuntimeStandaloneTest,
     ComparesProductionSmokeMorphologyAtProjectionIterationCounts) {
  constexpr std::array<std::pair<int, int>, 3> resolutions{{
      {96, 72}, {128, 96}, {192, 144}}};
  constexpr std::array<int, 4> iterationCounts{8, 20, 24, 40};
  constexpr int finalFrame = 28;
  struct Result final {
    double mass = 0.0;
    double centroidY = 0.0;
    double sigmaY = 0.0;
    std::vector<float> normalizedDensity;
  };
  std::array<std::array<Result, 4>, 3> results;
  const auto outputDirectory = qEnvironmentVariable(
      "ARTIFACT_SMOKE_RUNTIME_OUTPUT_DIR");
  if (!outputDirectory.isEmpty()) {
    ASSERT_TRUE(QDir().mkpath(outputDirectory));
  }
  const auto resample = [](const ArtifactCore::FluidSnapshot2D& field) {
    std::vector<float> values(static_cast<std::size_t>(kGridWidth * kGridHeight));
    for (int y = 0; y < kGridHeight; ++y) {
      for (int x = 0; x < kGridWidth; ++x) {
        const float sourceX = 1.0f + static_cast<float>(x) *
            static_cast<float>(field.width - 3) /
            static_cast<float>(kGridWidth - 1);
        const float sourceY = 1.0f + static_cast<float>(y) *
            static_cast<float>(field.height - 3) /
            static_cast<float>(kGridHeight - 1);
        const int x0 = std::clamp(static_cast<int>(std::floor(sourceX)),
                                  0, field.width - 1);
        const int y0 = std::clamp(static_cast<int>(std::floor(sourceY)),
                                  0, field.height - 1);
        const int x1 = std::min(field.width - 1, x0 + 1);
        const int y1 = std::min(field.height - 1, y0 + 1);
        const float tx = sourceX - static_cast<float>(x0);
        const float ty = sourceY - static_cast<float>(y0);
        const auto at = [&field](const int px, const int py) {
          return field.density[static_cast<std::size_t>(px + py * field.width)];
        };
        const float top = at(x0, y0) * (1.0f - tx) + at(x1, y0) * tx;
        const float bottom = at(x0, y1) * (1.0f - tx) + at(x1, y1) * tx;
        values[static_cast<std::size_t>(x + y * kGridWidth)] =
            top * (1.0f - ty) + bottom * ty;
      }
    }
    return values;
  };
  for (std::size_t grid = 0; grid < resolutions.size(); ++grid) {
    const auto [width, height] = resolutions[grid];
    for (std::size_t count = 0; count < iterationCounts.size(); ++count) {
      Artifact::LayerFluidRuntimeState state;
      Artifact::LayerSmokeRuntimeSettings settings;
      settings.gridWidth = width;
      settings.gridHeight = height;
      settings.solverIterations = iterationCounts[count];
      settings.emitterX = 0.5f;
      settings.emitterY = 0.84f;
      settings.emitterRadius = 0.08f;
      settings.emitterDensity = 0.6f;
      settings.emitterVelocityY = -1.2f;
      settings.emitterSwirl = 0.15f;
      settings.buoyancy = 0.7f;
      settings.vorticity = 0.1f;
      const auto startTime = std::chrono::steady_clock::now();
      for (int frame = 0; frame <= finalFrame; ++frame) {
        Artifact::advanceLayerSmokeRuntime(state, frame, 30.0, settings);
      }
      const double elapsedMilliseconds = std::chrono::duration<double,
          std::milli>(std::chrono::steady_clock::now() - startTime).count();
      const auto field = state.fluidSolver_->snapshot();
      auto& result = results[grid][count];
      double densitySum = 0.0;
      double weightedY = 0.0;
      double weightedY2 = 0.0;
      for (int y = 1; y < height - 1; ++y) {
        for (int x = 1; x < width - 1; ++x) {
          const double density = field.density[
              static_cast<std::size_t>(x + y * width)];
          const double ny = static_cast<double>(y - 1) / (height - 3);
          densitySum += density;
          weightedY += density * ny;
          weightedY2 += density * ny * ny;
        }
      }
      result.mass = densitySum /
          static_cast<double>((width - 2) * (height - 2));
      result.centroidY = densitySum > 0.0 ? weightedY / densitySum : 0.0;
      result.sigmaY = densitySum > 0.0 ? std::sqrt(std::max(0.0,
          weightedY2 / densitySum - result.centroidY * result.centroidY)) : 0.0;
      result.normalizedDensity = resample(field);
      double resampledSum = std::accumulate(result.normalizedDensity.begin(),
          result.normalizedDensity.end(), 0.0);
      if (resampledSum > 0.0) {
        const double scale = result.mass *
            static_cast<double>(result.normalizedDensity.size()) / resampledSum;
        for (auto& value : result.normalizedDensity)
          value = static_cast<float>(value * scale);
      }
      GTEST_LOG_(INFO) << "smoke-iteration-morphology grid=" << width << 'x'
          << height << " baseIterations=" << iterationCounts[count]
          << " mass=" << result.mass << " centroidY=" << result.centroidY
          << " sigmaY=" << result.sigmaY
          << " debugElapsedMs=" << elapsedMilliseconds;
      if (!outputDirectory.isEmpty()) {
        QImage image(kGridWidth * kImageScale, kGridHeight * kImageScale,
                     QImage::Format_ARGB32);
        image.fill(kBackground);
        const float peak = *std::max_element(result.normalizedDensity.begin(),
                                             result.normalizedDensity.end());
        for (int y = 0; y < kGridHeight; ++y) {
          for (int x = 0; x < kGridWidth; ++x) {
            const float density = result.normalizedDensity[
                static_cast<std::size_t>(x + y * kGridWidth)];
            const int value = static_cast<int>(std::lround(255.0f *
                std::sqrt(std::clamp(density / std::max(peak, 1.0e-6f),
                                     0.0f, 1.0f))));
            for (int sy = 0; sy < kImageScale; ++sy) {
              auto* row = reinterpret_cast<QRgb*>(
                  image.scanLine(y * kImageScale + sy));
              std::fill_n(row + x * kImageScale, kImageScale,
                          qRgba(value, value, value, 255));
            }
          }
        }
        const QString fileName = QStringLiteral(
            "smoke_iteration_%1_grid%2.png")
            .arg(iterationCounts[count]).arg(width);
        EXPECT_TRUE(image.save(QDir(outputDirectory).filePath(fileName), "PNG"));
      }
    }
  }
  const auto reportRmse = [&](const std::size_t lowGrid,
                              const std::size_t lowCount,
                              const std::size_t highGrid,
                              const std::size_t highCount,
                              const char* label) {
    const auto& low = results[lowGrid][lowCount].normalizedDensity;
    const auto& high = results[highGrid][highCount].normalizedDensity;
    double squaredError = 0.0;
    for (std::size_t index = 0; index < low.size(); ++index) {
      const double difference = low[index] - high[index];
      squaredError += difference * difference;
    }
    const double rmse = std::sqrt(squaredError / static_cast<double>(low.size()));
    GTEST_LOG_(INFO) << "smoke-iteration-cross-grid mode=" << label
        << " baseIterations=" << iterationCounts[lowCount] << "->"
        << iterationCounts[highCount] << " grids="
        << resolutions[lowGrid].first << "->" << resolutions[highGrid].first
        << " densityRmse=" << rmse;
    EXPECT_GT(rmse, 0.0);
    return rmse;
  };
  const double base20to40 = reportRmse(0, 1, 2, 3, "same-default-low-high");
  const double scaled20to24 = reportRmse(0, 1, 1, 2, "scaled-low-medium");
  const double scaled24to40 = reportRmse(1, 2, 2, 3, "scaled-medium-high");
  const double defaultAdaptive = reportRmse(
      0, 1, 2, 1, "default-20-to-adaptive-28");
  EXPECT_LT(scaled20to24, base20to40);
  EXPECT_LT(scaled24to40, base20to40);
  EXPECT_LT(defaultAdaptive, 0.102325);
}

TEST(ArtifactSmokeRuntimeStandaloneTest,
     MeasuresSmokeAdaptiveProjectionIterationCost) {
  constexpr std::array<std::pair<int, int>, 3> resolutions{{
      {96, 72}, {128, 96}, {192, 144}}};
  constexpr int baseIterations = 20;
  constexpr int warmupUpdates = 5;
  constexpr int measuredUpdates = 30;
  constexpr int repetitions = 5;
  constexpr int thresholdCells = 4608;
  constexpr int maxAdaptiveIterations = 28;

  for (const auto [width, height] : resolutions) {
    Artifact::LayerFluidRuntimeState seedState;
    Artifact::LayerSmokeRuntimeSettings settings;
    settings.gridWidth = width;
    settings.gridHeight = height;
    settings.solverIterations = baseIterations;
    settings.emitterX = 0.5f;
    settings.emitterY = 0.84f;
    settings.emitterRadius = 0.08f;
    settings.emitterDensity = 0.6f;
    settings.emitterVelocityY = -1.2f;
    settings.emitterSwirl = 0.15f;
    settings.buoyancy = 0.7f;
    settings.vorticity = 0.1f;
    for (int frame = 0; frame <= 28; ++frame)
      Artifact::advanceLayerSmokeRuntime(seedState, frame, 30.0, settings);
    const auto seed = seedState.fluidSolver_->snapshot();

    std::array<std::array<double, repetitions>, 2> samples{};
    std::array<int, 2> effectiveIterations{};
    for (int mode = 0; mode < 2; ++mode) {
      for (int repetition = 0; repetition < repetitions; ++repetition) {
        ArtifactCore::FluidSolver2D solver(width, height);
        ASSERT_TRUE(solver.restoreSnapshot(seed));
        solver.setSolverIterations(baseIterations);
        solver.setAdaptiveIterations(mode == 1);
        solver.setHighResThresholdCells(thresholdCells);
        solver.setMaxAdaptiveIterations(maxAdaptiveIterations);
        solver.setViscosity(settings.viscosity);
        solver.setDiffusion(settings.diffusion);
        solver.setBuoyancy(settings.buoyancy);
        solver.setVorticity(settings.vorticity);
        for (int update = 0; update < warmupUpdates; ++update)
          solver.update(1.0f / 30.0f);
        const auto start = std::chrono::steady_clock::now();
        for (int update = 0; update < measuredUpdates; ++update)
          solver.update(1.0f / 30.0f);
        const double elapsed = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - start).count();
        samples[mode][repetition] = elapsed / measuredUpdates;
        if (repetition == 0) {
          std::vector<SmokeProjectionStageCapture> observed;
          gSmokeProjectionCaptures = &observed;
          solver.update(1.0f / 30.0f);
          gSmokeProjectionCaptures = nullptr;
          ASSERT_EQ(observed.size(), 4U);
          effectiveIterations[mode] = observed.front().solverIterations;
        }
      }
    }
    for (auto& modeSamples : samples)
      std::sort(modeSamples.begin(), modeSamples.end());
    const double fixedMedian = samples[0][repetitions / 2];
    const double adaptiveMedian = samples[1][repetitions / 2];
    GTEST_LOG_(INFO) << "smoke-adaptive-iteration-cost grid=" << width << 'x'
        << height << " iterations=" << effectiveIterations[0] << "->"
        << effectiveIterations[1] << " fixedMedianMs=" << fixedMedian
        << " adaptiveMedianMs=" << adaptiveMedian << " ratio="
        << (fixedMedian > 0.0 ? adaptiveMedian / fixedMedian : 0.0);
    EXPECT_EQ(effectiveIterations[0], baseIterations);
    const int blocks = width * height / thresholdCells;
    const int expectedAdaptive = std::min(maxAdaptiveIterations,
        baseIterations + std::max(0, blocks - 1) * 4);
    EXPECT_EQ(effectiveIterations[1], expectedAdaptive);
    EXPECT_GT(fixedMedian, 0.0);
    EXPECT_GT(adaptiveMedian, 0.0);
  }
}

TEST(ArtifactSmokeRuntimeStandaloneTest,
     EmitterSwirlCreatesPlumeVariation) {
  const SmokeRun none = runProductionSmokeRuntime(
      0.5f, false, false, 30.0, 44, 0.1f, 0.0f);
  const SmokeRun moderate = runProductionSmokeRuntime(
      0.5f, false, false, 30.0, 44, 0.1f, 0.25f);
  const SmokeRun strong = runProductionSmokeRuntime(
      0.5f, false, false, 30.0, 44, 0.1f, 0.6f);

  EXPECT_GT(countDifferentPixels(none.image, moderate.image), 100);
  EXPECT_GT(countDifferentPixels(moderate.image, strong.image), 100);

  const QString outputDirectory =
      qEnvironmentVariable("ARTIFACT_SMOKE_RUNTIME_OUTPUT_DIR");
  if (!outputDirectory.isEmpty()) {
    ASSERT_TRUE(QDir().mkpath(outputDirectory.toUtf8().constData()));
    EXPECT_TRUE(none.image.save(QDir(outputDirectory).filePath(
        QStringLiteral("smoke_swirl_0.png")), "PNG"));
    EXPECT_TRUE(moderate.image.save(QDir(outputDirectory).filePath(
        QStringLiteral("smoke_swirl_0p25.png")), "PNG"));
    EXPECT_TRUE(strong.image.save(QDir(outputDirectory).filePath(
        QStringLiteral("smoke_swirl_0p6.png")), "PNG"));
  }
}

TEST(ArtifactSmokeRuntimeStandaloneTest,
     EmitterCountControlsDeterministicDistributedSources) {
  const SmokeRun single = runProductionSmokeRuntime(0.5f);
  const SmokeRun multiple = runProductionSmokeRuntime(
      0.5f, false, false, 30.0, kFrameCount, 0.1f, 0.15f, 9);
  const SmokeRun repeated = runProductionSmokeRuntime(
      0.5f, false, false, 30.0, kFrameCount, 0.1f, 0.15f, 9);

  EXPECT_GT(countDifferentPixels(single.image, multiple.image), 100);
  EXPECT_EQ(countDifferentPixels(multiple.image, repeated.image), 0);
  EXPECT_NEAR(multiple.meanParticleX, kGridWidth * 0.5, kGridWidth * 0.2);

  const QString outputDirectory =
      qEnvironmentVariable("ARTIFACT_SMOKE_RUNTIME_OUTPUT_DIR");
  if (!outputDirectory.isEmpty()) {
    ASSERT_TRUE(QDir().mkpath(outputDirectory.toUtf8().constData()));
    EXPECT_TRUE(multiple.image.save(
        QDir(outputDirectory).filePath(QStringLiteral("smoke_count_9.png")),
        "PNG"));
    EXPECT_TRUE(single.image.save(
        QDir(outputDirectory).filePath(QStringLiteral("smoke_count_1.png")),
        "PNG"));
  }
}

TEST(ArtifactSmokeRuntimeStandaloneTest,
     MeasuresSeededCurlDisturbanceAcrossPlumeAndFrames) {
  constexpr std::array<std::uint32_t, 2> seeds{17U, 71U};
  constexpr std::array<float, 3> strengths{0.0f, 0.5f, 1.0f};
  constexpr int finalFrame = 44;
  constexpr int width = kGridWidth;
  constexpr int height = kGridHeight;
  const auto index = [](const int x, const int y) {
    return static_cast<std::size_t>(x + width * y);
  };
  QMatrix4x4 transform;
  transform.setToIdentity();
  const QString outputDirectory =
      qEnvironmentVariable("ARTIFACT_SMOKE_RUNTIME_OUTPUT_DIR");

  for (const std::uint32_t seed : seeds) {
    for (const float strength : strengths) {
      Artifact::LayerFluidRuntimeState state;
      Artifact::LayerSmokeRuntimeSettings settings;
      settings.gridWidth = width;
      settings.gridHeight = height;
      settings.solverIterations = 20;
      settings.emitterX = 0.5f;
      settings.emitterY = 0.84f;
      settings.emitterRadius = 0.08f;
      settings.emitterDensity = 0.6f;
      settings.emitterVelocityY = -1.2f;
      settings.emitterSwirl = 0.15f;
      settings.vorticity = 0.1f;
      std::array<QImage, 3> frames;
      std::array<double, 3> imageDifference{};
      std::array<double, 3> imageHighPass{};

      for (int frame = 0; frame <= finalFrame; ++frame) {
        Artifact::advanceLayerSmokeRuntime(state, frame, 30.0, settings);
        if (frame >= finalFrame - 2) {
          const int capture = frame - (finalFrame - 2);
          const auto& particles = Artifact::prepareLayerSmokeRenderData(
              state, QRectF(0.0, 0.0, width, height), transform, 1.0f,
              frame, settings);
          frames[static_cast<std::size_t>(capture)] =
              rasterizeParticles(particles);
          double squaredHighPass = 0.0;
          std::size_t highPassCount = 0;
          for (int y = 1; y < height * kImageScale - 1; ++y) {
            for (int x = 1; x < width * kImageScale - 1; ++x) {
              const auto gray = [&](const int px, const int py) {
                const QRgb pixel = frames[capture].pixel(px, py);
                return 0.2126 * qRed(pixel) + 0.7152 * qGreen(pixel) +
                    0.0722 * qBlue(pixel);
              };
              const double center = gray(x, y);
              const double laplacian = 4.0 * center - gray(x - 1, y) -
                  gray(x + 1, y) - gray(x, y - 1) - gray(x, y + 1);
              squaredHighPass += laplacian * laplacian;
              ++highPassCount;
            }
          }
          imageHighPass[static_cast<std::size_t>(capture)] =
              highPassCount ? squaredHighPass / highPassCount : 0.0;
          if (capture > 0) {
            imageDifference[static_cast<std::size_t>(capture)] =
                static_cast<double>(countDifferentPixels(
                    frames[static_cast<std::size_t>(capture - 1)],
                    frames[static_cast<std::size_t>(capture)])) /
                static_cast<double>(frames[capture].width() *
                                    frames[capture].height());
          }
        }

        if (strength <= 0.0f || frame == finalFrame) continue;
        const auto field = state.fluidSolver_->snapshot();
        std::vector<float> potential(static_cast<std::size_t>(width * height));
        const float time = static_cast<float>(frame) / 30.0f;
        for (int y = 1; y < height - 1; ++y) {
          for (int x = 1; x < width - 1; ++x) {
            const float density = std::clamp(
                field.density[index(x, y)], 0.0f, 1.0f);
            const float control = std::clamp((density - 0.015f) / 0.22f,
                                             0.0f, 1.0f);
            const float envelope = control * control * (3.0f - 2.0f * control);
            const float broad = smokeSeededValueNoise(
                x * 0.09f + time * 0.17f, y * 0.09f - time * 0.11f, seed);
            const float middle = smokeSeededValueNoise(
                x * 0.18f - time * 0.23f, y * 0.18f + time * 0.15f,
                seed ^ 0x9E3779B9U);
            const float fine = smokeSeededValueNoise(
                x * 0.36f + time * 0.31f, y * 0.36f + time * 0.27f,
                seed ^ 0x85EBCA6BU);
            potential[index(x, y)] = envelope *
                (0.65f * broad + 0.25f * middle + 0.10f * fine) * strength;
          }
        }
        for (int y = 2; y < height - 2; ++y) {
          for (int x = 2; x < width - 2; ++x) {
            const float dPsiDy = 0.5f * (
                potential[index(x, y + 1)] - potential[index(x, y - 1)]);
            const float dPsiDx = 0.5f * (
                potential[index(x + 1, y)] - potential[index(x - 1, y)]);
            state.fluidSolver_->addVelocity(x, y, dPsiDy, -dPsiDx);
          }
        }
      }

      const auto finalField = state.fluidSolver_->snapshot();
      double mass = 0.0;
      double weightedX = 0.0;
      double weightedY = 0.0;
      double divergenceSquared = 0.0;
      std::size_t divergenceCount = 0;
      for (int y = 1; y < height - 1; ++y) {
        for (int x = 1; x < width - 1; ++x) {
          const double density = finalField.density[index(x, y)];
          mass += density;
          weightedX += density * x;
          weightedY += density * y;
          const float divergence = 0.5f * (
              finalField.velocityX[index(x + 1, y)] -
              finalField.velocityX[index(x - 1, y)] +
              finalField.velocityY[index(x, y + 1)] -
              finalField.velocityY[index(x, y - 1)]);
          divergenceSquared += static_cast<double>(divergence) * divergence;
          ++divergenceCount;
        }
      }
      const double centroidX = mass > 0.0 ? weightedX / mass : 0.0;
      const double centroidY = mass > 0.0 ? weightedY / mass : 0.0;
      GTEST_LOG_(INFO) << "test-only 2D curl disturbance seed=" << seed
          << " strength=" << strength << " mass=" << mass
          << " centroid=" << centroidX << ',' << centroidY
          << " divergenceRms=" << std::sqrt(divergenceSquared /
                                               std::max<std::size_t>(1, divergenceCount))
          << " highPass=" << imageHighPass[2]
          << " frameDelta=" << imageDifference[1] << '/' << imageDifference[2];
      EXPECT_GT(mass, 0.0);
      for (int capture = 0; capture < 3; ++capture) {
        const auto& image = frames[static_cast<std::size_t>(capture)];
        EXPECT_GT(measureSmokeImage(image).visiblePixels, 100);
        if (!outputDirectory.isEmpty()) {
          ASSERT_TRUE(QDir().mkpath(outputDirectory.toUtf8().constData()));
          const QString filename = QStringLiteral(
              "smoke_curl_seed%1_strength%2_frame%3.png")
              .arg(seed)
              .arg(static_cast<int>(strength * 100.0f))
              .arg(capture);
          EXPECT_TRUE(image.save(QDir(outputDirectory).filePath(filename), "PNG"));
        }
      }
    }
  }
}

TEST(ArtifactSmokeRuntimeStandaloneTest,
     ComparesIndependentRollAndFineDisturbanceOperators) {
  constexpr std::array<std::uint32_t, 2> seeds{17U, 71U};
  constexpr int width = kGridWidth;
  constexpr int height = kGridHeight;
  constexpr int finalFrame = 44;
  struct Condition final {
    const char* name; float roll; float fine;
    bool redirectVelocity; bool boundedCirculation; bool normalizeFinalMass;
    float trackedVortex; bool scaleVortexByFrameDt; bool pairedVortices;
  };
  constexpr std::array<Condition, 18> conditions{{
      {"baseline", 0.0f, 0.0f, false, false, false, 0.0f, false, false},
      {"roll-only", 0.24f, 0.0f, false, false, false, 0.0f, false, false},
      {"fine-only", 0.0f, 0.24f, false, false, false, 0.0f, false, false},
      {"roll-plus-fine", 0.24f, 0.24f, false, false, false, 0.0f, false, false},
      {"roll-redirect", 0.24f, 0.0f, true, false, false, 0.0f, false, false},
      {"bounded-circulation", 0.24f, 0.0f, false, true, false, 0.0f, false, false},
      {"bounded-circulation-normalized", 0.24f, 0.0f, false, true, true, 0.0f, false, false},
      {"tracked-vortex-012", 0.0f, 0.0f, false, false, false, 0.12f, false, false},
      {"tracked-vortex-024", 0.0f, 0.0f, false, false, false, 0.24f, false, false},
      {"tracked-vortex-024-normalized", 0.0f, 0.0f, false, false, true, 0.24f, false, false},
      {"tracked-vortex-accel-12", 0.0f, 0.0f, false, false, false, 1.2f, true, false},
      {"tracked-vortex-accel-36", 0.0f, 0.0f, false, false, false, 3.6f, true, false},
      {"tracked-vortex-accel-36-normalized", 0.0f, 0.0f, false, false, true, 3.6f, true, false},
      {"paired-vortex-accel-18", 0.0f, 0.0f, false, false, false, 1.8f, true, true},
      {"paired-vortex-accel-36", 0.0f, 0.0f, false, false, false, 3.6f, true, true},
      {"paired-vortex-accel-36-normalized", 0.0f, 0.0f, false, false, true, 3.6f, true, true},
      {"paired-vortex-accel-60", 0.0f, 0.0f, false, false, false, 6.0f, true, true},
      {"paired-vortex-accel-60-normalized", 0.0f, 0.0f, false, false, true, 6.0f, true, true}}};
  const auto index = [](const int x, const int y) {
    return static_cast<std::size_t>(x + width * y);
  };
  QMatrix4x4 transform;
  transform.setToIdentity();
  const QString outputDirectory =
      qEnvironmentVariable("ARTIFACT_SMOKE_RUNTIME_OUTPUT_DIR");

  for (const auto seed : seeds) {
    for (const auto& condition : conditions) {
      Artifact::LayerFluidRuntimeState state;
      Artifact::LayerSmokeRuntimeSettings settings;
      settings.gridWidth = width;
      settings.gridHeight = height;
      settings.solverIterations = 20;
      const bool alternateSeed = seed == 71U;
      settings.emitterX = alternateSeed ? 0.52f : 0.48f;
      settings.emitterY = 0.84f;
      settings.emitterRadius = 0.08f;
      settings.emitterDensity = 0.6f;
      settings.emitterVelocityX = alternateSeed ? 0.08f : -0.08f;
      settings.emitterVelocityY = -1.2f;
      settings.emitterSwirl = alternateSeed ? -0.15f : 0.15f;
      settings.vorticity = 0.1f;
      QImage finalImage;
      std::array<QImage, 3> tailImages;
      double preNormalizationMass = 0.0;

      for (int frame = 0; frame <= finalFrame; ++frame) {
        Artifact::advanceLayerSmokeRuntime(state, frame, 30.0, settings);
        if (frame == finalFrame && condition.normalizeFinalMass) {
          auto field = state.fluidSolver_->snapshot();
          double currentMass = 0.0;
          for (int y = 1; y < height - 1; ++y)
            for (int x = 1; x < width - 1; ++x)
              currentMass += field.density[index(x, y)];
          preNormalizationMass = currentMass;
          const float scale = currentMass > 0.0
              ? static_cast<float>(472.485 / currentMass) : 1.0f;
          for (int y = 1; y < height - 1; ++y)
            for (int x = 1; x < width - 1; ++x)
              field.density[index(x, y)] *= scale;
          ASSERT_TRUE(state.fluidSolver_->restoreSnapshot(field));
        }
        if (frame < finalFrame &&
            (condition.roll > 0.0f || condition.fine > 0.0f)) {
          auto field = state.fluidSolver_->snapshot();
          std::vector<float> potential(static_cast<std::size_t>(width * height));
          const float time = static_cast<float>(frame) / 30.0f;
          for (int y = 1; y < height - 1; ++y) {
            for (int x = 1; x < width - 1; ++x) {
              const float density = std::clamp(field.density[index(x, y)], 0.0f, 1.0f);
              const float plumeMask = std::clamp((density - 0.01f) / 0.16f, 0.0f, 1.0f);
              const float surfaceMask = std::clamp(
                  (std::abs(field.density[index(x + 1, y)] - field.density[index(x - 1, y)]) +
                   std::abs(field.density[index(x, y + 1)] - field.density[index(x, y - 1)]) - 0.012f) /
                      0.10f, 0.0f, 1.0f);
              const float largeNoise = smokeSeededValueNoise(
                  x * 0.055f + time * 0.12f, y * 0.055f - time * 0.08f, seed);
              const float fineNoise = smokeSeededValueNoise(
                  x * 0.31f - time * 0.27f, y * 0.31f + time * 0.19f,
                  seed ^ 0x9E3779B9U);
              potential[index(x, y)] =
                  condition.roll * plumeMask * largeNoise +
                  condition.fine * plumeMask * surfaceMask * fineNoise;
            }
          }
          for (int y = 2; y < height - 2; ++y) {
            for (int x = 2; x < width - 2; ++x) {
              if (condition.redirectVelocity) {
                const float density = std::clamp(field.density[index(x, y)], 0.0f, 1.0f);
                const float mask = std::clamp((density - 0.01f) / 0.16f, 0.0f, 1.0f);
                const float angle = condition.roll * mask * smokeSeededValueNoise(
                    x * 0.055f + time * 0.12f, y * 0.055f - time * 0.08f, seed);
                const float cosine = std::cos(angle);
                const float sine = std::sin(angle);
                const float vx = field.velocityX[index(x, y)];
                const float vy = field.velocityY[index(x, y)];
                state.fluidSolver_->addVelocity(
                    x, y, vx * cosine - vy * sine - vx,
                    vx * sine + vy * cosine - vy);
                continue;
              }
              const float dPsiDy = 0.5f * (
                  potential[index(x, y + 1)] - potential[index(x, y - 1)]);
              const float dPsiDx = 0.5f * (
                  potential[index(x + 1, y)] - potential[index(x - 1, y)]);
              if (condition.boundedCirculation) {
                const auto cell = index(x, y);
                const float baseX = field.velocityX[cell];
                const float baseY = field.velocityY[cell];
                float nextX = baseX + dPsiDy;
                float nextY = baseY - dPsiDx;
                const float baseSpeed = std::sqrt(baseX * baseX + baseY * baseY);
                const float nextSpeed = std::sqrt(nextX * nextX + nextY * nextY);
                if (nextSpeed > baseSpeed && nextSpeed > 1.0e-6f) {
                  const float scale = baseSpeed / nextSpeed;
                  nextX *= scale;
                  nextY *= scale;
                }
                field.velocityX[cell] = nextX;
                field.velocityY[cell] = nextY;
              } else {
                state.fluidSolver_->addVelocity(x, y, dPsiDy, -dPsiDx);
              }
            }
          }
          if (condition.boundedCirculation)
            ASSERT_TRUE(state.fluidSolver_->restoreSnapshot(field));
        }
        if (frame < finalFrame && condition.trackedVortex > 0.0f) {
          auto field = state.fluidSolver_->snapshot();
          double densityMass = 0.0;
          double densityWeightedX = 0.0;
          double densityWeightedY = 0.0;
          for (int y = 1; y < height - 1; ++y) {
            for (int x = 1; x < width - 1; ++x) {
              const double density = std::max(0.0f, field.density[index(x, y)]);
              densityMass += density;
              densityWeightedX += density * x;
              densityWeightedY += density * y;
            }
          }
          if (densityMass > 1.0e-8) {
            constexpr float kVortexRadius = 7.0f;
            const float centerX = static_cast<float>(densityWeightedX / densityMass);
            const float centerY = static_cast<float>(densityWeightedY / densityMass) - 5.0f;
            const float inverseRadius = 1.0f / kVortexRadius;
            const float inverseRadiusSquared = inverseRadius * inverseRadius;
            const float frameScaledStrength = condition.trackedVortex *
                (condition.scaleVortexByFrameDt ? 1.0f / 30.0f : 1.0f);
            for (int y = 2; y < height - 2; ++y) {
              for (int x = 2; x < width - 2; ++x) {
                const int vortexCount = condition.pairedVortices ? 2 : 1;
                for (int vortex = 0; vortex < vortexCount; ++vortex) {
                  const float side = vortex == 0 ? -1.0f : 1.0f;
                  const float centerOffset = condition.pairedVortices
                      ? side * 6.0f : 0.0f;
                  const float circulationSign = condition.pairedVortices
                      ? side : 1.0f;
                  const float dx = static_cast<float>(x) -
                      (centerX + centerOffset);
                  const float dy = static_cast<float>(y) - centerY;
                  const float radialWeight = std::exp(
                      -0.5f * (dx * dx + dy * dy) * inverseRadiusSquared);
                  field.velocityX[index(x, y)] -= circulationSign *
                      frameScaledStrength * dy * inverseRadius * radialWeight;
                  field.velocityY[index(x, y)] += circulationSign *
                      frameScaledStrength * dx * inverseRadius * radialWeight;
                }
              }
            }
            ASSERT_TRUE(state.fluidSolver_->restoreSnapshot(field));
          }
        }
        if (frame >= finalFrame - 2) {
          const auto& renderData = Artifact::prepareLayerSmokeRenderData(
              state, QRectF(0.0, 0.0, width, height), transform, 1.0f,
              frame, settings);
          tailImages[static_cast<std::size_t>(frame - (finalFrame - 2))] =
              rasterizeParticles(renderData);
        }
        if (frame == finalFrame) {
          const auto& renderData = Artifact::prepareLayerSmokeRenderData(
              state, QRectF(0.0, 0.0, width, height), transform, 1.0f,
              frame, settings);
          finalImage = rasterizeParticles(renderData);
        }
      }
      const auto field = state.fluidSolver_->snapshot();
      double mass = 0.0;
      double divergenceSquared = 0.0;
      double temporalDifference = 0.0;
      std::size_t samples = 0;
      for (int y = 1; y < height - 1; ++y) {
        for (int x = 1; x < width - 1; ++x) {
          mass += field.density[index(x, y)];
          const float div = 0.5f * (
              field.velocityX[index(x + 1, y)] - field.velocityX[index(x - 1, y)] +
              field.velocityY[index(x, y + 1)] - field.velocityY[index(x, y - 1)]);
          divergenceSquared += static_cast<double>(div) * div;
        }
      }
      if (!condition.normalizeFinalMass)
        preNormalizationMass = mass;
      for (std::size_t i = 1; i < tailImages.size(); ++i)
        temporalDifference += static_cast<double>(countDifferentPixels(
            tailImages[i - 1], tailImages[i])) /
            static_cast<double>(tailImages[i].width() * tailImages[i].height());
      const auto imageStats = measureSmokeImage(finalImage);
      GTEST_LOG_(INFO) << "separated smoke operator seed=" << seed
          << " condition=" << condition.name << " mass=" << mass
          << " preNormalizationMass=" << preNormalizationMass
          << " divergenceRms=" << std::sqrt(divergenceSquared /
              static_cast<double>((width - 2) * (height - 2)))
          << " adjacentImageDeltaMean=" << temporalDifference / 2.0
          << " visiblePixels=" << imageStats.visiblePixels;
      EXPECT_GT(mass, 0.0);
      EXPECT_GT(imageStats.visiblePixels, 100);
      if (!outputDirectory.isEmpty()) {
        ASSERT_TRUE(QDir().mkpath(outputDirectory.toUtf8().constData()));
        EXPECT_TRUE(finalImage.save(QDir(outputDirectory).filePath(
            QStringLiteral("smoke_operator_seed%1_%2.png")
                .arg(seed).arg(QString::fromLatin1(condition.name))), "PNG"));
        for (std::size_t tail = 0; tail < tailImages.size(); ++tail) {
          EXPECT_TRUE(tailImages[tail].save(QDir(outputDirectory).filePath(
              QStringLiteral("smoke_operator_seed%1_%2_tail%3.png")
                  .arg(seed).arg(QString::fromLatin1(condition.name))
                  .arg(tail)), "PNG"));
        }
      }
    }
  }
}

TEST(ArtifactSmokeRuntimeStandaloneTest,
     ComparesTemperatureCurlNoiseThroughProductionParticleExtraction) {
  constexpr int finalFrame = 44;
  constexpr int emissionFrames = 12;
  constexpr float framesPerSecond = 30.0f;
  constexpr float dt = 1.0f / framesPerSecond;
  constexpr std::array<int, 3> captureFrames{24, 36, 44};
  constexpr std::array<std::uint32_t, 2> seeds{17U, 71U};
  const auto index = [](const int x, const int y) {
    return static_cast<std::size_t>(x + y * kGridWidth);
  };
  QMatrix4x4 transform;
  transform.setToIdentity();
  const QRectF bounds(0.0, 0.0, kGridWidth, kGridHeight);
  const QString outputDirectory =
      qEnvironmentVariable("ARTIFACT_SMOKE_RUNTIME_OUTPUT_DIR");
  const auto imageHighPassRms = [](const QImage& image) {
    double squaredSum = 0.0;
    std::size_t sampleCount = 0;
    for (int y = 1; y < image.height() - 1; ++y) {
      for (int x = 1; x < image.width() - 1; ++x) {
        const auto luma = [&image](const int px, const int py) {
          const QRgb pixel = image.pixel(px, py);
          return 0.2126 * qRed(pixel) + 0.7152 * qGreen(pixel) +
              0.0722 * qBlue(pixel);
        };
        const double laplacian = 4.0 * luma(x, y) - luma(x - 1, y) -
            luma(x + 1, y) - luma(x, y - 1) - luma(x, y + 1);
        squaredSum += laplacian * laplacian;
        ++sampleCount;
      }
    }
    return sampleCount > 0
        ? std::sqrt(squaredSum / static_cast<double>(sampleCount)) : 0.0;
  };
  const auto resampleProductionSmokeParticles = [&](
      const ArtifactCore::FluidSolver2D& solver,
      const Artifact::LayerSmokeRuntimeSettings& settings,
      const int sampleCountX, const int sampleCountY) {
    ArtifactCore::ParticleRenderData data;
    data.options.blend = ArtifactCore::ParticleBlendPolicy::Alpha;
    data.options.billboard = ArtifactCore::ParticleBillboardPolicy::ScreenAligned;
    const float sampleStepX = static_cast<float>(settings.gridWidth - 1) /
        static_cast<float>(std::max(1, sampleCountX - 1));
    const float sampleStepY = static_cast<float>(settings.gridHeight - 1) /
        static_cast<float>(std::max(1, sampleCountY - 1));
    const float spacingX = static_cast<float>(bounds.width()) /
        static_cast<float>(std::max(1, sampleCountX - 1));
    const float spacingY = static_cast<float>(bounds.height()) /
        static_cast<float>(std::max(1, sampleCountY - 1));
    const float sampleSpacing = std::max(spacingX, spacingY);
    data.particles.reserve(static_cast<std::size_t>(sampleCountX * sampleCountY));
    for (int sampleY = 0; sampleY < sampleCountY; ++sampleY) {
      for (int sampleX = 0; sampleX < sampleCountX; ++sampleX) {
        std::uint32_t sampleHash =
            static_cast<std::uint32_t>(sampleX) * 0x9E3779B9u ^
            static_cast<std::uint32_t>(sampleY) * 0x85EBCA6Bu;
        sampleHash ^= sampleHash >> 16u;
        sampleHash *= 0x7FEB352Du;
        sampleHash ^= sampleHash >> 15u;
        const float jitterX = static_cast<float>(sampleHash & 0xFFFFu) *
            (2.0f / 65535.0f) - 1.0f;
        const float jitterY = static_cast<float>(sampleHash >> 16u) *
            (2.0f / 65535.0f) - 1.0f;
        const float gridX = std::clamp(
            static_cast<float>(sampleX) * sampleStepX +
                jitterX * sampleStepX * 0.30f,
            0.0f, static_cast<float>(settings.gridWidth - 1));
        const float gridY = std::clamp(
            static_cast<float>(sampleY) * sampleStepY +
                jitterY * sampleStepY * 0.30f,
            0.0f, static_cast<float>(settings.gridHeight - 1));
        const int x0 = static_cast<int>(std::floor(gridX));
        const int y0 = static_cast<int>(std::floor(gridY));
        const int x1 = std::min(x0 + 1, settings.gridWidth - 1);
        const int y1 = std::min(y0 + 1, settings.gridHeight - 1);
        const float tx = gridX - static_cast<float>(x0);
        const float ty = gridY - static_cast<float>(y0);
        const float top = std::lerp(
            solver.getDensity(x0, y0), solver.getDensity(x1, y0), tx);
        const float bottom = std::lerp(
            solver.getDensity(x0, y1), solver.getDensity(x1, y1), tx);
        const float density = std::lerp(top, bottom, ty);
        if (density < 0.025f) continue;
        ArtifactCore::ParticleVertex particle{};
        particle.px = static_cast<float>(bounds.left()) + gridX /
            static_cast<float>(settings.gridWidth - 1) *
                static_cast<float>(bounds.width());
        particle.py = static_cast<float>(bounds.top()) + gridY /
            static_cast<float>(settings.gridHeight - 1) *
                static_cast<float>(bounds.height());
        particle.ppx = particle.px;
        particle.ppy = particle.py;
        particle.r = settings.colorR;
        particle.g = settings.colorG;
        particle.b = settings.colorB;
        particle.a = std::clamp(std::sqrt(density) * 0.18f,
                                0.01f, 0.18f) * 0.79f;
        particle.size = sampleSpacing * (0.118f + density * 0.02f) * 1.125f;
        data.particles.push_back(particle);
      }
    }
    return data;
  };

  for (std::size_t seedIndex = 0; seedIndex < seeds.size(); ++seedIndex) {
    std::array<std::array<ArtifactCore::ParticleRenderData, 3>, 2> particleData;
    std::array<ArtifactCore::ParticleRenderData, 3> highResolutionCurlData;
    std::array<double, 2> finalMass{};
    std::array<double, 2> finalCentroidY{};
    for (int variant = 0; variant < 2; ++variant) {
      Artifact::LayerSmokeRuntimeSettings settings;
      settings.gridWidth = kGridWidth;
      settings.gridHeight = kGridHeight;
      settings.solverIterations = 20;
      settings.buoyancy = 0.0f;
      settings.vorticity = 0.1f;
      settings.emitterX = seedIndex == 0 ? 0.47f : 0.53f;
      settings.emitterY = 0.82f;
      settings.emitterRadius = 0.07f;
      settings.emitterDensity = 0.45f;
      settings.emitterVelocityX = seedIndex == 0 ? -0.04f : 0.04f;
      settings.emitterVelocityY = -0.55f;
      settings.emitterSwirl = seedIndex == 0 ? 0.55f : -0.55f;

      Artifact::LayerFluidRuntimeState state;
      ArtifactCore::NamedVector<float> temperature;
      ArtifactCore::NamedVector<float> nextTemperature;
      ArtifactCore::NamedVector<float> streamFunction;
      const std::size_t cellCount =
          static_cast<std::size_t>(kGridWidth * kGridHeight);
      temperature.resize(cellCount);
      nextTemperature.resize(cellCount);
      streamFunction.resize(cellCount);
      std::fill(temperature.begin(), temperature.end(), 0.0f);
      std::fill(nextTemperature.begin(), nextTemperature.end(), 0.0f);
      std::fill(streamFunction.begin(), streamFunction.end(), 0.0f);
      std::size_t captureIndex = 0;

      for (int frame = 0; frame <= finalFrame; ++frame) {
        settings.emitterDensity = frame < emissionFrames ? 0.45f : 0.0f;
        Artifact::advanceLayerSmokeRuntime(
            state, frame, framesPerSecond, settings);
        ASSERT_NE(state.fluidSolver_, nullptr);
        advectTestSmokeTemperature(temperature, nextTemperature,
                                   *state.fluidSolver_, dt, 0.5f);
        if (frame < emissionFrames)
          injectTestSmokeTemperature(temperature, kGridWidth, kGridHeight,
                                     settings);
        for (int y = 1; y < kGridHeight - 1; ++y) {
          for (int x = 1; x < kGridWidth - 1; ++x) {
            const float heat = temperature[index(x, y)];
            state.fluidSolver_->addVelocity(x, y, 0.0f,
                                             -heat * 0.01f * dt);
          }
        }
        if (variant == 1) {
          applyTestSmokeTemperatureCurlNoise(
              *state.fluidSolver_, temperature, streamFunction, dt,
              static_cast<float>(frame) * dt, seeds[seedIndex]);
        }
        if (captureIndex < captureFrames.size() &&
            frame == captureFrames[captureIndex]) {
          particleData[static_cast<std::size_t>(variant)][captureIndex] =
              Artifact::prepareLayerSmokeRenderData(
                  state, bounds, transform, 1.0f, frame, settings);
          if (variant == 1) {
            highResolutionCurlData[captureIndex] =
                resampleProductionSmokeParticles(
                    *state.fluidSolver_, settings, 96, 72);
          }
          ++captureIndex;
        }
      }
      ASSERT_EQ(captureIndex, captureFrames.size());
      const auto field = state.fluidSolver_->snapshot();
      double densityMass = 0.0;
      double weightedY = 0.0;
      for (int y = 1; y < kGridHeight - 1; ++y) {
        for (int x = 1; x < kGridWidth - 1; ++x) {
          const double density = std::max(0.0f, field.density[index(x, y)]);
          densityMass += density;
          weightedY += density * y;
        }
      }
      finalMass[static_cast<std::size_t>(variant)] = densityMass;
      finalCentroidY[static_cast<std::size_t>(variant)] =
          densityMass > 0.0 ? weightedY / densityMass : 0.0;
    }

    QImage contact(kGridWidth * kImageScale * 3,
                   kGridHeight * kImageScale * 3, QImage::Format_ARGB32);
    contact.fill(kBackground);
    for (std::size_t capture = 0; capture < captureFrames.size(); ++capture) {
      const QImage baseline = rasterizeParticles(
          particleData[0][capture]);
      const auto baselineMetrics = measureSmokeImage(baseline);
      float lowerScale = 0.1f;
      float upperScale = 4.0f;
      for (int iteration = 0; iteration < 16; ++iteration) {
        const float scale = (lowerScale + upperScale) * 0.5f;
        const auto candidateMetrics = measureSmokeImage(rasterizeParticles(
            particleData[1][capture], scale));
        if (candidateMetrics.greenExcess < baselineMetrics.greenExcess)
          lowerScale = scale;
        else
          upperScale = scale;
      }
      const float exposureScale = (lowerScale + upperScale) * 0.5f;
      const QImage candidate = rasterizeParticles(
          particleData[1][capture], exposureScale);
      const auto candidateMetrics = measureSmokeImage(candidate);
      float highResolutionLowerScale = 0.1f;
      float highResolutionUpperScale = 4.0f;
      for (int iteration = 0; iteration < 16; ++iteration) {
        const float scale =
            (highResolutionLowerScale + highResolutionUpperScale) * 0.5f;
        const auto highResolutionMetrics = measureSmokeImage(
            rasterizeParticles(highResolutionCurlData[capture], scale));
        if (highResolutionMetrics.greenExcess < baselineMetrics.greenExcess)
          highResolutionLowerScale = scale;
        else
          highResolutionUpperScale = scale;
      }
      const float highResolutionExposureScale =
          (highResolutionLowerScale + highResolutionUpperScale) * 0.5f;
      const QImage highResolutionCandidate = rasterizeParticles(
          highResolutionCurlData[capture], highResolutionExposureScale);
      const auto highResolutionMetrics =
          measureSmokeImage(highResolutionCandidate);
      EXPECT_GT(baselineMetrics.visiblePixels, 100);
      EXPECT_GT(candidateMetrics.visiblePixels, 100);
      EXPECT_GT(highResolutionMetrics.visiblePixels, 100);
      EXPECT_GT(countDifferentPixels(baseline, candidate), 100)
          << "temperature curl noise should alter production-extracted particle output";
      EXPECT_NEAR(static_cast<double>(candidateMetrics.greenExcess),
                  static_cast<double>(baselineMetrics.greenExcess),
                  static_cast<double>(baselineMetrics.greenExcess) * 0.04 + 100.0);
      GTEST_LOG_(INFO) << "production particle extraction temperature-curl seed="
          << seeds[seedIndex] << " frame=" << captureFrames[capture]
          << " baseline/curl particles="
          << particleData[0][capture].particles.size() << '/'
          << particleData[1][capture].particles.size()
          << " exposureScale=" << exposureScale
          << " highResParticles="
          << highResolutionCurlData[capture].particles.size()
          << " highResExposure=" << highResolutionExposureScale
          << " visible=" << baselineMetrics.visiblePixels << '/'
          << candidateMetrics.visiblePixels << '/'
          << highResolutionMetrics.visiblePixels
          << " highPass=" << imageHighPassRms(baseline) << '/'
          << imageHighPassRms(candidate) << '/'
          << imageHighPassRms(highResolutionCandidate)
          << " green=" << baselineMetrics.greenExcess << '/'
          << candidateMetrics.greenExcess;
      for (int row = 0; row < 3; ++row) {
        const QImage& source = row == 0 ? baseline
            : row == 1 ? candidate : highResolutionCandidate;
        const int offsetY = static_cast<int>(capture) * source.height();
        const int offsetX = row * source.width();
        for (int y = 0; y < source.height(); ++y) {
          auto* destination = reinterpret_cast<QRgb*>(
              contact.scanLine(offsetY + y)) + offsetX;
          const auto* sourceRow = reinterpret_cast<const QRgb*>(
              source.constScanLine(y));
          std::copy(sourceRow, sourceRow + source.width(), destination);
        }
      }
    }
    GTEST_LOG_(INFO) << "production particle extraction final field seed="
        << seeds[seedIndex] << " densityMass=" << finalMass[0] << '/'
        << finalMass[1] << " centroidY=" << finalCentroidY[0] << '/'
        << finalCentroidY[1];
    if (!outputDirectory.isEmpty()) {
      ASSERT_TRUE(QDir().mkpath(outputDirectory.toUtf8().constData()));
      const QString imagePath = QDir(outputDirectory).filePath(
          QStringLiteral("runtime_temperature_curl_particles_seed_%1.png")
              .arg(seeds[seedIndex]));
      EXPECT_TRUE(contact.save(imagePath, "PNG")) << imagePath.toStdString();
    }
  }
}

TEST(ArtifactSmokeRuntimeStandaloneTest,
     ComparesSemiLagrangianAndConservativeVortexTransport) {
  constexpr int width = 96;
  constexpr int height = 72;
  constexpr int steps = 60;
  constexpr float dt = 1.0f;
  constexpr float streamAmplitude = 52.0f;
  const auto index = [](const int x, const int y) {
    return static_cast<std::size_t>(x + width * y);
  };
  const QString outputDirectory =
      qEnvironmentVariable("ARTIFACT_SMOKE_RUNTIME_OUTPUT_DIR");
  std::vector<float> density(static_cast<std::size_t>(width * height), 0.0f);
  std::vector<float> velocityX(density.size(), 0.0f);
  std::vector<float> velocityY(density.size(), 0.0f);
  std::vector<float> streamfunction(density.size(), 0.0f);
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const float dx = (static_cast<float>(x) - width * 0.42f) / 7.0f;
      const float dy = (static_cast<float>(y) - height * 0.48f) / 10.0f;
      density[index(x, y)] = std::exp(-0.5f * (dx * dx + dy * dy));
      const float sx = std::sin(3.14159265f * x / (width - 1));
      const float sy = std::sin(3.14159265f * y / (height - 1));
      streamfunction[index(x, y)] = streamAmplitude * sx * sx * sy * sy;
    }
  }
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const int xl = std::max(0, x - 1);
      const int xr = std::min(width - 1, x + 1);
      const int yt = std::max(0, y - 1);
      const int yb = std::min(height - 1, y + 1);
      velocityX[index(x, y)] = 0.5f * (
          streamfunction[index(x, yb)] - streamfunction[index(x, yt)]);
      velocityY[index(x, y)] = -0.5f * (
          streamfunction[index(xr, y)] - streamfunction[index(xl, y)]);
    }
  }
  const auto sampleBilinear = [&](const std::vector<float>& field, float x, float y) {
    x = std::clamp(x, 0.0f, static_cast<float>(width - 1));
    y = std::clamp(y, 0.0f, static_cast<float>(height - 1));
    const int x0 = static_cast<int>(x);
    const int y0 = static_cast<int>(y);
    const int x1 = std::min(x0 + 1, width - 1);
    const int y1 = std::min(y0 + 1, height - 1);
    const float tx = x - x0;
    const float ty = y - y0;
    return (1.0f - ty) * ((1.0f - tx) * field[index(x0, y0)] +
        tx * field[index(x1, y0)]) + ty * ((1.0f - tx) *
        field[index(x0, y1)] + tx * field[index(x1, y1)]);
  };
  const auto makeMetrics = [&](const std::vector<float>& field) {
    std::array<double, 5> result{};
    double sum = 0.0;
    for (const float value : field) sum += value;
    result[0] = sum;
    double highPassSquared = 0.0;
    double gradientSquared = 0.0;
    for (int y = 1; y < height - 1; ++y) {
      for (int x = 1; x < width - 1; ++x) {
        const float c = field[index(x, y)];
        const float lap = 4.0f * c - field[index(x - 1, y)] -
            field[index(x + 1, y)] - field[index(x, y - 1)] - field[index(x, y + 1)];
        const float gx = 0.5f * (field[index(x + 1, y)] - field[index(x - 1, y)]);
        const float gy = 0.5f * (field[index(x, y + 1)] - field[index(x, y - 1)]);
        highPassSquared += static_cast<double>(lap) * lap;
        gradientSquared += static_cast<double>(gx) * gx + static_cast<double>(gy) * gy;
      }
    }
    result[1] = std::sqrt(highPassSquared / ((width - 2) * (height - 2)));
    result[2] = std::sqrt(gradientSquared / ((width - 2) * (height - 2)));
    result[3] = *std::max_element(field.begin(), field.end());
    result[4] = result[3] > 0.0 ? result[1] / result[3] : 0.0;
    return result;
  };
  const auto saveField = [&](const std::vector<float>& field, const QString& filename) {
    if (outputDirectory.isEmpty()) return;
    ASSERT_TRUE(QDir().mkpath(outputDirectory.toUtf8().constData()));
    const float peak = *std::max_element(field.begin(), field.end());
    constexpr int displayScale = 3;
    QImage image(width * displayScale, height * displayScale,
                 QImage::Format_Grayscale8);
    for (int y = 0; y < height * displayScale; ++y) {
      auto* row = image.scanLine(y);
      for (int x = 0; x < width * displayScale; ++x)
        row[x] = static_cast<uchar>(std::clamp(static_cast<int>(std::lround(
            (peak > 0.0f ? field[index(x / displayScale, y / displayScale)] / peak : 0.0f)
                * 255.0f)), 0, 255));
    }
    EXPECT_TRUE(image.save(QDir(outputDirectory).filePath(filename), "PNG"));
  };
  float maxSpeed = 0.0f;
  for (std::size_t i = 0; i < velocityX.size(); ++i)
    maxSpeed = std::max(maxSpeed, std::max(std::abs(velocityX[i]), std::abs(velocityY[i])));
  const int substeps = std::max(1, static_cast<int>(std::ceil(maxSpeed * dt / 0.45f)));
  const float subDt = dt / static_cast<float>(substeps);
  std::vector<float> semiLagrangian = density;
  std::vector<float> next(density.size(), 0.0f);
  for (int step = 0; step < steps; ++step) {
    for (int substep = 0; substep < substeps; ++substep) {
      for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
          next[index(x, y)] = sampleBilinear(semiLagrangian,
              x - velocityX[index(x, y)] * subDt,
              y - velocityY[index(x, y)] * subDt);
      semiLagrangian.swap(next);
    }
  }

  std::vector<float> conservative = density;
  std::vector<float> fluxNext(density.size(), 0.0f);
  const auto minmod = [](const float a, const float b) {
    if (a * b <= 0.0f) return 0.0f;
    return std::copysign(std::min(std::abs(a), std::abs(b)), a);
  };
  for (int step = 0; step < steps; ++step) {
    for (int substep = 0; substep < substeps; ++substep) {
      for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
          const auto cell = index(x, y);
          const float leftU = x == 0 ? 0.0f : 0.5f *
              (velocityX[index(x - 1, y)] + velocityX[cell]);
          const float rightU = x == width - 1 ? 0.0f : 0.5f *
              (velocityX[cell] + velocityX[index(x + 1, y)]);
          const float topV = y == 0 ? 0.0f : 0.5f *
              (velocityY[index(x, y - 1)] + velocityY[cell]);
          const float bottomV = y == height - 1 ? 0.0f : 0.5f *
              (velocityY[cell] + velocityY[index(x, y + 1)]);
          const float leftDensity = leftU >= 0.0f
              ? (x == 0 ? conservative[cell] : std::max(0.0f,
                  conservative[index(x - 1, y)] + 0.5f * minmod(
                      conservative[index(x - 1, y)] -
                          conservative[index(std::max(0, x - 2), y)],
                      conservative[cell] - conservative[index(x - 1, y)])))
              : std::max(0.0f, conservative[cell] - 0.5f * minmod(
                    conservative[cell] - conservative[index(std::max(0, x - 1), y)],
                    conservative[index(std::min(width - 1, x + 1), y)] - conservative[cell]));
          const float rightDensity = rightU >= 0.0f
              ? std::max(0.0f, conservative[cell] + 0.5f * minmod(
                    conservative[cell] - conservative[index(std::max(0, x - 1), y)],
                    conservative[index(std::min(width - 1, x + 1), y)] - conservative[cell]))
              : (x == width - 1 ? conservative[cell] : std::max(0.0f,
                    conservative[index(x + 1, y)] - 0.5f * minmod(
                        conservative[index(x + 1, y)] - conservative[cell],
                        conservative[index(std::min(width - 1, x + 2), y)] -
                            conservative[index(x + 1, y)])));
          const float topDensity = topV >= 0.0f
              ? (y == 0 ? conservative[cell] : std::max(0.0f,
                  conservative[index(x, y - 1)] + 0.5f * minmod(
                      conservative[index(x, y - 1)] -
                          conservative[index(x, std::max(0, y - 2))],
                      conservative[cell] - conservative[index(x, y - 1)])))
              : std::max(0.0f, conservative[cell] - 0.5f * minmod(
                    conservative[cell] - conservative[index(x, std::max(0, y - 1))],
                    conservative[index(x, std::min(height - 1, y + 1))] - conservative[cell]));
          const float bottomDensity = bottomV >= 0.0f
              ? std::max(0.0f, conservative[cell] + 0.5f * minmod(
                    conservative[cell] - conservative[index(x, std::max(0, y - 1))],
                    conservative[index(x, std::min(height - 1, y + 1))] - conservative[cell]))
              : (y == height - 1 ? conservative[cell] : std::max(0.0f,
                    conservative[index(x, y + 1)] - 0.5f * minmod(
                        conservative[index(x, y + 1)] - conservative[cell],
                        conservative[index(x, std::min(height - 1, y + 2))] -
                            conservative[index(x, y + 1)])));
          const float netFlux = rightU * rightDensity - leftU * leftDensity +
              bottomV * bottomDensity - topV * topDensity;
          fluxNext[cell] = std::max(0.0f, conservative[cell] - subDt * netFlux);
        }
      }
      conservative.swap(fluxNext);
    }
  }
  const auto slMetrics = makeMetrics(semiLagrangian);
  const auto fluxMetrics = makeMetrics(conservative);
  GTEST_LOG_(INFO) << "2D fixed vortex transport substeps=" << substeps
      << " semiLagrangian mass/highPass/normalizedHighPass/gradient/max="
      << slMetrics[0] << '/' << slMetrics[1] << '/' << slMetrics[4] << '/'
      << slMetrics[2] << '/' << slMetrics[3]
      << " conservative mass/highPass/normalizedHighPass/gradient/max="
      << fluxMetrics[0] << '/' << fluxMetrics[1] << '/' << fluxMetrics[4] << '/'
      << fluxMetrics[2] << '/' << fluxMetrics[3];
  EXPECT_NEAR(fluxMetrics[0], makeMetrics(density)[0], makeMetrics(density)[0] * 1.0e-4);
  EXPECT_GT(slMetrics[0], 0.0);
  EXPECT_GT(fluxMetrics[0], 0.0);
  saveField(semiLagrangian, QStringLiteral("transport_2d_semilagrangian.png"));
  saveField(conservative, QStringLiteral("transport_2d_fluxform.png"));
}

TEST(ArtifactSmokeRuntimeStandaloneTest,
     ReplaysRuntimeVelocityHistoryWithTwoDensityTransportSchemes) {
  constexpr int width = kGridWidth;
  constexpr int height = kGridHeight;
  constexpr int finalFrame = 44;
  constexpr float frameRate = 30.0f;
  const auto index = [](const int x, const int y) {
    return static_cast<std::size_t>(x + width * y);
  };
  struct VelocityFrame final {
    std::vector<float> x;
    std::vector<float> y;
  };
  std::vector<VelocityFrame> velocityHistory;
  velocityHistory.reserve(finalFrame + 1);
  std::vector<float> productionFinalDensity;
  double divergenceSquaredSum = 0.0;
  double peakFrameDivergenceRms = 0.0;
  double faceDivergenceSquaredSum = 0.0;
  double scaledCenteredDivergenceSquaredSum = 0.0;
  double maxCenterFaceDivergenceDifference = 0.0;
  Artifact::LayerFluidRuntimeState runtime;
  Artifact::LayerSmokeRuntimeSettings settings;
  settings.gridWidth = width;
  settings.gridHeight = height;
  settings.solverIterations = 20;
  settings.emitterX = 0.5f;
  settings.emitterY = 0.84f;
  settings.emitterRadius = 0.08f;
  settings.emitterDensity = 0.6f;
  settings.emitterVelocityY = -1.2f;
  settings.emitterSwirl = 0.15f;
  settings.buoyancy = 0.0f;
  settings.vorticity = 0.1f;
  for (int frame = 0; frame <= finalFrame; ++frame) {
    Artifact::advanceLayerSmokeRuntime(runtime, frame, frameRate, settings);
    const auto snapshot = runtime.fluidSolver_->snapshot();
    if (frame == finalFrame)
      productionFinalDensity.assign(snapshot.density.begin(), snapshot.density.end());
    VelocityFrame velocities;
    velocities.x.assign(snapshot.velocityX.begin(), snapshot.velocityX.end());
    velocities.y.assign(snapshot.velocityY.begin(), snapshot.velocityY.end());
    double frameDivergenceSquared = 0.0;
    double frameFaceDivergenceSquared = 0.0;
    double frameScaledCenteredDivergenceSquared = 0.0;
    std::size_t divergenceSamples = 0;
    for (int y = 1; y < height - 1; ++y)
      for (int x = 1; x < width - 1; ++x) {
        const float div = 0.5f * (
            snapshot.velocityX[index(x + 1, y)] - snapshot.velocityX[index(x - 1, y)] +
            snapshot.velocityY[index(x, y + 1)] - snapshot.velocityY[index(x, y - 1)]);
        frameDivergenceSquared += static_cast<double>(div) * div;
        const float cellDtX = (width - 2) / frameRate;
        const float cellDtY = (height - 2) / frameRate;
        const float scaledCenteredDiv = 0.5f * cellDtX * (
            snapshot.velocityX[index(x + 1, y)] - snapshot.velocityX[index(x - 1, y)]) +
            0.5f * cellDtY * (
                snapshot.velocityY[index(x, y + 1)] - snapshot.velocityY[index(x, y - 1)]);
        const float leftFace = 0.5f * cellDtX * (
            snapshot.velocityX[index(x - 1, y)] + snapshot.velocityX[index(x, y)]);
        const float rightFace = 0.5f * cellDtX * (
            snapshot.velocityX[index(x, y)] + snapshot.velocityX[index(x + 1, y)]);
        const float topFace = 0.5f * cellDtY * (
            snapshot.velocityY[index(x, y - 1)] + snapshot.velocityY[index(x, y)]);
        const float bottomFace = 0.5f * cellDtY * (
            snapshot.velocityY[index(x, y)] + snapshot.velocityY[index(x, y + 1)]);
        const float faceFluxDiv = rightFace - leftFace + bottomFace - topFace;
        frameFaceDivergenceSquared += static_cast<double>(faceFluxDiv) * faceFluxDiv;
        frameScaledCenteredDivergenceSquared += static_cast<double>(scaledCenteredDiv) *
            scaledCenteredDiv;
        maxCenterFaceDivergenceDifference = std::max(
            maxCenterFaceDivergenceDifference,
            static_cast<double>(std::abs(scaledCenteredDiv - faceFluxDiv)));
        ++divergenceSamples;
      }
    const double frameDivergenceRms = std::sqrt(
        frameDivergenceSquared / std::max<std::size_t>(1, divergenceSamples));
    divergenceSquaredSum += frameDivergenceSquared /
        std::max<std::size_t>(1, divergenceSamples);
    faceDivergenceSquaredSum += frameFaceDivergenceSquared /
        std::max<std::size_t>(1, divergenceSamples);
    scaledCenteredDivergenceSquaredSum += frameScaledCenteredDivergenceSquared /
        std::max<std::size_t>(1, divergenceSamples);
    peakFrameDivergenceRms = std::max(peakFrameDivergenceRms, frameDivergenceRms);
    velocityHistory.push_back(std::move(velocities));
  }
  const auto rawVelocityHistory = velocityHistory;
  const auto setBoundary = [&](const int boundary, std::vector<float>& field) {
    for (int x = 1; x < width - 1; ++x) {
      field[index(x, 0)] = boundary == 2 ? -field[index(x, 1)] : field[index(x, 1)];
      field[index(x, height - 1)] = boundary == 2
          ? -field[index(x, height - 2)] : field[index(x, height - 2)];
    }
    for (int y = 1; y < height - 1; ++y) {
      field[index(0, y)] = boundary == 1 ? -field[index(1, y)] : field[index(1, y)];
      field[index(width - 1, y)] = boundary == 1
          ? -field[index(width - 2, y)] : field[index(width - 2, y)];
    }
    field[index(0, 0)] = 0.5f * (field[index(1, 0)] + field[index(0, 1)]);
    field[index(0, height - 1)] = 0.5f *
        (field[index(1, height - 1)] + field[index(0, height - 2)]);
    field[index(width - 1, 0)] = 0.5f *
        (field[index(width - 2, 0)] + field[index(width - 1, 1)]);
    field[index(width - 1, height - 1)] = 0.5f *
        (field[index(width - 2, height - 1)] + field[index(width - 1, height - 2)]);
  };
  double projectedDivergenceSquaredSum = 0.0;
  double projectedPeakFrameDivergenceRms = 0.0;
  double projectedFaceDivergenceSquaredSum = 0.0;
  double projectedScaledCenteredDivergenceSquaredSum = 0.0;
  double projectedMaxCenterFaceDifference = 0.0;
  for (auto& frame : velocityHistory) {
    std::vector<float> divergence(static_cast<std::size_t>(width * height), 0.0f);
    std::vector<float> pressure(static_cast<std::size_t>(width * height), 0.0f);
    for (int y = 1; y < height - 1; ++y)
      for (int x = 1; x < width - 1; ++x)
        divergence[index(x, y)] = -0.5f * (
            frame.x[index(x + 1, y)] - frame.x[index(x - 1, y)] +
            frame.y[index(x, y + 1)] - frame.y[index(x, y - 1)]) /
            std::sqrt(static_cast<float>(width * height));
    setBoundary(0, divergence);
    setBoundary(0, pressure);
    constexpr int projectionIterations = 80;
    for (int iteration = 0; iteration < projectionIterations; ++iteration) {
      for (int y = 1; y < height - 1; ++y)
        for (int x = 1; x < width - 1; ++x)
          pressure[index(x, y)] = 0.25f * (divergence[index(x, y)] +
              pressure[index(x - 1, y)] + pressure[index(x + 1, y)] +
              pressure[index(x, y - 1)] + pressure[index(x, y + 1)]);
      setBoundary(0, pressure);
    }
    for (int y = 1; y < height - 1; ++y)
      for (int x = 1; x < width - 1; ++x) {
        frame.x[index(x, y)] -= 0.5f *
            (pressure[index(x + 1, y)] - pressure[index(x - 1, y)]) * width;
        frame.y[index(x, y)] -= 0.5f *
            (pressure[index(x, y + 1)] - pressure[index(x, y - 1)]) * height;
      }
    setBoundary(1, frame.x);
    setBoundary(2, frame.y);
    double projectedSquared = 0.0;
    double projectedFaceSquared = 0.0;
    double projectedScaledCenteredSquared = 0.0;
    std::size_t projectedSamples = 0;
    for (int y = 1; y < height - 1; ++y)
      for (int x = 1; x < width - 1; ++x) {
        const float div = 0.5f * (
            frame.x[index(x + 1, y)] - frame.x[index(x - 1, y)] +
            frame.y[index(x, y + 1)] - frame.y[index(x, y - 1)]);
        projectedSquared += static_cast<double>(div) * div;
        const float cellDtX = (width - 2) / frameRate;
        const float cellDtY = (height - 2) / frameRate;
        const float scaledCenteredDiv = 0.5f * cellDtX * (
            frame.x[index(x + 1, y)] - frame.x[index(x - 1, y)]) +
            0.5f * cellDtY * (
                frame.y[index(x, y + 1)] - frame.y[index(x, y - 1)]);
        const float leftFace = 0.5f * cellDtX *
            (frame.x[index(x - 1, y)] + frame.x[index(x, y)]);
        const float rightFace = 0.5f * cellDtX *
            (frame.x[index(x, y)] + frame.x[index(x + 1, y)]);
        const float topFace = 0.5f * cellDtY *
            (frame.y[index(x, y - 1)] + frame.y[index(x, y)]);
        const float bottomFace = 0.5f * cellDtY *
            (frame.y[index(x, y)] + frame.y[index(x, y + 1)]);
        const float faceFluxDiv = rightFace - leftFace + bottomFace - topFace;
        projectedFaceSquared += static_cast<double>(faceFluxDiv) * faceFluxDiv;
        projectedScaledCenteredSquared +=
            static_cast<double>(scaledCenteredDiv) * scaledCenteredDiv;
        projectedMaxCenterFaceDifference = std::max(
            projectedMaxCenterFaceDifference,
            static_cast<double>(std::abs(scaledCenteredDiv - faceFluxDiv)));
        ++projectedSamples;
      }
    const double projectedRms = std::sqrt(projectedSquared /
        std::max<std::size_t>(1, projectedSamples));
    projectedDivergenceSquaredSum += projectedSquared /
        std::max<std::size_t>(1, projectedSamples);
    projectedFaceDivergenceSquaredSum += projectedFaceSquared /
        std::max<std::size_t>(1, projectedSamples);
    projectedScaledCenteredDivergenceSquaredSum +=
        projectedScaledCenteredSquared /
        std::max<std::size_t>(1, projectedSamples);
    projectedPeakFrameDivergenceRms = std::max(
        projectedPeakFrameDivergenceRms, projectedRms);
  }
  const QString outputDirectory =
      qEnvironmentVariable("ARTIFACT_SMOKE_RUNTIME_OUTPUT_DIR");
  const auto sampleBilinear = [&](const std::vector<float>& field, float x, float y) {
    x = std::clamp(x, 0.0f, static_cast<float>(width - 1));
    y = std::clamp(y, 0.0f, static_cast<float>(height - 1));
    const int x0 = static_cast<int>(x);
    const int y0 = static_cast<int>(y);
    const int x1 = std::min(x0 + 1, width - 1);
    const int y1 = std::min(y0 + 1, height - 1);
    const float tx = x - x0;
    const float ty = y - y0;
    return (1.0f - ty) * ((1.0f - tx) * field[index(x0, y0)] +
        tx * field[index(x1, y0)]) + ty * ((1.0f - tx) *
        field[index(x0, y1)] + tx * field[index(x1, y1)]);
  };
  const auto saveField = [&](const std::vector<float>& field, const QString& filename) {
    if (outputDirectory.isEmpty()) return;
    ASSERT_TRUE(QDir().mkpath(outputDirectory.toUtf8().constData()));
    const float peak = *std::max_element(field.begin(), field.end());
    constexpr int scale = 3;
    QImage image(width * scale, height * scale, QImage::Format_Grayscale8);
    for (int y = 0; y < image.height(); ++y) {
      auto* row = image.scanLine(y);
      for (int x = 0; x < image.width(); ++x) {
        const float normalized = peak > 0.0f
            ? field[index(x / scale, y / scale)] / peak : 0.0f;
        row[x] = static_cast<uchar>(std::clamp(
            static_cast<int>(std::lround(normalized * 255.0f)), 0, 255));
      }
    }
    EXPECT_TRUE(image.save(QDir(outputDirectory).filePath(filename), "PNG"));
  };
  std::vector<float> initial(static_cast<std::size_t>(width * height), 0.0f);
  std::vector<float> sl = initial;
  std::vector<float> slNext(initial.size(), 0.0f);
  std::vector<float> slProductionStep = initial;
  std::vector<float> slProductionNext(initial.size(), 0.0f);
  std::vector<float> slCoreExact = initial;
  std::vector<float> slCoreDiffuse(initial.size(), 0.0f);
  std::vector<float> slCoreNext(initial.size(), 0.0f);
  std::vector<float> slProductionLag = initial;
  std::vector<float> slProductionLagNext(initial.size(), 0.0f);
  std::vector<float> slPhaseShifted = initial;
  std::vector<float> slPhaseShiftedNext(initial.size(), 0.0f);
  std::vector<float> flux = initial;
  std::vector<float> fluxNext(initial.size(), 0.0f);
  const auto minmod = [](const float a, const float b) {
    if (a * b <= 0.0f) return 0.0f;
    return std::copysign(std::min(std::abs(a), std::abs(b)), a);
  };
  int maxSubsteps = 1;
  double totalInjectedDensity = 0.0;
  double expectedFinalDensity = 0.0;
  constexpr float densityDissipationPerSecond = 2.5f;
  const float densityRetention = std::exp(-densityDissipationPerSecond / frameRate);
  for (int frame = 0; frame <= finalFrame; ++frame) {
    const auto& velocity = rawVelocityHistory[static_cast<std::size_t>(frame)];
    // Runtime sway modulates injected horizontal velocity, not emitter position.
    const float sourceX = settings.emitterX;
    const int centerX = 1 + static_cast<int>(std::round(
        sourceX * static_cast<float>(width - 3)));
    const int centerY = 1 + static_cast<int>(std::round(
        settings.emitterY * static_cast<float>(height - 3)));
    constexpr int sourcePhaseOffsetFrames = 15;
    const float shiftedPhase = static_cast<float>(frame + sourcePhaseOffsetFrames) /
        frameRate * (6.28318530717958647692f * 0.22f);
    const int shiftedCenterX = 1 + static_cast<int>(std::round(
        (settings.emitterX + std::sin(shiftedPhase) * 0.04f) *
        static_cast<float>(width - 3)));
    const float radiusX = settings.emitterRadius * static_cast<float>(width - 2);
    const float radiusY = settings.emitterRadius * static_cast<float>(height - 2);
    const float safeRadiusX = std::max(radiusX, 0.5f);
    const float safeRadiusY = std::max(radiusY, 0.5f);
    double frameInjectedDensity = 0.0;
    for (int y = 1; y < height - 1; ++y)
      for (int x = 1; x < width - 1; ++x) {
        const float nx = static_cast<float>(x - centerX) / safeRadiusX;
        const float ny = static_cast<float>(y - centerY) / safeRadiusY;
        const float distanceSquared = nx * nx + ny * ny;
        if (distanceSquared > 1.0f) continue;
        const float sourceDensity = settings.emitterDensity *
            (1.0f - distanceSquared);
        const float shiftedNx = static_cast<float>(x - shiftedCenterX) / safeRadiusX;
        const float shiftedDistanceSquared = shiftedNx * shiftedNx + ny * ny;
        if (shiftedDistanceSquared <= 1.0f)
          slPhaseShifted[index(x, y)] += settings.emitterDensity *
              (1.0f - shiftedDistanceSquared);
        sl[index(x, y)] += sourceDensity;
        slProductionStep[index(x, y)] += sourceDensity;
        slCoreExact[index(x, y)] += sourceDensity;
        slProductionLag[index(x, y)] += sourceDensity;
        flux[index(x, y)] += sourceDensity;
        frameInjectedDensity += sourceDensity;
      }
    totalInjectedDensity += frameInjectedDensity;
    expectedFinalDensity =
        (expectedFinalDensity + frameInjectedDensity) * densityRetention;
    const float cellDtX = (width - 2) / frameRate;
    const float cellDtY = (height - 2) / frameRate;
    float maxCellSpeed = 0.0f;
    for (std::size_t i = 0; i < velocity.x.size(); ++i)
      maxCellSpeed = std::max(maxCellSpeed, std::max(
          std::abs(velocity.x[i] * cellDtX),
          std::abs(velocity.y[i] * cellDtY)));
    const int substeps = std::max(1, static_cast<int>(std::ceil(maxCellSpeed / 0.45f)));
    maxSubsteps = std::max(maxSubsteps, substeps);
    for (int y = 0; y < height; ++y)
      for (int x = 0; x < width; ++x) {
        const auto cell = index(x, y);
        slProductionNext[cell] = sampleBilinear(slProductionStep,
            static_cast<float>(x) - velocity.x[cell] * cellDtX,
            static_cast<float>(y) - velocity.y[cell] * cellDtY);
      }
    slProductionStep.swap(slProductionNext);
    // Match FluidSolver2D's density step: Jacobi diffusion, ghost-cell
    // boundary handling, then the solver's half-cell-clamped backtrace.
    const float diffusionA = (1.0f / frameRate) * 1.0e-5f *
        (width - 2) * (height - 2);
    std::copy(slCoreExact.begin(), slCoreExact.end(), slCoreDiffuse.begin());
    for (int iteration = 0; iteration < 20; ++iteration) {
      for (int y = 1; y < height - 1; ++y)
        for (int x = 1; x < width - 1; ++x) {
          const auto cell = index(x, y);
          slCoreDiffuse[cell] = (slCoreExact[cell] + diffusionA * (
              slCoreDiffuse[index(x + 1, y)] + slCoreDiffuse[index(x - 1, y)] +
              slCoreDiffuse[index(x, y + 1)] + slCoreDiffuse[index(x, y - 1)])) /
              (1.0f + 4.0f * diffusionA);
        }
      for (int x = 1; x < width - 1; ++x) {
        slCoreDiffuse[index(x, 0)] = slCoreDiffuse[index(x, 1)];
        slCoreDiffuse[index(x, height - 1)] = slCoreDiffuse[index(x, height - 2)];
      }
      for (int y = 1; y < height - 1; ++y) {
        slCoreDiffuse[index(0, y)] = slCoreDiffuse[index(1, y)];
        slCoreDiffuse[index(width - 1, y)] = slCoreDiffuse[index(width - 2, y)];
      }
      slCoreDiffuse[index(0, 0)] = 0.5f * (slCoreDiffuse[index(1, 0)] + slCoreDiffuse[index(0, 1)]);
      slCoreDiffuse[index(0, height - 1)] = 0.5f * (slCoreDiffuse[index(1, height - 1)] + slCoreDiffuse[index(0, height - 2)]);
      slCoreDiffuse[index(width - 1, 0)] = 0.5f * (slCoreDiffuse[index(width - 2, 0)] + slCoreDiffuse[index(width - 1, 1)]);
      slCoreDiffuse[index(width - 1, height - 1)] = 0.5f * (slCoreDiffuse[index(width - 2, height - 1)] + slCoreDiffuse[index(width - 1, height - 2)]);
    }
    for (int y = 1; y < height - 1; ++y)
      for (int x = 1; x < width - 1; ++x) {
        const auto cell = index(x, y);
        const float traceX = std::clamp(static_cast<float>(x) -
            velocity.x[cell] * cellDtX, 0.5f, static_cast<float>(width) - 1.5f);
        const float traceY = std::clamp(static_cast<float>(y) -
            velocity.y[cell] * cellDtY, 0.5f, static_cast<float>(height) - 1.5f);
        const int x0 = static_cast<int>(std::floor(traceX));
        const int y0 = static_cast<int>(std::floor(traceY));
        const int x1 = x0 + 1;
        const int y1 = y0 + 1;
        const float tx = traceX - x0;
        const float ty = traceY - y0;
        slCoreNext[cell] = (1.0f - tx) * ((1.0f - ty) * slCoreDiffuse[index(x0, y0)] +
            ty * slCoreDiffuse[index(x0, y1)]) + tx * ((1.0f - ty) *
            slCoreDiffuse[index(x1, y0)] + ty * slCoreDiffuse[index(x1, y1)]);
      }
    for (int x = 1; x < width - 1; ++x) {
      slCoreNext[index(x, 0)] = slCoreNext[index(x, 1)];
      slCoreNext[index(x, height - 1)] = slCoreNext[index(x, height - 2)];
    }
    for (int y = 1; y < height - 1; ++y) {
      slCoreNext[index(0, y)] = slCoreNext[index(1, y)];
      slCoreNext[index(width - 1, y)] = slCoreNext[index(width - 2, y)];
    }
    slCoreNext[index(0, 0)] = 0.5f * (slCoreNext[index(1, 0)] + slCoreNext[index(0, 1)]);
    slCoreNext[index(0, height - 1)] = 0.5f * (slCoreNext[index(1, height - 1)] + slCoreNext[index(0, height - 2)]);
    slCoreNext[index(width - 1, 0)] = 0.5f * (slCoreNext[index(width - 2, 0)] + slCoreNext[index(width - 1, 1)]);
    slCoreNext[index(width - 1, height - 1)] = 0.5f * (slCoreNext[index(width - 2, height - 1)] + slCoreNext[index(width - 1, height - 2)]);
    slCoreExact.swap(slCoreNext);
    for (auto& value : slCoreExact) value *= densityRetention;
    for (int y = 0; y < height; ++y)
      for (int x = 0; x < width; ++x) {
        const auto cell = index(x, y);
        slPhaseShiftedNext[cell] = sampleBilinear(slPhaseShifted,
            static_cast<float>(x) - velocity.x[cell] * cellDtX,
            static_cast<float>(y) - velocity.y[cell] * cellDtY);
      }
    slPhaseShifted.swap(slPhaseShiftedNext);
    const auto& lagVelocity = rawVelocityHistory[static_cast<std::size_t>(
        frame > 0 ? frame - 1 : frame)];
    for (int y = 0; y < height; ++y)
      for (int x = 0; x < width; ++x) {
        const auto cell = index(x, y);
        slProductionLagNext[cell] = sampleBilinear(slProductionLag,
            static_cast<float>(x) - lagVelocity.x[cell] * cellDtX,
            static_cast<float>(y) - lagVelocity.y[cell] * cellDtY);
      }
    slProductionLag.swap(slProductionLagNext);
    const float subDt = 1.0f / static_cast<float>(substeps);
    for (int substep = 0; substep < substeps; ++substep) {
      for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x) {
          const auto cell = index(x, y);
          slNext[cell] = sampleBilinear(sl,
              static_cast<float>(x) - velocity.x[cell] * cellDtX * subDt,
              static_cast<float>(y) - velocity.y[cell] * cellDtY * subDt);
        }
      sl.swap(slNext);
      for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
          const auto cell = index(x, y);
          const auto ux = [&](const int px, const int py) {
            return velocity.x[index(px, py)] * cellDtX;
          };
          const auto vy = [&](const int px, const int py) {
            return velocity.y[index(px, py)] * cellDtY;
          };
          const float leftU = x == 0 ? 0.0f : 0.5f * (ux(x - 1, y) + ux(x, y));
          const float rightU = x == width - 1 ? 0.0f : 0.5f * (ux(x, y) + ux(x + 1, y));
          const float topV = y == 0 ? 0.0f : 0.5f * (vy(x, y - 1) + vy(x, y));
          const float bottomV = y == height - 1 ? 0.0f : 0.5f * (vy(x, y) + vy(x, y + 1));
          const float center = flux[cell];
          const float left = leftU >= 0.0f
              ? (x == 0 ? center : std::max(0.0f, flux[index(x - 1, y)] + 0.5f * minmod(
                    flux[index(x - 1, y)] - flux[index(std::max(0, x - 2), y)],
                    center - flux[index(x - 1, y)])))
              : std::max(0.0f, center - 0.5f * minmod(
                    center - flux[index(std::max(0, x - 1), y)],
                    flux[index(std::min(width - 1, x + 1), y)] - center));
          const float right = rightU >= 0.0f
              ? std::max(0.0f, center + 0.5f * minmod(
                    center - flux[index(std::max(0, x - 1), y)],
                    flux[index(std::min(width - 1, x + 1), y)] - center))
              : (x == width - 1 ? center : std::max(0.0f,
                    flux[index(x + 1, y)] - 0.5f * minmod(
                        flux[index(x + 1, y)] - center,
                        flux[index(std::min(width - 1, x + 2), y)] -
                            flux[index(x + 1, y)])));
          const float top = topV >= 0.0f
              ? (y == 0 ? center : std::max(0.0f, flux[index(x, y - 1)] + 0.5f * minmod(
                    flux[index(x, y - 1)] - flux[index(x, std::max(0, y - 2))],
                    center - flux[index(x, y - 1)])))
              : std::max(0.0f, center - 0.5f * minmod(
                    center - flux[index(x, std::max(0, y - 1))],
                    flux[index(x, std::min(height - 1, y + 1))] - center));
          const float bottom = bottomV >= 0.0f
              ? std::max(0.0f, center + 0.5f * minmod(
                    center - flux[index(x, std::max(0, y - 1))],
                    flux[index(x, std::min(height - 1, y + 1))] - center))
              : (y == height - 1 ? center : std::max(0.0f,
                    flux[index(x, y + 1)] - 0.5f * minmod(
                        flux[index(x, y + 1)] - center,
                        flux[index(x, std::min(height - 1, y + 2))] -
                            flux[index(x, y + 1)])));
          fluxNext[cell] = std::max(0.0f, center - subDt * (
              rightU * right - leftU * left + bottomV * bottom - topV * top));
        }
      }
      flux.swap(fluxNext);
    }
    for (std::size_t cell = 0; cell < sl.size(); ++cell) {
      sl[cell] *= densityRetention;
      slProductionStep[cell] *= densityRetention;
      slProductionLag[cell] *= densityRetention;
      slPhaseShifted[cell] *= densityRetention;
      flux[cell] *= densityRetention;
    }
  }
  const auto totalMass = [](const std::vector<float>& field) {
    double sum = 0.0;
    for (const float value : field) sum += value;
    return sum;
  };
  const auto metrics = [&](const std::vector<float>& field) {
    const auto [lo, hi] = std::minmax_element(field.begin(), field.end());
    double hp = 0.0;
    for (int y = 1; y < height - 1; ++y)
      for (int x = 1; x < width - 1; ++x) {
        const float lap = 4.0f * field[index(x, y)] - field[index(x - 1, y)] -
            field[index(x + 1, y)] - field[index(x, y - 1)] - field[index(x, y + 1)];
        hp += static_cast<double>(lap) * lap;
      }
    return std::array<double, 3>{totalMass(field), *hi,
        std::sqrt(hp / ((width - 2) * (height - 2)))};
  };
  const auto slStats = metrics(sl);
  const auto slProductionStats = metrics(slProductionStep);
  const auto slCoreExactStats = metrics(slCoreExact);
  const auto slLagStats = metrics(slProductionLag);
  const auto slPhaseStats = metrics(slPhaseShifted);
  const auto fluxStats = metrics(flux);
  const double initialMass = totalMass(initial);
  const double productionMass = totalMass(productionFinalDensity);
  double exactReplayMaxAbsError = 0.0;
  double exactReplaySquaredError = 0.0;
  for (std::size_t cell = 0; cell < slCoreExact.size(); ++cell) {
    const double error = static_cast<double>(slCoreExact[cell]) -
        productionFinalDensity[cell];
    exactReplayMaxAbsError = std::max(exactReplayMaxAbsError, std::abs(error));
    exactReplaySquaredError += error * error;
  }
  const double exactReplayRmsError = std::sqrt(
      exactReplaySquaredError / static_cast<double>(slCoreExact.size()));
  GTEST_LOG_(INFO) << "runtime velocity replay source-coupled rawHistory maxSubsteps=" << maxSubsteps
      << " projectionIterations=80"
      << " velocityDivergenceRmsMean=" << std::sqrt(
          divergenceSquaredSum / static_cast<double>(finalFrame + 1))
      << " scaledCenteredDivergenceRmsMean=" << std::sqrt(
          scaledCenteredDivergenceSquaredSum / static_cast<double>(finalFrame + 1))
      << " velocityFaceFluxDivergenceRmsMean=" << std::sqrt(
          faceDivergenceSquaredSum / static_cast<double>(finalFrame + 1))
      << " maxCenteredFaceDifference=" << maxCenterFaceDivergenceDifference
      << " peakFrameDivergenceRms=" << peakFrameDivergenceRms
      << " projectedVelocityDivergenceRmsMean=" << std::sqrt(
          projectedDivergenceSquaredSum / static_cast<double>(finalFrame + 1))
      << " projectedScaledCenteredDivergenceRmsMean=" << std::sqrt(
          projectedScaledCenteredDivergenceSquaredSum /
          static_cast<double>(finalFrame + 1))
      << " projectedFaceFluxDivergenceRmsMean=" << std::sqrt(
          projectedFaceDivergenceSquaredSum / static_cast<double>(finalFrame + 1))
      << " projectedMaxCenteredFaceDifference=" << projectedMaxCenterFaceDifference
      << " projectedPeakFrameDivergenceRms=" << projectedPeakFrameDivergenceRms
      << " injectedDensity=" << totalInjectedDensity
      << " expectedWithDissipation=" << expectedFinalDensity
      << " productionRuntimeMass=" << productionMass
      << " semiLagrangian="
      << slStats[0] << '/' << slStats[1] << '/' << slStats[2]
      << " productionStepSemiLagrangian=" << slProductionStats[0] << '/'
      << slProductionStats[1] << '/' << slProductionStats[2]
      << " coreExactDensityStep=" << slCoreExactStats[0] << '/'
      << slCoreExactStats[1] << '/' << slCoreExactStats[2]
      << " exactReplayMaxAbsError=" << exactReplayMaxAbsError
      << " exactReplayRmsError=" << exactReplayRmsError
      << " oneFrameLagSemiLagrangian=" << slLagStats[0] << '/'
      << slLagStats[1] << '/' << slLagStats[2]
      << " sourceSwayPhasePlus15SemiLagrangian=" << slPhaseStats[0] << '/'
      << slPhaseStats[1] << '/' << slPhaseStats[2]
      << " MUSCL=" << fluxStats[0] << '/' << fluxStats[1] << '/' << fluxStats[2];
  EXPECT_NEAR(fluxStats[0], expectedFinalDensity,
              std::max(1.0, expectedFinalDensity) * 1.0e-3);
  EXPECT_LT(exactReplayMaxAbsError, 1.0e-4);
  EXPECT_GE(*std::min_element(flux.begin(), flux.end()), 0.0f);
  EXPECT_LT(maxCenterFaceDivergenceDifference, 1.0e-5);
  EXPECT_LT(projectedMaxCenterFaceDifference, 1.0e-5);

  const QRectF smokeBounds(0.0, 0.0, width, height);
  QMatrix4x4 smokeTransform;
  smokeTransform.setToIdentity();
  const auto& productionParticles = Artifact::prepareLayerSmokeRenderData(
      runtime, smokeBounds, smokeTransform, 1.0f, finalFrame, settings);
  const QImage productionParticleImage = rasterizeParticles(productionParticles);
  auto musclSnapshot = runtime.fluidSolver_->snapshot();
  const float massMatchScale = productionMass > 0.0 && fluxStats[0] > 0.0
      ? static_cast<float>(productionMass / fluxStats[0]) : 1.0f;
  for (std::size_t cell = 0; cell < flux.size(); ++cell)
    musclSnapshot.density[cell] = flux[cell] * massMatchScale;
  ASSERT_TRUE(runtime.fluidSolver_->restoreSnapshot(musclSnapshot));
  const auto& musclParticles = Artifact::prepareLayerSmokeRenderData(
      runtime, smokeBounds, smokeTransform, 1.0f, finalFrame, settings);
  const QImage musclParticleImage = rasterizeParticles(musclParticles);
  const auto productionParticleMetrics = measureSmokeImage(productionParticleImage);
  const auto musclParticleMetrics = measureSmokeImage(musclParticleImage);
  const float musclExposureScale = musclParticleMetrics.greenExcess > 0
      ? std::clamp(static_cast<float>(productionParticleMetrics.greenExcess) /
          static_cast<float>(musclParticleMetrics.greenExcess), 1.0f, 2.0f)
      : 1.0f;
  const QImage musclExposureMatchedImage = rasterizeParticles(
      musclParticles, musclExposureScale);
  const auto musclExposureMatchedMetrics =
      measureSmokeImage(musclExposureMatchedImage);
  const auto imageHighPassRms = [](const QImage& image) {
    double squaredSum = 0.0;
    std::size_t sampleCount = 0;
    for (int y = 1; y < image.height() - 1; ++y)
      for (int x = 1; x < image.width() - 1; ++x) {
        const auto luma = [&image](const int px, const int py) {
          const QRgb pixel = image.pixel(px, py);
          return 0.2126 * qRed(pixel) + 0.7152 * qGreen(pixel) +
              0.0722 * qBlue(pixel);
        };
        const double laplacian = 4.0 * luma(x, y) - luma(x - 1, y) -
            luma(x + 1, y) - luma(x, y - 1) - luma(x, y + 1);
        squaredSum += laplacian * laplacian;
        ++sampleCount;
      }
    return sampleCount > 0
        ? std::sqrt(squaredSum / static_cast<double>(sampleCount)) : 0.0;
  };
  GTEST_LOG_(INFO) << "Mass-matched production smoke particle extraction: semiLagrangian visible/highPass="
      << productionParticleMetrics.visiblePixels << '/'
      << imageHighPassRms(productionParticleImage)
      << " MUSCL=" << musclParticleMetrics.visiblePixels << '/'
      << imageHighPassRms(musclParticleImage)
      << " exposure-green=" << productionParticleMetrics.greenExcess << '/'
      << musclParticleMetrics.greenExcess
      << " MUSCL exposureMatchedScale=" << musclExposureScale
      << " visible/highPass/green=" << musclExposureMatchedMetrics.visiblePixels
      << '/' << imageHighPassRms(musclExposureMatchedImage) << '/'
      << musclExposureMatchedMetrics.greenExcess;
  EXPECT_GT(productionParticleMetrics.visiblePixels, 100);
  EXPECT_GT(musclParticleMetrics.visiblePixels, 100);
  if (!outputDirectory.isEmpty()) {
    ASSERT_TRUE(QDir().mkpath(outputDirectory.toUtf8().constData()));
    EXPECT_TRUE(productionParticleImage.save(QDir(outputDirectory).filePath(
        QStringLiteral("runtime_velocity_particles_semilagrangian.png")), "PNG"));
    EXPECT_TRUE(musclParticleImage.save(QDir(outputDirectory).filePath(
        QStringLiteral("runtime_velocity_particles_muscl_massmatched.png")), "PNG"));
    EXPECT_TRUE(musclExposureMatchedImage.save(QDir(outputDirectory).filePath(
        QStringLiteral("runtime_velocity_particles_muscl_exposurematched.png")), "PNG"));
  }
  saveField(sl, QStringLiteral("runtime_velocity_semilagrangian.png"));
  saveField(slProductionStep,
            QStringLiteral("runtime_velocity_semilagrangian_production_step.png"));
  saveField(slCoreExact,
            QStringLiteral("runtime_velocity_semilagrangian_core_exact.png"));
  saveField(slProductionLag,
            QStringLiteral("runtime_velocity_semilagrangian_one_frame_lag.png"));
  saveField(slPhaseShifted,
            QStringLiteral("runtime_velocity_semilagrangian_source_phase_plus15.png"));
  saveField(flux, QStringLiteral("runtime_velocity_muscl.png"));
  saveField(productionFinalDensity,
            QStringLiteral("runtime_velocity_actual_runtime_density.png"));
}

TEST(ArtifactSmokeRuntimeStandaloneTest,
     ComparesClosedTopDensitySpongeAndVelocityOutflowAdapters) {
  constexpr int width = kGridWidth;
  constexpr int height = kGridHeight;
  constexpr int finalFrame = 44;
  constexpr int boundaryBand = 12;
  constexpr std::array<int, 5> sampleFrames{0, 11, 22, 33, 44};
  struct Condition final {
    const char* name;
    int mode; // 0=closed, 1=sponge, 2=velocity-only, 3=open-top projection
  };
  constexpr std::array<Condition, 4> conditions{{
      {"closed", 0}, {"density-sponge", 1},
      {"velocity-only", 2}, {"open-projection", 3}}};
  const auto index = [](const int x, const int y) {
    return static_cast<std::size_t>(x + width * y);
  };
  const auto smoothStep = [](float value) {
    value = std::clamp(value, 0.0f, 1.0f);
    return value * value * (3.0f - 2.0f * value);
  };
  const auto applyOpenTopProjection = [&](ArtifactCore::FluidSnapshot2D& field) {
    const std::size_t cellCount = static_cast<std::size_t>(width * height);
    std::vector<float> divergence(cellCount, 0.0f);
    std::vector<float> pressure(cellCount, 0.0f);
    std::vector<float> nextPressure(cellCount, 0.0f);
    // Replace the reflected top ghost with an outward-only, zero-gradient
    // velocity extension before building the pressure right-hand side.
    for (int x = 1; x < width - 1; ++x) {
      field.velocityX[index(x, 0)] = field.velocityX[index(x, 1)];
      field.velocityY[index(x, 0)] = std::min(
          field.velocityY[index(x, 1)], 0.0f);
    }
    const float inverseScale = 1.0f /
        std::sqrt(static_cast<float>(width * height));
    for (int y = 1; y < height - 1; ++y) {
      for (int x = 1; x < width - 1; ++x) {
        divergence[index(x, y)] = -0.5f * (
            field.velocityX[index(x + 1, y)] -
            field.velocityX[index(x - 1, y)] +
            field.velocityY[index(x, y + 1)] -
            field.velocityY[index(x, y - 1)]) * inverseScale;
      }
    }
    constexpr int pressureIterations = 20;
    for (int iteration = 0; iteration < pressureIterations; ++iteration) {
      for (int y = 1; y < height - 1; ++y) {
        for (int x = 1; x < width - 1; ++x) {
          const float left = x == 1
              ? pressure[index(x, y)] : pressure[index(x - 1, y)];
          const float right = x == width - 2
              ? pressure[index(x, y)] : pressure[index(x + 1, y)];
          const float top = y == 1
              ? 0.0f : pressure[index(x, y - 1)]; // atmospheric pressure
          const float bottom = y == height - 2
              ? pressure[index(x, y)] : pressure[index(x, y + 1)];
          nextPressure[index(x, y)] = 0.25f * (
              divergence[index(x, y)] + left + right + top + bottom);
        }
      }
      pressure.swap(nextPressure);
    }
    for (int y = 1; y < height - 1; ++y) {
      for (int x = 1; x < width - 1; ++x) {
        field.velocityX[index(x, y)] -= 0.5f * (
            pressure[index(x + 1, y)] - pressure[index(x - 1, y)]) * width;
        field.velocityY[index(x, y)] -= 0.5f * (
            pressure[index(x, y + 1)] - pressure[index(x, y - 1)]) * height;
      }
    }
    for (int x = 1; x < width - 1; ++x) {
      field.velocityX[index(x, 0)] = field.velocityX[index(x, 1)];
      field.velocityY[index(x, 0)] = std::min(
          field.velocityY[index(x, 1)], 0.0f);
      field.velocityX[index(x, height - 1)] =
          field.velocityX[index(x, height - 2)];
      field.velocityY[index(x, height - 1)] =
          -field.velocityY[index(x, height - 2)];
    }
    for (int y = 1; y < height - 1; ++y) {
      field.velocityX[index(0, y)] = -field.velocityX[index(1, y)];
      field.velocityX[index(width - 1, y)] =
          -field.velocityX[index(width - 2, y)];
      field.velocityY[index(0, y)] = field.velocityY[index(1, y)];
      field.velocityY[index(width - 1, y)] =
          field.velocityY[index(width - 2, y)];
    }
    for (int x = 1; x < width - 1; ++x)
      field.density[index(x, 0)] = field.density[index(x, 1)];
  };
  struct FieldStats final {
    double mass = 0.0;
    double centroidX = 0.0;
    double centroidY = 0.0;
    double topEightMass = 0.0;
    double topEightMeanDensity = 0.0;
    double divergenceRms = 0.0;
    double topOutflowFlux = 0.0;
  };
  const auto measureField = [&](const ArtifactCore::FluidSnapshot2D& field) {
    FieldStats stats;
    double divergenceSquared = 0.0;
    std::size_t divergenceCount = 0;
    for (int y = 1; y < height - 1; ++y) {
      for (int x = 1; x < width - 1; ++x) {
        const double density = field.density[index(x, y)];
        stats.mass += density;
        stats.centroidX += density * x;
        stats.centroidY += density * y;
        if (y <= 8) stats.topEightMass += density;
        const float divergence = 0.5f * (
            field.velocityX[index(x + 1, y)] -
            field.velocityX[index(x - 1, y)] +
            field.velocityY[index(x, y + 1)] -
            field.velocityY[index(x, y - 1)]);
        divergenceSquared += static_cast<double>(divergence) * divergence;
        ++divergenceCount;
      }
    }
    stats.divergenceRms = std::sqrt(divergenceSquared /
        std::max<std::size_t>(1, divergenceCount));
    for (int x = 1; x < width - 1; ++x)
      stats.topOutflowFlux += field.density[index(x, 1)] *
          std::max(0.0f, -field.velocityY[index(x, 1)]);
    if (stats.mass > 0.0) {
      stats.centroidX /= stats.mass;
      stats.centroidY /= stats.mass;
    }
    stats.topEightMeanDensity = stats.topEightMass /
        static_cast<double>((width - 2) * 8);
    return stats;
  };
  QMatrix4x4 transform;
  transform.setToIdentity();
  const QString outputDirectory =
      qEnvironmentVariable("ARTIFACT_SMOKE_RUNTIME_OUTPUT_DIR");

  for (const auto& condition : conditions) {
    Artifact::LayerFluidRuntimeState state;
    Artifact::LayerSmokeRuntimeSettings settings;
    settings.gridWidth = width;
    settings.gridHeight = height;
    settings.solverIterations = 20;
    settings.emitterX = 0.5f;
    settings.emitterY = 0.84f;
    settings.emitterRadius = 0.08f;
    settings.emitterDensity = 0.6f;
    settings.emitterVelocityY = -1.2f;
    settings.emitterSwirl = 0.15f;
    settings.vorticity = 0.1f;
    std::array<FieldStats, sampleFrames.size()> history{};
    double adapterRemovedDensity = 0.0;
    QImage finalImage;

    for (int frame = 0; frame <= finalFrame; ++frame) {
      Artifact::advanceLayerSmokeRuntime(state, frame, 30.0, settings);
      if (condition.mode != 0) {
        auto snapshot = state.fluidSolver_->snapshot();
        const double massBefore = measureField(snapshot).mass;
        if (condition.mode == 1) {
          for (int y = 1; y <= boundaryBand; ++y) {
            const float coordinate = static_cast<float>(y - 1) /
                static_cast<float>(boundaryBand - 1);
            const float retention = smoothStep(coordinate);
            for (int x = 1; x < width - 1; ++x)
              snapshot.density[index(x, y)] *= retention;
          }
        } else if (condition.mode == 2) {
          constexpr float dt = 1.0f / 30.0f;
          for (int x = 1; x < width - 1; ++x) {
            const auto boundary = index(x, 1);
            const auto interior = index(x, 2);
            const float outwardNormalVelocity = std::min(
                snapshot.velocityY[interior], 0.0f);
            const float outwardCourant = std::clamp(
                -outwardNormalVelocity * dt * static_cast<float>(height - 2),
                0.0f, 1.0f);
            snapshot.density[boundary] *= 1.0f - outwardCourant;
            snapshot.velocityX[boundary] = snapshot.velocityX[interior];
            snapshot.velocityY[boundary] = outwardNormalVelocity;
            snapshot.density[index(x, 0)] = snapshot.density[boundary];
            snapshot.velocityX[index(x, 0)] = snapshot.velocityX[boundary];
            snapshot.velocityY[index(x, 0)] = snapshot.velocityY[boundary];
          }
        } else {
          applyOpenTopProjection(snapshot);
          constexpr float dt = 1.0f / 30.0f;
          for (int x = 1; x < width - 1; ++x) {
            const auto boundary = index(x, 1);
            const float outwardCourant = std::clamp(
                -snapshot.velocityY[boundary] * dt *
                    static_cast<float>(height - 2),
                0.0f, 1.0f);
            snapshot.density[boundary] *= 1.0f - outwardCourant;
            snapshot.density[index(x, 0)] = snapshot.density[boundary];
          }
        }
        const double massAfter = measureField(snapshot).mass;
        adapterRemovedDensity += std::max(0.0, massBefore - massAfter);
        ASSERT_TRUE(state.fluidSolver_->restoreSnapshot(snapshot));
      }

      for (std::size_t sample = 0; sample < sampleFrames.size(); ++sample) {
        if (frame != sampleFrames[sample]) continue;
        history[sample] = measureField(state.fluidSolver_->snapshot());
      }
      if (frame == finalFrame) {
        const auto& renderData = Artifact::prepareLayerSmokeRenderData(
            state, QRectF(0.0, 0.0, width, height), transform, 1.0f,
            frame, settings);
        finalImage = rasterizeParticles(renderData);
      }
    }

    const auto imageStats = measureSmokeImage(finalImage);
    GTEST_LOG_(INFO) << "top boundary " << condition.name
        << " mass@0/11/22/33/44=" << history[0].mass << '/'
        << history[1].mass << '/' << history[2].mass << '/'
        << history[3].mass << '/' << history[4].mass
        << " centroidY@22/33/44=" << history[2].centroidY << '/'
        << history[3].centroidY << '/' << history[4].centroidY
        << " top8MeanDensity@44=" << history[4].topEightMeanDensity
        << " divergenceRms@44=" << history[4].divergenceRms
        << " topOutflowFlux@44=" << history[4].topOutflowFlux
        << " cumulativeAdapterRemoval=" << adapterRemovedDensity
        << " visiblePixels=" << imageStats.visiblePixels;
    EXPECT_GT(history[4].mass, 0.0);
    EXPECT_GT(imageStats.visiblePixels, 100);
    if (!outputDirectory.isEmpty()) {
      ASSERT_TRUE(QDir().mkpath(outputDirectory.toUtf8().constData()));
      const QString filename = QStringLiteral("smoke_top_boundary_%1.png")
          .arg(QString::fromLatin1(condition.name));
      EXPECT_TRUE(finalImage.save(QDir(outputDirectory).filePath(filename), "PNG"));
    }
  }
}

TEST(ArtifactSmokeRuntimeStandaloneTest,
     ComparesFixedDensityOpticalDepthAndDirectionalSelfShadowRendering) {
  constexpr int width = kGridWidth;
  constexpr int height = kGridHeight;
  constexpr int frames = 34;
  constexpr int scale = 6;
  const auto index = [](const int x, const int y) {
    return static_cast<std::size_t>(x + width * y);
  };
  Artifact::LayerFluidRuntimeState runtime;
  Artifact::LayerSmokeRuntimeSettings settings;
  settings.gridWidth = width;
  settings.gridHeight = height;
  settings.solverIterations = 20;
  settings.emitterX = 0.5f;
  settings.emitterY = 0.84f;
  settings.emitterRadius = 0.08f;
  settings.emitterDensity = 0.6f;
  settings.emitterVelocityY = -1.2f;
  settings.emitterSwirl = 0.15f;
  settings.buoyancy = 0.0f;
  settings.vorticity = 0.1f;
  for (int frame = 0; frame <= frames; ++frame)
    Artifact::advanceLayerSmokeRuntime(runtime, frame, 30.0, settings);
  const auto density = runtime.fluidSolver_->snapshot().density;
  const auto sampleDensity = [&](const float gridX, const float gridY) {
    const float x = std::clamp(gridX, 0.0f, static_cast<float>(width - 1));
    const float y = std::clamp(gridY, 0.0f, static_cast<float>(height - 1));
    const int x0 = static_cast<int>(std::floor(x));
    const int y0 = static_cast<int>(std::floor(y));
    const int x1 = std::min(x0 + 1, width - 1);
    const int y1 = std::min(y0 + 1, height - 1);
    const float tx = x - x0;
    const float ty = y - y0;
    return std::lerp(std::lerp(density[index(x0, y0)], density[index(x1, y0)], tx),
                     std::lerp(density[index(x0, y1)], density[index(x1, y1)], tx), ty);
  };
  const float lightX = -0.70710678f;
  const float lightY = -0.70710678f;
  constexpr int shadowSteps = 24;
  constexpr float shadowStepCells = 0.75f;
  constexpr float extinction = 0.55f;
  const auto outputDirectory = qEnvironmentVariable(
      "ARTIFACT_SMOKE_RUNTIME_OUTPUT_DIR");
  bool imageSavesSucceeded = true;
  struct RenderStats final {
    double meanExcess = 0.0;
    double lumaStdDev = 0.0;
    std::size_t visible = 0;
  };
  const auto render = [&](const float ambientFill,
                          const bool useGradientNormal,
                          const QString& filename) {
    QImage image(width * scale, height * scale, QImage::Format_ARGB32);
    image.fill(kBackground);
    double lumaSum = 0.0;
    double lumaSquaredSum = 0.0;
    double excessSum = 0.0;
    std::size_t visible = 0;
    for (int py = 0; py < image.height(); ++py) {
      auto* row = reinterpret_cast<QRgb*>(image.scanLine(py));
      const float gy = (static_cast<float>(py) + 0.5f) / scale;
      for (int px = 0; px < image.width(); ++px) {
        const float gx = (static_cast<float>(px) + 0.5f) / scale;
        const float localDensity = std::max(0.0f, sampleDensity(gx, gy));
        float opticalDepth = 0.0f;
        for (int step = 1; step <= shadowSteps; ++step) {
          const float distance = step * shadowStepCells;
          opticalDepth += sampleDensity(gx + lightX * distance,
              gy + lightY * distance) * shadowStepCells;
        }
        const float transmittance = std::exp(-0.20f * opticalDepth);
        const float densityAlpha = 1.0f - std::exp(-extinction * localDensity);
        float diffuse = 1.0f;
        if (useGradientNormal) {
          const float gradientX = sampleDensity(gx + 0.5f, gy) -
              sampleDensity(gx - 0.5f, gy);
          const float gradientY = sampleDensity(gx, gy + 0.5f) -
              sampleDensity(gx, gy - 0.5f);
          const float gradientLength = std::sqrt(
              gradientX * gradientX + gradientY * gradientY) + 1.0e-6f;
          const float facing = std::clamp((-
              gradientX * lightX - gradientY * lightY) / gradientLength,
              0.0f, 1.0f);
          diffuse = 0.25f + 0.75f * facing;
        }
        const float illumination = std::clamp(
            ambientFill + (1.0f - ambientFill) * transmittance * diffuse,
            0.0f, 1.0f);
        const int smoke = std::clamp(static_cast<int>(std::lround(
            220.0f * illumination)), 0, 255);
        const int red = static_cast<int>(std::lround(
            smoke * densityAlpha + qRed(kBackground) * (1.0f - densityAlpha)));
        const int green = static_cast<int>(std::lround(
            smoke * densityAlpha + qGreen(kBackground) * (1.0f - densityAlpha)));
        const int blue = static_cast<int>(std::lround(
            smoke * densityAlpha + qBlue(kBackground) * (1.0f - densityAlpha)));
        row[px] = qRgba(red, green, blue, 255);
        const float luma = 0.2126f * red + 0.7152f * green + 0.0722f * blue;
        lumaSum += luma;
        lumaSquaredSum += static_cast<double>(luma) * luma;
        excessSum += std::max(0.0f, luma - 16.0f);
        if (luma > 24.0f) ++visible;
      }
    }
    const double pixelCount = static_cast<double>(image.width()) * image.height();
    const double meanLuma = lumaSum / pixelCount;
    if (!outputDirectory.isEmpty()) {
      imageSavesSucceeded = imageSavesSucceeded &&
          QDir().mkpath(outputDirectory.toUtf8().constData()) &&
          image.save(QDir(outputDirectory).filePath(filename), "PNG");
    }
    return RenderStats{excessSum / pixelCount,
        std::sqrt(std::max(0.0, lumaSquaredSum / pixelCount - meanLuma * meanLuma)),
        visible};
  };

  const auto unlit = render(1.0f, false,
      QStringLiteral("smoke_optical_depth_unlit.png"));
  const auto direct = render(0.0f, false,
      QStringLiteral("smoke_optical_depth_self_shadow.png"));
  const auto ambient = render(0.28f, false,
      QStringLiteral("smoke_optical_depth_ambient_fill.png"));
  const auto gradientLit = render(0.72f, true,
      QStringLiteral("smoke_optical_depth_gradient_lit.png"));
  GTEST_LOG_(INFO) << "fixed-field 2D optical-depth render (mean excess / luma std / visible): unlit="
      << unlit.meanExcess << '/' << unlit.lumaStdDev << '/' << unlit.visible
      << " direct-shadow=" << direct.meanExcess << '/' << direct.lumaStdDev
      << '/' << direct.visible << " ambient-fill=" << ambient.meanExcess << '/'
      << ambient.lumaStdDev << '/' << ambient.visible;
  GTEST_LOG_(INFO) << "gradient-lit=" << gradientLit.meanExcess << '/'
      << gradientLit.lumaStdDev << '/' << gradientLit.visible;
  EXPECT_GT(unlit.visible, 100);
  EXPECT_GT(direct.visible, 100);
  EXPECT_GT(ambient.visible, 100);
  EXPECT_GT(gradientLit.visible, 100);
  EXPECT_GT(unlit.lumaStdDev, 0.0);
  EXPECT_TRUE(imageSavesSucceeded);
}

TEST(ArtifactSmokeRuntimeStandaloneTest,
     RendersDepthVaryingVolumeFromTheSameTwoDimensionalSmokePlume) {
  using namespace ArtifactCore::RayTrace;
  constexpr int width = kGridWidth;
  constexpr int height = kGridHeight;
  constexpr int depth = 32;
  constexpr int frames = 34;
  const auto index2D = [](const int x, const int y) {
    return static_cast<std::size_t>(x + width * y);
  };
  const VolumeResolution resolution{width, height, depth};
  const auto index3D = [resolution](const int x, const int y, const int z) {
    return resolution.indexOf(x, y, z);
  };
  Artifact::LayerFluidRuntimeState runtime;
  Artifact::LayerSmokeRuntimeSettings smokeSettings;
  smokeSettings.gridWidth = width;
  smokeSettings.gridHeight = height;
  smokeSettings.solverIterations = 20;
  smokeSettings.emitterX = 0.5f;
  smokeSettings.emitterY = 0.84f;
  smokeSettings.emitterRadius = 0.08f;
  smokeSettings.emitterDensity = 0.6f;
  smokeSettings.emitterVelocityY = -1.2f;
  smokeSettings.emitterSwirl = 0.15f;
  smokeSettings.buoyancy = 0.0f;
  smokeSettings.vorticity = 0.1f;
  for (int frame = 0; frame <= frames; ++frame)
    Artifact::advanceLayerSmokeRuntime(runtime, frame, 30.0, smokeSettings);
  const auto snapshot = runtime.fluidSolver_->snapshot();
  const auto sample2D = [&](const float gridX, const float sourceY) {
    const float x = std::clamp(gridX, 0.0f, static_cast<float>(width - 1));
    const float y = std::clamp(sourceY, 0.0f, static_cast<float>(height - 1));
    const int x0 = static_cast<int>(std::floor(x));
    const int y0 = static_cast<int>(std::floor(y));
    const int x1 = std::min(x0 + 1, width - 1);
    const int y1 = std::min(y0 + 1, height - 1);
    const float tx = x - x0;
    const float ty = y - y0;
    return std::lerp(std::lerp(snapshot.density[index2D(x0, y0)],
                               snapshot.density[index2D(x1, y0)], tx),
                     std::lerp(snapshot.density[index2D(x0, y1)],
                               snapshot.density[index2D(x1, y1)], tx), ty);
  };
  std::vector<float> extruded(resolution.cellCount(), 0.0f);
  std::vector<float> depthVarying(resolution.cellCount(), 0.0f);
  std::vector<float> depthWarped(resolution.cellCount(), 0.0f);
  constexpr float twoPi = 6.28318530718f;
  for (int y = 0; y < height; ++y) {
    const float sourceY = static_cast<float>(height - 1 - y);
    for (int x = 0; x < width; ++x) {
      const float base = sample2D(static_cast<float>(x), sourceY);
      double envelopeSum = 0.0;
      double warpedDensitySum = 0.0;
      for (int z = 0; z < depth; ++z) {
        const float zNorm = (static_cast<float>(z) + 0.5f) / depth * 2.0f - 1.0f;
        const float envelope = std::exp(-2.5f * zNorm * zNorm);
        envelopeSum += envelope;
        extruded[index3D(x, y, z)] = base;
        depthVarying[index3D(x, y, z)] = base * envelope;
        const float warpX = 2.5f * smokeValueNoise(
            0.13f * x + 4.0f * zNorm, 0.13f * sourceY - 2.0f * zNorm + 7.0f) +
            0.8f * smokeValueNoise(
                0.4f * x - 7.0f * zNorm, 0.4f * sourceY + 3.0f * zNorm);
        const float warpY = 2.5f * smokeValueNoise(
            0.13f * x - 2.0f * zNorm + 21.0f,
            0.13f * sourceY + 4.0f * zNorm - 17.0f) +
            0.8f * smokeValueNoise(
                0.4f * x + 3.0f * zNorm, 0.4f * sourceY - 7.0f * zNorm);
        const float warpedSample = std::max(0.0f,
            sample2D(static_cast<float>(x) + warpX, sourceY + warpY));
        depthWarped[index3D(x, y, z)] = warpedSample * envelope;
        warpedDensitySum += warpedSample * envelope;
      }
      const float massNormalization = static_cast<float>(depth / envelopeSum);
      const float warpNormalization = warpedDensitySum > 1.0e-8
          ? static_cast<float>(depth * base / warpedDensitySum) : 0.0f;
      for (int z = 0; z < depth; ++z)
      {
        depthVarying[index3D(x, y, z)] *= massNormalization;
        depthWarped[index3D(x, y, z)] *= warpNormalization;
      }
      double warpedColumnMass = 0.0;
      for (int z = 0; z < depth; ++z)
        warpedColumnMass += depthWarped[index3D(x, y, z)];
      EXPECT_NEAR(warpedColumnMass, static_cast<double>(depth * base), 1.0e-4);
    }
  }
  const VolumeAABB bounds{{-0.75f, 0.0f, -0.25f}, {0.75f, 1.125f, 0.25f}};
  const auto makeVolumeRenderer = [&](std::vector<float>& field,
                                      const bool sideView) {
    CPUVolumeRenderer renderer;
    renderer.bounds = bounds;
    VolumeFieldSet fields;
    fields.density = {field.data(), resolution};
    renderer.setVolumeData(bounds, fields);
    renderer.settings.stepSize = 0.018f;
    renderer.settings.maxSteps = 256;
    renderer.settings.densityScale = 1.8f;
    renderer.settings.useTemperatureEmission = false;
    renderer.settings.backgroundColor = {0.015f, 0.02f, 0.03f};
    renderer.settings.lightIntensity = 3.0f;
    renderer.settings.phaseAnisotropy = 0.25f;
    renderer.settings.multipleScatteringStrength = 0.18f;
    renderer.settings.shadowIntensity = 0.25f;
    renderer.settings.shadowStepSize = 0.04f;
    renderer.settings.shadowMaxSteps = 96;
    renderer.settings.shadowDensityScale = 0.08f;
    const Point3 lookAt{0.0f, 0.56f, 0.0f};
    const Point3 lookFrom = sideView ? Point3{1.8f, 0.56f, 0.0f}
                                     : Point3{0.0f, 0.56f, 1.8f};
    renderer.camera = Camera(1.0f, 36.0f, 1.8f, 0.0f, lookFrom,
                             lookAt, {0.0f, 1.0f, 0.0f});
    return renderer;
  };
  auto extrudedFrontRenderer = makeVolumeRenderer(extruded, false);
  auto extrudedSideRenderer = makeVolumeRenderer(extruded, true);
  auto depthFrontRenderer = makeVolumeRenderer(depthVarying, false);
  auto depthSideRenderer = makeVolumeRenderer(depthVarying, true);
  auto warpedFrontRenderer = makeVolumeRenderer(depthWarped, false);
  auto warpedSideRenderer = makeVolumeRenderer(depthWarped, true);
  const auto extrudedFront = extrudedFrontRenderer.render(160, 160);
  const auto extrudedSide = extrudedSideRenderer.render(160, 160);
  const auto depthFront = depthFrontRenderer.render(160, 160);
  const auto depthSide = depthSideRenderer.render(160, 160);
  const auto warpedFront = warpedFrontRenderer.render(160, 160);
  const auto warpedSide = warpedSideRenderer.render(160, 160);
  const auto savePng = [](const ImageBuffer& buffer, const QString& path,
                          const bool srgbEncode) {
    QImage image(buffer.width, buffer.height, QImage::Format_ARGB32);
    for (int y = 0; y < buffer.height; ++y) {
      auto* row = reinterpret_cast<QRgb*>(image.scanLine(y));
      for (int x = 0; x < buffer.width; ++x) {
        const auto offset = static_cast<std::size_t>((y * buffer.width + x) * 3);
        const auto encode = [srgbEncode](const std::uint8_t channel) {
          if (!srgbEncode) return static_cast<int>(channel);
          const float linear = static_cast<float>(channel) / 255.0f;
          return static_cast<int>(std::lround(
              std::pow(linear, 1.0f / 2.2f) * 255.0f));
        };
        row[x] = qRgba(encode(buffer.pixels[offset]),
                       encode(buffer.pixels[offset + 1]),
                       encode(buffer.pixels[offset + 2]), 255);
      }
    }
    return image.save(path, "PNG");
  };
  const auto outputDirectory = qEnvironmentVariable(
      "ARTIFACT_SMOKE_RUNTIME_OUTPUT_DIR");
  if (!outputDirectory.isEmpty()) {
    ASSERT_TRUE(QDir().mkpath(outputDirectory.toUtf8().constData()));
    EXPECT_TRUE(savePng(extrudedFront, QDir(outputDirectory).filePath(
        QStringLiteral("smoke_2p5d_extruded_front_linear.png")), false));
    EXPECT_TRUE(savePng(extrudedSide, QDir(outputDirectory).filePath(
        QStringLiteral("smoke_2p5d_extruded_side_linear.png")), false));
    EXPECT_TRUE(savePng(depthFront, QDir(outputDirectory).filePath(
        QStringLiteral("smoke_2p5d_depth_varying_front_linear.png")), false));
    EXPECT_TRUE(savePng(depthSide, QDir(outputDirectory).filePath(
        QStringLiteral("smoke_2p5d_depth_varying_side_linear.png")), false));
    EXPECT_TRUE(savePng(warpedFront, QDir(outputDirectory).filePath(
        QStringLiteral("smoke_2p5d_depth_warped_front_linear.png")), false));
    EXPECT_TRUE(savePng(warpedSide, QDir(outputDirectory).filePath(
        QStringLiteral("smoke_2p5d_depth_warped_side_linear.png")), false));
    EXPECT_TRUE(savePng(extrudedFront, QDir(outputDirectory).filePath(
        QStringLiteral("smoke_2p5d_extruded_front_srgb.png")), true));
    EXPECT_TRUE(savePng(extrudedSide, QDir(outputDirectory).filePath(
        QStringLiteral("smoke_2p5d_extruded_side_srgb.png")), true));
    EXPECT_TRUE(savePng(depthFront, QDir(outputDirectory).filePath(
        QStringLiteral("smoke_2p5d_depth_varying_front_srgb.png")), true));
    EXPECT_TRUE(savePng(depthSide, QDir(outputDirectory).filePath(
        QStringLiteral("smoke_2p5d_depth_varying_side_srgb.png")), true));
    EXPECT_TRUE(savePng(warpedFront, QDir(outputDirectory).filePath(
        QStringLiteral("smoke_2p5d_depth_warped_front_srgb.png")), true));
    EXPECT_TRUE(savePng(warpedSide, QDir(outputDirectory).filePath(
        QStringLiteral("smoke_2p5d_depth_warped_side_srgb.png")), true));
  }
  const auto imageStats = [](const ImageBuffer& image) {
    struct VolumeImageStats final {
      std::size_t occupiedPixels = 0;
      double luminanceSum = 0.0;
      double luminanceSquaredSum = 0.0;
      double luminanceStdDev = 0.0;
    } stats;
    for (int y = 0; y < image.height; ++y)
      for (int x = 0; x < image.width; ++x) {
        const auto offset = static_cast<std::size_t>((y * image.width + x) * 3);
        const double luma = 0.2126 * image.pixels[offset] +
            0.7152 * image.pixels[offset + 1] + 0.0722 * image.pixels[offset + 2];
        if (luma <= 10.0) continue;
        ++stats.occupiedPixels;
        stats.luminanceSum += luma;
        stats.luminanceSquaredSum += luma * luma;
      }
    if (stats.occupiedPixels > 0) {
      const double meanLuma = stats.luminanceSum / stats.occupiedPixels;
      stats.luminanceStdDev = std::sqrt(std::max(0.0,
          stats.luminanceSquaredSum / stats.occupiedPixels - meanLuma * meanLuma));
    }
    return stats;
  };
  const auto extrudedStats = imageStats(extrudedFront);
  const auto extrudedSideStats = imageStats(extrudedSide);
  const auto depthFrontStats = imageStats(depthFront);
  const auto depthSideStats = imageStats(depthSide);
  const auto warpedFrontStats = imageStats(warpedFront);
  const auto warpedSideStats = imageStats(warpedSide);
  GTEST_LOG_(INFO) << "2.5D CPU volume render fixed 2D density mass="
      << std::accumulate(snapshot.density.begin(), snapshot.density.end(), 0.0)
      << " extrudedFront occupied/luma/std=" << extrudedStats.occupiedPixels
      << '/' << extrudedStats.luminanceSum << '/' << extrudedStats.luminanceStdDev
      << " extrudedSide=" << extrudedSideStats.occupiedPixels << '/'
      << extrudedSideStats.luminanceSum << '/' << extrudedSideStats.luminanceStdDev
      << " depthVaryingFront=" << depthFrontStats.occupiedPixels << '/'
      << depthFrontStats.luminanceSum << '/' << depthFrontStats.luminanceStdDev
      << " depthVaryingSide=" << depthSideStats.occupiedPixels << '/'
      << depthSideStats.luminanceSum << '/' << depthSideStats.luminanceStdDev;
  GTEST_LOG_(INFO) << "depthWarpedFront=" << warpedFrontStats.occupiedPixels
      << '/' << warpedFrontStats.luminanceSum << '/'
      << warpedFrontStats.luminanceStdDev << " depthWarpedSide="
      << warpedSideStats.occupiedPixels << '/' << warpedSideStats.luminanceSum
      << '/' << warpedSideStats.luminanceStdDev;
  EXPECT_GT(extrudedStats.occupiedPixels, 100);
  EXPECT_GT(depthFrontStats.occupiedPixels, 100);
  EXPECT_GT(depthSideStats.occupiedPixels, 100);
  EXPECT_GT(warpedFrontStats.occupiedPixels, 100);
  EXPECT_GT(warpedSideStats.occupiedPixels, 100);
}

TEST(ArtifactSmokeRuntimeStandaloneTest,
     ComparesStaticTwoPointFiveDWithTimeEvolvingThreeDimensionalEmitter) {
  using namespace ArtifactCore;
  using namespace ArtifactCore::RayTrace;
  constexpr int width2D = 96;
  constexpr int height2D = 72;
  constexpr int frames = 36;
  constexpr int captureCount = 3;
  const auto index2D = [](const int x, const int y) {
    return static_cast<std::size_t>(x + width2D * y);
  };

  Artifact::LayerFluidRuntimeState runtime2D;
  Artifact::LayerSmokeRuntimeSettings smokeSettings;
  smokeSettings.gridWidth = width2D;
  smokeSettings.gridHeight = height2D;
  smokeSettings.solverIterations = 20;
  smokeSettings.emitterX = 0.5f;
  smokeSettings.emitterY = 0.84f;
  smokeSettings.emitterRadius = 0.08f;
  smokeSettings.emitterDensity = 0.6f;
  smokeSettings.emitterVelocityY = -1.2f;
  smokeSettings.emitterSwirl = 0.15f;
  smokeSettings.buoyancy = 0.0f;
  smokeSettings.vorticity = 0.1f;
  for (int frame = 0; frame < frames; ++frame)
    Artifact::advanceLayerSmokeRuntime(runtime2D, frame, 30.0, smokeSettings);
  const auto twoDSnapshot = runtime2D.fluidSolver_->snapshot();
  double twoDDensityMass = 0.0;
  for (const float value : twoDSnapshot.density) twoDDensityMass += value;

  PyroDomain domain;
  domain.min = {-0.75f, 0.0f, -0.5f};
  domain.max = {0.75f, 1.125f, 0.5f};
  domain.voxelSize = 1.0f / 32.0f;
  domain.fixedTimeStep = 1.0 / 60.0;
  domain.boundaryMode = PyroBoundaryMode::Open;
  domain.memoryBudgetBytes = 32u * 1024u * 1024u;
  const auto pyroResolution = domain.resolution();
  const VolumeResolution volumeResolution{
      pyroResolution.width, pyroResolution.height, pyroResolution.depth};
  const auto index3D = [volumeResolution](const int x, const int y, const int z) {
    return volumeResolution.indexOf(x, y, z);
  };

  std::vector<float> staticTwoPointFiveD(volumeResolution.cellCount(), 0.0f);
  constexpr float twoPi = 6.28318530718f;
  double staticVolumeMass = 0.0;
  for (int y = 0; y < volumeResolution.height; ++y) {
    const float sourceY = static_cast<float>(height2D - 1) -
        static_cast<float>(y) * static_cast<float>(height2D - 1) /
            static_cast<float>(volumeResolution.height - 1);
    for (int x = 0; x < volumeResolution.width; ++x) {
      const float sourceX = static_cast<float>(x) *
          static_cast<float>(width2D - 1) /
          static_cast<float>(volumeResolution.width - 1);
      const int x0 = static_cast<int>(std::floor(sourceX));
      const int y0 = static_cast<int>(std::floor(sourceY));
      const int x1 = std::min(x0 + 1, width2D - 1);
      const int y1 = std::min(y0 + 1, height2D - 1);
      const float tx = sourceX - x0;
      const float ty = sourceY - y0;
      const float base = std::lerp(std::lerp(
          twoDSnapshot.density[index2D(x0, y0)],
          twoDSnapshot.density[index2D(x1, y0)], tx), std::lerp(
          twoDSnapshot.density[index2D(x0, y1)],
          twoDSnapshot.density[index2D(x1, y1)], tx), ty);
      double envelopeSum = 0.0;
      for (int z = 0; z < volumeResolution.depth; ++z) {
        const float zNorm = (static_cast<float>(z) + 0.5f) /
            static_cast<float>(volumeResolution.depth) * 2.0f - 1.0f;
        const float envelope = std::exp(-2.5f * zNorm * zNorm);
        staticTwoPointFiveD[index3D(x, y, z)] = base * envelope;
        envelopeSum += envelope;
      }
      const float normalization = static_cast<float>(
          volumeResolution.depth / envelopeSum);
      for (int z = 0; z < volumeResolution.depth; ++z)
      {
        staticTwoPointFiveD[index3D(x, y, z)] *= normalization;
        staticVolumeMass += staticTwoPointFiveD[index3D(x, y, z)];
      }
    }
  }

  const auto makeVolumeRenderer = [&](float* density, const bool sideView,
                                      const float densityScale = 1.8f) {
    CPUVolumeRenderer renderer;
    VolumeAABB bounds;
    bounds.min = {domain.min.x, domain.min.y, domain.min.z};
    bounds.max = {domain.max.x, domain.max.y, domain.max.z};
    renderer.bounds = bounds;
    VolumeFieldSet fields;
    fields.density = {density, volumeResolution};
    renderer.setVolumeData(bounds, fields);
    renderer.settings.stepSize = 0.018f;
    renderer.settings.maxSteps = 256;
    renderer.settings.densityScale = densityScale;
    renderer.settings.useTemperatureEmission = false;
    renderer.settings.backgroundColor = {0.015f, 0.02f, 0.03f};
    renderer.settings.lightIntensity = 3.0f;
    renderer.settings.phaseAnisotropy = 0.25f;
    renderer.settings.multipleScatteringStrength = 0.18f;
    renderer.settings.shadowIntensity = 0.25f;
    renderer.settings.shadowStepSize = 0.04f;
    renderer.settings.shadowMaxSteps = 96;
    renderer.settings.shadowDensityScale = 0.08f;
    const Point3 lookAt{0.0f, 0.56f, 0.0f};
    const Point3 lookFrom = sideView ? Point3{2.4f, 0.56f, 0.0f}
                                     : Point3{0.0f, 0.56f, 2.4f};
    renderer.camera = Camera(1.0f, 36.0f, 2.4f, 0.0f, lookFrom,
                             lookAt, {0.0f, 1.0f, 0.0f});
    return renderer;
  };
  const auto outputDirectory = qEnvironmentVariable(
      "ARTIFACT_SMOKE_RUNTIME_OUTPUT_DIR");
  const auto savePng = [](const ImageBuffer& buffer, const QString& path) {
    QImage image(buffer.width, buffer.height, QImage::Format_ARGB32);
    for (int y = 0; y < buffer.height; ++y) {
      auto* row = reinterpret_cast<QRgb*>(image.scanLine(y));
      for (int x = 0; x < buffer.width; ++x) {
        const auto offset = static_cast<std::size_t>((y * buffer.width + x) * 3);
        const auto encode = [](const std::uint8_t channel) {
          const float linear = static_cast<float>(channel) / 255.0f;
          return static_cast<int>(std::lround(
              std::pow(linear, 1.0f / 2.2f) * 255.0f));
        };
        row[x] = qRgba(encode(buffer.pixels[offset]),
                       encode(buffer.pixels[offset + 1]),
                       encode(buffer.pixels[offset + 2]), 255);
      }
    }
    return image.save(path, "PNG");
  };
  struct RenderMetrics final {
    std::size_t occupied = 0;
    double luminance = 0.0;
    double centroidX = 0.0;
    double centroidY = 0.0;
  };
  const auto measure = [](const ImageBuffer& image) {
    RenderMetrics metrics;
    double weightedX = 0.0;
    double weightedY = 0.0;
    for (int y = 0; y < image.height; ++y)
      for (int x = 0; x < image.width; ++x) {
        const auto offset = static_cast<std::size_t>((y * image.width + x) * 3);
        const double luma = 0.2126 * image.pixels[offset] +
            0.7152 * image.pixels[offset + 1] +
            0.0722 * image.pixels[offset + 2];
        if (luma <= 10.0) continue;
        ++metrics.occupied;
        metrics.luminance += luma;
        weightedX += luma * x;
        weightedY += luma * y;
      }
    if (metrics.luminance > 0.0) {
      metrics.centroidX = weightedX / metrics.luminance;
      metrics.centroidY = weightedY / metrics.luminance;
    }
    return metrics;
  };
  const auto staticFront = makeVolumeRenderer(staticTwoPointFiveD.data(), false).render(160, 160);
  const auto staticSide = makeVolumeRenderer(staticTwoPointFiveD.data(), true).render(160, 160);
  const auto staticFrontMetrics = measure(staticFront);
  const auto staticSideMetrics = measure(staticSide);
  if (!outputDirectory.isEmpty()) {
    ASSERT_TRUE(QDir().mkpath(outputDirectory.toUtf8().constData()));
    EXPECT_TRUE(savePng(staticFront, QDir(outputDirectory).filePath(
        QStringLiteral("smoke_static_2p5d_front.png"))));
    EXPECT_TRUE(savePng(staticSide, QDir(outputDirectory).filePath(
        QStringLiteral("smoke_static_2p5d_side.png"))));
  }

  PyroSimulation simulation(domain);
  PyroSimulation curlSimulation(domain);
  PyroSimulationSettings pyroSettings;
  pyroSettings.dissipation = 0.035f;
  pyroSettings.coolingRate = 0.18f;
  pyroSettings.buoyancy = 0.7f;
  pyroSettings.vorticity = 0.45f;
  pyroSettings.pressureIterations = 8.0f;
  simulation.setSettings(pyroSettings);
  curlSimulation.setSettings(pyroSettings);
  PyroSourceState source;
  source.position = {0.0f, 0.18f, 0.0f};
  source.extent = {0.12f, 0.09f, 0.12f};
  source.velocity = {0.0f, 240.0f, 0.0f};
  source.density = 4.0f;
  source.temperature = 1.5f;
  source.enabled = true;
  const auto applyTemporalCurl = [&](PyroSimulation& target,
                                     const float timeSeconds,
                                     const float strength) {
    const auto resolution = domain.resolution();
    auto velocity = target.fields().velocityView();
    const auto densityField = target.fields().densityView();
    constexpr std::array<float, 3> wavelengths{0.36f, 0.18f, 0.09f};
    constexpr std::array<float, 3> weights{1.0f, 0.55f, 0.25f};
    constexpr float twoPi = 6.28318530718f;
    const float seedPhase = 17.0f * 0.173f;
    const float timePhase = timeSeconds * 0.9f;
    const auto potential = [&](int x, int y, int z) {
      x = std::clamp(x, 0, resolution.width - 1);
      y = std::clamp(y, 0, resolution.height - 1);
      z = std::clamp(z, 0, resolution.depth - 1);
      const float xw = domain.min.x + (static_cast<float>(x) + 0.5f) * domain.voxelSize;
      const float yw = domain.min.y + (static_cast<float>(y) + 0.5f) * domain.voxelSize;
      const float zw = domain.min.z + (static_cast<float>(z) + 0.5f) * domain.voxelSize;
      PyroVec3 result{};
      for (int band = 0; band < 3; ++band) {
        const float k = twoPi / wavelengths[band];
        const float phase = seedPhase + timePhase * (0.55f + band * 0.3f);
        const float amplitude = 0.11f * strength * weights[band];
        result.x += amplitude * std::sin(k *
            (0.31f * xw + yw + 0.19f * zw) + phase);
        result.y += amplitude * std::sin(k *
            (0.23f * xw + 0.37f * yw + zw) - phase * 0.7f);
        result.z += amplitude * std::sin(k *
            (xw - 0.29f * yw + 0.41f * zw) + phase * 0.43f);
      }
      const float rho = std::max(0.0f, densityField.at(x, y, z));
      const float envelope = rho / (rho + 0.12f);
      result.x *= envelope;
      result.y *= envelope;
      result.z *= envelope;
      return result;
    };
    const float invTwoVoxel = 0.5f / domain.voxelSize;
    for (int z = 0; z < resolution.depth; ++z)
      for (int y = 0; y < resolution.height; ++y)
        for (int x = 0; x < resolution.width; ++x) {
          const auto axp = potential(x + 1, y, z);
          const auto axm = potential(x - 1, y, z);
          const auto ayp = potential(x, y + 1, z);
          const auto aym = potential(x, y - 1, z);
          const auto azp = potential(x, y, z + 1);
          const auto azm = potential(x, y, z - 1);
          velocity.at(x, y, z).x +=
              (ayp.z - aym.z - azp.y + azm.y) * invTwoVoxel;
          velocity.at(x, y, z).y +=
              (azp.x - azm.x - axp.z + axm.z) * invTwoVoxel;
          velocity.at(x, y, z).z +=
              (axp.y - axm.y - ayp.x + aym.x) * invTwoVoxel;
        }
  };
  std::vector<float> previousDensity;
  std::vector<float> previousCurlDensity;
  double temporalCosineSum = 0.0;
  double temporalRelativeL1Sum = 0.0;
  double curlTemporalCosineSum = 0.0;
  double curlTemporalRelativeL1Sum = 0.0;
  int temporalComparisons = 0;
  int curlTemporalComparisons = 0;
  double finalBaselineLaplacianRms = 0.0;
  double finalCurlLaplacianRms = 0.0;
  double finalMass = 0.0;
  double finalCentroidY = 0.0;
  double finalCentroidZ = 0.0;
  std::size_t finalOccupiedFront = 0;
  std::size_t finalOccupiedSide = 0;
  for (int frame = 0; frame < frames; ++frame) {
    for (int substep = 0; substep < 2; ++substep) {
      const float timeSeconds = static_cast<float>(frame * 2 + substep) / 60.0f;
      applyTemporalCurl(curlSimulation, timeSeconds, 0.0625f);
      curlSimulation.setSources(std::span<const PyroSourceState>(&source, 1));
      curlSimulation.step(1.0 / 60.0);
      simulation.setSources(std::span<const PyroSourceState>(&source, 1));
      simulation.step(1.0 / 60.0);
    }
    const auto snapshot = simulation.snapshot();
    const auto currentDensity = simulation.fields().densityStorage();
    const auto currentCurlDensity = curlSimulation.fields().densityStorage();
    if (!previousDensity.empty()) {
      double dot = 0.0;
      double previousSquared = 0.0;
      double currentSquared = 0.0;
      double absoluteDelta = 0.0;
      double previousMass = 0.0;
      for (std::size_t i = 0; i < currentDensity.size(); ++i) {
        const double a = previousDensity[i];
        const double b = currentDensity[i];
        dot += a * b;
        previousSquared += a * a;
        currentSquared += b * b;
        absoluteDelta += std::abs(a - b);
        previousMass += std::abs(a);
      }
      temporalCosineSum += dot / std::max(1.0e-12,
          std::sqrt(previousSquared * currentSquared));
      temporalRelativeL1Sum += absoluteDelta / std::max(1.0e-12, previousMass);
      ++temporalComparisons;
    }
    previousDensity = currentDensity;
    if (!previousCurlDensity.empty()) {
      double dot = 0.0;
      double previousSquared = 0.0;
      double currentSquared = 0.0;
      double absoluteDelta = 0.0;
      double previousMass = 0.0;
      for (std::size_t i = 0; i < currentCurlDensity.size(); ++i) {
        const double a = previousCurlDensity[i];
        const double b = currentCurlDensity[i];
        dot += a * b;
        previousSquared += a * a;
        currentSquared += b * b;
        absoluteDelta += std::abs(a - b);
        previousMass += std::abs(a);
      }
      curlTemporalCosineSum += dot / std::max(1.0e-12,
          std::sqrt(previousSquared * currentSquared));
      curlTemporalRelativeL1Sum += absoluteDelta /
          std::max(1.0e-12, previousMass);
      ++curlTemporalComparisons;
    }
    previousCurlDensity = currentCurlDensity;
    if (frame < frames - captureCount) continue;
    double mass = 0.0;
    double weightedY = 0.0;
    double weightedZ = 0.0;
    for (int z = 0; z < pyroResolution.depth; ++z)
      for (int y = 0; y < pyroResolution.height; ++y)
        for (int x = 0; x < pyroResolution.width; ++x) {
          const float rho = snapshot.density.values[index3D(x, y, z)];
          mass += rho;
          weightedY += rho * y;
          weightedZ += rho * z;
        }
    finalMass = mass;
    finalCentroidY = mass > 0.0 ? weightedY / mass : 0.0;
    finalCentroidZ = mass > 0.0 ? weightedZ / mass : 0.0;
    const auto laplacianRms = [&](const std::vector<float>& field) {
      double sumSquared = 0.0;
      std::size_t count = 0;
      for (int z = 1; z + 1 < pyroResolution.depth; ++z)
        for (int y = 1; y + 1 < pyroResolution.height; ++y)
          for (int x = 1; x + 1 < pyroResolution.width; ++x) {
            const auto i = index3D(x, y, z);
            const double lap = 6.0 * field[i] - field[index3D(x - 1, y, z)] -
                field[index3D(x + 1, y, z)] - field[index3D(x, y - 1, z)] -
                field[index3D(x, y + 1, z)] - field[index3D(x, y, z - 1)] -
                field[index3D(x, y, z + 1)];
            sumSquared += lap * lap;
            ++count;
          }
      return count ? std::sqrt(sumSquared / count) : 0.0;
    };
    finalBaselineLaplacianRms = laplacianRms(currentDensity);
    finalCurlLaplacianRms = laplacianRms(currentCurlDensity);
    auto frontRenderer = makeVolumeRenderer(
        const_cast<float*>(snapshot.density.values.data()), false);
    auto sideRenderer = makeVolumeRenderer(
        const_cast<float*>(snapshot.density.values.data()), true);
    const auto frontImage = frontRenderer.render(160, 160);
    const auto sideImage = sideRenderer.render(160, 160);
    const int captureFrame = frame - (frames - captureCount);
    const auto frontMetrics = measure(frontImage);
    const auto sideMetrics = measure(sideImage);
    finalOccupiedFront = frontMetrics.occupied;
    finalOccupiedSide = sideMetrics.occupied;
    if (!outputDirectory.isEmpty()) {
      const QString suffix = QStringLiteral("%1").arg(captureFrame);
      EXPECT_TRUE(savePng(frontImage, QDir(outputDirectory).filePath(
          QStringLiteral("smoke_pyro3d_front_%1.png").arg(suffix))));
      EXPECT_TRUE(savePng(sideImage, QDir(outputDirectory).filePath(
          QStringLiteral("smoke_pyro3d_side_%1.png").arg(suffix))));
    }
    GTEST_LOG_(INFO) << "3D Pyro emitter frame=" << frame
        << " densityMass=" << mass << " centroidY/Z=" << finalCentroidY
        << '/' << finalCentroidZ << " frontOccupied/luma/centroid="
        << frontMetrics.occupied << '/' << frontMetrics.luminance << '/'
        << frontMetrics.centroidX << ',' << frontMetrics.centroidY
        << " sideOccupied/luma/centroid=" << sideMetrics.occupied << '/'
        << sideMetrics.luminance << '/' << sideMetrics.centroidX << ','
        << sideMetrics.centroidY;
    if (captureFrame == captureCount - 1) {
      auto denseFrontRenderer = makeVolumeRenderer(
          const_cast<float*>(snapshot.density.values.data()), false, 7.2f);
      auto denseSideRenderer = makeVolumeRenderer(
          const_cast<float*>(snapshot.density.values.data()), true, 7.2f);
      const auto denseFront = denseFrontRenderer.render(160, 160);
      const auto denseSide = denseSideRenderer.render(160, 160);
      const auto denseFrontMetrics = measure(denseFront);
      const auto denseSideMetrics = measure(denseSide);
      GTEST_LOG_(INFO) << "3D final densityScale=7.2 front/side occupied/luma="
          << denseFrontMetrics.occupied << '/' << denseFrontMetrics.luminance
          << ' ' << denseSideMetrics.occupied << '/' << denseSideMetrics.luminance;
      if (!outputDirectory.isEmpty()) {
        EXPECT_TRUE(savePng(denseFront, QDir(outputDirectory).filePath(
            QStringLiteral("smoke_pyro3d_front_densityScale72.png"))));
        EXPECT_TRUE(savePng(denseSide, QDir(outputDirectory).filePath(
            QStringLiteral("smoke_pyro3d_side_densityScale72.png"))));
      }
    }
    const auto curlSnapshot = curlSimulation.snapshot();
    auto curlFrontRenderer = makeVolumeRenderer(
        const_cast<float*>(curlSnapshot.density.values.data()), false, 7.2f);
    auto curlSideRenderer = makeVolumeRenderer(
        const_cast<float*>(curlSnapshot.density.values.data()), true, 7.2f);
    const auto curlFront = curlFrontRenderer.render(160, 160);
    const auto curlSide = curlSideRenderer.render(160, 160);
    const auto curlFrontMetrics = measure(curlFront);
    const auto curlSideMetrics = measure(curlSide);
    if (!outputDirectory.isEmpty()) {
      const QString suffix = QStringLiteral("%1").arg(captureFrame);
      EXPECT_TRUE(savePng(curlFront, QDir(outputDirectory).filePath(
          QStringLiteral("smoke_pyro3d_curl_front_%1.png").arg(suffix))));
      EXPECT_TRUE(savePng(curlSide, QDir(outputDirectory).filePath(
          QStringLiteral("smoke_pyro3d_curl_side_%1.png").arg(suffix))));
    }
    GTEST_LOG_(INFO) << "3D Pyro curl strength=.0625 frame=" << frame
        << " densityMass=" << std::accumulate(
            curlSnapshot.density.values.begin(),
            curlSnapshot.density.values.end(), 0.0)
        << " densityLaplacianRms=" << finalCurlLaplacianRms
        << " front/side occupied/luma=" << curlFrontMetrics.occupied << '/'
        << curlFrontMetrics.luminance << ' ' << curlSideMetrics.occupied << '/'
        << curlSideMetrics.luminance;
  }
  GTEST_LOG_(INFO) << "2D mass=" << twoDDensityMass
      << " static2p5D volume mass=" << staticVolumeMass
      << " 2D-static-2p5D front/side occupied/luma="
      << staticFrontMetrics.occupied << '/' << staticFrontMetrics.luminance
      << ' ' << staticSideMetrics.occupied << '/' << staticSideMetrics.luminance
      << " 3D-final mass/centroidY/Z=" << finalMass << '/'
      << finalCentroidY << '/' << finalCentroidZ << " temporalCosineMean/L1Mean="
      << temporalCosineSum / std::max(1, temporalComparisons) << '/'
      << temporalRelativeL1Sum / std::max(1, temporalComparisons)
      << " curlTemporalCosineMean/L1Mean="
      << curlTemporalCosineSum / std::max(1, curlTemporalComparisons) << '/'
      << curlTemporalRelativeL1Sum / std::max(1, curlTemporalComparisons)
      << " baseline/curlFinalLaplacianRms=" << finalBaselineLaplacianRms
      << '/' << finalCurlLaplacianRms
      << " front/side occupied=" << finalOccupiedFront << '/'
      << finalOccupiedSide;
  EXPECT_GT(finalMass, 0.0);
  EXPECT_GT(temporalComparisons, 0);
  EXPECT_GT(curlTemporalComparisons, 0);
  EXPECT_GT(temporalRelativeL1Sum, 0.0);
  EXPECT_GT(staticFrontMetrics.occupied, 100u);
  EXPECT_GT(staticSideMetrics.occupied, 100u);
  EXPECT_GT(finalOccupiedFront, 100u);
  EXPECT_GT(finalOccupiedSide, 100u);
}

TEST(ArtifactSmokeRuntimeStandaloneTest,
     EvolvesProductionTwoDimensionalSmokeStateInsideDepthResolvedPyro) {
  using namespace ArtifactCore;
  using namespace ArtifactCore::RayTrace;
  constexpr int sourceWidth = 96;
  constexpr int sourceHeight = 72;
  constexpr int sourceFrames = 28;
  constexpr int captureEverySteps = 8;

  Artifact::LayerFluidRuntimeState sourceRuntime;
  Artifact::LayerSmokeRuntimeSettings sourceSettings;
  sourceSettings.gridWidth = sourceWidth;
  sourceSettings.gridHeight = sourceHeight;
  sourceSettings.solverIterations = 20;
  sourceSettings.emitterX = 0.5f;
  sourceSettings.emitterY = 0.84f;
  sourceSettings.emitterRadius = 0.08f;
  sourceSettings.emitterDensity = 0.6f;
  sourceSettings.emitterVelocityY = -1.2f;
  sourceSettings.emitterSwirl = 0.15f;
  sourceSettings.buoyancy = 0.0f;
  sourceSettings.vorticity = 0.1f;
  for (int frame = 0; frame <= sourceFrames; ++frame)
    Artifact::advanceLayerSmokeRuntime(sourceRuntime, frame, 30.0,
                                       sourceSettings);
  ASSERT_NE(sourceRuntime.fluidSolver_, nullptr);
  const auto source = sourceRuntime.fluidSolver_->snapshot();

  PyroDomain domain;
  domain.min = {-0.75f, 0.0f, -0.5f};
  domain.max = {0.75f, 1.125f, 0.5f};
  domain.voxelSize = 1.0f / 32.0f;
  domain.fixedTimeStep = 1.0 / 60.0;
  domain.boundaryMode = PyroBoundaryMode::Open;
  domain.memoryBudgetBytes = 32u * 1024u * 1024u;
  const auto pyroResolution = domain.resolution();
  const VolumeResolution volumeResolution{pyroResolution.width,
      pyroResolution.height, pyroResolution.depth};
  PyroSimulation baselineSimulation(domain);
  PyroSimulation depthRollSimulation(domain);
  PyroSimulation buoyantSimulation(domain);
  PyroSimulation highProjectionSimulation(domain);
  PyroSimulationSettings settings;
  settings.dissipation = 0.035f;
  settings.coolingRate = 0.0f;
  settings.buoyancy = 0.0f;
  settings.vorticity = 0.0f;
  settings.pressureIterations = 24.0f;
  baselineSimulation.setSettings(settings);
  depthRollSimulation.setSettings(settings);
  auto highProjectionSettings = settings;
  highProjectionSettings.pressureIterations = 72.0f;
  highProjectionSimulation.setSettings(highProjectionSettings);
  auto buoyantSettings = settings;
  buoyantSettings.buoyancy = 0.7f;
  buoyantSimulation.setSettings(buoyantSettings);

  const auto sourceIndex = [](const int x, const int y) {
    return static_cast<std::size_t>(x + sourceWidth * y);
  };
  const auto sampleSource = [&](const NamedVector<float>& field,
                                const float x, const float y) {
    const float sx = std::clamp(x, 0.0f,
                                static_cast<float>(sourceWidth - 1));
    const float sy = std::clamp(y, 0.0f,
                                static_cast<float>(sourceHeight - 1));
    const int x0 = static_cast<int>(sx);
    const int y0 = static_cast<int>(sy);
    const int x1 = std::min(x0 + 1, sourceWidth - 1);
    const int y1 = std::min(y0 + 1, sourceHeight - 1);
    const float tx = sx - static_cast<float>(x0);
    const float ty = sy - static_cast<float>(y0);
    return std::lerp(std::lerp(field[sourceIndex(x0, y0)],
                               field[sourceIndex(x1, y0)], tx),
                     std::lerp(field[sourceIndex(x0, y1)],
                               field[sourceIndex(x1, y1)], tx), ty);
  };

  std::vector<float> depthWeights(
      static_cast<std::size_t>(pyroResolution.depth), 0.0f);
  for (int z = 0; z < pyroResolution.depth; ++z) {
    const float zNorm = (static_cast<float>(z) + 0.5f) /
        static_cast<float>(pyroResolution.depth) * 2.0f - 1.0f;
    depthWeights[static_cast<std::size_t>(z)] =
        std::exp(-2.5f * zNorm * zNorm);
  }
  const auto initializeFromProduction2D = [&](PyroSimulation& target) {
    auto density = target.fields().densityView();
    auto velocity = target.fields().velocityView();
    for (int z = 0; z < pyroResolution.depth; ++z)
      for (int y = 0; y < pyroResolution.height; ++y)
        for (int x = 0; x < pyroResolution.width; ++x) {
        const float sourceX = static_cast<float>(x) *
            static_cast<float>(sourceWidth - 1) /
            static_cast<float>(pyroResolution.width - 1);
        const float sourceY = static_cast<float>(sourceHeight - 1) -
            static_cast<float>(y) *
            static_cast<float>(sourceHeight - 1) /
            static_cast<float>(pyroResolution.height - 1);
        const float layerDensity = sampleSource(source.density, sourceX,
                                                sourceY);
        const auto cell = static_cast<std::size_t>(
            volumeResolution.indexOf(x, y, z));
        density.values[cell] = layerDensity *
            depthWeights[static_cast<std::size_t>(z)];
        velocity.values[cell] = {
            sampleSource(source.velocityX, sourceX, sourceY) *
                static_cast<float>(pyroResolution.width - 2),
            -sampleSource(source.velocityY, sourceX, sourceY) *
                static_cast<float>(pyroResolution.height - 2),
            0.0f};
      }
  };
  initializeFromProduction2D(baselineSimulation);
  initializeFromProduction2D(depthRollSimulation);

  const auto projectInitialVelocity = [&](PyroSimulation& target,
                                          const int projectionIterations) {
    auto velocity = target.fields().velocityView();
    auto pressure = target.fields().pressureView();
    auto divergence = target.fields().divergenceView();
    std::vector<float> nextPressure(volumeResolution.cellCount(), 0.0f);
    for (int z = 0; z < pyroResolution.depth; ++z)
      for (int y = 0; y < pyroResolution.height; ++y)
        for (int x = 0; x < pyroResolution.width; ++x) {
          const int xm = std::max(0, x - 1);
          const int xp = std::min(pyroResolution.width - 1, x + 1);
          const int ym = std::max(0, y - 1);
          const int yp = std::min(pyroResolution.height - 1, y + 1);
          const int zm = std::max(0, z - 1);
          const int zp = std::min(pyroResolution.depth - 1, z + 1);
          const auto vxm = velocity.at(xm, y, z);
          const auto vxp = velocity.at(xp, y, z);
          const auto vym = velocity.at(x, ym, z);
          const auto vyp = velocity.at(x, yp, z);
          const auto vzm = velocity.at(x, y, zm);
          const auto vzp = velocity.at(x, y, zp);
          divergence.at(x, y, z) = ((vxp.x - vxm.x) + (vyp.y - vym.y) +
                                    (vzp.z - vzm.z)) * 0.5f;
          pressure.at(x, y, z) = 0.0f;
        }
    for (int iteration = 0; iteration < projectionIterations; ++iteration) {
      for (int z = 0; z < pyroResolution.depth; ++z)
        for (int y = 0; y < pyroResolution.height; ++y)
          for (int x = 0; x < pyroResolution.width; ++x) {
            const int xm = std::max(0, x - 2);
            const int xp = std::min(pyroResolution.width - 1, x + 2);
            const int ym = std::max(0, y - 2);
            const int yp = std::min(pyroResolution.height - 1, y + 2);
            const int zm = std::max(0, z - 2);
            const int zp = std::min(pyroResolution.depth - 1, z + 2);
            const float neighborSum = pressure.at(xm, y, z) +
                pressure.at(xp, y, z) + pressure.at(x, ym, z) +
                pressure.at(x, yp, z) + pressure.at(x, y, zm) +
                pressure.at(x, y, zp);
            nextPressure[volumeResolution.indexOf(x, y, z)] =
                (neighborSum - 4.0f * divergence.at(x, y, z)) / 6.0f;
          }
      for (int z = 0; z < pyroResolution.depth; ++z)
        for (int y = 0; y < pyroResolution.height; ++y)
          for (int x = 0; x < pyroResolution.width; ++x)
            pressure.at(x, y, z) =
                nextPressure[volumeResolution.indexOf(x, y, z)];
    }
    for (int z = 0; z < pyroResolution.depth; ++z)
      for (int y = 0; y < pyroResolution.height; ++y)
        for (int x = 0; x < pyroResolution.width; ++x) {
          const int xm = std::max(0, x - 1);
          const int xp = std::min(pyroResolution.width - 1, x + 1);
          const int ym = std::max(0, y - 1);
          const int yp = std::min(pyroResolution.height - 1, y + 1);
          const int zm = std::max(0, z - 1);
          const int zp = std::min(pyroResolution.depth - 1, z + 1);
          velocity.at(x, y, z).x -= 0.5f *
              (pressure.at(xp, y, z) - pressure.at(xm, y, z));
          velocity.at(x, y, z).y -= 0.5f *
              (pressure.at(x, yp, z) - pressure.at(x, ym, z));
          velocity.at(x, y, z).z -= 0.5f *
              (pressure.at(x, y, zp) - pressure.at(x, y, zm));
        }
  };
  const auto measureProjectionDivergence = [&](const PyroSimulation& target) {
    const auto velocity = target.fields().velocityView();
    const auto density = target.fields().densityView();
    double divergenceSquared = 0.0;
    double densityWeight = 0.0;
    for (int z = 1; z + 1 < pyroResolution.depth; ++z)
      for (int y = 1; y + 1 < pyroResolution.height; ++y)
        for (int x = 1; x + 1 < pyroResolution.width; ++x) {
          const double rho = std::max(0.0f, density.at(x, y, z));
          if (rho <= 1.0e-5) continue;
          const auto xp = velocity.at(x + 1, y, z);
          const auto xm = velocity.at(x - 1, y, z);
          const auto yp = velocity.at(x, y + 1, z);
          const auto ym = velocity.at(x, y - 1, z);
          const auto zp = velocity.at(x, y, z + 1);
          const auto zm = velocity.at(x, y, z - 1);
          const double divergence = 0.5 * ((xp.x - xm.x) +
              (yp.y - ym.y) + (zp.z - zm.z));
          divergenceSquared += rho * divergence * divergence;
          densityWeight += rho;
        }
    return densityWeight > 0.0
        ? std::sqrt(divergenceSquared / densityWeight) : 0.0;
  };
  constexpr std::array<int, 7> projectionSweepIterations{
      4, 8, 16, 24, 36, 48, 72};
  for (const int iterations : projectionSweepIterations) {
    PyroSimulation sweepSimulation(domain);
    sweepSimulation.setSettings(settings);
    initializeFromProduction2D(sweepSimulation);
    const double before = measureProjectionDivergence(sweepSimulation);
    projectInitialVelocity(sweepSimulation, iterations);
    const double after = measureProjectionDivergence(sweepSimulation);
    GTEST_LOG_(INFO) << "depth-projection sweep iterations=" << iterations
        << " density-weighted divergence RMS=" << before << " -> " << after
        << " ratio=" << after / std::max(1.0e-12, before);
    EXPECT_TRUE(std::isfinite(after));
    EXPECT_LT(after, before);
  }
  projectInitialVelocity(baselineSimulation, 24);
  projectInitialVelocity(depthRollSimulation, 24);
  initializeFromProduction2D(highProjectionSimulation);
  projectInitialVelocity(highProjectionSimulation, 72);
  initializeFromProduction2D(buoyantSimulation);
  auto buoyantDensity = buoyantSimulation.fields().densityView();
  auto buoyantTemperature = buoyantSimulation.fields().temperatureView();
  for (std::size_t i = 0; i < buoyantDensity.values.size(); ++i)
    buoyantTemperature.values[i] = buoyantDensity.values[i] * 1.5f;
  projectInitialVelocity(buoyantSimulation, 24);

  const auto outputDirectory = qEnvironmentVariable(
      "ARTIFACT_SMOKE_RUNTIME_OUTPUT_DIR");
  const auto renderView = [&](const PyroScalarFieldView density,
                              const bool sideView) {
    CPUVolumeRenderer renderer;
    VolumeAABB bounds;
    bounds.min = {domain.min.x, domain.min.y, domain.min.z};
    bounds.max = {domain.max.x, domain.max.y, domain.max.z};
    renderer.bounds = bounds;
    VolumeFieldSet fields;
    fields.density = {const_cast<float*>(density.values.data()),
                      volumeResolution};
    renderer.setVolumeData(bounds, fields);
    renderer.settings.stepSize = 0.018f;
    renderer.settings.maxSteps = 256;
    renderer.settings.densityScale = 7.2f;
    renderer.settings.useTemperatureEmission = false;
    renderer.settings.backgroundColor = {0.015f, 0.02f, 0.03f};
    renderer.settings.lightIntensity = 3.0f;
    renderer.settings.phaseAnisotropy = 0.25f;
    renderer.settings.multipleScatteringStrength = 0.18f;
    renderer.settings.shadowIntensity = 0.25f;
    renderer.settings.shadowStepSize = 0.04f;
    renderer.settings.shadowMaxSteps = 96;
    renderer.settings.shadowDensityScale = 0.08f;
    const Point3 lookAt{0.0f, 0.56f, 0.0f};
    const Point3 lookFrom = sideView ? Point3{2.4f, 0.56f, 0.0f}
                                     : Point3{0.0f, 0.56f, 2.4f};
    renderer.camera = Camera(1.0f, 36.0f, 2.4f, 0.0f, lookFrom,
                             lookAt, {0.0f, 1.0f, 0.0f});
    return renderer.render(160, 160);
  };
  const auto savePng = [](const ImageBuffer& buffer, const QString& path) {
    QImage image(buffer.width, buffer.height, QImage::Format_ARGB32);
    for (int y = 0; y < buffer.height; ++y) {
      auto* row = reinterpret_cast<QRgb*>(image.scanLine(y));
      for (int x = 0; x < buffer.width; ++x) {
        const auto offset = static_cast<std::size_t>(
            (y * buffer.width + x) * 3);
        const auto encode = [](const std::uint8_t channel) {
          const float linear = static_cast<float>(channel) / 255.0f;
          return static_cast<int>(std::lround(
              std::pow(linear, 1.0f / 2.2f) * 255.0f));
        };
        row[x] = qRgba(encode(buffer.pixels[offset]),
                       encode(buffer.pixels[offset + 1]),
                       encode(buffer.pixels[offset + 2]), 255);
      }
    }
    return image.save(path, "PNG");
  };
  const auto capture = [&](const PyroScalarFieldView density,
                           const char* branch, const int step) {
    const auto front = renderView(density, false);
    const auto side = renderView(density, true);
    if (outputDirectory.isEmpty()) return;
    const auto captureDirectory = QDir(outputDirectory).filePath(
        QStringLiteral("depth-evolved-2d-state-rnd"));
    ASSERT_TRUE(QDir().mkpath(captureDirectory));
    const auto label = QStringLiteral("%1_step_%2").arg(
        QString::fromLatin1(branch)).arg(step, 3, 10, QLatin1Char('0'));
    EXPECT_TRUE(savePng(front, QDir(captureDirectory).filePath(
        label + QStringLiteral("_front.png"))));
    EXPECT_TRUE(savePng(side, QDir(captureDirectory).filePath(
        label + QStringLiteral("_side.png"))));
  };
  const auto measureField = [&](const PyroScalarFieldView density) {
    double mass = 0.0;
    double weightedY = 0.0;
    double weightedZ = 0.0;
    for (int z = 0; z < pyroResolution.depth; ++z)
      for (int y = 0; y < pyroResolution.height; ++y)
        for (int x = 0; x < pyroResolution.width; ++x) {
          const double value = density.at(x, y, z);
          mass += value;
          weightedY += value * y;
          weightedZ += value * z;
        }
    return std::array<double, 3>{mass,
        mass > 0.0 ? weightedY / mass : 0.0,
        mass > 0.0 ? weightedZ / mass : 0.0};
  };

  const auto measureVelocity = [&](const PyroSimulation& target) {
    const auto velocity = target.fields().velocityView();
    const auto density = target.fields().densityView();
    double weightedSpeedSquared = 0.0;
    double densityWeight = 0.0;
    double divergenceSquared = 0.0;
    std::size_t divergenceCount = 0;
    for (int z = 1; z + 1 < pyroResolution.depth; ++z)
      for (int y = 1; y + 1 < pyroResolution.height; ++y)
        for (int x = 1; x + 1 < pyroResolution.width; ++x) {
          const double rho = std::max(0.0f, density.at(x, y, z));
          if (rho <= 1.0e-5) continue;
          const auto center = velocity.at(x, y, z);
          weightedSpeedSquared += rho * (center.x * center.x +
              center.y * center.y + center.z * center.z);
          densityWeight += rho;
          const auto xp = velocity.at(x + 1, y, z);
          const auto xm = velocity.at(x - 1, y, z);
          const auto yp = velocity.at(x, y + 1, z);
          const auto ym = velocity.at(x, y - 1, z);
          const auto zp = velocity.at(x, y, z + 1);
          const auto zm = velocity.at(x, y, z - 1);
          const double divergence = 0.5 * ((xp.x - xm.x) +
              (yp.y - ym.y) + (zp.z - zm.z));
          divergenceSquared += divergence * divergence;
          ++divergenceCount;
        }
    return std::array<double, 2>{
        densityWeight > 0.0
            ? std::sqrt(weightedSpeedSquared / densityWeight) : 0.0,
        divergenceCount > 0
            ? std::sqrt(divergenceSquared / divergenceCount) : 0.0};
  };

  // Add a deterministic, density-supported curl field. Its velocity RMS is
  // matched to a fraction of the 2D-derived base flow so this is not merely a
  // speed increase. The curl is evaluated with the same centered differences
  // used by the discrete divergence diagnostic below.
  auto rollDensity = depthRollSimulation.fields().densityView();
  auto rollVelocity = depthRollSimulation.fields().velocityView();
  const auto potential = [&](const int sampleX, const int sampleY,
                             const int sampleZ) {
    const int x = std::clamp(sampleX, 0, pyroResolution.width - 1);
    const int y = std::clamp(sampleY, 0, pyroResolution.height - 1);
    const int z = std::clamp(sampleZ, 0, pyroResolution.depth - 1);
    const double rho = std::max(0.0f, rollDensity.at(x, y, z));
    const float gate = static_cast<float>(rho / (rho + 0.08));
    constexpr float twoPi = 6.28318530718f;
    return PyroVec3{
        gate * std::sin(twoPi * (static_cast<float>(y) / 17.0f +
                                 static_cast<float>(z) / 23.0f) + 0.37f),
        gate * std::sin(twoPi * (static_cast<float>(z) / 19.0f +
                                 static_cast<float>(x) / 29.0f) - 0.81f),
        gate * std::sin(twoPi * (static_cast<float>(x) / 23.0f -
                                 static_cast<float>(y) / 31.0f) + 1.13f)};
  };
  std::vector<PyroVec3> curlVelocity(volumeResolution.cellCount());
  double baseWeightedSpeedSquared = 0.0;
  double curlWeightedSpeedSquared = 0.0;
  double densityWeight = 0.0;
  constexpr float inverseTwo = 0.5f;
  for (int z = 0; z < pyroResolution.depth; ++z)
    for (int y = 0; y < pyroResolution.height; ++y)
      for (int x = 0; x < pyroResolution.width; ++x) {
        const auto cell = volumeResolution.indexOf(x, y, z);
        const double rho = std::max(0.0f, rollDensity.at(x, y, z));
        const auto axp = potential(x + 1, y, z);
        const auto axm = potential(x - 1, y, z);
        const auto ayp = potential(x, y + 1, z);
        const auto aym = potential(x, y - 1, z);
        const auto azp = potential(x, y, z + 1);
        const auto azm = potential(x, y, z - 1);
        const PyroVec3 curl{
            ((ayp.z - aym.z) - (azp.y - azm.y)) * inverseTwo,
            ((azp.x - azm.x) - (axp.z - axm.z)) * inverseTwo,
            ((axp.y - axm.y) - (ayp.x - aym.x)) * inverseTwo};
        curlVelocity[cell] = curl;
        if (x <= 0 || x + 1 >= pyroResolution.width ||
            y <= 0 || y + 1 >= pyroResolution.height ||
            z <= 0 || z + 1 >= pyroResolution.depth || rho <= 1.0e-5)
          continue;
        const auto base = rollVelocity.at(x, y, z);
        baseWeightedSpeedSquared += rho * (base.x * base.x + base.y * base.y +
                                            base.z * base.z);
        curlWeightedSpeedSquared += rho * (curl.x * curl.x + curl.y * curl.y +
                                            curl.z * curl.z);
        densityWeight += rho;
      }
  const double baseVelocityRms = densityWeight > 0.0
      ? std::sqrt(baseWeightedSpeedSquared / densityWeight) : 0.0;
  const double curlVelocityRms = densityWeight > 0.0
      ? std::sqrt(curlWeightedSpeedSquared / densityWeight) : 0.0;
  const float curlScale = curlVelocityRms > 1.0e-12
      ? static_cast<float>(0.15 * baseVelocityRms / curlVelocityRms) : 0.0f;
  double curlDivergenceSquared = 0.0;
  double curlDivergenceWeight = 0.0;
  for (int z = 1; z + 1 < pyroResolution.depth; ++z)
    for (int y = 1; y + 1 < pyroResolution.height; ++y)
      for (int x = 1; x + 1 < pyroResolution.width; ++x) {
        const double rho = std::max(0.0f, rollDensity.at(x, y, z));
        if (rho <= 1.0e-5) continue;
        const auto xp = curlVelocity[volumeResolution.indexOf(x + 1, y, z)];
        const auto xm = curlVelocity[volumeResolution.indexOf(x - 1, y, z)];
        const auto yp = curlVelocity[volumeResolution.indexOf(x, y + 1, z)];
        const auto ym = curlVelocity[volumeResolution.indexOf(x, y - 1, z)];
        const auto zp = curlVelocity[volumeResolution.indexOf(x, y, z + 1)];
        const auto zm = curlVelocity[volumeResolution.indexOf(x, y, z - 1)];
        const double divergence = 0.5 * ((xp.x - xm.x) + (yp.y - ym.y) +
                                          (zp.z - zm.z));
        curlDivergenceSquared += rho * divergence * divergence;
        curlDivergenceWeight += rho;
      }
  const double curlDivergenceRms = curlDivergenceWeight > 0.0
      ? std::sqrt(curlDivergenceSquared / curlDivergenceWeight) : 0.0;
  for (int z = 0; z < pyroResolution.depth; ++z)
    for (int y = 0; y < pyroResolution.height; ++y)
      for (int x = 0; x < pyroResolution.width; ++x) {
        const auto cell = volumeResolution.indexOf(x, y, z);
        rollVelocity.at(x, y, z).x += curlVelocity[cell].x * curlScale;
        rollVelocity.at(x, y, z).y += curlVelocity[cell].y * curlScale;
        rollVelocity.at(x, y, z).z += curlVelocity[cell].z * curlScale;
      }

  const auto runBranch = [&](const char* label, PyroSimulation& target) {
    auto density = target.fields().densityView();
    auto previous = std::vector<float>(density.values.begin(),
                                       density.values.end());
    const auto initialMetrics = measureField(density);
    ASSERT_GT(initialMetrics[0], 0.0);
    const auto initialVelocity = measureVelocity(target);
    GTEST_LOG_(INFO) << "depth-evolved branch=" << label
        << " initial mass=" << initialMetrics[0] << " centroidY/Z="
        << initialMetrics[1] << '/' << initialMetrics[2]
        << " velocityRms/divergenceRms=" << initialVelocity[0] << '/'
        << initialVelocity[1];
    capture(density, label, 0);
    for (int step = 1; step <= captureEverySteps * 3; ++step) {
      target.step(1.0 / 60.0);
      if (step % captureEverySteps != 0) continue;
      const auto current = density.values;
      double dot = 0.0;
      double previousSquared = 0.0;
      double currentSquared = 0.0;
      double absoluteDelta = 0.0;
      double previousMass = 0.0;
      for (std::size_t i = 0; i < current.size(); ++i) {
        const double a = previous[i];
        const double b = current[i];
        dot += a * b;
        previousSquared += a * a;
        currentSquared += b * b;
        absoluteDelta += std::abs(a - b);
        previousMass += std::abs(a);
      }
      const auto metrics = measureField(density);
      const auto velocityMetrics = measureVelocity(target);
      const double cosine = dot / std::max(1.0e-12,
          std::sqrt(previousSquared * currentSquared));
      const double relativeL1 = absoluteDelta /
          std::max(1.0e-12, previousMass);
      GTEST_LOG_(INFO) << "depth-evolved branch=" << label << " step="
          << step << " mass=" << metrics[0] << " centroidY/Z="
          << metrics[1] << '/' << metrics[2] << " intervalCosine/L1="
          << cosine << '/' << relativeL1 << " velocityRms/divergenceRms="
          << velocityMetrics[0] << '/' << velocityMetrics[1];
      EXPECT_GT(metrics[0], 0.0);
      EXPECT_TRUE(std::isfinite(cosine));
      EXPECT_TRUE(std::isfinite(relativeL1));
      capture(density, label, step);
      previous.assign(current.begin(), current.end());
    }
  };
  const auto baselineVelocity = measureVelocity(baselineSimulation);
  const auto depthRollVelocity = measureVelocity(depthRollSimulation);
  GTEST_LOG_(INFO) << "depth-roll calibration base/curlRms="
      << baseVelocityRms << '/' << curlVelocityRms
      << " scale=" << curlScale << " curl-divergenceRms="
      << curlDivergenceRms << " initial divergence base/roll="
      << baselineVelocity[1] << '/' << depthRollVelocity[1];
  EXPECT_NEAR(curlVelocityRms * curlScale,
              baseVelocityRms * 0.15, baseVelocityRms * 0.001 + 1.0e-5);
  EXPECT_LT(curlDivergenceRms, 1.0e-5);
  runBranch("base", baselineSimulation);
  runBranch("projection-72", highProjectionSimulation);
  runBranch("depth-roll", depthRollSimulation);
  runBranch("temperature-buoyant", buoyantSimulation);
}

TEST(ArtifactSmokeRuntimeStandaloneTest,
     ProductionRuntimeHonorsSmokeDensityDissipationRate) {
  const auto runPulse = [](const float dissipationPerSecond) {
    Artifact::LayerFluidRuntimeState state;
    Artifact::LayerSmokeRuntimeSettings settings;
    settings.gridWidth = kGridWidth;
    settings.gridHeight = kGridHeight;
    settings.emitterX = 0.5f;
    settings.emitterY = 0.84f;
    settings.emitterRadius = 0.08f;
    settings.emitterDensity = 0.6f;
    settings.emitterVelocityY = -1.2f;
    settings.buoyancy = 0.0f;
    settings.densityDissipationPerSecond = dissipationPerSecond;
    for (int frame = 0; frame < 60; ++frame) {
      settings.emitterDensity = frame < 12 ? 0.6f : 0.0f;
      Artifact::advanceLayerSmokeRuntime(state, frame, 30.0, settings);
    }
    double densityMass = 0.0;
    for (int y = 0; y < state.fluidSolver_->height(); ++y) {
      for (int x = 0; x < state.fluidSolver_->width(); ++x) {
        densityMass += state.fluidSolver_->getDensity(x, y);
      }
    }
    return densityMass;
  };

  const double slowDecayMass = runPulse(0.5f);
  const double defaultDecayMass = runPulse(2.5f);
  GTEST_LOG_(INFO) << "Production 2D smoke pulse density mass at frame 60: 0.5/s="
                   << slowDecayMass << " 2.5/s=" << defaultDecayMass;
  EXPECT_GT(slowDecayMass, defaultDecayMass * 10.0);
}

TEST(ArtifactSmokeRuntimeStandaloneTest,
     ComparesResolutionScaledThreeDimensionalSmoke) {
  using namespace ArtifactCore;
  using namespace ArtifactCore::RayTrace;
  struct ResolutionCondition final {
    float voxelSize;
    int pressureIterations;
    const char* label;
  };
  constexpr std::array<ResolutionCondition, 11> conditions{{
      {1.0f / 24.0f, 8, "voxel24-p8"},
      {1.0f / 24.0f, 18, "voxel24-p18"},
      {1.0f / 24.0f, 36, "voxel24-p36"},
      {1.0f / 32.0f, 8, "voxel32-p8"},
      {1.0f / 32.0f, 18, "voxel32-p18"},
      {1.0f / 32.0f, 36, "voxel32-p36"},
      {1.0f / 40.0f, 8, "voxel40-p8"},
      {1.0f / 48.0f, 8, "voxel48-p8"},
      {1.0f / 48.0f, 18, "voxel48-p18"},
      {1.0f / 48.0f, 36, "voxel48-p36"},
      {1.0f / 48.0f, 72, "voxel48-p72"}}};
  constexpr float referenceVoxelSize = 1.0f / 32.0f;
  constexpr float fixedStep = 1.0f / 60.0f;
  constexpr int frameCount = 36;
  constexpr int fixedSteps = frameCount * 2;
  const QString outputDirectory = qEnvironmentVariable(
      "ARTIFACT_SMOKE_RUNTIME_OUTPUT_DIR");
  const auto sourceKernelIntegral = [](const float voxelSize) {
    PyroDomain domain;
    domain.min = {-0.75f, 0.0f, -0.5f};
    domain.max = {0.75f, 1.125f, 0.5f};
    domain.voxelSize = voxelSize;
    const auto resolution = domain.resolution();
    const PyroVec3 sourcePosition{0.0f, 0.18f, 0.0f};
    const PyroVec3 sourceExtent{0.12f, 0.09f, 0.12f};
    const int centerX = static_cast<int>(
        (sourcePosition.x - domain.min.x) / voxelSize);
    const int centerY = static_cast<int>(
        (sourcePosition.y - domain.min.y) / voxelSize);
    const int centerZ = static_cast<int>(
        (sourcePosition.z - domain.min.z) / voxelSize);
    const int radiusX = static_cast<int>(std::ceil(sourceExtent.x / voxelSize));
    const int radiusY = static_cast<int>(std::ceil(sourceExtent.y / voxelSize));
    const int radiusZ = static_cast<int>(std::ceil(sourceExtent.z / voxelSize));
    double integratedFalloff = 0.0;
    for (int z = std::max(0, centerZ - radiusZ);
         z <= std::min(resolution.depth - 1, centerZ + radiusZ); ++z)
      for (int y = std::max(0, centerY - radiusY);
           y <= std::min(resolution.height - 1, centerY + radiusY); ++y)
        for (int x = std::max(0, centerX - radiusX);
             x <= std::min(resolution.width - 1, centerX + radiusX); ++x) {
          const float dx = (static_cast<float>(x) + 0.5f) * voxelSize +
              domain.min.x - sourcePosition.x;
          const float dy = (static_cast<float>(y) + 0.5f) * voxelSize +
              domain.min.y - sourcePosition.y;
          const float dz = (static_cast<float>(z) + 0.5f) * voxelSize +
              domain.min.z - sourcePosition.z;
          const float falloff = std::clamp(1.0f - std::sqrt(
              dx * dx + dy * dy + dz * dz) / 0.12f, 0.0f, 1.0f);
          integratedFalloff += falloff;
        }
    return integratedFalloff * voxelSize * voxelSize * voxelSize;
  };
  const double referenceKernelIntegral =
      sourceKernelIntegral(referenceVoxelSize);
  ASSERT_GT(referenceKernelIntegral, 0.0);

  struct ImageMetrics final {
    std::size_t visiblePixels = 0;
    double luminanceSum = 0.0;
    double highPassRms = 0.0;
  };
  const auto measureImage = [](const ImageBuffer& image) {
    ImageMetrics metrics;
    const auto lumaAt = [&image](const int x, const int y) {
      const auto offset = static_cast<std::size_t>((y * image.width + x) * 3);
      return 0.2126 * image.pixels[offset] +
          0.7152 * image.pixels[offset + 1] +
          0.0722 * image.pixels[offset + 2];
    };
    double highPassSquared = 0.0;
    std::size_t highPassCount = 0;
    for (int y = 0; y < image.height; ++y)
      for (int x = 0; x < image.width; ++x) {
        const double luma = lumaAt(x, y);
        if (luma > 10.0) {
          ++metrics.visiblePixels;
          metrics.luminanceSum += luma;
        }
        if (x == 0 || y == 0 || x + 1 == image.width ||
            y + 1 == image.height)
          continue;
        const double highPass = 4.0 * luma - lumaAt(x - 1, y) -
            lumaAt(x + 1, y) - lumaAt(x, y - 1) - lumaAt(x, y + 1);
        highPassSquared += highPass * highPass;
        ++highPassCount;
      }
    metrics.highPassRms = std::sqrt(highPassSquared /
        static_cast<double>(std::max<std::size_t>(1, highPassCount))) / 255.0;
    return metrics;
  };
  const auto savePng = [](const ImageBuffer& buffer, const QString& path) {
    QImage image(buffer.width, buffer.height, QImage::Format_ARGB32);
    for (int y = 0; y < buffer.height; ++y) {
      auto* row = reinterpret_cast<QRgb*>(image.scanLine(y));
      for (int x = 0; x < buffer.width; ++x) {
        const auto offset = static_cast<std::size_t>((y * buffer.width + x) * 3);
        const auto encode = [](const std::uint8_t channel) {
          const float linear = static_cast<float>(channel) / 255.0f;
          return static_cast<int>(std::lround(
              std::pow(linear, 1.0f / 2.2f) * 255.0f));
        };
        row[x] = qRgba(encode(buffer.pixels[offset]),
                       encode(buffer.pixels[offset + 1]),
                       encode(buffer.pixels[offset + 2]), 255);
      }
    }
    return image.save(path, "PNG");
  };

  for (const auto& condition : conditions) {
    const float voxelSize = condition.voxelSize;
    PyroDomain domain;
    domain.min = {-0.75f, 0.0f, -0.5f};
    domain.max = {0.75f, 1.125f, 0.5f};
    domain.voxelSize = voxelSize;
    domain.fixedTimeStep = fixedStep;
    domain.boundaryMode = PyroBoundaryMode::Open;
    domain.memoryBudgetBytes = 128u * 1024u * 1024u;
    const auto resolution = domain.resolution();
    const auto cellCount = static_cast<std::size_t>(resolution.width) *
        static_cast<std::size_t>(resolution.height) *
        static_cast<std::size_t>(resolution.depth);
    ASSERT_GT(cellCount, 0u);
    PyroSimulation simulation(domain);
    simulation.setCacheInterval(16);
    PyroSimulationSettings settings;
    settings.dissipation = 0.035f;
    settings.coolingRate = 0.18f;
    // Pyro's advection traces in voxel coordinates. Scale accelerations and
    // source velocity so the corresponding world-space motion stays fixed.
    const float gridScale = referenceVoxelSize / voxelSize;
    settings.buoyancy = 0.7f * gridScale;
    settings.vorticity = 0.45f * gridScale;
    settings.pressureIterations = static_cast<float>(condition.pressureIterations);
    simulation.setSettings(settings);
    PyroSourceState source;
    source.position = {0.0f, 0.18f, 0.0f};
    source.extent = {0.12f, 0.09f, 0.12f};
    const double actualKernelIntegral = sourceKernelIntegral(voxelSize);
    ASSERT_GT(actualKernelIntegral, 0.0);
    const float sourceQuadratureCorrection = static_cast<float>(
        referenceKernelIntegral / actualKernelIntegral);
    source.velocity = {0.0f, 240.0f * gridScale * sourceQuadratureCorrection,
                       0.0f};
    source.density = 4.0f * sourceQuadratureCorrection;
    source.temperature = 1.5f * sourceQuadratureCorrection;
    source.enabled = true;
    simulation.setSources(std::span<const PyroSourceState>(&source, 1));

    std::vector<float> previousDensity;
    const auto simulationStart = std::chrono::steady_clock::now();
    for (int frame = 0; frame < frameCount; ++frame) {
      simulation.step(1.0 / 30.0);
      if (frame == frameCount - 2)
        previousDensity = simulation.fields().densityStorage();
    }
    const auto simulationEnd = std::chrono::steady_clock::now();
    const auto snapshot = simulation.snapshot();
    ASSERT_EQ(snapshot.resolution.width, resolution.width);
    ASSERT_EQ(snapshot.resolution.height, resolution.height);
    ASSERT_EQ(snapshot.resolution.depth, resolution.depth);
    EXPECT_NEAR(simulation.timeSeconds(), 1.2, 1e-6);
    ASSERT_EQ(previousDensity.size(), cellCount);

    const double cellVolume = static_cast<double>(voxelSize) * voxelSize * voxelSize;
    double mass = 0.0;
    double weightedX = 0.0;
    double weightedY = 0.0;
    double weightedZ = 0.0;
    double interfaceGradientSquared = 0.0;
    double interfaceCurlSquared = 0.0;
    double interfaceDivergenceSquared = 0.0;
    double previousMass = 0.0;
    double temporalL1 = 0.0;
    double temporalDot = 0.0;
    double previousNormSquared = 0.0;
    double currentNormSquared = 0.0;
    std::size_t interfaceCount = 0;
    const auto fieldIndex = [&resolution](const int x, const int y, const int z) {
      return static_cast<std::size_t>(x) +
          static_cast<std::size_t>(resolution.width) *
              (static_cast<std::size_t>(y) +
               static_cast<std::size_t>(resolution.height) *
                   static_cast<std::size_t>(z));
    };
    for (int z = 0; z < resolution.depth; ++z)
      for (int y = 0; y < resolution.height; ++y)
        for (int x = 0; x < resolution.width; ++x) {
          const auto cell = fieldIndex(x, y, z);
          const double density = std::max(0.0f, snapshot.density.values[cell]);
          const double weightedDensity = density * cellVolume;
          mass += weightedDensity;
          weightedX += weightedDensity * (domain.min.x + (x + 0.5) * voxelSize);
          weightedY += weightedDensity * (domain.min.y + (y + 0.5) * voxelSize);
          weightedZ += weightedDensity * (domain.min.z + (z + 0.5) * voxelSize);
          const double prior = std::max(0.0f, previousDensity[cell]);
          const double priorWeighted = prior * cellVolume;
          previousMass += priorWeighted;
          temporalL1 += std::abs(density - prior) * cellVolume;
          temporalDot += density * prior;
          previousNormSquared += prior * prior;
          currentNormSquared += density * density;
          if (x == 0 || y == 0 || z == 0 ||
              x + 1 == resolution.width || y + 1 == resolution.height ||
              z + 1 == resolution.depth || density < 0.01 || density > 1.0)
            continue;
          const auto& velocity = snapshot.velocity.values;
          const auto vectorAt = [&](const int sx, const int sy, const int sz)
              -> const PyroVec3& {
            return velocity[fieldIndex(sx, sy, sz)];
          };
          const float gx = (snapshot.density.values[fieldIndex(x + 1, y, z)] -
              snapshot.density.values[fieldIndex(x - 1, y, z)]) /
              (2.0f * voxelSize);
          const float gy = (snapshot.density.values[fieldIndex(x, y + 1, z)] -
              snapshot.density.values[fieldIndex(x, y - 1, z)]) /
              (2.0f * voxelSize);
          const float gz = (snapshot.density.values[fieldIndex(x, y, z + 1)] -
              snapshot.density.values[fieldIndex(x, y, z - 1)]) /
              (2.0f * voxelSize);
          interfaceGradientSquared += static_cast<double>(gx * gx + gy * gy + gz * gz);
          const auto& vxP = vectorAt(x + 1, y, z);
          const auto& vxM = vectorAt(x - 1, y, z);
          const auto& vyP = vectorAt(x, y + 1, z);
          const auto& vyM = vectorAt(x, y - 1, z);
          const auto& vzP = vectorAt(x, y, z + 1);
          const auto& vzM = vectorAt(x, y, z - 1);
          const float curlX = 0.5f * ((vyP.z - vyM.z) - (vzP.y - vzM.y));
          const float curlY = 0.5f * ((vzP.x - vzM.x) - (vxP.z - vxM.z));
          const float curlZ = 0.5f * ((vxP.y - vxM.y) - (vyP.x - vyM.x));
          interfaceCurlSquared += static_cast<double>(curlX * curlX +
              curlY * curlY + curlZ * curlZ);
          const float divergence = 0.5f * ((vxP.x - vxM.x) +
              (vyP.y - vyM.y) + (vzP.z - vzM.z));
          interfaceDivergenceSquared +=
              static_cast<double>(divergence * divergence);
          ++interfaceCount;
        }
    ASSERT_GT(mass, 0.0);
    ASSERT_GT(previousMass, 0.0);
    ASSERT_GT(interfaceCount, 0u);
    const double cosine = temporalDot / std::sqrt(
        std::max(1e-30, previousNormSquared * currentNormSquared));
    const double voxelCountScale = referenceVoxelSize / voxelSize;
    const double integratedSourceMass = 4.0 * fixedStep * fixedSteps *
        referenceKernelIntegral;
    const double expectedWorldSpeed = 7.5;
    EXPECT_NEAR(source.velocity.y * voxelSize / sourceQuadratureCorrection,
                expectedWorldSpeed, 1e-4);

    VolumeResolution volumeResolution{
        resolution.width, resolution.height, resolution.depth};
    VolumeAABB bounds;
    bounds.min = {domain.min.x, domain.min.y, domain.min.z};
    bounds.max = {domain.max.x, domain.max.y, domain.max.z};
    const auto makeRenderer = [&](const bool sideView) {
      CPUVolumeRenderer renderer;
      renderer.bounds = bounds;
      VolumeFieldSet fields;
      fields.density = {
          const_cast<float*>(snapshot.density.values.data()), volumeResolution};
      renderer.setVolumeData(bounds, fields);
      renderer.settings.stepSize = 0.0125f;
      renderer.settings.maxSteps = 256;
      renderer.settings.densityScale = 7.2f;
      renderer.settings.useTemperatureEmission = false;
      renderer.settings.backgroundColor = {0.015f, 0.02f, 0.03f};
      renderer.settings.lightIntensity = 3.0f;
      renderer.settings.phaseAnisotropy = 0.25f;
      renderer.settings.multipleScatteringStrength = 0.18f;
      renderer.settings.shadowIntensity = 0.25f;
      renderer.settings.shadowStepSize = 0.04f / voxelSize;
      renderer.settings.shadowMaxSteps = 96;
      renderer.settings.shadowDensityScale = 0.08f;
      const Point3 lookAt{0.0f, 0.5625f, 0.0f};
      const Point3 lookFrom = sideView ? Point3{2.4f, 0.5625f, 0.0f}
                                       : Point3{0.0f, 0.5625f, 2.4f};
      renderer.camera = Camera(1.0f, 36.0f, 2.4f, 0.0f, lookFrom,
                               lookAt, {0.0f, 1.0f, 0.0f});
      return renderer;
    };
    const auto frontImage = makeRenderer(false).render(160, 160);
    const auto sideImage = makeRenderer(true).render(160, 160);
    const auto frontMetrics = measureImage(frontImage);
    const auto sideMetrics = measureImage(sideImage);
    ASSERT_GT(frontMetrics.visiblePixels, 100u);
    ASSERT_GT(sideMetrics.visiblePixels, 100u);
    if (!outputDirectory.isEmpty()) {
      const auto captureDirectory = QDir(outputDirectory).filePath(
          QStringLiteral("3d-resolution-rnd"));
      ASSERT_TRUE(QDir().mkpath(captureDirectory));
      EXPECT_TRUE(savePng(frontImage, QDir(captureDirectory).filePath(
          QStringLiteral("pyro_%1_front.png").arg(
              QString::fromLatin1(condition.label)))));
      EXPECT_TRUE(savePng(sideImage, QDir(captureDirectory).filePath(
          QStringLiteral("pyro_%1_side.png").arg(
              QString::fromLatin1(condition.label)))));
    }
    const auto memoryEstimate = simulation.estimateMemory();
    const double simulationMilliseconds = std::chrono::duration<double,
        std::milli>(simulationEnd - simulationStart).count();
    GTEST_LOG_(INFO) << "3D smoke resolution case=" << condition.label
        << " voxel=" << voxelSize
        << " grid=" << resolution.width << 'x' << resolution.height << 'x'
        << resolution.depth << " fieldBytes=" << memoryEstimate.totalBytes
        << " pressureIterations=" << condition.pressureIterations
        << " sourceCorrection=" << sourceQuadratureCorrection
        << " nominalInjectedMass=" << integratedSourceMass
        << " finalWorldMass=" << mass << " massRetention="
        << mass / integratedSourceMass << " centroidWorld="
        << weightedX / mass << ',' << weightedY / mass << ',' << weightedZ / mass
        << " interfaceGradientRms=" << std::sqrt(interfaceGradientSquared /
            static_cast<double>(interfaceCount))
        << " interfaceCurlRms=" << std::sqrt(interfaceCurlSquared /
            static_cast<double>(interfaceCount))
        << " interfaceDivergenceRms=" << std::sqrt(
            interfaceDivergenceSquared / static_cast<double>(interfaceCount))
        << " temporalCosine/L1=" << cosine << '/' << temporalL1 / previousMass
        << " frontPixels/luma/highPass=" << frontMetrics.visiblePixels << '/'
        << frontMetrics.luminanceSum << '/' << frontMetrics.highPassRms
        << " sidePixels/luma/highPass=" << sideMetrics.visiblePixels << '/'
        << sideMetrics.luminanceSum << '/' << sideMetrics.highPassRms
        << " simulationMs=" << simulationMilliseconds
        << " gridScale=" << voxelCountScale;
  }
}

TEST(ArtifactSmokeRuntimeStandaloneTest,
     MeasuresMassNormalizedThreeDimensionalEmitterFootprintSweep) {
  using namespace ArtifactCore;
  PyroDomain domain;
  domain.min = {-0.75f, 0.0f, -0.5f};
  domain.max = {0.75f, 1.125f, 0.5f};
  domain.voxelSize = 1.0f / 32.0f;
  domain.fixedTimeStep = 1.0 / 60.0;
  domain.boundaryMode = PyroBoundaryMode::Open;
  domain.memoryBudgetBytes = 32u * 1024u * 1024u;
  const auto resolution = domain.resolution();
  struct Footprint final { const char* name; float radius; };
  constexpr std::array<Footprint, 3> footprints{{
      {"narrow", 0.075f}, {"baseline", 0.12f}, {"wide", 0.18f}}};
  for (const auto& footprint : footprints) {
    PyroSimulation simulation(domain);
    PyroSimulationSettings settings;
    settings.dissipation = 0.035f;
    settings.coolingRate = 0.18f;
    settings.buoyancy = 0.7f;
    settings.vorticity = 0.45f;
    settings.pressureIterations = 8.0f;
    simulation.setSettings(settings);
    PyroSourceState source;
    source.position = {0.0f, 0.18f, 0.0f};
    source.extent = {footprint.radius, footprint.radius * 0.75f,
                     footprint.radius};
    source.velocity = {0.0f, 240.0f, 0.0f};
    const float baselineVolume = 0.12f * 0.09f * 0.12f;
    const float sourceVolume = source.extent.x * source.extent.y * source.extent.z;
    source.density = 4.0f * baselineVolume / sourceVolume;
    source.temperature = 1.5f;
    source.enabled = true;
    for (int step = 0; step < 72; ++step) {
      simulation.setSources(std::span<const PyroSourceState>(&source, 1));
      simulation.step(1.0 / 60.0);
    }
    const auto& density = simulation.fields().densityStorage();
    const auto index = [&resolution](const int x, const int y, const int z) {
      return static_cast<std::size_t>(x + resolution.width *
          (y + resolution.height * z));
    };
    double mass = 0.0, weightedX = 0.0, weightedY = 0.0, weightedZ = 0.0;
    double weightedX2 = 0.0, weightedZ2 = 0.0, laplacianSquared = 0.0;
    std::size_t laplacianCount = 0;
    for (int z = 0; z < resolution.depth; ++z)
      for (int y = 0; y < resolution.height; ++y)
        for (int x = 0; x < resolution.width; ++x) {
          const double rho = density[index(x, y, z)];
          mass += rho;
          weightedX += rho * x;
          weightedY += rho * y;
          weightedZ += rho * z;
          weightedX2 += rho * x * x;
          weightedZ2 += rho * z * z;
          if (x == 0 || y == 0 || z == 0 || x + 1 == resolution.width ||
              y + 1 == resolution.height || z + 1 == resolution.depth)
            continue;
          const double lap = 6.0 * rho - density[index(x - 1, y, z)] -
              density[index(x + 1, y, z)] - density[index(x, y - 1, z)] -
              density[index(x, y + 1, z)] - density[index(x, y, z - 1)] -
              density[index(x, y, z + 1)];
          laplacianSquared += lap * lap;
          ++laplacianCount;
        }
    ASSERT_GT(mass, 0.0);
    const double spreadX = std::sqrt(std::max(0.0,
        weightedX2 / mass - std::pow(weightedX / mass, 2.0)));
    const double spreadZ = std::sqrt(std::max(0.0,
        weightedZ2 / mass - std::pow(weightedZ / mass, 2.0)));
    GTEST_LOG_(INFO) << "3D emitter footprint=" << footprint.name
        << " radius=" << footprint.radius << " mass=" << mass
        << " centroidY=" << weightedY / mass << " spreadX/Z=" << spreadX
        << '/' << spreadZ << " laplacianRms="
        << std::sqrt(laplacianSquared / std::max<std::size_t>(1, laplacianCount));
  }
}

TEST(ArtifactSmokeRuntimeStandaloneTest,
     ComparesSingleAndThreeLobedPyroEmitterAtMatchedNominalSourceVolume) {
  using namespace ArtifactCore;
  using namespace ArtifactCore::RayTrace;
  PyroDomain domain;
  domain.min = {-0.75f, 0.0f, -0.5f};
  domain.max = {0.75f, 1.125f, 0.5f};
  domain.voxelSize = 1.0f / 32.0f;
  domain.fixedTimeStep = 1.0 / 60.0;
  domain.boundaryMode = PyroBoundaryMode::Open;
  domain.memoryBudgetBytes = 32u * 1024u * 1024u;
  const auto pyroResolution = domain.resolution();
  const VolumeResolution resolution{pyroResolution.width,
      pyroResolution.height, pyroResolution.depth};
  const auto outputDirectory = qEnvironmentVariable(
      "ARTIFACT_SMOKE_RUNTIME_OUTPUT_DIR");
  const auto savePng = [](const ImageBuffer& buffer, const QString& path) {
    QImage image(buffer.width, buffer.height, QImage::Format_ARGB32);
    for (int y = 0; y < buffer.height; ++y) {
      auto* row = reinterpret_cast<QRgb*>(image.scanLine(y));
      for (int x = 0; x < buffer.width; ++x) {
        const auto offset = static_cast<std::size_t>((y * buffer.width + x) * 3);
        const auto encode = [](const std::uint8_t channel) {
          const float linear = static_cast<float>(channel) / 255.0f;
          return static_cast<int>(std::lround(
              std::pow(linear, 1.0f / 2.2f) * 255.0f));
        };
        row[x] = qRgba(encode(buffer.pixels[offset]),
                       encode(buffer.pixels[offset + 1]),
                       encode(buffer.pixels[offset + 2]), 255);
      }
    }
    return image.save(path, "PNG");
  };
  const auto render = [&](float* density, const bool sideView) {
    CPUVolumeRenderer renderer;
    VolumeAABB bounds;
    bounds.min = {domain.min.x, domain.min.y, domain.min.z};
    bounds.max = {domain.max.x, domain.max.y, domain.max.z};
    renderer.bounds = bounds;
    VolumeFieldSet fields;
    fields.density = {density, resolution};
    renderer.setVolumeData(bounds, fields);
    renderer.settings.stepSize = 0.018f;
    renderer.settings.maxSteps = 256;
    renderer.settings.densityScale = 7.2f;
    renderer.settings.useTemperatureEmission = false;
    renderer.settings.backgroundColor = {0.015f, 0.02f, 0.03f};
    renderer.settings.lightIntensity = 3.0f;
    renderer.settings.phaseAnisotropy = 0.25f;
    renderer.settings.multipleScatteringStrength = 0.18f;
    renderer.settings.shadowIntensity = 0.25f;
    renderer.settings.shadowStepSize = 0.04f;
    renderer.settings.shadowMaxSteps = 96;
    renderer.settings.shadowDensityScale = 0.08f;
    const Point3 lookAt{0.0f, 0.56f, 0.0f};
    const Point3 lookFrom = sideView ? Point3{2.4f, 0.56f, 0.0f}
                                     : Point3{0.0f, 0.56f, 2.4f};
    renderer.camera = Camera(1.0f, 36.0f, 2.4f, 0.0f, lookFrom,
        lookAt, {0.0f, 1.0f, 0.0f});
    return renderer.render(160, 160);
  };
  struct Condition final {
    const char* name;
    int lobeCount;
    float spacing;
    int pulsePeriodSteps;
  };
  constexpr std::array<Condition, 7> conditions{{
      {"single", 1, 0.0f, 0},
      {"triple_tight", 3, 0.10f, 0},
      {"triple", 3, 0.14f, 0},
      {"triple_wide", 3, 0.22f, 0},
      {"triple_pulse_0p2", 3, 0.14f, 12},
      {"triple_pulse_0p4", 3, 0.14f, 24},
      {"triple_pulse_0p6", 3, 0.14f, 36}}};
  constexpr float baseRadius = 0.12f;
  constexpr float lobeRadius = baseRadius / 1.44224957f;
  const auto sourceKernelIntegral = [&](const PyroVec3& position,
                                        const float radius) {
    const int centerX = static_cast<int>((position.x - domain.min.x) / domain.voxelSize);
    const int centerY = static_cast<int>((position.y - domain.min.y) / domain.voxelSize);
    const int centerZ = static_cast<int>((position.z - domain.min.z) / domain.voxelSize);
    const int radiusCells = static_cast<int>(std::ceil(radius / domain.voxelSize));
    double integral = 0.0;
    for (int z = std::max(0, centerZ - radiusCells);
         z <= std::min(resolution.depth - 1, centerZ + radiusCells); ++z)
      for (int y = std::max(0, centerY - radiusCells);
           y <= std::min(resolution.height - 1, centerY + radiusCells); ++y)
        for (int x = std::max(0, centerX - radiusCells);
             x <= std::min(resolution.width - 1, centerX + radiusCells); ++x) {
          const float dx = (static_cast<float>(x) + 0.5f) * domain.voxelSize +
              domain.min.x - position.x;
          const float dy = (static_cast<float>(y) + 0.5f) * domain.voxelSize +
              domain.min.y - position.y;
          const float dz = (static_cast<float>(z) + 0.5f) * domain.voxelSize +
              domain.min.z - position.z;
          integral += std::clamp(1.0f - std::sqrt(dx * dx + dy * dy + dz * dz) /
              radius, 0.0f, 1.0f);
        }
    return integral;
  };
  const double singleKernelIntegral = sourceKernelIntegral({0.0f, 0.18f, 0.0f},
      baseRadius);
  for (const auto& condition : conditions) {
    PyroSimulation simulation(domain);
    PyroSimulationSettings settings;
    settings.dissipation = 0.035f;
    settings.coolingRate = 0.18f;
    settings.buoyancy = 0.7f;
    settings.vorticity = 0.45f;
    settings.pressureIterations = 8.0f;
    simulation.setSettings(settings);
    std::array<PyroSourceState, 3> sources{};
    std::array<float, 3> nominalDensities{};
    std::array<float, 3> nominalTemperatures{};
    std::array<PyroVec3, 3> nominalVelocities{};
    for (int i = 0; i < condition.lobeCount; ++i) {
      auto& source = sources[static_cast<std::size_t>(i)];
      constexpr float halfSqrtThree = 0.866025404f;
      const PyroVec3 offset = condition.lobeCount == 1 ? PyroVec3{} :
          (i == 0 ? PyroVec3{condition.spacing, 0.0f, 0.0f} :
           PyroVec3{-condition.spacing * 0.5f, 0.0f,
               (i == 1 ? 1.0f : -1.0f) * condition.spacing * halfSqrtThree});
      const float radius = condition.lobeCount == 1 ? baseRadius : lobeRadius;
      source.position = {offset.x, 0.18f, offset.z};
      source.extent = {radius, radius * 0.75f, radius};
      source.velocity = {0.0f, 240.0f, 0.0f};
      source.density = static_cast<float>(4.0 * singleKernelIntegral /
          (condition.lobeCount * sourceKernelIntegral(source.position, radius)));
      nominalDensities[static_cast<std::size_t>(i)] = source.density;
      nominalTemperatures[static_cast<std::size_t>(i)] = source.temperature;
      nominalVelocities[static_cast<std::size_t>(i)] = source.velocity;
      source.temperature = 1.5f;
      source.enabled = true;
    }
    for (int step = 0; step < 72; ++step) {
      const bool pulseOn = condition.pulsePeriodSteps == 0 ||
          (step % condition.pulsePeriodSteps) < condition.pulsePeriodSteps / 2;
      const float pulseScale = condition.pulsePeriodSteps == 0 ? 1.0f :
          (pulseOn ? 2.0f : 0.0f);
      for (int i = 0; i < condition.lobeCount; ++i) {
        sources[static_cast<std::size_t>(i)].density =
            nominalDensities[static_cast<std::size_t>(i)] * pulseScale;
        sources[static_cast<std::size_t>(i)].temperature =
            nominalTemperatures[static_cast<std::size_t>(i)] * pulseScale;
        sources[static_cast<std::size_t>(i)].velocity = {
            nominalVelocities[static_cast<std::size_t>(i)].x * pulseScale,
            nominalVelocities[static_cast<std::size_t>(i)].y * pulseScale,
            nominalVelocities[static_cast<std::size_t>(i)].z * pulseScale};
      }
      simulation.setSources(std::span<const PyroSourceState>(
          sources.data(), static_cast<std::size_t>(condition.lobeCount)));
      simulation.step(1.0 / 60.0);
      const int checkpointInterval = condition.pulsePeriodSteps == 0 ? 12 :
          condition.pulsePeriodSteps / 2;
      if (condition.spacing == 0.14f &&
          (step + 1) % checkpointInterval == 0) {
        const auto& checkpointDensity = simulation.fields().densityStorage();
        double checkpointMass = 0.0;
        double checkpointWeightedY = 0.0;
        for (int z = 0; z < resolution.depth; ++z)
          for (int y = 0; y < resolution.height; ++y)
            for (int x = 0; x < resolution.width; ++x) {
              const double rho = checkpointDensity[resolution.indexOf(x, y, z)];
              checkpointMass += rho;
              checkpointWeightedY += rho * y;
            }
        GTEST_LOG_(INFO) << "Pyro pulse checkpoint condition=" << condition.name
            << " time=" << (step + 1) / 60.0 << " mass=" << checkpointMass
            << " centroidY=" << checkpointWeightedY / checkpointMass;
        if (!outputDirectory.isEmpty()) {
          const auto captureDirectory = QDir(outputDirectory).filePath(
              QStringLiteral("3d-emitter-topology-rnd"));
          ASSERT_TRUE(QDir().mkpath(captureDirectory));
          const QString suffix = QStringLiteral("%1_t%2")
              .arg(condition.name).arg(step + 1, 2, 10, QLatin1Char('0'));
          EXPECT_TRUE(savePng(render(const_cast<float*>(checkpointDensity.data()), false),
              QDir(captureDirectory).filePath(QStringLiteral("pyro_%1_front.png").arg(suffix))));
          EXPECT_TRUE(savePng(render(const_cast<float*>(checkpointDensity.data()), true),
              QDir(captureDirectory).filePath(QStringLiteral("pyro_%1_side.png").arg(suffix))));
        }
      }
    }
    const auto& density = simulation.fields().densityStorage();
    const double mass = std::accumulate(density.begin(), density.end(), 0.0);
    ASSERT_GT(mass, 0.0);
    auto front = render(const_cast<float*>(density.data()), false);
    auto side = render(const_cast<float*>(density.data()), true);
    const auto imageStats = [](const ImageBuffer& image) {
      std::array<double, 3> stats{};
      for (int y = 0; y < image.height; ++y)
        for (int x = 0; x < image.width; ++x) {
          const auto offset = static_cast<std::size_t>((y * image.width + x) * 3);
          const double luma = 0.2126 * image.pixels[offset] +
              0.7152 * image.pixels[offset + 1] + 0.0722 * image.pixels[offset + 2];
          if (luma <= 10.0) continue;
          ++stats[0];
          stats[1] += luma;
        }
      return stats;
    };
    const auto frontStats = imageStats(front);
    const auto sideStats = imageStats(side);
    GTEST_LOG_(INFO) << "Pyro source topology=" << condition.name
        << " lobes=" << condition.lobeCount << " finalMass=" << mass
        << " front/side occupied=" << frontStats[0] << '/' << sideStats[0]
        << " front/side luma=" << frontStats[1] << '/' << sideStats[1];
    if (!outputDirectory.isEmpty()) {
      const auto captureDirectory = QDir(outputDirectory).filePath(
          QStringLiteral("3d-emitter-topology-rnd"));
      ASSERT_TRUE(QDir().mkpath(captureDirectory));
      EXPECT_TRUE(savePng(front, QDir(captureDirectory).filePath(
          QStringLiteral("pyro_%1_front.png").arg(condition.name))));
      EXPECT_TRUE(savePng(side, QDir(captureDirectory).filePath(
          QStringLiteral("pyro_%1_side.png").arg(condition.name))));
    }
  }
}

TEST(ArtifactSmokeRuntimeStandaloneTest,
     SeparatesBuoyancyAdvectionAndDensityDecayAfterEmitterPulse) {
  using namespace ArtifactCore;
  using namespace ArtifactCore::RayTrace;
  PyroDomain domain;
  domain.min = {-0.75f, 0.0f, -0.5f};
  domain.max = {0.75f, 1.125f, 0.5f};
  domain.voxelSize = 1.0f / 32.0f;
  domain.fixedTimeStep = 1.0 / 60.0;
  domain.boundaryMode = PyroBoundaryMode::Open;
  domain.memoryBudgetBytes = 32u * 1024u * 1024u;
  const auto pyroResolution = domain.resolution();
  const VolumeResolution resolution{pyroResolution.width,
      pyroResolution.height, pyroResolution.depth};
  PyroSimulationSettings sourceSettings;
  sourceSettings.dissipation = 0.035f;
  sourceSettings.coolingRate = 0.18f;
  sourceSettings.buoyancy = 0.7f;
  sourceSettings.vorticity = 0.45f;
  sourceSettings.pressureIterations = 8.0f;
  PyroSimulation emitter(domain);
  emitter.setSettings(sourceSettings);
  PyroSourceState source;
  source.position = {0.0f, 0.18f, 0.0f};
  source.extent = {0.12f, 0.09f, 0.12f};
  source.velocity = {0.0f, 240.0f, 0.0f};
  source.density = 4.0f;
  source.temperature = 1.5f;
  source.enabled = true;
  for (int step = 0; step < 12; ++step) {
    emitter.setSources(std::span<const PyroSourceState>(&source, 1));
    emitter.step(1.0 / 60.0);
  }

  const auto pulseDensity = emitter.fields().densityStorage();
  const auto pulseTemperature = emitter.fields().temperatureStorage();
  const auto pulseFuel = emitter.fields().fuelStorage();
  const auto pulseVelocity = emitter.fields().velocityStorage();
  const double initialMass = std::accumulate(
      pulseDensity.begin(), pulseDensity.end(), 0.0);
  double pulseTemperatureIntegral = 0.0;
  double pulseTemperatureMaximum = 0.0;
  std::size_t pulseTemperatureSupportCells = 0;
  for (const float temperature : pulseTemperature) {
    pulseTemperatureIntegral += temperature;
    pulseTemperatureMaximum = std::max<double>(pulseTemperatureMaximum, temperature);
    if (temperature > 1.0e-4f) ++pulseTemperatureSupportCells;
  }
  struct Condition final {
    const char* label;
    float buoyancy;
    float dissipation;
    float vorticity;
    bool clearVelocity;
    int pressureIterations;
    float testConfinement;
    bool interfaceGate;
    float densityShredAmplitude = 0.0f;
    int densityShredBands = 0;
    bool captureShredTemporalMetric = false;
  };
  constexpr std::array<Condition, 18> conditions{{
      {"full_flow", 0.7f, 0.035f, 0.45f, false, 8, 0.0f, false},
      {"no_new_buoyancy", 0.0f, 0.035f, 0.45f, false, 8, 0.0f, false},
      {"buoyancy_rest_p1", 0.7f, 0.035f, 0.0f, true, 1, 0.0f, false},
      {"buoyancy_rest_p4", 0.7f, 0.035f, 0.0f, true, 4, 0.0f, false},
      {"buoyancy_from_rest", 0.7f, 0.035f, 0.0f, true, 8, 0.0f, false},
      {"buoyancy_rest_p16", 0.7f, 0.035f, 0.0f, true, 16, 0.0f, false},
      {"buoyancy_rest_p32", 0.7f, 0.035f, 0.0f, true, 32, 0.0f, false},
      {"buoyancy_strength_7", 7.0f, 0.035f, 0.0f, true, 8, 0.0f, false},
      {"buoyancy_strength_20", 20.0f, 0.035f, 0.0f, true, 8, 0.0f, false},
      {"density_decay_only", 0.0f, 0.035f, 0.0f, true, 8, 0.0f, false},
      {"static_density_control", 0.0f, 0.0f, 0.0f, true, 8, 0.0f, false},
      {"residual_curl_conf_2", 0.7f, 0.035f, 0.0f, false, 8, 2.0f, false},
      {"residual_curl_conf_8", 0.7f, 0.035f, 0.0f, false, 8, 8.0f, false},
      {"interface_curl_conf_8", 0.7f, 0.035f, 0.0f, false, 8, 8.0f, true},
      {"density_shred_control", 0.7f, 0.035f, 0.0f, false, 8, 0.0f, false,
          0.0f, 0, true},
      {"density_shred_low_12", 0.7f, 0.035f, 0.0f, false, 8, 0.0f, false,
          0.12f, 1, true},
      {"density_shred_multi_12", 0.7f, 0.035f, 0.0f, false, 8, 0.0f, false,
          0.12f, 3, true},
      {"density_shred_multi_28", 0.7f, 0.035f, 0.0f, false, 8, 0.0f, false,
          0.28f, 3, true}}};
  const auto outputDirectory = qEnvironmentVariable(
      "ARTIFACT_SMOKE_RUNTIME_OUTPUT_DIR");
  const auto render = [&](float* density, const bool sideView) {
    CPUVolumeRenderer renderer;
    VolumeAABB bounds;
    bounds.min = {domain.min.x, domain.min.y, domain.min.z};
    bounds.max = {domain.max.x, domain.max.y, domain.max.z};
    renderer.bounds = bounds;
    VolumeFieldSet fields;
    fields.density = {density, resolution};
    renderer.setVolumeData(bounds, fields);
    renderer.settings.stepSize = 0.018f;
    renderer.settings.maxSteps = 256;
    renderer.settings.densityScale = 7.2f;
    renderer.settings.useTemperatureEmission = false;
    renderer.settings.backgroundColor = {0.015f, 0.02f, 0.03f};
    renderer.settings.lightIntensity = 3.0f;
    renderer.settings.phaseAnisotropy = 0.25f;
    renderer.settings.multipleScatteringStrength = 0.18f;
    renderer.settings.shadowIntensity = 0.25f;
    renderer.settings.shadowStepSize = 0.04f;
    renderer.settings.shadowMaxSteps = 96;
    renderer.settings.shadowDensityScale = 0.08f;
    const Point3 lookAt{0.0f, 0.56f, 0.0f};
    const Point3 lookFrom = sideView ? Point3{2.4f, 0.56f, 0.0f}
                                     : Point3{0.0f, 0.56f, 2.4f};
    renderer.camera = Camera(1.0f, 36.0f, 2.4f, 0.0f, lookFrom,
        lookAt, {0.0f, 1.0f, 0.0f});
    return renderer.render(160, 160);
  };
  const auto savePng = [](const ImageBuffer& buffer, const QString& path) {
    QImage image(buffer.width, buffer.height, QImage::Format_ARGB32);
    for (int y = 0; y < buffer.height; ++y) {
      auto* row = reinterpret_cast<QRgb*>(image.scanLine(y));
      for (int x = 0; x < buffer.width; ++x) {
        const auto offset = static_cast<std::size_t>((y * buffer.width + x) * 3);
        const auto encode = [](const std::uint8_t channel) {
          const float linear = static_cast<float>(channel) / 255.0f;
          return static_cast<int>(std::lround(
              std::pow(linear, 1.0f / 2.2f) * 255.0f));
        };
        row[x] = qRgba(encode(buffer.pixels[offset]),
                       encode(buffer.pixels[offset + 1]),
                       encode(buffer.pixels[offset + 2]), 255);
      }
    }
    return image.save(path, "PNG");
  };
  const auto applyTestConfinement = [&](PyroSimulation& simulation,
      const float epsilon, const float dt, const bool interfaceGate) {
    if (epsilon <= 0.0f) return;
    const auto& density = simulation.fields().densityStorage();
    const auto& temperature = simulation.fields().temperatureStorage();
    auto& velocity = simulation.fields().velocityStorage();
    std::vector<PyroVec3> curl(velocity.size());
    std::vector<float> curlMagnitude(velocity.size());
    const auto at = [&](const int x, const int y, const int z) {
      return resolution.indexOf(std::clamp(x, 0, resolution.width - 1),
          std::clamp(y, 0, resolution.height - 1),
          std::clamp(z, 0, resolution.depth - 1));
    };
    const float inverseTwoCellWidth = 0.5f / domain.voxelSize;
    for (int z = 0; z < resolution.depth; ++z)
      for (int y = 0; y < resolution.height; ++y)
        for (int x = 0; x < resolution.width; ++x) {
          const auto index = at(x, y, z);
          const auto vxm = velocity[at(x - 1, y, z)];
          const auto vxp = velocity[at(x + 1, y, z)];
          const auto vym = velocity[at(x, y - 1, z)];
          const auto vyp = velocity[at(x, y + 1, z)];
          const auto vzm = velocity[at(x, y, z - 1)];
          const auto vzp = velocity[at(x, y, z + 1)];
          auto& omega = curl[index];
          omega.x = ((vyp.z - vym.z) - (vzp.y - vzm.y)) *
              inverseTwoCellWidth;
          omega.y = ((vzp.x - vzm.x) - (vxp.z - vxm.z)) *
              inverseTwoCellWidth;
          omega.z = ((vxp.y - vxm.y) - (vyp.x - vym.x)) *
              inverseTwoCellWidth;
          curlMagnitude[index] = std::sqrt(omega.x * omega.x +
              omega.y * omega.y + omega.z * omega.z);
        }
    for (int z = 0; z < resolution.depth; ++z)
      for (int y = 0; y < resolution.height; ++y)
        for (int x = 0; x < resolution.width; ++x) {
          const auto index = at(x, y, z);
          if (interfaceGate && temperature[index] <= 1.0e-4f) continue;
          const float gx = (curlMagnitude[at(x + 1, y, z)] -
              curlMagnitude[at(x - 1, y, z)]) * inverseTwoCellWidth;
          const float gy = (curlMagnitude[at(x, y + 1, z)] -
              curlMagnitude[at(x, y - 1, z)]) * inverseTwoCellWidth;
          const float gz = (curlMagnitude[at(x, y, z + 1)] -
              curlMagnitude[at(x, y, z - 1)]) * inverseTwoCellWidth;
          const float gradientLength = std::sqrt(gx * gx + gy * gy + gz * gz);
          if (gradientLength <= 1.0e-8f) continue;
          const float nx = gx / gradientLength;
          const float ny = gy / gradientLength;
          const float nz = gz / gradientLength;
          float gate = 1.0f;
          if (interfaceGate) {
            const float densityGradientX = (density[at(x + 1, y, z)] -
                density[at(x - 1, y, z)]) * inverseTwoCellWidth;
            const float densityGradientY = (density[at(x, y + 1, z)] -
                density[at(x, y - 1, z)]) * inverseTwoCellWidth;
            const float densityGradientZ = (density[at(x, y, z + 1)] -
                density[at(x, y, z - 1)]) * inverseTwoCellWidth;
            const float relativeGradient = domain.voxelSize *
                std::sqrt(densityGradientX * densityGradientX +
                    densityGradientY * densityGradientY +
                    densityGradientZ * densityGradientZ) /
                std::max(density[index], 1.0e-3f);
            gate = std::clamp(relativeGradient, 0.0f, 1.0f);
          }
          const auto omega = curl[index];
          const float strength = epsilon * domain.voxelSize * dt * gate;
          velocity[index].x += (ny * omega.z - nz * omega.y) * strength;
          velocity[index].y += (nz * omega.x - nx * omega.z) * strength;
          velocity[index].z += (nx * omega.y - ny * omega.x) * strength;
        }
  };
  const auto applyTestDensityShred = [&](PyroSimulation& simulation,
      const float amplitude, const int bandCount, const int step) {
    if (amplitude <= 0.0f || bandCount <= 0) return 0.0;
    auto& density = simulation.fields().densityStorage();
    const float simulatedTime = static_cast<float>(step) / 60.0f;
    const auto latticeHash = [](int x, int y, int z) {
      std::uint32_t hash = static_cast<std::uint32_t>(x) * 0x8da6b343u ^
          static_cast<std::uint32_t>(y) * 0xd8163841u ^
          static_cast<std::uint32_t>(z) * 0xcb1ab31fu ^ 0x9e3779b9u;
      hash ^= hash >> 16;
      hash *= 0x7feb352du;
      hash ^= hash >> 15;
      hash *= 0x846ca68bu;
      hash ^= hash >> 16;
      return 2.0f * static_cast<float>(hash & 0x00ffffffu) /
          static_cast<float>(0x00ffffffu) - 1.0f;
    };
    const auto smoothNoise = [&](const float px, const float py,
                                 const float pz) {
      const int x0 = static_cast<int>(std::floor(px));
      const int y0 = static_cast<int>(std::floor(py));
      const int z0 = static_cast<int>(std::floor(pz));
      const float tx0 = px - static_cast<float>(x0);
      const float ty0 = py - static_cast<float>(y0);
      const float tz0 = pz - static_cast<float>(z0);
      const float tx = tx0 * tx0 * (3.0f - 2.0f * tx0);
      const float ty = ty0 * ty0 * (3.0f - 2.0f * ty0);
      const float tz = tz0 * tz0 * (3.0f - 2.0f * tz0);
      const auto alongZ = [&](const int x, const int y) {
        return std::lerp(latticeHash(x, y, z0),
                         latticeHash(x, y, z0 + 1), tz);
      };
      return std::lerp(
          std::lerp(alongZ(x0, y0), alongZ(x0 + 1, y0), tx),
          std::lerp(alongZ(x0, y0 + 1), alongZ(x0 + 1, y0 + 1), tx), ty);
    };
    const double initialMass = std::accumulate(density.begin(), density.end(), 0.0);
    if (initialMass <= 1.0e-12) return 0.0;
    std::vector<float> modulation(density.size(), 0.0f);
    double densityWeightedModulation = 0.0;
    const float inverseTwoCellWidth = 0.5f / domain.voxelSize;
    const auto at = [&](const int x, const int y, const int z) {
      return resolution.indexOf(std::clamp(x, 0, resolution.width - 1),
          std::clamp(y, 0, resolution.height - 1),
          std::clamp(z, 0, resolution.depth - 1));
    };
    for (int z = 0; z < resolution.depth; ++z)
      for (int y = 0; y < resolution.height; ++y)
        for (int x = 0; x < resolution.width; ++x) {
          const auto index = at(x, y, z);
          if (density[index] <= 1.0e-4f) continue;
          const float gx = (density[at(x + 1, y, z)] -
              density[at(x - 1, y, z)]) * inverseTwoCellWidth;
          const float gy = (density[at(x, y + 1, z)] -
              density[at(x, y - 1, z)]) * inverseTwoCellWidth;
          const float gz = (density[at(x, y, z + 1)] -
              density[at(x, y, z - 1)]) * inverseTwoCellWidth;
          const float relativeGradient = domain.voxelSize *
              std::sqrt(gx * gx + gy * gy + gz * gz) /
              std::max(density[index], 1.0e-3f);
          const float interfaceWeight = std::clamp(relativeGradient, 0.0f, 1.0f);
          const float px = static_cast<float>(x) + 0.07f * simulatedTime /
              (domain.voxelSize * 0.24f);
          const float py = static_cast<float>(y) - 0.11f * simulatedTime /
              (domain.voxelSize * 0.24f);
          const float pz = static_cast<float>(z) + 0.05f * simulatedTime /
              (domain.voxelSize * 0.24f);
          float noise = smoothNoise(px / 8.0f + 4.1f,
              py / 8.0f - 2.7f, pz / 8.0f + 1.9f);
          if (bandCount >= 2) {
            noise = 0.58f * noise + 0.42f * smoothNoise(
                px / 4.0f - 7.3f, py / 4.0f + 5.2f, pz / 4.0f - 3.6f);
          }
          if (bandCount >= 3) {
            noise = 0.58f * noise + 0.42f * smoothNoise(
                px / 2.5f + 9.6f, py / 2.5f - 8.8f, pz / 2.5f + 6.4f);
          }
          modulation[index] = interfaceWeight * noise;
          densityWeightedModulation += density[index] * modulation[index];
        }
    const float meanModulation = static_cast<float>(
        densityWeightedModulation / initialMass);
    double finalMass = 0.0;
    for (std::size_t index = 0; index < density.size(); ++index) {
      density[index] *= 1.0f + amplitude *
          (modulation[index] - meanModulation);
      finalMass += density[index];
    }
    return std::abs(finalMass - initialMass) / initialMass;
  };
  for (const auto& condition : conditions) {
    PyroSimulation simulation(domain);
    auto settings = sourceSettings;
    settings.buoyancy = condition.buoyancy;
    settings.dissipation = condition.dissipation;
    settings.vorticity = condition.vorticity;
    settings.pressureIterations = static_cast<float>(condition.pressureIterations);
    simulation.setSettings(settings);
    simulation.fields().resize(pyroResolution);
    simulation.fields().densityStorage() = pulseDensity;
    simulation.fields().temperatureStorage() = pulseTemperature;
    simulation.fields().fuelStorage() = pulseFuel;
    simulation.fields().velocityStorage() = pulseVelocity;
    simulation.fields().pressureStorage() = emitter.fields().pressureStorage();
    simulation.fields().divergenceStorage() = emitter.fields().divergenceStorage();
    if (condition.clearVelocity)
      std::fill(simulation.fields().velocityStorage().begin(),
          simulation.fields().velocityStorage().end(), PyroVec3{});
    double firstStepVelocityRms = 0.0;
    double firstStepMaximumVelocity = 0.0;
    std::size_t firstStepTemperatureSupportCells = 0;
    double firstStepTemperatureIntegral = 0.0;
    double firstStepTemperatureWeightedVerticalVelocity = 0.0;
    double firstStepTemperatureWeightedVorticity = 0.0;
    double firstStepMaximumActiveVelocity = 0.0;
    double firstStepDivergenceBeforeProjectionRms = 0.0;
    double firstStepDivergenceAfterProjectionRms = 0.0;
    double maximumShredMassError = 0.0;
    double checkpointDensityChange = 0.0;
    std::vector<float> firstShredCheckpoint;
    for (int step = 0; step < 60; ++step) {
      simulation.setSources(std::span<const PyroSourceState>{});
      maximumShredMassError = std::max(maximumShredMassError,
          applyTestDensityShred(simulation, condition.densityShredAmplitude,
              condition.densityShredBands, step));
      applyTestConfinement(simulation, condition.testConfinement,
          1.0f / 60.0f, condition.interfaceGate);
      simulation.step(1.0 / 60.0);
      if (step == 29 && condition.captureShredTemporalMetric) {
        firstShredCheckpoint = simulation.fields().densityStorage();
        if (!outputDirectory.isEmpty()) {
          const auto checkpointDirectory = QDir(outputDirectory).filePath(
              QStringLiteral("3d-emitter-decay-rnd"));
          ASSERT_TRUE(QDir().mkpath(checkpointDirectory));
          const auto checkpointFront = render(firstShredCheckpoint.data(), false);
          const auto checkpointSide = render(firstShredCheckpoint.data(), true);
          EXPECT_TRUE(savePng(checkpointFront, QDir(checkpointDirectory).filePath(
              QStringLiteral("post_pulse_%1_half_front.png").arg(condition.label))));
          EXPECT_TRUE(savePng(checkpointSide, QDir(checkpointDirectory).filePath(
              QStringLiteral("post_pulse_%1_half_side.png").arg(condition.label))));
        }
      }
      if (step == 59 && !firstShredCheckpoint.empty()) {
        const auto& finalCheckpoint = simulation.fields().densityStorage();
        double absoluteChange = 0.0;
        double checkpointMass = 0.0;
        for (std::size_t i = 0; i < finalCheckpoint.size(); ++i) {
          absoluteChange += std::abs(finalCheckpoint[i] - firstShredCheckpoint[i]);
          checkpointMass += firstShredCheckpoint[i];
        }
        checkpointDensityChange = absoluteChange /
            std::max(1.0e-12, 2.0 * checkpointMass);
      }
      if (step == 0) {
        const auto& firstVelocity = simulation.fields().velocityStorage();
        const auto& preProjectionDivergence =
            simulation.fields().divergenceStorage();
        const auto& firstTemperature =
            simulation.fields().temperatureStorage();
        double velocitySquared = 0.0;
        double divergenceBeforeSquared = 0.0;
        double divergenceAfterSquared = 0.0;
        for (int z = 0; z < resolution.depth; ++z)
          for (int y = 0; y < resolution.height; ++y)
            for (int x = 0; x < resolution.width; ++x) {
              const auto index = resolution.indexOf(x, y, z);
              const auto v = firstVelocity[index];
              velocitySquared += static_cast<double>(v.x) * v.x +
                  static_cast<double>(v.y) * v.y +
                  static_cast<double>(v.z) * v.z;
              firstStepMaximumVelocity = std::max(firstStepMaximumVelocity,
                  std::sqrt(static_cast<double>(v.x) * v.x +
                      static_cast<double>(v.y) * v.y +
                      static_cast<double>(v.z) * v.z));
              divergenceBeforeSquared +=
                  static_cast<double>(preProjectionDivergence[index]) *
                  preProjectionDivergence[index];
              const int xm = std::max(0, x - 1), xp =
                  std::min(resolution.width - 1, x + 1);
              const int ym = std::max(0, y - 1), yp =
                  std::min(resolution.height - 1, y + 1);
              const int zm = std::max(0, z - 1), zp =
                  std::min(resolution.depth - 1, z + 1);
              const auto vxm = firstVelocity[resolution.indexOf(xm, y, z)];
              const auto vxp = firstVelocity[resolution.indexOf(xp, y, z)];
              const auto vym = firstVelocity[resolution.indexOf(x, ym, z)];
              const auto vyp = firstVelocity[resolution.indexOf(x, yp, z)];
              const auto vzm = firstVelocity[resolution.indexOf(x, y, zm)];
              const auto vzp = firstVelocity[resolution.indexOf(x, y, zp)];
              const float temperature = firstTemperature[index];
              if (temperature > 1.0e-4f) {
                const double speed = std::sqrt(static_cast<double>(v.x) * v.x +
                    static_cast<double>(v.y) * v.y +
                    static_cast<double>(v.z) * v.z);
                const double curlX = 0.5 * ((vyp.z - vym.z) - (vzp.y - vzm.y));
                const double curlY = 0.5 * ((vzp.x - vzm.x) - (vxp.z - vxm.z));
                const double curlZ = 0.5 * ((vxp.y - vxm.y) - (vyp.x - vym.x));
                firstStepTemperatureIntegral += temperature;
                firstStepTemperatureWeightedVerticalVelocity +=
                    temperature * v.y;
                firstStepTemperatureWeightedVorticity += temperature *
                    std::sqrt(curlX * curlX + curlY * curlY + curlZ * curlZ);
                firstStepMaximumActiveVelocity = std::max(
                    firstStepMaximumActiveVelocity, speed);
                ++firstStepTemperatureSupportCells;
              }
              const double divergence = 0.5 * ((vxp.x - vxm.x) +
                  (vyp.y - vym.y) + (vzp.z - vzm.z));
              divergenceAfterSquared += divergence * divergence;
            }
        firstStepVelocityRms = std::sqrt(velocitySquared / firstVelocity.size());
        firstStepDivergenceBeforeProjectionRms = std::sqrt(
            divergenceBeforeSquared / preProjectionDivergence.size());
        firstStepDivergenceAfterProjectionRms = std::sqrt(
            divergenceAfterSquared / firstVelocity.size());
      }
    }

    const auto& density = simulation.fields().densityStorage();
    const auto& velocity = simulation.fields().velocityStorage();
    double mass = 0.0, weightedY = 0.0, boundaryMass = 0.0;
    double maximumDensity = 0.0, kineticSquared = 0.0;
    double maximumTemperature = 0.0, densityWeightedSpeed = 0.0;
    double temperatureWeightedSpeed = 0.0, temperatureMass = 0.0;
    double maximumVelocity = 0.0;
    double densityLaplacianSquared = 0.0;
    std::size_t densityLaplacianSamples = 0;
    for (int z = 0; z < resolution.depth; ++z)
      for (int y = 0; y < resolution.height; ++y)
        for (int x = 0; x < resolution.width; ++x) {
          const auto index = resolution.indexOf(x, y, z);
          const double rho = density[index];
          mass += rho;
          weightedY += rho * y;
          maximumDensity = std::max(maximumDensity, rho);
          const auto v = velocity[index];
          const double speed = std::sqrt(static_cast<double>(v.x) * v.x +
              static_cast<double>(v.y) * v.y + static_cast<double>(v.z) * v.z);
          kineticSquared += speed * speed;
          maximumVelocity = std::max(maximumVelocity, speed);
          densityWeightedSpeed += rho * speed;
          const double temperature = simulation.fields().temperatureStorage()[index];
          temperatureMass += temperature;
          temperatureWeightedSpeed += temperature * speed;
          maximumTemperature = std::max(maximumTemperature, temperature);
          if (x > 0 && x < resolution.width - 1 && y > 0 &&
              y < resolution.height - 1 && z > 0 && z < resolution.depth - 1) {
            const double densityLaplacian =
                density[resolution.indexOf(x - 1, y, z)] +
                density[resolution.indexOf(x + 1, y, z)] +
                density[resolution.indexOf(x, y - 1, z)] +
                density[resolution.indexOf(x, y + 1, z)] +
                density[resolution.indexOf(x, y, z - 1)] +
                density[resolution.indexOf(x, y, z + 1)] - 6.0 * rho;
            densityLaplacianSquared += densityLaplacian * densityLaplacian;
            ++densityLaplacianSamples;
          }
          if (x < 2 || x >= resolution.width - 2 ||
              y < 2 || y >= resolution.height - 2 ||
              z < 2 || z >= resolution.depth - 2)
            boundaryMass += rho;
        }
    ASSERT_GT(mass, 0.0);
    auto front = render(const_cast<float*>(density.data()), false);
    auto side = render(const_cast<float*>(density.data()), true);
    GTEST_LOG_(INFO) << "Pyro post-pulse branch=" << condition.label
        << " pulseTempMass/max/support=" << pulseTemperatureIntegral << '/'
        << pulseTemperatureMaximum << '/' << pulseTemperatureSupportCells
        << " mass=" << mass << " retention=" << mass / initialMass
        << " centroidYworld=" << domain.min.y +
            (weightedY / mass + 0.5) * domain.voxelSize
        << " boundaryMassFraction=" << boundaryMass / mass
        << " maxDensity/temperature=" << maximumDensity << '/'
        << maximumTemperature << " rmsVelocity="
        << std::sqrt(kineticSquared / density.size())
        << " maxVelocity/rhoWeightedSpeed/tempWeightedSpeed="
        << maximumVelocity << '/' << densityWeightedSpeed / mass << '/'
        << temperatureWeightedSpeed / std::max(1.0e-12, temperatureMass)
        << " firstStepRmsVelocity/divergenceBeforeAfter="
        << firstStepVelocityRms << '/' << firstStepMaximumVelocity << '/'
        << firstStepDivergenceBeforeProjectionRms << '/'
        << firstStepDivergenceAfterProjectionRms
        << " activeTempCells/activeMaxSpeed/tempWeightedVy/tempWeightedCurl="
        << firstStepTemperatureSupportCells << '/'
        << firstStepMaximumActiveVelocity << '/'
        << firstStepTemperatureWeightedVerticalVelocity /
            std::max(1.0e-12, firstStepTemperatureIntegral) << '/'
        << firstStepTemperatureWeightedVorticity /
            std::max(1.0e-12, firstStepTemperatureIntegral)
        << " densityLaplacianRms/shredMassError/halfRunDensityChange="
        << std::sqrt(densityLaplacianSquared /
            std::max<std::size_t>(1, densityLaplacianSamples)) << '/'
        << maximumShredMassError << '/' << checkpointDensityChange;
    if (!outputDirectory.isEmpty()) {
      const auto captureDirectory = QDir(outputDirectory).filePath(
          QStringLiteral("3d-emitter-decay-rnd"));
      ASSERT_TRUE(QDir().mkpath(captureDirectory));
      EXPECT_TRUE(savePng(front, QDir(captureDirectory).filePath(
          QStringLiteral("post_pulse_%1_front.png").arg(condition.label))));
      EXPECT_TRUE(savePng(side, QDir(captureDirectory).filePath(
          QStringLiteral("post_pulse_%1_side.png").arg(condition.label))));
    }
  }
}
