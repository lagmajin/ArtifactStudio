#include <gtest/gtest.h>

#include <QFile>
#include <QImage>
#include <QColor>
#include <QDir>
#include <QFileInfo>
#include <QSize>
#include <QString>
#include <array>
#include <algorithm>
#include <cstddef>
#include <cmath>
#include <cstdint>
#include <limits>

import Physics.Fluid;
import Container.NamedVector;

namespace {

float signedCurlAt(const ArtifactCore::FluidSnapshot2D& field,
                   const int x, const int y) {
  const auto index = [width = field.width](const int px, const int py) {
    return static_cast<std::size_t>(px + py * width);
  };
  const float dvDx =
      (field.velocityY[index(x + 1, y)] - field.velocityY[index(x - 1, y)]) *
      0.5f;
  const float duDy =
      (field.velocityX[index(x, y + 1)] - field.velocityX[index(x, y - 1)]) *
      0.5f;
  return dvDx - duDy;
}

// Test-only reference operator: retain signed vorticity when applying the
// standard confinement force. The production solver remains untouched.
void applySignedVorticityConfinement(ArtifactCore::FluidSolver2D& solver,
                                     const float dt,
                                     const float strength) {
  const auto field = solver.snapshot();
  const int width = field.width;
  const int height = field.height;
  const auto magnitudeAt = [&](const int x, const int y) {
    return std::abs(signedCurlAt(field, x, y));
  };
  for (int y = 2; y < height - 2; ++y) {
    for (int x = 2; x < width - 2; ++x) {
      const float dx = (magnitudeAt(x + 1, y) - magnitudeAt(x - 1, y)) * 0.5f;
      const float dy = (magnitudeAt(x, y + 1) - magnitudeAt(x, y - 1)) * 0.5f;
      float normalizedX = dx;
      float normalizedY = dy;
      const float length = std::sqrt(dx * dx + dy * dy) + 1.0e-5f;
      normalizedX /= length;
      normalizedY /= length;
      const float omega = signedCurlAt(field, x, y);
      solver.addVelocity(x, y, normalizedY * omega * strength * dt,
                         -normalizedX * omega * strength * dt);
    }
  }
}

void applySmokeBuoyancy(ArtifactCore::FluidSolver2D& solver,
                        const float strength, const float dt) {
  for (int y = 0; y < solver.height(); ++y) {
    for (int x = 0; x < solver.width(); ++x) {
      solver.addVelocity(x, y, 0.0f,
                         -solver.getDensity(x, y) * strength * dt);
    }
  }
}

QImage renderSmokeDensity(const ArtifactCore::FluidSolver2D& solver,
                          const int scale = 4,
                          const float exposureScale = 1.0f) {
  QImage image(solver.width() * scale, solver.height() * scale,
               QImage::Format_ARGB32);
  image.fill(qRgba(8, 12, 18, 255));
  for (int y = 1; y < solver.height() - 1; ++y) {
    for (int x = 1; x < solver.width() - 1; ++x) {
      const float density = std::clamp(
          solver.getDensity(x, y) * exposureScale, 0.0f, 1.0f);
      const int red = static_cast<int>(8.0f + density * 120.0f);
      const int green = static_cast<int>(12.0f + density * 220.0f);
      const int blue = static_cast<int>(18.0f + density * 150.0f);
      const QRgb pixel = qRgba(red, green, blue, 255);
      for (int oy = 0; oy < scale; ++oy) {
        auto* row = reinterpret_cast<QRgb*>(image.scanLine(y * scale + oy));
        for (int ox = 0; ox < scale; ++ox) row[x * scale + ox] = pixel;
      }
    }
  }
  return image;
}

double smokeMass(const ArtifactCore::FluidSolver2D& solver) {
  double mass = 0.0;
  for (int y = 1; y < solver.height() - 1; ++y) {
    for (int x = 1; x < solver.width() - 1; ++x) {
      mass += std::max(0.0f, solver.getDensity(x, y));
    }
  }
  return mass;
}

void injectSmokeTemperature(
    ArtifactCore::NamedVector<float>& temperature, const int width,
    const int height, const ArtifactCore::SmokeEmitter2D& emitter) {
  const float x = std::clamp(emitter.x, 0.0f, 1.0f);
  const float y = std::clamp(emitter.y, 0.0f, 1.0f);
  const float radius = std::clamp(emitter.radius, 0.0f, 1.0f);
  const int centerX = 1 + static_cast<int>(
      std::round(x * static_cast<float>(width - 3)));
  const int centerY = 1 + static_cast<int>(
      std::round(y * static_cast<float>(height - 3)));
  const float radiusX = std::max(radius * static_cast<float>(width - 2), 0.5f);
  const float radiusY = std::max(radius * static_cast<float>(height - 2), 0.5f);
  const int extentX = static_cast<int>(std::ceil(radiusX));
  const int extentY = static_cast<int>(std::ceil(radiusY));
  for (int yCell = std::max(1, centerY - extentY);
       yCell <= std::min(height - 2, centerY + extentY); ++yCell) {
    for (int xCell = std::max(1, centerX - extentX);
         xCell <= std::min(width - 2, centerX + extentX); ++xCell) {
      const float dx = static_cast<float>(xCell - centerX) / radiusX;
      const float dy = static_cast<float>(yCell - centerY) / radiusY;
      const float distanceSquared = dx * dx + dy * dy;
      if (distanceSquared > 1.0f) continue;
      temperature[static_cast<std::size_t>(xCell + yCell * width)] +=
          (1.0f - distanceSquared) * std::max(0.0f, emitter.density);
    }
  }
}

void applyTemperatureBuoyancy(ArtifactCore::FluidSolver2D& solver,
                              const ArtifactCore::NamedVector<float>& temperature,
                              const float buoyancy, const float dt) {
  for (int y = 1; y < solver.height() - 1; ++y) {
    for (int x = 1; x < solver.width() - 1; ++x) {
      const float heat = temperature[
          static_cast<std::size_t>(x + y * solver.width())];
      solver.addVelocity(x, y, 0.0f, -heat * buoyancy * dt);
    }
  }
}

float smokeSmoothValueNoise(float x, float y, std::uint32_t seed);

// Test-only low/mid frequency forcing built as the curl of a scalar stream
// function, so each mode adds rotational motion without directly injecting
// divergent velocity. Temperature masks the force to the hot plume.
void applyTemperatureCurlTurbulence(
    ArtifactCore::FluidSolver2D& solver,
    const ArtifactCore::NamedVector<float>& temperature, const float dt,
    const float simulationTime, const float lowFrequencyStrength,
    const float midFrequencyStrength, const float phaseOffset,
    const bool useNonPeriodicNoise,
    ArtifactCore::NamedVector<float>& streamFunction) {
  const int width = solver.width();
  const int height = solver.height();
  const auto index = [width](const int x, const int y) {
    return static_cast<std::size_t>(x + y * width);
  };
  constexpr float twoPi = 6.2831853071795864769f;
  struct Mode final {
    float wavelengthX;
    float wavelengthY;
    float temporalRate;
    float strength;
    float phase;
  };
  const std::array<Mode, 3> modes{{
      {25.0f, 21.0f, 0.65f, lowFrequencyStrength, 0.3f},
      {19.0f, 28.0f, -0.47f, lowFrequencyStrength * 0.63f, 2.1f},
      {10.0f, 13.0f, 0.91f, midFrequencyStrength, 1.4f},
  }};
  std::fill(streamFunction.begin(), streamFunction.end(), 0.0f);
  for (const Mode& mode : modes) {
    if (mode.strength == 0.0f) continue;
    const float kx = twoPi / mode.wavelengthX;
    const float ky = twoPi / mode.wavelengthY;
    const float phase = mode.phase + phaseOffset +
        simulationTime * mode.temporalRate;
    const std::uint32_t noiseSeed = 0xB5297A4Du ^
        static_cast<std::uint32_t>(std::lround((mode.phase + 1.0f) * 1000.0f)) ^
        static_cast<std::uint32_t>(std::lround(phaseOffset * 997.0f));
    for (int y = 1; y < height - 1; ++y) {
      for (int x = 1; x < width - 1; ++x) {
        const auto cell = index(x, y);
        const float envelope = std::clamp(temperature[cell] * 3.0f,
                                          0.0f, 1.0f);
        if (envelope <= 0.0f) continue;
        const float fieldValue = useNonPeriodicNoise
            ? smokeSmoothValueNoise(
                  static_cast<float>(x) / mode.wavelengthX +
                      simulationTime * mode.temporalRate * 0.11f,
                  static_cast<float>(y) / mode.wavelengthY -
                      simulationTime * mode.temporalRate * 0.07f,
                  noiseSeed)
            : std::sin(kx * static_cast<float>(x) + phase) *
                  std::sin(ky * static_cast<float>(y) - phase * 0.73f);
        streamFunction[cell] += mode.strength * envelope * fieldValue;
      }
    }
  }
  for (int y = 2; y < height - 2; ++y) {
    for (int x = 2; x < width - 2; ++x) {
      const float streamX = (streamFunction[index(x + 1, y)] -
                             streamFunction[index(x - 1, y)]) * 0.5f;
      const float streamY = (streamFunction[index(x, y + 1)] -
                             streamFunction[index(x, y - 1)]) * 0.5f;
      solver.addVelocity(x, y, streamY * dt, -streamX * dt);
    }
  }
}

void advectSmokeTemperature(ArtifactCore::NamedVector<float>& temperature,
                            ArtifactCore::NamedVector<float>& next,
                            const ArtifactCore::FluidSolver2D& solver,
                            const float dt, const float coolingRate) {
  const auto velocity = solver.snapshot();
  const int width = solver.width();
  const int height = solver.height();
  const float dtx = dt * static_cast<float>(width - 2);
  const float dty = dt * static_cast<float>(height - 2);
  const float retention = std::exp(-std::max(0.0f, coolingRate) * dt);
  std::fill(next.begin(), next.end(), 0.0f);
  const auto index = [width](const int x, const int y) {
    return static_cast<std::size_t>(x + y * width);
  };
  for (int y = 1; y < height - 1; ++y) {
    for (int x = 1; x < width - 1; ++x) {
      const auto cell = index(x, y);
      float backX = static_cast<float>(x) -
                    dtx * velocity.velocityX[cell];
      float backY = static_cast<float>(y) -
                    dty * velocity.velocityY[cell];
      backX = std::clamp(backX, 0.5f, static_cast<float>(width) - 1.5f);
      backY = std::clamp(backY, 0.5f, static_cast<float>(height) - 1.5f);
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
      next[cell] = std::lerp(top, bottom, ty) * retention;
    }
  }
  temperature = next;
}

double smokeCentroidY(const ArtifactCore::FluidSolver2D& solver) {
  double weightedY = 0.0;
  double mass = 0.0;
  for (int y = 1; y < solver.height() - 1; ++y) {
    for (int x = 1; x < solver.width() - 1; ++x) {
      const double density = std::max(0.0f, solver.getDensity(x, y));
      weightedY += density * static_cast<double>(y);
      mass += density;
    }
  }
  return mass > 0.0 ? weightedY / mass : 0.0;
}

double smokeDensityGradientRms(const ArtifactCore::FluidSolver2D& solver) {
  double squaredGradientSum = 0.0;
  std::size_t sampleCount = 0;
  for (int y = 1; y < solver.height() - 1; ++y) {
    for (int x = 1; x < solver.width() - 1; ++x) {
      const double gradientX = 0.5 * (solver.getDensity(x + 1, y) -
                                      solver.getDensity(x - 1, y));
      const double gradientY = 0.5 * (solver.getDensity(x, y + 1) -
                                      solver.getDensity(x, y - 1));
      squaredGradientSum += gradientX * gradientX + gradientY * gradientY;
      ++sampleCount;
    }
  }
  return sampleCount > 0
      ? std::sqrt(squaredGradientSum / static_cast<double>(sampleCount))
      : 0.0;
}

double smokeMaximumCflCells(const ArtifactCore::FluidSolver2D& solver,
                            const float dt) {
  double maximum = 0.0;
  const double scaleX = static_cast<double>(solver.width() - 2) * dt;
  const double scaleY = static_cast<double>(solver.height() - 2) * dt;
  for (int y = 1; y < solver.height() - 1; ++y) {
    for (int x = 1; x < solver.width() - 1; ++x) {
      float velocityX = 0.0f;
      float velocityY = 0.0f;
      solver.getVelocity(x, y, velocityX, velocityY);
      maximum = std::max(maximum, std::hypot(
          static_cast<double>(velocityX) * scaleX,
          static_cast<double>(velocityY) * scaleY));
    }
  }
  return maximum;
}

double scalarMass(const ArtifactCore::NamedVector<float>& field) {
  double mass = 0.0;
  for (const float value : field) mass += std::max(0.0f, value);
  return mass;
}

float smokeHashValue(const int x, const int y, const std::uint32_t seed) {
  std::uint32_t hash = seed ^ static_cast<std::uint32_t>(x) * 0x9E3779B9u ^
      static_cast<std::uint32_t>(y) * 0x85EBCA6Bu;
  hash ^= hash >> 16u;
  hash *= 0x7FEB352Du;
  hash ^= hash >> 15u;
  hash *= 0x846CA68Bu;
  hash ^= hash >> 16u;
  return static_cast<float>(hash & 0x00FFFFFFu) / 8388607.5f - 1.0f;
}

float smokeSmoothValueNoise(const float x, const float y,
                            const std::uint32_t seed) {
  const int x0 = static_cast<int>(std::floor(x));
  const int y0 = static_cast<int>(std::floor(y));
  const float rawX = x - static_cast<float>(x0);
  const float rawY = y - static_cast<float>(y0);
  const float tx = rawX * rawX * (3.0f - 2.0f * rawX);
  const float ty = rawY * rawY * (3.0f - 2.0f * rawY);
  const float top = std::lerp(smokeHashValue(x0, y0, seed),
                              smokeHashValue(x0 + 1, y0, seed), tx);
  const float bottom = std::lerp(smokeHashValue(x0, y0 + 1, seed),
                                 smokeHashValue(x0 + 1, y0 + 1, seed), tx);
  return std::lerp(top, bottom, ty);
}

}  // namespace

TEST(SmokeEmitterStandaloneTest,
     MeasuresSmokeEmitterRateAcrossCompositionFrameRates) {
  constexpr int gridWidth = 64;
  constexpr int gridHeight = 96;
  constexpr float durationSeconds = 2.0f;
  constexpr std::array<int, 5> frameRates{10, 15, 20, 30, 60};
  for (const int frameRate : frameRates) {
    ArtifactCore::FluidSolver2D solver(gridWidth, gridHeight);
    solver.setViscosity(0.00001f);
    solver.setDiffusion(0.00001f);
    solver.setBuoyancy(0.05f);
    solver.setVorticity(0.1f);
    solver.setSolverIterations(20);
    const float frameDt = 1.0f / static_cast<float>(frameRate);
    const float emissionTimeScale = frameDt * 30.0f;
    const float densityRetention = std::exp(-1.0f * frameDt);
    double solverAdvancedSeconds = 0.0;
    for (int frame = 0; frame < static_cast<int>(durationSeconds * frameRate);
         ++frame) {
      const ArtifactCore::SmokeEmitter2D emitter{
          0.5f, 0.86f, 0.06f, 0.25f * emissionTimeScale,
          0.0f, -0.12f, 0.25f};
      ArtifactCore::injectSmokeEmitter2D(
          solver, emitter, emissionTimeScale);
      solver.update(frameDt);
      solverAdvancedSeconds += std::min(frameDt, 0.05f);
      solver.scaleDensity(densityRetention);
    }

    const auto snapshot = solver.snapshot();
    double densityMass = 0.0;
    double densityWeightedY = 0.0;
    float peakDensity = 0.0f;
    float minDensity = std::numeric_limits<float>::infinity();
    float maxVelocity = 0.0f;
    for (int y = 1; y < gridHeight - 1; ++y) {
      for (int x = 1; x < gridWidth - 1; ++x) {
        const auto cell = static_cast<std::size_t>(x + y * gridWidth);
        const float density = snapshot.density[cell];
        densityMass += density;
        densityWeightedY += density * static_cast<double>(y);
        peakDensity = std::max(peakDensity, density);
        minDensity = std::min(minDensity, density);
        maxVelocity = std::max(maxVelocity, std::hypot(
            snapshot.velocityX[cell], snapshot.velocityY[cell]));
      }
    }
    const double centerY = densityMass > 0.0
        ? densityWeightedY / densityMass : 0.0;
    GTEST_LOG_(INFO) << "production smoke emitter rate frameRate=" << frameRate
        << " frames=" << static_cast<int>(durationSeconds * frameRate)
        << " requestedSeconds=" << durationSeconds
        << " solverAdvancedSeconds=" << solverAdvancedSeconds
        << " densityMass=" << densityMass
        << " densityCenterY=" << centerY << " peakDensity=" << peakDensity
        << " minDensity=" << minDensity << " maxVelocity=" << maxVelocity;
    EXPECT_TRUE(std::isfinite(densityMass));
    EXPECT_TRUE(std::isfinite(centerY));
    EXPECT_TRUE(std::isfinite(peakDensity));
    EXPECT_TRUE(std::isfinite(maxVelocity));
  }
}

TEST(SmokeEmitterStandaloneTest, InjectsDensityAtNormalizedSourcePosition) {
  ArtifactCore::FluidSolver2D solver(32, 24);
  ArtifactCore::SmokeEmitter2D emitter;
  emitter.x = 0.25f;
  emitter.y = 0.5f;
  emitter.radius = 0.08f;
  emitter.density = 2.0f;

  ArtifactCore::injectSmokeEmitter2D(solver, emitter);

  EXPECT_GT(solver.getDensity(8, 12), 0.0f);
  EXPECT_FLOAT_EQ(solver.getDensity(16, 12), 0.0f);
}

TEST(SmokeEmitterStandaloneTest, VelocityFollowsEmitterDirectionAndScale) {
  ArtifactCore::FluidSolver2D solver(32, 32);
  ArtifactCore::SmokeEmitter2D emitter;
  emitter.x = 0.5f;
  emitter.y = 0.5f;
  emitter.radius = 0.05f;
  emitter.velocityX = 2.0f;
  emitter.velocityY = -3.0f;

  ArtifactCore::injectSmokeEmitter2D(solver, emitter, 0.5f);

  float vx = 0.0f;
  float vy = 0.0f;
  solver.getVelocity(16, 16, vx, vy);
  EXPECT_GT(vx, 0.0f);
  EXPECT_LT(vy, 0.0f);
}

TEST(SmokeEmitterStandaloneTest, EmitterSwirlAddsTangentialVelocity) {
  ArtifactCore::FluidSolver2D solver(32, 32);
  ArtifactCore::SmokeEmitter2D emitter;
  emitter.x = 0.5f;
  emitter.y = 0.5f;
  emitter.radius = 0.25f;
  emitter.density = 1.0f;
  emitter.velocityX = 0.0f;
  emitter.velocityY = 0.0f;
  emitter.swirl = 1.0f;

  ArtifactCore::injectSmokeEmitter2D(solver, emitter);

  float vx = 0.0f;
  float vy = 0.0f;
  solver.getVelocity(20, 16, vx, vy);
  EXPECT_GT(vy, 0.0f)
      << "the right side of a positive swirl should rotate downward";
}

TEST(SmokeEmitterStandaloneTest, DensityScalingIsBoundedAndFinite) {
  ArtifactCore::FluidSolver2D solver(16, 12);
  solver.addDensity(8, 6, 4.0f);
  solver.scaleDensity(0.5f);
  EXPECT_FLOAT_EQ(solver.getDensity(8, 6), 2.0f);

  solver.scaleDensity(2.0f);
  EXPECT_FLOAT_EQ(solver.getDensity(8, 6), 2.0f);
  solver.scaleDensity(-1.0f);
  EXPECT_FLOAT_EQ(solver.getDensity(8, 6), 0.0f);

  solver.addDensity(8, 6, 3.0f);
  solver.scaleDensity(std::numeric_limits<float>::quiet_NaN());
  EXPECT_FLOAT_EQ(solver.getDensity(8, 6), 3.0f);
}

TEST(SmokeEmitterStandaloneTest, SmokeStirInjectsLocalizedVelocity) {
  ArtifactCore::FluidSolver2D solver(40, 32);
  ArtifactCore::SmokeStir2D stir;
  stir.x = 0.5f;
  stir.y = 0.5f;
  stir.radius = 0.1f;
  stir.velocityX = 2.0f;
  stir.velocityY = -1.0f;
  stir.strength = 1.5f;

  ArtifactCore::injectSmokeStir2D(solver, stir);

  float centerX = 0.0f;
  float centerY = 0.0f;
  solver.getVelocity(20, 16, centerX, centerY);
  EXPECT_GT(centerX, 0.0f);
  EXPECT_LT(centerY, 0.0f);
  float outsideX = 0.0f;
  float outsideY = 0.0f;
  solver.getVelocity(3, 3, outsideX, outsideY);
  EXPECT_FLOAT_EQ(outsideX, 0.0f);
  EXPECT_FLOAT_EQ(outsideY, 0.0f);

  ArtifactCore::FluidSolver2D tiny(2, 2);
  ArtifactCore::injectSmokeStir2D(tiny, stir);
  tiny.getVelocity(0, 0, outsideX, outsideY);
  EXPECT_FLOAT_EQ(outsideX, 0.0f);
  EXPECT_FLOAT_EQ(outsideY, 0.0f);

  stir.x = std::numeric_limits<float>::quiet_NaN();
  ArtifactCore::FluidSolver2D nonFinite(40, 32);
  ArtifactCore::injectSmokeStir2D(nonFinite, stir);
  nonFinite.getVelocity(20, 16, outsideX, outsideY);
  EXPECT_FLOAT_EQ(outsideX, 0.0f);
  EXPECT_FLOAT_EQ(outsideY, 0.0f);
}

TEST(SmokeEmitterStandaloneTest, ClampsSourceToInteriorAndRejectsTinyGrids) {
  ArtifactCore::FluidSolver2D solver(8, 8);
  ArtifactCore::SmokeEmitter2D emitter;
  emitter.x = -10.0f;
  emitter.y = 10.0f;
  emitter.radius = 0.0f;

  ArtifactCore::injectSmokeEmitter2D(solver, emitter);
  EXPECT_GT(solver.getDensity(1, 6), 0.0f);

  ArtifactCore::FluidSolver2D tiny(2, 2);
  ArtifactCore::injectSmokeEmitter2D(tiny, emitter);
  EXPECT_FLOAT_EQ(tiny.getDensity(0, 0), 0.0f);
}

TEST(SmokeEmitterStandaloneTest, ExportsImageAndChecksVisualSignal) {
  constexpr int gridWidth = 96;
  constexpr int gridHeight = 72;
  constexpr int imageScale = 8;
  constexpr int frameCount = 24;
  ArtifactCore::FluidSolver2D solver(gridWidth, gridHeight);
  solver.setViscosity(0.00001f);
  solver.setDiffusion(0.00001f);
  solver.setBuoyancy(0.05f);
  solver.setVorticity(0.1f);
  ArtifactCore::SmokeEmitter2D emitter;
  emitter.x = 0.5f;
  emitter.y = 0.84f;
  emitter.radius = 0.08f;
  emitter.density = 0.6f;
  emitter.velocityY = -1.2f;

  for (int frame = 0; frame < frameCount; ++frame) {
    ArtifactCore::injectSmokeEmitter2D(solver, emitter);
    solver.update(1.0f / 30.0f);
  }

  const auto rasterizeDensity = [&](const ArtifactCore::FluidSolver2D& source) {
    QImage output(gridWidth * imageScale, gridHeight * imageScale,
                  QImage::Format_ARGB32);
    output.fill(QColor(12, 16, 24, 255));
    for (int y = 1; y < gridHeight - 1; ++y) {
      for (int x = 1; x < gridWidth - 1; ++x) {
        const float density =
            std::clamp(source.getDensity(x, y), 0.0f, 1.0f);
        const int red = static_cast<int>(
            std::clamp(12.0f + 150.0f * density, 0.0f, 255.0f));
        const int green = static_cast<int>(
            std::clamp(16.0f + 205.0f * density, 0.0f, 255.0f));
        const int blue = static_cast<int>(
            std::clamp(24.0f + 231.0f * density, 0.0f, 255.0f));
        for (int oy = 0; oy < imageScale; ++oy) {
          auto* row = reinterpret_cast<QRgb*>(
              output.scanLine(y * imageScale + oy));
          for (int ox = 0; ox < imageScale; ++ox) {
            row[x * imageScale + ox] = qRgba(red, green, blue, 255);
          }
        }
      }
    }
    return output;
  };

  float maxDensity = 0.0f;
  float upperHalfDensity = 0.0f;
  float totalDensity = 0.0f;
  for (int y = 1; y < gridHeight - 1; ++y) {
    for (int x = 1; x < gridWidth - 1; ++x) {
      const float density = std::clamp(solver.getDensity(x, y), 0.0f, 1.0f);
      maxDensity = std::max(maxDensity, density);
      totalDensity += density;
      if (y < gridHeight / 2) upperHalfDensity += density;
    }
  }

  const QImage image = rasterizeDensity(solver);

  ArtifactCore::FluidSolver2D repeatSolver(gridWidth, gridHeight);
  repeatSolver.setViscosity(0.00001f);
  repeatSolver.setDiffusion(0.00001f);
  repeatSolver.setBuoyancy(0.05f);
  repeatSolver.setVorticity(0.1f);
  ArtifactCore::SmokeEmitter2D repeatEmitter = emitter;
  repeatEmitter.x = 0.5f;
  repeatEmitter.y = 0.84f;
  repeatEmitter.radius = 0.08f;
  repeatEmitter.density = 0.6f;
  repeatEmitter.velocityY = -1.2f;
  for (int frame = 0; frame < frameCount; ++frame) {
    ArtifactCore::injectSmokeEmitter2D(repeatSolver, repeatEmitter);
    repeatSolver.update(1.0f / 30.0f);
  }
  const QImage repeatedImage = rasterizeDensity(repeatSolver);

  const QString outputDirectory = qEnvironmentVariable("ARTIFACT_SMOKE_OUTPUT_DIR");
  ASSERT_FALSE(outputDirectory.isEmpty())
      << "Set ARTIFACT_SMOKE_OUTPUT_DIR to an absolute output directory";
  ASSERT_TRUE(QDir().mkpath(outputDirectory.toUtf8().constData()));
  const QString outputPath = QDir(outputDirectory).filePath(QStringLiteral("smoke_emitter.png"));
  ASSERT_TRUE(image.save(outputPath, "PNG")) << outputPath.toStdString();
  const QString repeatedPath =
      QDir(outputDirectory).filePath(QStringLiteral("smoke_emitter_repeat.png"));
  ASSERT_TRUE(repeatedImage.save(repeatedPath, "PNG"))
      << repeatedPath.toStdString();
  EXPECT_GT(maxDensity, 0.1f);
  EXPECT_GT(totalDensity, 10.0f);
  EXPECT_GT(upperHalfDensity, 0.1f);
  EXPECT_EQ(image.size(), QSize(gridWidth * imageScale, gridHeight * imageScale));
  EXPECT_GT(QFileInfo(outputPath).size(), 1024);
  int topEdgeSmokePixels = 0;
  for (int x = 0; x < image.width(); ++x) {
    if (image.pixel(x, 0) != qRgba(12, 16, 24, 255)) ++topEdgeSmokePixels;
  }
  EXPECT_EQ(topEdgeSmokePixels, 0)
      << "the selected R&D plume should remain inside the image frame";
  ASSERT_EQ(image.size(), repeatedImage.size());
  int repeatedChangedPixels = 0;
  for (int y = 0; y < image.height(); ++y) {
    for (int x = 0; x < image.width(); ++x) {
      if (image.pixel(x, y) != repeatedImage.pixel(x, y)) {
        ++repeatedChangedPixels;
      }
    }
  }
  EXPECT_EQ(repeatedChangedPixels, 0)
      << "same emitter settings and frame count must produce identical pixels";

  ArtifactCore::FluidSolver2D shiftedSolver(gridWidth, gridHeight);
  shiftedSolver.setViscosity(0.00001f);
  shiftedSolver.setDiffusion(0.00001f);
  shiftedSolver.setBuoyancy(0.05f);
  shiftedSolver.setVorticity(0.1f);
  emitter.x = 0.25f;
  for (int frame = 0; frame < frameCount; ++frame) {
    ArtifactCore::injectSmokeEmitter2D(shiftedSolver, emitter);
    shiftedSolver.update(1.0f / 30.0f);
  }
  const QImage shiftedImage = rasterizeDensity(shiftedSolver);
  const QString shiftedPath =
      QDir(outputDirectory).filePath(QStringLiteral("smoke_emitter_shifted.png"));
  ASSERT_TRUE(shiftedImage.save(shiftedPath, "PNG"))
      << shiftedPath.toStdString();
  int changedPixels = 0;
  for (int y = 0; y < image.height(); ++y) {
    for (int x = 0; x < image.width(); ++x) {
      if (image.pixel(x, y) != shiftedImage.pixel(x, y)) ++changedPixels;
    }
  }
  EXPECT_GT(changedPixels, 1000);
}

TEST(SmokeEmitterStandaloneTest,
     ComparesSignedVorticityConfinementAcrossSeedsAndFrames) {
  constexpr int gridWidth = 96;
  constexpr int gridHeight = 72;
  constexpr int frameCount = 45;
  constexpr float dt = 1.0f / 30.0f;
  constexpr int imageScale = 4;
  constexpr int cellWidth = gridWidth * imageScale;
  constexpr int cellHeight = gridHeight * imageScale;
  constexpr std::array<int, 3> captureFrames{15, 30, 45};
  const QString outputDirectory =
      qEnvironmentVariable("ARTIFACT_SMOKE_OUTPUT_DIR");
  ASSERT_FALSE(outputDirectory.isEmpty())
      << "Set ARTIFACT_SMOKE_OUTPUT_DIR to an absolute output directory";
  ASSERT_TRUE(QDir().mkpath(outputDirectory.toUtf8().constData()));

  for (int seedIndex = 0; seedIndex < 2; ++seedIndex) {
    const int seed = seedIndex == 0 ? 17 : 71;
    QImage contact(cellWidth * 3, cellHeight * 2, QImage::Format_ARGB32);
    contact.fill(qRgba(8, 12, 18, 255));
    std::array<std::array<double, 3>, 2> masses{};

    for (int mode = 0; mode < 2; ++mode) {
      ArtifactCore::FluidSolver2D solver(gridWidth, gridHeight);
      solver.setViscosity(0.00001f);
      solver.setDiffusion(0.00001f);
      solver.setBuoyancy(0.0f);
      solver.setVorticity(mode == 0 ? 0.1f : 0.0f);
      ArtifactCore::SmokeEmitter2D emitter;
      emitter.x = seedIndex == 0 ? 0.46f : 0.54f;
      emitter.y = 0.84f;
      emitter.radius = 0.08f;
      emitter.density = 0.55f;
      emitter.velocityX = seedIndex == 0 ? -0.08f : 0.08f;
      emitter.velocityY = -1.2f;
      emitter.swirl = seedIndex == 0 ? 0.65f : -0.65f;

      int captureIndex = 0;
      for (int frame = 1; frame <= frameCount; ++frame) {
        ArtifactCore::injectSmokeEmitter2D(solver, emitter);
        applySmokeBuoyancy(solver, 0.05f, dt);
        if (mode == 1) {
          applySignedVorticityConfinement(solver, dt, 0.1f);
        }
        solver.update(dt);
        if (captureIndex >= static_cast<int>(captureFrames.size()) ||
            frame != captureFrames[static_cast<std::size_t>(captureIndex)]) {
          continue;
        }

        const QImage sample = renderSmokeDensity(solver, imageScale);
        const int offsetY = mode * cellHeight;
        const int offsetX = captureIndex * cellWidth;
        for (int y = 0; y < cellHeight; ++y) {
          auto* dst = reinterpret_cast<QRgb*>(
              contact.scanLine(offsetY + y)) + offsetX;
          const auto* src = reinterpret_cast<const QRgb*>(sample.constScanLine(y));
          std::copy(src, src + cellWidth, dst);
        }
        masses[static_cast<std::size_t>(mode)]
              [static_cast<std::size_t>(captureIndex)] = smokeMass(solver);
        ++captureIndex;
      }
      ASSERT_EQ(captureIndex, static_cast<int>(captureFrames.size()));
    }

    for (int frameIndex = 0; frameIndex < 3; ++frameIndex) {
      EXPECT_GT(masses[0][static_cast<std::size_t>(frameIndex)], 1.0)
          << "baseline mass vanished for seed " << seed;
      EXPECT_GT(masses[1][static_cast<std::size_t>(frameIndex)], 1.0)
          << "signed-reference mass vanished for seed " << seed;
      GTEST_LOG_(INFO) << "smoke vorticity seed=" << seed
                       << " frame=" << captureFrames[static_cast<std::size_t>(frameIndex)]
                       << " baseline-mass="
                       << masses[0][static_cast<std::size_t>(frameIndex)]
                       << " signed-reference-mass="
                       << masses[1][static_cast<std::size_t>(frameIndex)];
      const int offsetX = frameIndex * cellWidth;
      for (int y = 0; y < cellHeight; ++y) {
        const auto* baseline = reinterpret_cast<const QRgb*>(
            contact.constScanLine(y)) + offsetX;
        const auto* signedReference = reinterpret_cast<const QRgb*>(
            contact.constScanLine(cellHeight + y)) + offsetX;
        for (int x = 0; x < cellWidth; ++x) {
          EXPECT_EQ(baseline[x], signedReference[x])
              << "production and signed reference diverged for seed " << seed
              << " at frame "
              << captureFrames[static_cast<std::size_t>(frameIndex)];
        }
      }
    }

    const QString imagePath = QDir(outputDirectory).filePath(
        QStringLiteral("smoke_vorticity_sign_seed_%1.png").arg(seed));
    ASSERT_TRUE(contact.save(imagePath, "PNG")) << imagePath.toStdString();
  }

  for (int seedIndex = 0; seedIndex < 2; ++seedIndex) {
    const int seed = seedIndex == 0 ? 17 : 71;
    QImage contact(cellWidth * 3, cellHeight * 2, QImage::Format_ARGB32);
    contact.fill(qRgba(8, 12, 18, 255));
    for (int mode = 0; mode < 2; ++mode) {
      ArtifactCore::FluidSolver2D solver(gridWidth, gridHeight);
      solver.setViscosity(0.0f);
      solver.setDiffusion(0.0f);
      solver.setBuoyancy(0.0f);
      solver.setVorticity(mode == 0 ? 0.8f : 0.0f);
      const float leftSign = seedIndex == 0 ? 1.0f : -1.0f;
      const float rightSign = -leftSign;
      const float centerY = 36.0f;
      const float sigma = 8.0f;
      for (int y = 1; y < gridHeight - 1; ++y) {
        for (int x = 1; x < gridWidth - 1; ++x) {
          const float dxLeft = static_cast<float>(x) - 34.0f;
          const float dxRight = static_cast<float>(x) - 62.0f;
          const float dy = static_cast<float>(y) - centerY;
          const float leftPsi = leftSign * 8.0f * std::exp(
              -(dxLeft * dxLeft + dy * dy) / (2.0f * sigma * sigma));
          const float rightPsi = rightSign * 8.0f * std::exp(
              -(dxRight * dxRight + dy * dy) / (2.0f * sigma * sigma));
          const float inverseSigmaSquared = 1.0f / (sigma * sigma);
          solver.addVelocity(x, y,
                             -(dy * inverseSigmaSquared) * (leftPsi + rightPsi),
                             (dxLeft * inverseSigmaSquared) * leftPsi +
                                 (dxRight * inverseSigmaSquared) * rightPsi);
          const float leftDensity = std::exp(
              -(dxLeft * dxLeft + dy * dy) / 50.0f);
          const float rightDensity = std::exp(
              -(dxRight * dxRight + dy * dy) / 50.0f);
          solver.addDensity(x, y, 0.7f * std::max(leftDensity, rightDensity));
        }
      }

      int captureIndex = 0;
      for (int frame = 1; frame <= 30; ++frame) {
        if (mode == 1) {
          applySignedVorticityConfinement(solver, dt, 0.8f);
        }
        solver.update(dt);
        const int wantedFrame = 10 * (captureIndex + 1);
        if (captureIndex >= 3 || frame != wantedFrame) continue;
        const QImage sample = renderSmokeDensity(solver, imageScale);
        const int offsetY = mode * cellHeight;
        const int offsetX = captureIndex * cellWidth;
        for (int y = 0; y < cellHeight; ++y) {
          auto* dst = reinterpret_cast<QRgb*>(
              contact.scanLine(offsetY + y)) + offsetX;
          const auto* src = reinterpret_cast<const QRgb*>(sample.constScanLine(y));
          std::copy(src, src + cellWidth, dst);
        }
        ++captureIndex;
      }
      ASSERT_EQ(captureIndex, 3);
      EXPECT_GT(smokeMass(solver), 1.0) << "vortex pair lost smoke for seed " << seed;
    }
    for (int frameIndex = 0; frameIndex < 3; ++frameIndex) {
      const int offsetX = frameIndex * cellWidth;
      for (int y = 0; y < cellHeight; ++y) {
        const auto* baseline = reinterpret_cast<const QRgb*>(
            contact.constScanLine(y)) + offsetX;
        const auto* signedReference = reinterpret_cast<const QRgb*>(
            contact.constScanLine(cellHeight + y)) + offsetX;
        for (int x = 0; x < cellWidth; ++x) {
          EXPECT_EQ(baseline[x], signedReference[x])
              << "production and signed reference diverged for counter-rotating "
                 "vortices, seed "
              << seed << " at frame " << (10 * (frameIndex + 1));
        }
      }
    }
    const QString imagePath = QDir(outputDirectory).filePath(
        QStringLiteral("smoke_vorticity_pair_seed_%1.png").arg(seed));
    ASSERT_TRUE(contact.save(imagePath, "PNG")) << imagePath.toStdString();
  }
}

TEST(SmokeEmitterStandaloneTest,
     ComparesSeparatedThermalMemoryAfterEqualSmokePulse) {
  constexpr int gridWidth = 72;
  constexpr int gridHeight = 88;
  constexpr int totalFrames = 60;
  constexpr int emissionFrames = 12;
  constexpr float dt = 1.0f / 30.0f;
  constexpr float densityDissipation = 2.5f;
  constexpr float densityRetention =
      0.9200444146293233f;  // exp(-densityDissipation / 30)
  constexpr int imageScale = 4;
  constexpr int cellWidth = gridWidth * imageScale;
  constexpr int cellHeight = gridHeight * imageScale;
  constexpr std::array<int, 4> captureFrames{12, 24, 36, 60};
  struct ThermalCondition {
    const char* name;
    float coolingRate;
    float buoyancy;
    float lowFrequencyStrength;
    float midFrequencyStrength;
    bool useNonPeriodicNoise;
  };
  constexpr std::array<ThermalCondition, 8> conditions{{
      {"density-as-heat", 0.0f, 0.05f, 0.0f, 0.0f, false},
      {"separate-matched-cooling", densityDissipation, 0.05f, 0.0f, 0.0f, false},
      {"separate-slow-cooling", 0.5f, 0.01f, 0.0f, 0.0f, false},
      {"temperature-low-frequency-curl", 0.5f, 0.01f, 5.0f, 0.0f, false},
      {"temperature-low-mid-frequency-curl", 0.5f, 0.01f, 5.0f, 2.5f, false},
      {"temperature-strong-low-frequency-curl", 0.5f, 0.01f, 8.0f, 0.0f, false},
      {"temperature-low-frequency-noise-curl", 0.5f, 0.01f, 5.0f, 0.0f, true},
      {"temperature-low-mid-frequency-noise-curl", 0.5f, 0.01f, 5.0f, 2.5f, true},
  }};
  constexpr std::array<float, 4> captureExposure{1.0f, 4.0f, 12.0f, 32.0f};

  const QString outputDirectory =
      qEnvironmentVariable("ARTIFACT_SMOKE_OUTPUT_DIR");
  ASSERT_FALSE(outputDirectory.isEmpty())
      << "Set ARTIFACT_SMOKE_OUTPUT_DIR to an absolute output directory";
  ASSERT_TRUE(QDir().mkpath(outputDirectory.toUtf8().constData()));

  for (int seedIndex = 0; seedIndex < 2; ++seedIndex) {
    const int seed = seedIndex == 0 ? 17 : 71;
    QImage contact(cellWidth * 4, cellHeight * 8, QImage::Format_ARGB32);
    contact.fill(qRgba(8, 12, 18, 255));
    std::array<std::array<double, 4>, 8> masses{};
    std::array<std::array<double, 4>, 8> centroids{};
    std::array<std::array<double, 4>, 8> thermalMasses{};

    for (std::size_t conditionIndex = 0;
         conditionIndex < conditions.size(); ++conditionIndex) {
      const auto& condition = conditions[conditionIndex];
      ArtifactCore::FluidSolver2D solver(gridWidth, gridHeight);
      solver.setViscosity(0.00001f);
      solver.setDiffusion(0.00001f);
      solver.setBuoyancy(conditionIndex == 0 ? condition.buoyancy : 0.0f);
      solver.setVorticity(0.1f);
      solver.setSolverIterations(20);

      ArtifactCore::SmokeEmitter2D emitter;
      emitter.x = seedIndex == 0 ? 0.47f : 0.53f;
      emitter.y = 0.82f;
      emitter.radius = 0.07f;
      emitter.density = 0.45f;
      emitter.velocityX = seedIndex == 0 ? -0.04f : 0.04f;
      emitter.velocityY = -0.55f;
      emitter.swirl = seedIndex == 0 ? 0.55f : -0.55f;

      ArtifactCore::NamedVector<float> temperature;
      ArtifactCore::NamedVector<float> temperatureNext;
      ArtifactCore::NamedVector<float> streamFunction;
      temperature.resize(static_cast<std::size_t>(gridWidth * gridHeight));
      temperatureNext.resize(static_cast<std::size_t>(gridWidth * gridHeight));
      streamFunction.resize(static_cast<std::size_t>(gridWidth * gridHeight));
      std::fill(temperature.begin(), temperature.end(), 0.0f);
      std::fill(temperatureNext.begin(), temperatureNext.end(), 0.0f);
      int captureIndex = 0;

      for (int frame = 1; frame <= totalFrames; ++frame) {
        if (frame <= emissionFrames) {
          ArtifactCore::injectSmokeEmitter2D(solver, emitter);
          if (conditionIndex > 0) {
            injectSmokeTemperature(temperature, gridWidth, gridHeight, emitter);
          }
        }
        if (conditionIndex > 0) {
          applyTemperatureBuoyancy(solver, temperature, condition.buoyancy, dt);
          applyTemperatureCurlTurbulence(
              solver, temperature, dt, static_cast<float>(frame) * dt,
              condition.lowFrequencyStrength, condition.midFrequencyStrength,
              static_cast<float>(seedIndex) * 1.73f,
              condition.useNonPeriodicNoise, streamFunction);
        }

        solver.update(dt);
        solver.scaleDensity(densityRetention);
        if (conditionIndex > 0) {
          advectSmokeTemperature(temperature, temperatureNext, solver, dt,
                                 condition.coolingRate);
        }

        if (captureIndex >= static_cast<int>(captureFrames.size()) ||
            frame != captureFrames[static_cast<std::size_t>(captureIndex)]) {
          continue;
        }
        const QImage sample = renderSmokeDensity(
            solver, imageScale,
            captureExposure[static_cast<std::size_t>(captureIndex)]);
        const int offsetX = captureIndex * cellWidth;
        const int offsetY = static_cast<int>(conditionIndex) * cellHeight;
        for (int y = 0; y < cellHeight; ++y) {
          auto* dst = reinterpret_cast<QRgb*>(
              contact.scanLine(offsetY + y)) + offsetX;
          const auto* src = reinterpret_cast<const QRgb*>(sample.constScanLine(y));
          std::copy(src, src + cellWidth, dst);
        }
        masses[conditionIndex][static_cast<std::size_t>(captureIndex)] =
            smokeMass(solver);
        centroids[conditionIndex][static_cast<std::size_t>(captureIndex)] =
            smokeCentroidY(solver);
        thermalMasses[conditionIndex][static_cast<std::size_t>(captureIndex)] =
            conditionIndex == 0 ? 0.0 : scalarMass(temperature);
        ++captureIndex;
      }
      ASSERT_EQ(captureIndex, static_cast<int>(captureFrames.size()));
      EXPECT_GT(masses[conditionIndex][3], 1.0)
          << "smoke density vanished for " << condition.name << " seed " << seed;
      if (conditionIndex > 0) {
        EXPECT_GT(thermalMasses[conditionIndex][3], 0.01)
            << "separate thermal field vanished for " << condition.name
            << " seed " << seed;
      }
    }

    EXPECT_GT(thermalMasses[2][3], thermalMasses[1][3])
        << "slow cooling should retain more temperature after the same pulse";
    for (std::size_t turbulenceCondition = 6;
         turbulenceCondition < conditions.size(); ++turbulenceCondition) {
      EXPECT_GT(std::abs(centroids[turbulenceCondition][3] - centroids[2][3]),
                1.0)
          << "temperature-masked curl noise should move the smoke mass for "
          << conditions[turbulenceCondition].name << " seed " << seed;
    }
    for (std::size_t frameIndex = 0; frameIndex < captureFrames.size();
         ++frameIndex) {
      const double densityHeatMass = masses[0][frameIndex];
      const double matchedTemperatureMass = masses[1][frameIndex];
      EXPECT_NEAR(densityHeatMass, matchedTemperatureMass,
                  densityHeatMass * 0.01 + 1.0e-4)
          << "equal density/temperature cooling should stay close at frame "
          << captureFrames[frameIndex];
    }
    for (std::size_t conditionIndex = 0;
         conditionIndex < conditions.size(); ++conditionIndex) {
      for (std::size_t frameIndex = 0; frameIndex < captureFrames.size();
           ++frameIndex) {
        GTEST_LOG_(INFO) << "smoke thermal seed=" << seed
                         << " mode=" << conditions[conditionIndex].name
                         << " frame=" << captureFrames[frameIndex]
                         << " density-mass="
                         << masses[conditionIndex][frameIndex]
                         << " density-centroid-y="
                         << centroids[conditionIndex][frameIndex]
                         << " temperature-mass="
                         << thermalMasses[conditionIndex][frameIndex];
      }
    }

    const QString imagePath = QDir(outputDirectory).filePath(
        QStringLiteral("smoke_thermal_response_seed_%1.png").arg(seed));
    ASSERT_TRUE(contact.save(imagePath, "PNG")) << imagePath.toStdString();
  }
}

TEST(SmokeEmitterStandaloneTest,
     MeasuresSmokeEmitterAdvectionSubstepTradeoff) {
  constexpr int gridWidth = 72;
  constexpr int gridHeight = 88;
  constexpr int totalFrames = 60;
  constexpr int emissionFrames = 12;
  constexpr float frameDt = 1.0f / 30.0f;
  constexpr int imageScale = 4;
  constexpr int cellWidth = gridWidth * imageScale;
  constexpr int cellHeight = gridHeight * imageScale;
  constexpr std::array<int, 4> captureFrames{12, 24, 36, 60};
  constexpr std::array<int, 4> substepCounts{1, 2, 4, 8};
  const QString outputDirectory =
      qEnvironmentVariable("ARTIFACT_SMOKE_OUTPUT_DIR");
  ASSERT_FALSE(outputDirectory.isEmpty())
      << "Set ARTIFACT_SMOKE_OUTPUT_DIR to an absolute output directory";
  ASSERT_TRUE(QDir().mkpath(outputDirectory.toUtf8().constData()));

  for (int seedIndex = 0; seedIndex < 2; ++seedIndex) {
    const int seed = seedIndex == 0 ? 17 : 71;
    QImage contact(cellWidth * 4, cellHeight * 4, QImage::Format_ARGB32);
    contact.fill(qRgba(8, 12, 18, 255));
    std::array<std::array<double, 4>, 4> masses{};
    std::array<std::array<double, 4>, 4> centroids{};
    std::array<std::array<double, 4>, 4> gradients{};
    std::array<std::array<double, 4>, 4> cflCells{};

    for (std::size_t conditionIndex = 0;
         conditionIndex < substepCounts.size(); ++conditionIndex) {
      const int substeps = substepCounts[conditionIndex];
      ArtifactCore::FluidSolver2D solver(gridWidth, gridHeight);
      solver.setViscosity(0.00001f);
      solver.setDiffusion(0.00001f);
      solver.setBuoyancy(0.05f);
      solver.setVorticity(0.1f);
      solver.setSolverIterations(20);
      ArtifactCore::SmokeEmitter2D emitter;
      emitter.x = seedIndex == 0 ? 0.47f : 0.53f;
      emitter.y = 0.82f;
      emitter.radius = 0.07f;
      emitter.density = 0.45f;
      emitter.velocityX = seedIndex == 0 ? -0.04f : 0.04f;
      emitter.velocityY = -0.55f;
      emitter.swirl = seedIndex == 0 ? 0.55f : -0.55f;

      int captureIndex = 0;
      for (int frame = 1; frame <= totalFrames; ++frame) {
        if (frame <= emissionFrames) {
          ArtifactCore::injectSmokeEmitter2D(solver, emitter);
        }
        const float substepDt = frameDt / static_cast<float>(substeps);
        for (int substep = 0; substep < substeps; ++substep) {
          solver.update(substepDt);
        }
        if (captureIndex >= static_cast<int>(captureFrames.size()) ||
            frame != captureFrames[static_cast<std::size_t>(captureIndex)]) {
          continue;
        }
        const QImage sample = renderSmokeDensity(solver, imageScale);
        const int offsetX = captureIndex * cellWidth;
        const int offsetY = static_cast<int>(conditionIndex) * cellHeight;
        for (int y = 0; y < cellHeight; ++y) {
          auto* dst = reinterpret_cast<QRgb*>(
              contact.scanLine(offsetY + y)) + offsetX;
          const auto* src = reinterpret_cast<const QRgb*>(
              sample.constScanLine(y));
          std::copy(src, src + cellWidth, dst);
        }
        masses[conditionIndex][static_cast<std::size_t>(captureIndex)] =
            smokeMass(solver);
        centroids[conditionIndex][static_cast<std::size_t>(captureIndex)] =
            smokeCentroidY(solver);
        gradients[conditionIndex][static_cast<std::size_t>(captureIndex)] =
            smokeDensityGradientRms(solver);
        cflCells[conditionIndex][static_cast<std::size_t>(captureIndex)] =
            smokeMaximumCflCells(solver, frameDt);
        ++captureIndex;
      }
      ASSERT_EQ(captureIndex, static_cast<int>(captureFrames.size()));
      EXPECT_GT(masses[conditionIndex][3], 1.0)
          << "smoke vanished for substep count " << substeps
          << " and seed " << seed;
    }

    for (std::size_t conditionIndex = 0;
         conditionIndex < substepCounts.size(); ++conditionIndex) {
      for (std::size_t frameIndex = 0; frameIndex < captureFrames.size();
           ++frameIndex) {
        GTEST_LOG_(INFO) << "smoke substeps seed=" << seed
                         << " count=" << substepCounts[conditionIndex]
                         << " frame=" << captureFrames[frameIndex]
                         << " density-mass="
                         << masses[conditionIndex][frameIndex]
                         << " density-centroid-y="
                         << centroids[conditionIndex][frameIndex]
                         << " density-gradient-rms="
                         << gradients[conditionIndex][frameIndex]
                         << " max-frame-cfl-cells="
                         << cflCells[conditionIndex][frameIndex];
      }
    }

    const QString imagePath = QDir(outputDirectory).filePath(
        QStringLiteral("smoke_advection_substeps_seed_%1.png").arg(seed));
    ASSERT_TRUE(contact.save(imagePath, "PNG")) << imagePath.toStdString();
  }
}

TEST(SmokeEmitterStandaloneTest,
     MeasuresFixedVelocityScalarTransportMethodsAcrossSubstepCounts) {
  constexpr int gridWidth = 72;
  constexpr int gridHeight = 88;
  constexpr int totalFrames = 60;
  constexpr float frameDt = 1.0f / 30.0f;
  constexpr int imageScale = 4;
  constexpr int cellWidth = gridWidth * imageScale;
  constexpr int cellHeight = gridHeight * imageScale;
  constexpr std::array<int, 4> captureFrames{12, 24, 36, 60};
  constexpr std::array<int, 4> substepCounts{1, 2, 4, 8};
  const auto index = [](const int x, const int y) {
    return static_cast<std::size_t>(x + y * gridWidth);
  };
  const QString outputDirectory =
      qEnvironmentVariable("ARTIFACT_SMOKE_OUTPUT_DIR");
  ASSERT_FALSE(outputDirectory.isEmpty())
      << "Set ARTIFACT_SMOKE_OUTPUT_DIR to an absolute output directory";
  ASSERT_TRUE(QDir().mkpath(outputDirectory.toUtf8().constData()));

  ArtifactCore::NamedVector<float> velocityX;
  ArtifactCore::NamedVector<float> velocityY;
  ArtifactCore::NamedVector<float> streamFunction;
  const std::size_t cellCount =
      static_cast<std::size_t>(gridWidth * gridHeight);
  velocityX.resize(cellCount);
  velocityY.resize(cellCount);
  streamFunction.resize(cellCount);
  std::fill(velocityX.begin(), velocityX.end(), 0.0f);
  std::fill(velocityY.begin(), velocityY.end(), 0.0f);
  std::fill(streamFunction.begin(), streamFunction.end(), 0.0f);
  constexpr float kPi = 3.14159265358979323846f;
  constexpr float streamAmplitude = 28.0f;
  for (int y = 0; y < gridHeight; ++y) {
    for (int x = 0; x < gridWidth; ++x) {
      const float sx = std::sin(kPi * static_cast<float>(x) /
                                static_cast<float>(gridWidth - 1));
      const float sy = std::sin(kPi * static_cast<float>(y) /
                                static_cast<float>(gridHeight - 1));
      streamFunction[index(x, y)] =
          streamAmplitude * sx * sx * sy * sy;
    }
  }
  for (int y = 0; y < gridHeight; ++y) {
    for (int x = 0; x < gridWidth; ++x) {
      const int x0 = std::max(0, x - 1);
      const int x1 = std::min(gridWidth - 1, x + 1);
      const int y0 = std::max(0, y - 1);
      const int y1 = std::min(gridHeight - 1, y + 1);
      velocityX[index(x, y)] = 0.5f * (
          streamFunction[index(x, y1)] - streamFunction[index(x, y0)]);
      velocityY[index(x, y)] = -0.5f * (
          streamFunction[index(x1, y)] - streamFunction[index(x0, y)]);
    }
  }
  double maximumFrameCfl = 0.0;
  for (std::size_t cell = 0; cell < cellCount; ++cell) {
    maximumFrameCfl = std::max(maximumFrameCfl, std::hypot(
        static_cast<double>(velocityX[cell]) * frameDt * (gridWidth - 2),
        static_cast<double>(velocityY[cell]) * frameDt * (gridHeight - 2)));
  }
  double faceDivergenceSquared = 0.0;
  double maximumFaceDivergence = 0.0;
  double outwardBoundaryFlux = 0.0;
  std::size_t faceDivergenceCount = 0;
  for (int y = 1; y < gridHeight - 1; ++y) {
    for (int x = 1; x < gridWidth - 1; ++x) {
      const auto cell = index(x, y);
      const float uLeft = 0.5f *
          (velocityX[index(x - 1, y)] + velocityX[cell]) *
          static_cast<float>(gridWidth - 2);
      const float uRight = 0.5f *
          (velocityX[cell] + velocityX[index(x + 1, y)]) *
          static_cast<float>(gridWidth - 2);
      const float vTop = 0.5f *
          (velocityY[index(x, y - 1)] + velocityY[cell]) *
          static_cast<float>(gridHeight - 2);
      const float vBottom = 0.5f *
          (velocityY[cell] + velocityY[index(x, y + 1)]) *
          static_cast<float>(gridHeight - 2);
      const double divergence = (uRight - uLeft) + (vBottom - vTop);
      faceDivergenceSquared += divergence * divergence;
      maximumFaceDivergence = std::max(
          maximumFaceDivergence, std::abs(divergence));
      ++faceDivergenceCount;
      if (x == 1) outwardBoundaryFlux -= uLeft;
      if (x == gridWidth - 2) outwardBoundaryFlux += uRight;
      if (y == 1) outwardBoundaryFlux -= vTop;
      if (y == gridHeight - 2) outwardBoundaryFlux += vBottom;
    }
  }
  const double faceDivergenceRms = std::sqrt(
      faceDivergenceSquared / static_cast<double>(faceDivergenceCount));
  GTEST_LOG_(INFO) << "fixed scalar discrete face flow rms-divergence="
      << faceDivergenceRms << " max-divergence=" << maximumFaceDivergence
      << " outward-domain-boundary-flux=" << outwardBoundaryFlux;

  double initialInteriorMass = 0.0;
  double firstOrderFluxSum = 0.0;
  double limitedFluxSum = 0.0;
  double initialLimitedReconstructionDerivative = 0.0;
  ArtifactCore::NamedVector<float> leftFaceVelocity;
  ArtifactCore::NamedVector<float> rightFaceVelocity;
  ArtifactCore::NamedVector<float> topFaceVelocity;
  ArtifactCore::NamedVector<float> bottomFaceVelocity;
  ArtifactCore::NamedVector<float> macLeftFaceVelocity;
  ArtifactCore::NamedVector<float> macRightFaceVelocity;
  ArtifactCore::NamedVector<float> macTopFaceVelocity;
  ArtifactCore::NamedVector<float> macBottomFaceVelocity;
  ArtifactCore::NamedVector<float> initialDensity;
  ArtifactCore::NamedVector<float> firstOrderNext;
  ArtifactCore::NamedVector<float> limitedNext;
  initialDensity.resize(cellCount);
  firstOrderNext.resize(cellCount);
  limitedNext.resize(cellCount);
  leftFaceVelocity.resize(cellCount);
  rightFaceVelocity.resize(cellCount);
  topFaceVelocity.resize(cellCount);
  bottomFaceVelocity.resize(cellCount);
  macLeftFaceVelocity.resize(cellCount);
  macRightFaceVelocity.resize(cellCount);
  macTopFaceVelocity.resize(cellCount);
  macBottomFaceVelocity.resize(cellCount);
  const float diagnosticCenterX =
      0.4f * static_cast<float>(gridWidth - 2) + 1.0f;
  const float diagnosticCenterY =
      0.79f * static_cast<float>(gridHeight - 2) + 1.0f;
  for (int y = 0; y < gridHeight; ++y) {
    for (int x = 0; x < gridWidth; ++x) {
      const float dx = (static_cast<float>(x) - diagnosticCenterX) / 5.0f;
      const float dy = (static_cast<float>(y) - diagnosticCenterY) / 7.0f;
      initialDensity[index(x, y)] = std::exp(-0.5f * (dx * dx + dy * dy));
    }
  }
  const auto diagnosticMinmod = [](const float a, const float b) {
    return a * b <= 0.0f ? 0.0f
        : std::copysign(std::min(std::abs(a), std::abs(b)), a);
  };
  const auto advanceLimitedFlux = [&](
      const ArtifactCore::NamedVector<float>& density,
      ArtifactCore::NamedVector<float>& next, const float dt,
      const ArtifactCore::NamedVector<float>& uLeftField,
      const ArtifactCore::NamedVector<float>& uRightField,
      const ArtifactCore::NamedVector<float>& vTopField,
      const ArtifactCore::NamedVector<float>& vBottomField,
      const bool clampReconstructedStates,
      double* cumulativeBoundaryMassDelta,
      double* cumulativeCellFluxMassDelta) {
    double netOutwardBoundaryFlux = 0.0;
    double summedCellFluxDivergence = 0.0;
    for (int y = 1; y < gridHeight - 1; ++y) {
      for (int x = 1; x < gridWidth - 1; ++x) {
        const auto cell = index(x, y);
        const auto rho = [&](const int px, const int py) {
          return density[index(px, py)];
        };
        const float uLeft = uLeftField[cell];
        const float uRight = uRightField[cell];
        const float vTop = vTopField[cell];
        const float vBottom = vBottomField[cell];
        float leftState = uLeft >= 0.0f
            ? rho(x - 1, y) + 0.5f * diagnosticMinmod(
                  rho(x - 1, y) - rho(std::max(0, x - 2), y),
                  rho(x, y) - rho(x - 1, y))
            : rho(x, y) - 0.5f * diagnosticMinmod(
                  rho(x, y) - rho(std::max(0, x - 1), y),
                  rho(std::min(gridWidth - 1, x + 1), y) - rho(x, y));
        float rightState = uRight >= 0.0f
            ? rho(x, y) + 0.5f * diagnosticMinmod(
                  rho(x, y) - rho(std::max(0, x - 1), y),
                  rho(std::min(gridWidth - 1, x + 1), y) - rho(x, y))
            : rho(std::min(gridWidth - 1, x + 1), y) - 0.5f *
                  diagnosticMinmod(
                      rho(std::min(gridWidth - 1, x + 1), y) - rho(x, y),
                      rho(std::min(gridWidth - 1, x + 2), y) -
                          rho(std::min(gridWidth - 1, x + 1), y));
        float topState = vTop >= 0.0f
            ? rho(x, y - 1) + 0.5f * diagnosticMinmod(
                  rho(x, y - 1) - rho(x, std::max(0, y - 2)),
                  rho(x, y) - rho(x, y - 1))
            : rho(x, y) - 0.5f * diagnosticMinmod(
                  rho(x, y) - rho(x, std::max(0, y - 1)),
                  rho(x, std::min(gridHeight - 1, y + 1)) - rho(x, y));
        float bottomState = vBottom >= 0.0f
            ? rho(x, y) + 0.5f * diagnosticMinmod(
                  rho(x, y) - rho(x, std::max(0, y - 1)),
                  rho(x, std::min(gridHeight - 1, y + 1)) - rho(x, y))
            : rho(x, std::min(gridHeight - 1, y + 1)) - 0.5f *
                  diagnosticMinmod(
                      rho(x, std::min(gridHeight - 1, y + 1)) - rho(x, y),
                      rho(x, std::min(gridHeight - 1, y + 2)) -
                          rho(x, std::min(gridHeight - 1, y + 1)));
        if (clampReconstructedStates) {
          leftState = std::max(leftState, std::min(rho(x - 1, y), rho(x, y)));
          rightState = std::max(rightState, std::min(rho(x, y), rho(x + 1, y)));
          topState = std::max(topState, std::min(rho(x, y - 1), rho(x, y)));
          bottomState = std::max(bottomState, std::min(rho(x, y), rho(x, y + 1)));
        }
        const float fluxDivergence =
            uRight * rightState - uLeft * leftState +
            vBottom * bottomState - vTop * topState;
        next[cell] = density[cell] - dt * fluxDivergence;
        summedCellFluxDivergence += fluxDivergence;
        if (x == 1) netOutwardBoundaryFlux -= uLeft * leftState;
        if (x == gridWidth - 2) netOutwardBoundaryFlux += uRight * rightState;
        if (y == 1) netOutwardBoundaryFlux -= vTop * topState;
        if (y == gridHeight - 2) netOutwardBoundaryFlux +=
            vBottom * bottomState;
      }
    }
    if (cumulativeBoundaryMassDelta != nullptr) {
      *cumulativeBoundaryMassDelta -= dt * netOutwardBoundaryFlux;
    }
    if (cumulativeCellFluxMassDelta != nullptr) {
      *cumulativeCellFluxMassDelta -= dt * summedCellFluxDivergence;
    }
  };
  for (int y = 1; y < gridHeight - 1; ++y) {
    for (int x = 1; x < gridWidth - 1; ++x) {
      const auto cell = index(x, y);
      const float uLeft = 0.5f *
          (velocityX[index(x - 1, y)] + velocityX[cell]) *
          static_cast<float>(gridWidth - 2);
      const float uRight = 0.5f *
          (velocityX[cell] + velocityX[index(x + 1, y)]) *
          static_cast<float>(gridWidth - 2);
      const float vTop = 0.5f *
          (velocityY[index(x, y - 1)] + velocityY[cell]) *
          static_cast<float>(gridHeight - 2);
      const float vBottom = 0.5f *
          (velocityY[cell] + velocityY[index(x, y + 1)]) *
          static_cast<float>(gridHeight - 2);
      leftFaceVelocity[cell] = uLeft;
      rightFaceVelocity[cell] = uRight;
      topFaceVelocity[cell] = vTop;
      bottomFaceVelocity[cell] = vBottom;
      macLeftFaceVelocity[cell] = streamFunction[index(x - 1, y)] -
          streamFunction[index(x - 1, y - 1)];
      macRightFaceVelocity[cell] = streamFunction[index(x, y)] -
          streamFunction[index(x, y - 1)];
      macTopFaceVelocity[cell] = -(
          streamFunction[index(x, y - 1)] -
          streamFunction[index(x - 1, y - 1)]);
      macBottomFaceVelocity[cell] = -(
          streamFunction[index(x, y)] -
          streamFunction[index(x - 1, y)]);
      const auto rho = [&](const int px, const int py) {
        return initialDensity[index(px, py)];
      };
      const float leftState = uLeft >= 0.0f
          ? rho(x - 1, y) + 0.5f * diagnosticMinmod(
                rho(x - 1, y) - rho(std::max(0, x - 2), y),
                rho(x, y) - rho(x - 1, y))
          : rho(x, y) - 0.5f * diagnosticMinmod(
                rho(x, y) - rho(std::max(0, x - 1), y),
                rho(std::min(gridWidth - 1, x + 1), y) - rho(x, y));
      const float rightState = uRight >= 0.0f
          ? rho(x, y) + 0.5f * diagnosticMinmod(
                rho(x, y) - rho(std::max(0, x - 1), y),
                rho(std::min(gridWidth - 1, x + 1), y) - rho(x, y))
          : rho(std::min(gridWidth - 1, x + 1), y) - 0.5f *
                diagnosticMinmod(
                    rho(std::min(gridWidth - 1, x + 1), y) - rho(x, y),
                    rho(std::min(gridWidth - 1, x + 2), y) -
                        rho(std::min(gridWidth - 1, x + 1), y));
      const float topState = vTop >= 0.0f
          ? rho(x, y - 1) + 0.5f * diagnosticMinmod(
                rho(x, y - 1) - rho(x, std::max(0, y - 2)),
                rho(x, y) - rho(x, y - 1))
          : rho(x, y) - 0.5f * diagnosticMinmod(
                rho(x, y) - rho(x, std::max(0, y - 1)),
                rho(x, std::min(gridHeight - 1, y + 1)) - rho(x, y));
      const float bottomState = vBottom >= 0.0f
          ? rho(x, y) + 0.5f * diagnosticMinmod(
                rho(x, y) - rho(x, std::max(0, y - 1)),
                rho(x, std::min(gridHeight - 1, y + 1)) - rho(x, y))
          : rho(x, std::min(gridHeight - 1, y + 1)) - 0.5f *
                diagnosticMinmod(
                    rho(x, std::min(gridHeight - 1, y + 1)) - rho(x, y),
                    rho(x, std::min(gridHeight - 1, y + 2)) -
                        rho(x, std::min(gridHeight - 1, y + 1)));
      const float divergence = (uRight - uLeft) + (vBottom - vTop);
      const float limitedDerivative =
          uRight * (rightState - rho(x, y)) -
          uLeft * (leftState - rho(x, y)) +
          vBottom * (bottomState - rho(x, y)) -
          vTop * (topState - rho(x, y));
      firstOrderFluxSum -= static_cast<double>(rho(x, y)) * divergence;
      limitedFluxSum -= static_cast<double>(rho(x, y)) * divergence +
          static_cast<double>(limitedDerivative);
      initialLimitedReconstructionDerivative -= limitedDerivative;
      initialInteriorMass += rho(x, y);
      firstOrderNext[cell] = rho(x, y) - frameDt * rho(x, y) * divergence;
      limitedNext[cell] = rho(x, y) - frameDt *
          (rho(x, y) * divergence + limitedDerivative);
    }
  }
  double averagedFaceSpeedSquared = 0.0;
  double macFaceSpeedSquared = 0.0;
  for (int y = 1; y < gridHeight - 1; ++y) {
    for (int x = 1; x < gridWidth - 1; ++x) {
      const auto cell = index(x, y);
      averagedFaceSpeedSquared +=
          leftFaceVelocity[cell] * leftFaceVelocity[cell] +
          rightFaceVelocity[cell] * rightFaceVelocity[cell] +
          topFaceVelocity[cell] * topFaceVelocity[cell] +
          bottomFaceVelocity[cell] * bottomFaceVelocity[cell];
      macFaceSpeedSquared +=
          macLeftFaceVelocity[cell] * macLeftFaceVelocity[cell] +
          macRightFaceVelocity[cell] * macRightFaceVelocity[cell] +
          macTopFaceVelocity[cell] * macTopFaceVelocity[cell] +
          macBottomFaceVelocity[cell] * macBottomFaceVelocity[cell];
    }
  }
  const float macVelocityScale = macFaceSpeedSquared > 0.0
      ? static_cast<float>(std::sqrt(averagedFaceSpeedSquared /
                                     macFaceSpeedSquared))
      : 1.0f;
  double macFaceDivergenceSquared = 0.0;
  double macMaximumFaceDivergence = 0.0;
  double macMaximumFrameCfl = 0.0;
  std::size_t macFaceDivergenceCount = 0;
  for (int y = 1; y < gridHeight - 1; ++y) {
    for (int x = 1; x < gridWidth - 1; ++x) {
      const auto cell = index(x, y);
      macLeftFaceVelocity[cell] *= macVelocityScale;
      macRightFaceVelocity[cell] *= macVelocityScale;
      macTopFaceVelocity[cell] *= macVelocityScale;
      macBottomFaceVelocity[cell] *= macVelocityScale;
      const double divergence = macRightFaceVelocity[cell] -
          macLeftFaceVelocity[cell] + macBottomFaceVelocity[cell] -
          macTopFaceVelocity[cell];
      macFaceDivergenceSquared += divergence * divergence;
      macMaximumFaceDivergence = std::max(
          macMaximumFaceDivergence, std::abs(divergence));
      const double horizontalCfl = std::max(
          std::abs(static_cast<double>(macLeftFaceVelocity[cell])),
          std::abs(static_cast<double>(macRightFaceVelocity[cell])));
      const double verticalCfl = std::max(
          std::abs(static_cast<double>(macTopFaceVelocity[cell])),
          std::abs(static_cast<double>(macBottomFaceVelocity[cell])));
      macMaximumFrameCfl = std::max(macMaximumFrameCfl,
                                    (horizontalCfl + verticalCfl) * frameDt);
      ++macFaceDivergenceCount;
    }
  }
  const double macFaceDivergenceRms = std::sqrt(
      macFaceDivergenceSquared / static_cast<double>(macFaceDivergenceCount));
  EXPECT_LT(macFaceDivergenceRms, 1.0e-4);
  EXPECT_LT(macMaximumFaceDivergence, 1.0e-4);
  GTEST_LOG_(INFO) << "fixed scalar streamfunction MAC face field scale="
      << macVelocityScale << " rms-divergence=" << macFaceDivergenceRms
      << " max-divergence=" << macMaximumFaceDivergence
      << " max-frame-CFL=" << macMaximumFrameCfl;
  double firstOrderMassDelta = 0.0;
  double limitedMassDelta = 0.0;
  double limitedMassDeltaClamped = 0.0;
  for (int y = 1; y < gridHeight - 1; ++y) {
    for (int x = 1; x < gridWidth - 1; ++x) {
      const auto cell = index(x, y);
      firstOrderMassDelta += firstOrderNext[cell] - initialDensity[cell];
      limitedMassDelta += limitedNext[cell] - initialDensity[cell];
      limitedMassDeltaClamped +=
          std::max(0.0f, limitedNext[cell]) - initialDensity[cell];
    }
  }
  GTEST_LOG_(INFO) << "fixed scalar one-step frameDt=" << frameDt
      << " mass=" << initialInteriorMass
      << " upwind-flux-sum=" << firstOrderFluxSum
      << " upwind-mass-delta=" << firstOrderMassDelta
      << " MUSCL-flux-sum=" << limitedFluxSum
      << " MUSCL-mass-delta=" << limitedMassDelta
      << " MUSCL-clamped-mass-delta=" << limitedMassDeltaClamped
      << " MUSCL-reconstruction-derivative="
      << initialLimitedReconstructionDerivative;

  for (const int frameSubsteps : {1, 2, 4, 8, 16, 32}) {
    auto density = initialDensity;
    auto next = initialDensity;
    double minimumBeforeClamp = 0.0;
    double clampedMassAdded = 0.0;
    double massAfter = 0.0;
    double maximumSubstepCfl = 0.0;
    const float dt = frameDt / static_cast<float>(frameSubsteps);
    for (std::size_t cell = 0; cell < cellCount; ++cell) {
      maximumSubstepCfl = std::max(maximumSubstepCfl, std::hypot(
          static_cast<double>(velocityX[cell]) * dt * (gridWidth - 2),
          static_cast<double>(velocityY[cell]) * dt * (gridHeight - 2)));
    }
    for (int substep = 0; substep < frameSubsteps; ++substep) {
      advanceLimitedFlux(density, next, dt, leftFaceVelocity,
                         rightFaceVelocity, topFaceVelocity,
                         bottomFaceVelocity, false, nullptr, nullptr);
      std::swap(density, next);
      if (substep + 1 < frameSubsteps) {
        for (int x = 0; x < gridWidth; ++x) {
          density[index(x, 0)] = density[index(x, 1)];
          density[index(x, gridHeight - 1)] = density[index(x, gridHeight - 2)];
        }
        for (int y = 0; y < gridHeight; ++y) {
          density[index(0, y)] = density[index(1, y)];
          density[index(gridWidth - 1, y)] = density[index(gridWidth - 2, y)];
        }
      }
    }
    minimumBeforeClamp = std::numeric_limits<double>::infinity();
    for (int y = 1; y < gridHeight - 1; ++y) {
      for (int x = 1; x < gridWidth - 1; ++x) {
        const auto cell = index(x, y);
        minimumBeforeClamp = std::min(minimumBeforeClamp,
                                      static_cast<double>(density[cell]));
        if (density[cell] < 0.0f) {
          clampedMassAdded -= density[cell];
          density[cell] = 0.0f;
        }
        massAfter += density[cell];
      }
    }
    GTEST_LOG_(INFO) << "fixed scalar MUSCL frame-CFL split=" << frameSubsteps
        << " maxSubstepCfl=" << maximumSubstepCfl
        << " dt=" << dt << " minimum-before-clamp=" << minimumBeforeClamp
        << " clamp-added-mass=" << clampedMassAdded
        << " final-mass=" << massAfter;
  }

  const auto sampleBilinear = [&](const ArtifactCore::NamedVector<float>& field,
                                  float x, float y) {
    x = std::clamp(x, 0.5f, static_cast<float>(gridWidth) - 1.5f);
    y = std::clamp(y, 0.5f, static_cast<float>(gridHeight) - 1.5f);
    const int x0 = static_cast<int>(std::floor(x));
    const int y0 = static_cast<int>(std::floor(y));
    const int x1 = std::min(x0 + 1, gridWidth - 1);
    const int y1 = std::min(y0 + 1, gridHeight - 1);
    const float tx = x - static_cast<float>(x0);
    const float ty = y - static_cast<float>(y0);
    return std::lerp(
        std::lerp(field[index(x0, y0)], field[index(x1, y0)], tx),
        std::lerp(field[index(x0, y1)], field[index(x1, y1)], tx), ty);
  };
  for (int seedIndex = 0; seedIndex < 2; ++seedIndex) {
    const int seed = seedIndex == 0 ? 17 : 71;
    const float sourceX = seedIndex == 0 ? 0.40f : 0.60f;
    QImage contact(cellWidth * 4, cellHeight * 10, QImage::Format_ARGB32);
    contact.fill(qRgba(8, 12, 18, 255));
  std::array<std::array<double, 4>, 12> masses{};
  std::array<std::array<double, 4>, 12> gradients{};
  std::array<std::array<double, 4>, 12> peaks{};

    for (std::size_t condition = 0; condition < substepCounts.size(); ++condition) {
      const int substeps = substepCounts[condition];
      ArtifactCore::NamedVector<float> density;
      ArtifactCore::NamedVector<float> next;
      density.resize(cellCount);
      next.resize(cellCount);
      const float centerX = sourceX * static_cast<float>(gridWidth - 2) + 1.0f;
      const float centerY = 0.79f * static_cast<float>(gridHeight - 2) + 1.0f;
      for (int y = 0; y < gridHeight; ++y) {
        for (int x = 0; x < gridWidth; ++x) {
          const float dx = (static_cast<float>(x) - centerX) / 5.0f;
          const float dy = (static_cast<float>(y) - centerY) / 7.0f;
          density[index(x, y)] = std::exp(-0.5f * (dx * dx + dy * dy));
        }
      }

      int captureIndex = 0;
      const float substepDt = frameDt / static_cast<float>(substeps);
      for (int frame = 1; frame <= totalFrames; ++frame) {
        for (int substep = 0; substep < substeps; ++substep) {
          for (int y = 1; y < gridHeight - 1; ++y) {
            for (int x = 1; x < gridWidth - 1; ++x) {
              const auto cell = index(x, y);
              const float backX = static_cast<float>(x) -
                  velocityX[cell] * substepDt * (gridWidth - 2);
              const float backY = static_cast<float>(y) -
                  velocityY[cell] * substepDt * (gridHeight - 2);
              next[cell] = sampleBilinear(density, backX, backY);
            }
          }
          for (int x = 0; x < gridWidth; ++x) {
            next[index(x, 0)] = next[index(x, 1)];
            next[index(x, gridHeight - 1)] =
                next[index(x, gridHeight - 2)];
          }
          for (int y = 0; y < gridHeight; ++y) {
            next[index(0, y)] = next[index(1, y)];
            next[index(gridWidth - 1, y)] =
                next[index(gridWidth - 2, y)];
          }
          std::copy(next.begin(), next.end(), density.begin());
        }

        if (captureIndex >= static_cast<int>(captureFrames.size()) ||
            frame != captureFrames[static_cast<std::size_t>(captureIndex)]) {
          continue;
        }
        double mass = 0.0;
        double gradientSquared = 0.0;
        float peak = 0.0f;
        for (int y = 1; y < gridHeight - 1; ++y) {
          for (int x = 1; x < gridWidth - 1; ++x) {
            const float value = density[index(x, y)];
            mass += std::max(0.0f, value);
            peak = std::max(peak, value);
            const double gx = 0.5 * (density[index(x + 1, y)] -
                                     density[index(x - 1, y)]);
            const double gy = 0.5 * (density[index(x, y + 1)] -
                                     density[index(x, y - 1)]);
            gradientSquared += gx * gx + gy * gy;
          }
        }
        const double interiorCells =
            static_cast<double>((gridWidth - 2) * (gridHeight - 2));
        masses[condition][static_cast<std::size_t>(captureIndex)] = mass;
        gradients[condition][static_cast<std::size_t>(captureIndex)] =
            std::sqrt(gradientSquared / interiorCells);
        peaks[condition][static_cast<std::size_t>(captureIndex)] = peak;
        QImage sample(cellWidth, cellHeight, QImage::Format_ARGB32);
        sample.fill(qRgba(8, 12, 18, 255));
        for (int y = 1; y < gridHeight - 1; ++y) {
          auto* row = reinterpret_cast<QRgb*>(
              sample.scanLine(y * imageScale));
          for (int x = 1; x < gridWidth - 1; ++x) {
            const float value = peak > 0.0f ? density[index(x, y)] / peak : 0.0f;
            const int intensity = static_cast<int>(std::lround(
                std::clamp(value, 0.0f, 1.0f) * 220.0f));
            const QRgb pixel = qRgba(8, 12 + intensity, 18, 255);
            for (int oy = 0; oy < imageScale; ++oy) {
              auto* dst = reinterpret_cast<QRgb*>(
                  sample.scanLine(y * imageScale + oy)) + x * imageScale;
              std::fill(dst, dst + imageScale, pixel);
            }
          }
        }
        const int offsetX = captureIndex * cellWidth;
        const int offsetY = static_cast<int>(condition) * cellHeight;
        for (int y = 0; y < cellHeight; ++y) {
          auto* dst = reinterpret_cast<QRgb*>(
              contact.scanLine(offsetY + y)) + offsetX;
          const auto* src = reinterpret_cast<const QRgb*>(sample.constScanLine(y));
          std::copy(src, src + cellWidth, dst);
        }
        ++captureIndex;
      }
      ASSERT_EQ(captureIndex, static_cast<int>(captureFrames.size()));
    }

    const auto minmod = [](const float a, const float b) {
      if (a * b <= 0.0f) return 0.0f;
      return std::copysign(std::min(std::abs(a), std::abs(b)), a);
    };
    for (std::size_t methodIndex = 0; methodIndex < 4; ++methodIndex) {
      const std::size_t substepIndex = methodIndex % 2 + 2;
      const int substeps = substepCounts[substepIndex];
      const std::size_t outputRow = methodIndex + 4;
      const bool useLimitedMuscl = methodIndex >= 2;
      ArtifactCore::NamedVector<float> density;
      ArtifactCore::NamedVector<float> next;
      density.resize(cellCount);
      next.resize(cellCount);
      const float centerX = sourceX * static_cast<float>(gridWidth - 2) + 1.0f;
      const float centerY = 0.79f * static_cast<float>(gridHeight - 2) + 1.0f;
      for (int y = 0; y < gridHeight; ++y) {
        for (int x = 0; x < gridWidth; ++x) {
          const float dx = (static_cast<float>(x) - centerX) / 5.0f;
          const float dy = (static_cast<float>(y) - centerY) / 7.0f;
          density[index(x, y)] = std::exp(-0.5f * (dx * dx + dy * dy));
        }
      }

      int captureIndex = 0;
      const float substepDt = frameDt / static_cast<float>(substeps);
      for (int frame = 1; frame <= totalFrames; ++frame) {
        for (int substep = 0; substep < substeps; ++substep) {
          std::fill(next.begin(), next.end(), 0.0f);
          for (int y = 1; y < gridHeight - 1; ++y) {
            for (int x = 1; x < gridWidth - 1; ++x) {
              const auto cell = index(x, y);
              const float uLeft = 0.5f *
                  (velocityX[index(x - 1, y)] + velocityX[cell]) *
                      static_cast<float>(gridWidth - 2);
              const float uRight = 0.5f *
                  (velocityX[cell] + velocityX[index(x + 1, y)]) *
                      static_cast<float>(gridWidth - 2);
              const float vTop = 0.5f *
                  (velocityY[index(x, y - 1)] + velocityY[cell]) *
                      static_cast<float>(gridHeight - 2);
              const float vBottom = 0.5f *
                  (velocityY[cell] + velocityY[index(x, y + 1)]) *
                      static_cast<float>(gridHeight - 2);
              const float leftState = useLimitedMuscl && uLeft >= 0.0f
                  ? std::max(0.0f,
                      density[index(x - 1, y)] + 0.5f * minmod(
                          density[index(x - 1, y)] -
                              density[index(std::max(0, x - 2), y)],
                          density[cell] - density[index(x - 1, y)]))
                  : (useLimitedMuscl ? std::max(0.0f, density[cell] - 0.5f * minmod(
                        density[cell] - density[index(std::max(0, x - 1), y)],
                        density[index(std::min(gridWidth - 1, x + 1), y)] -
                            density[cell])) : density[cell]);
              const float rightState = useLimitedMuscl && uRight >= 0.0f
                  ? std::max(0.0f, density[cell] + 0.5f * minmod(
                        density[cell] - density[index(std::max(0, x - 1), y)],
                        density[index(std::min(gridWidth - 1, x + 1), y)] -
                            density[cell]))
                  : (!useLimitedMuscl || x == gridWidth - 1 ? density[cell] : std::max(0.0f,
                      density[index(x + 1, y)] - 0.5f * minmod(
                          density[index(x + 1, y)] - density[cell],
                          density[index(std::min(gridWidth - 1, x + 2), y)] -
                              density[index(x + 1, y)])));
              const float topState = useLimitedMuscl && vTop >= 0.0f
                  ? std::max(0.0f,
                      density[index(x, y - 1)] + 0.5f * minmod(
                          density[index(x, y - 1)] -
                              density[index(x, std::max(0, y - 2))],
                          density[cell] - density[index(x, y - 1)]))
                  : (useLimitedMuscl ? std::max(0.0f, density[cell] - 0.5f * minmod(
                        density[cell] - density[index(x, std::max(0, y - 1))],
                        density[index(x, std::min(gridHeight - 1, y + 1))] -
                            density[cell])) : density[cell]);
              const float bottomState = useLimitedMuscl && vBottom >= 0.0f
                  ? std::max(0.0f, density[cell] + 0.5f * minmod(
                        density[cell] - density[index(x, std::max(0, y - 1))],
                        density[index(x, std::min(gridHeight - 1, y + 1))] -
                            density[cell]))
                  : (!useLimitedMuscl || y == gridHeight - 1 ? density[cell] : std::max(0.0f,
                      density[index(x, y + 1)] - 0.5f * minmod(
                          density[index(x, y + 1)] - density[cell],
                          density[index(x, std::min(gridHeight - 1, y + 2))] -
                              density[index(x, y + 1)])));
              const float fluxDivergence =
                  uRight * rightState - uLeft * leftState +
                  vBottom * bottomState - vTop * topState;
              next[cell] = useLimitedMuscl
                  ? std::max(0.0f, density[cell] - substepDt * fluxDivergence)
                  : density[cell] - substepDt * fluxDivergence;
            }
          }
          std::copy(next.begin(), next.end(), density.begin());
        }

        if (captureIndex >= static_cast<int>(captureFrames.size()) ||
            frame != captureFrames[static_cast<std::size_t>(captureIndex)]) {
          continue;
        }
        double mass = 0.0;
        double gradientSquared = 0.0;
        float peak = 0.0f;
        for (int y = 1; y < gridHeight - 1; ++y) {
          for (int x = 1; x < gridWidth - 1; ++x) {
            const float value = density[index(x, y)];
            mass += std::max(0.0f, value);
            peak = std::max(peak, value);
            const double gx = 0.5 * (density[index(x + 1, y)] -
                                     density[index(x - 1, y)]);
            const double gy = 0.5 * (density[index(x, y + 1)] -
                                     density[index(x, y - 1)]);
            gradientSquared += gx * gx + gy * gy;
          }
        }
        const double interiorCells =
            static_cast<double>((gridWidth - 2) * (gridHeight - 2));
        masses[outputRow][static_cast<std::size_t>(captureIndex)] = mass;
        gradients[outputRow][static_cast<std::size_t>(captureIndex)] =
            std::sqrt(gradientSquared / interiorCells);
        peaks[outputRow][static_cast<std::size_t>(captureIndex)] = peak;
        QImage sample(cellWidth, cellHeight, QImage::Format_ARGB32);
        sample.fill(qRgba(8, 12, 18, 255));
        for (int y = 1; y < gridHeight - 1; ++y) {
          for (int x = 1; x < gridWidth - 1; ++x) {
            const float value = peak > 0.0f ? density[index(x, y)] / peak : 0.0f;
            const int intensity = static_cast<int>(std::lround(
                std::clamp(value, 0.0f, 1.0f) * 220.0f));
            const QRgb pixel = qRgba(8, 12 + intensity, 18, 255);
            for (int oy = 0; oy < imageScale; ++oy) {
              auto* dst = reinterpret_cast<QRgb*>(
                  sample.scanLine(y * imageScale + oy)) + x * imageScale;
              std::fill(dst, dst + imageScale, pixel);
            }
          }
        }
        const int offsetX = captureIndex * cellWidth;
        const int offsetY = static_cast<int>(outputRow) * cellHeight;
        for (int y = 0; y < cellHeight; ++y) {
          auto* dst = reinterpret_cast<QRgb*>(
              contact.scanLine(offsetY + y)) + offsetX;
          const auto* src = reinterpret_cast<const QRgb*>(sample.constScanLine(y));
          std::copy(src, src + cellWidth, dst);
        }
        ++captureIndex;
      }
      ASSERT_EQ(captureIndex, static_cast<int>(captureFrames.size()));
    }

    const int cflLimitedSteps = std::max(1, static_cast<int>(std::ceil(
        maximumFrameCfl)));
    const int macCflLimitedSteps = std::max(1, static_cast<int>(std::ceil(
        macMaximumFrameCfl)));
    ArtifactCore::NamedVector<float> cflLimitedDensity;
    ArtifactCore::NamedVector<float> cflLimitedNext;
    cflLimitedDensity.resize(cellCount);
    cflLimitedNext.resize(cellCount);
    const float cflLimitedCenterX =
        sourceX * static_cast<float>(gridWidth - 2) + 1.0f;
    const float cflLimitedCenterY =
        0.79f * static_cast<float>(gridHeight - 2) + 1.0f;
    for (int y = 0; y < gridHeight; ++y) {
      for (int x = 0; x < gridWidth; ++x) {
        const float dx = (static_cast<float>(x) - cflLimitedCenterX) / 5.0f;
        const float dy = (static_cast<float>(y) - cflLimitedCenterY) / 7.0f;
        cflLimitedDensity[index(x, y)] = std::exp(-0.5f *
            (dx * dx + dy * dy));
      }
    }
    int cflLimitedCapture = 0;
    double cflLimitedMaximumFrameCfl = 0.0;
    double cflLimitedInitialMass = 0.0;
    double cflLimitedCumulativeBoundaryMassDelta = 0.0;
    double cflLimitedCumulativeCellFluxMassDelta = 0.0;
    for (int y = 1; y < gridHeight - 1; ++y) {
      for (int x = 1; x < gridWidth - 1; ++x) {
        cflLimitedInitialMass += cflLimitedDensity[index(x, y)];
      }
    }
    const float cflLimitedDt = frameDt / static_cast<float>(cflLimitedSteps);
    const float macCflLimitedDt = frameDt /
        static_cast<float>(macCflLimitedSteps);
    for (int frame = 1; frame <= totalFrames; ++frame) {
      for (int substep = 0; substep < cflLimitedSteps; ++substep) {
        advanceLimitedFlux(cflLimitedDensity, cflLimitedNext, cflLimitedDt,
                           leftFaceVelocity, rightFaceVelocity,
                           topFaceVelocity, bottomFaceVelocity, false,
                           &cflLimitedCumulativeBoundaryMassDelta,
                           &cflLimitedCumulativeCellFluxMassDelta);
        std::swap(cflLimitedDensity, cflLimitedNext);
        for (int x = 0; x < gridWidth; ++x) {
          cflLimitedDensity[index(x, 0)] = cflLimitedDensity[index(x, 1)];
          cflLimitedDensity[index(x, gridHeight - 1)] =
              cflLimitedDensity[index(x, gridHeight - 2)];
        }
        for (int y = 0; y < gridHeight; ++y) {
          cflLimitedDensity[index(0, y)] = cflLimitedDensity[index(1, y)];
          cflLimitedDensity[index(gridWidth - 1, y)] =
              cflLimitedDensity[index(gridWidth - 2, y)];
        }
      }

      if (cflLimitedCapture >= static_cast<int>(captureFrames.size()) ||
          frame != captureFrames[static_cast<std::size_t>(cflLimitedCapture)]) {
        continue;
      }
      double mass = 0.0;
      double signedInteriorMass = 0.0;
      double negativeDensityMass = 0.0;
      double allGridMass = 0.0;
      double boundaryShellMass = 0.0;
      double gradientSquared = 0.0;
      float peak = 0.0f;
      float minimumDensity = std::numeric_limits<float>::infinity();
      for (int y = 1; y < gridHeight - 1; ++y) {
        for (int x = 1; x < gridWidth - 1; ++x) {
          const float value = cflLimitedDensity[index(x, y)];
          mass += std::max(0.0f, value);
          signedInteriorMass += value;
          negativeDensityMass += std::min(0.0f, value);
          minimumDensity = std::min(minimumDensity, value);
          if (x == 1 || x == gridWidth - 2 || y == 1 ||
              y == gridHeight - 2) {
            boundaryShellMass += std::max(0.0f, value);
          }
          peak = std::max(peak, value);
          const double gx = 0.5 * (cflLimitedDensity[index(x + 1, y)] -
                                    cflLimitedDensity[index(x - 1, y)]);
          const double gy = 0.5 * (cflLimitedDensity[index(x, y + 1)] -
                                    cflLimitedDensity[index(x, y - 1)]);
          gradientSquared += gx * gx + gy * gy;
        }
      }
      for (const float value : cflLimitedDensity) {
        allGridMass += std::max(0.0f, value);
      }
      const double interiorCells =
          static_cast<double>((gridWidth - 2) * (gridHeight - 2));
      constexpr std::size_t outputRow = 8;
      masses[outputRow][static_cast<std::size_t>(cflLimitedCapture)] = mass;
      gradients[outputRow][static_cast<std::size_t>(cflLimitedCapture)] =
          std::sqrt(gradientSquared / interiorCells);
      peaks[outputRow][static_cast<std::size_t>(cflLimitedCapture)] = peak;
      QImage sample(cellWidth, cellHeight, QImage::Format_ARGB32);
      sample.fill(qRgba(8, 12, 18, 255));
      for (int y = 1; y < gridHeight - 1; ++y) {
        for (int x = 1; x < gridWidth - 1; ++x) {
          const float value = peak > 0.0f
              ? cflLimitedDensity[index(x, y)] / peak : 0.0f;
          const int intensity = static_cast<int>(std::lround(
              std::clamp(value, 0.0f, 1.0f) * 220.0f));
          const QRgb pixel = qRgba(8, 12 + intensity, 18, 255);
          for (int oy = 0; oy < imageScale; ++oy) {
            auto* dst = reinterpret_cast<QRgb*>(
                sample.scanLine(y * imageScale + oy)) + x * imageScale;
            std::fill(dst, dst + imageScale, pixel);
          }
        }
      }
      const int offsetX = cflLimitedCapture * cellWidth;
      const int offsetY = static_cast<int>(outputRow) * cellHeight;
      for (int y = 0; y < cellHeight; ++y) {
        auto* dst = reinterpret_cast<QRgb*>(
            contact.scanLine(offsetY + y)) + offsetX;
        const auto* src = reinterpret_cast<const QRgb*>(sample.constScanLine(y));
        std::copy(src, src + cellWidth, dst);
      }
      GTEST_LOG_(INFO) << "fixed scalar MUSCL frame-CFL-capped seed=" << seed
          << " substeps=" << cflLimitedSteps
          << " maxSubstepCfl=" << maximumFrameCfl / cflLimitedSteps
          << " frame=" << frame << " mass=" << mass
          << " signedInteriorMass=" << signedInteriorMass
          << " negativeDensityMass=" << negativeDensityMass
          << " minimumDensity=" << minimumDensity
          << " expectedBoundaryMassDelta="
          << cflLimitedCumulativeBoundaryMassDelta
          << " measuredInteriorMassDelta=" << mass - cflLimitedInitialMass
          << " summedCellFluxMassDelta="
          << cflLimitedCumulativeCellFluxMassDelta
          << " fluxAggregationResidual=" << cflLimitedCumulativeCellFluxMassDelta -
                 cflLimitedCumulativeBoundaryMassDelta
          << " signedInteriorBalanceResidual=" << signedInteriorMass -
                 cflLimitedInitialMass -
                 cflLimitedCumulativeBoundaryMassDelta
          << " allGridMass=" << allGridMass
          << " boundaryShellMass=" << boundaryShellMass
          << " gradientRms=" << gradients[outputRow][
                 static_cast<std::size_t>(cflLimitedCapture)]
          << " peak=" << peak;
      ++cflLimitedCapture;
    }
    ASSERT_EQ(cflLimitedCapture, static_cast<int>(captureFrames.size()));

    ArtifactCore::NamedVector<float> macDensity;
    ArtifactCore::NamedVector<float> macNext;
    ArtifactCore::NamedVector<float> macClampedDensity;
    ArtifactCore::NamedVector<float> macClampedNext;
    macDensity.resize(cellCount);
    macNext.resize(cellCount);
    macClampedDensity.resize(cellCount);
    macClampedNext.resize(cellCount);
    for (int y = 0; y < gridHeight; ++y) {
      for (int x = 0; x < gridWidth; ++x) {
        const float dx = (static_cast<float>(x) - cflLimitedCenterX) / 5.0f;
        const float dy = (static_cast<float>(y) - cflLimitedCenterY) / 7.0f;
        macDensity[index(x, y)] = std::exp(-0.5f * (dx * dx + dy * dy));
        macClampedDensity[index(x, y)] = macDensity[index(x, y)];
      }
    }
    double macInitialMass = 0.0;
    double macBoundaryMassDelta = 0.0;
    double macCellFluxMassDelta = 0.0;
    double macClampedBoundaryMassDelta = 0.0;
    int macCapture = 0;
    for (int y = 1; y < gridHeight - 1; ++y) {
      for (int x = 1; x < gridWidth - 1; ++x) {
        macInitialMass += macDensity[index(x, y)];
      }
    }
    for (int frame = 1; frame <= totalFrames; ++frame) {
      for (int substep = 0; substep < macCflLimitedSteps; ++substep) {
        advanceLimitedFlux(macDensity, macNext, macCflLimitedDt,
                           macLeftFaceVelocity, macRightFaceVelocity,
                           macTopFaceVelocity, macBottomFaceVelocity, false,
                           &macBoundaryMassDelta, &macCellFluxMassDelta);
        advanceLimitedFlux(macClampedDensity, macClampedNext, macCflLimitedDt,
                           macLeftFaceVelocity, macRightFaceVelocity,
                           macTopFaceVelocity, macBottomFaceVelocity, true,
                           &macClampedBoundaryMassDelta, nullptr);
        std::swap(macDensity, macNext);
        std::swap(macClampedDensity, macClampedNext);
        for (int x = 0; x < gridWidth; ++x) {
          macDensity[index(x, 0)] = macDensity[index(x, 1)];
          macDensity[index(x, gridHeight - 1)] =
              macDensity[index(x, gridHeight - 2)];
          macClampedDensity[index(x, 0)] = macClampedDensity[index(x, 1)];
          macClampedDensity[index(x, gridHeight - 1)] =
              macClampedDensity[index(x, gridHeight - 2)];
        }
        for (int y = 0; y < gridHeight; ++y) {
          macDensity[index(0, y)] = macDensity[index(1, y)];
          macDensity[index(gridWidth - 1, y)] =
              macDensity[index(gridWidth - 2, y)];
          macClampedDensity[index(0, y)] = macClampedDensity[index(1, y)];
          macClampedDensity[index(gridWidth - 1, y)] =
              macClampedDensity[index(gridWidth - 2, y)];
        }
      }
      if (macCapture >= static_cast<int>(captureFrames.size()) ||
          frame != captureFrames[static_cast<std::size_t>(macCapture)]) {
        continue;
      }
      double positiveMass = 0.0;
      double signedMass = 0.0;
      double negativeMass = 0.0;
      double gradientSquared = 0.0;
      float peak = 0.0f;
      float minimum = std::numeric_limits<float>::infinity();
      double clampedPositiveMass = 0.0;
      double clampedSignedMass = 0.0;
      double clampedNegativeMass = 0.0;
      float clampedMinimum = std::numeric_limits<float>::infinity();
      float clampedPeak = 0.0f;
      for (int y = 1; y < gridHeight - 1; ++y) {
        for (int x = 1; x < gridWidth - 1; ++x) {
          const float value = macDensity[index(x, y)];
          positiveMass += std::max(0.0f, value);
          signedMass += value;
          negativeMass += std::min(0.0f, value);
          minimum = std::min(minimum, value);
          peak = std::max(peak, value);
          const double gx = 0.5 * (macDensity[index(x + 1, y)] -
                                    macDensity[index(x - 1, y)]);
          const double gy = 0.5 * (macDensity[index(x, y + 1)] -
                                    macDensity[index(x, y - 1)]);
          gradientSquared += gx * gx + gy * gy;
          const float clampedValue = macClampedDensity[index(x, y)];
          clampedPositiveMass += std::max(0.0f, clampedValue);
          clampedSignedMass += clampedValue;
          clampedNegativeMass += std::min(0.0f, clampedValue);
          clampedMinimum = std::min(clampedMinimum, clampedValue);
          clampedPeak = std::max(clampedPeak, clampedValue);
        }
      }
      const double interiorCells =
          static_cast<double>((gridWidth - 2) * (gridHeight - 2));
      const double gradientRms = std::sqrt(gradientSquared / interiorCells);
      EXPECT_GE(minimum, -1.0e-6);
      EXPECT_LT(std::abs(signedMass - macInitialMass - macBoundaryMassDelta),
                2.0e-5);
      constexpr std::size_t macOutputRow = 9;
      masses[macOutputRow][static_cast<std::size_t>(macCapture)] = positiveMass;
      gradients[macOutputRow][static_cast<std::size_t>(macCapture)] = gradientRms;
      peaks[macOutputRow][static_cast<std::size_t>(macCapture)] = peak;
      QImage sample(cellWidth, cellHeight, QImage::Format_ARGB32);
      sample.fill(qRgba(8, 12, 18, 255));
      for (int y = 1; y < gridHeight - 1; ++y) {
        for (int x = 1; x < gridWidth - 1; ++x) {
          const float value = peak > 0.0f
              ? macDensity[index(x, y)] / peak : 0.0f;
          const int intensity = static_cast<int>(std::lround(
              std::clamp(value, 0.0f, 1.0f) * 220.0f));
          const QRgb pixel = qRgba(8, 12 + intensity, 18, 255);
          for (int oy = 0; oy < imageScale; ++oy) {
            auto* dst = reinterpret_cast<QRgb*>(
                sample.scanLine(y * imageScale + oy)) + x * imageScale;
            std::fill(dst, dst + imageScale, pixel);
          }
        }
      }
      const int offsetX = macCapture * cellWidth;
      const int offsetY = static_cast<int>(macOutputRow) * cellHeight;
      for (int y = 0; y < cellHeight; ++y) {
        auto* dst = reinterpret_cast<QRgb*>(
            contact.scanLine(offsetY + y)) + offsetX;
        const auto* src = reinterpret_cast<const QRgb*>(sample.constScanLine(y));
        std::copy(src, src + cellWidth, dst);
      }
      GTEST_LOG_(INFO) << "fixed scalar MUSCL MAC-curl seed=" << seed
          << " substeps=" << macCflLimitedSteps
          << " maxSubstepCfl=" << macMaximumFrameCfl / macCflLimitedSteps
          << " frame=" << frame << " positiveMass=" << positiveMass
          << " signedMass=" << signedMass << " negativeMass=" << negativeMass
          << " minimumDensity=" << minimum
          << " boundaryMassDelta=" << macBoundaryMassDelta
          << " signedBalanceResidual=" << signedMass - macInitialMass -
                 macBoundaryMassDelta
          << " gradientRms=" << gradientRms << " peak=" << peak;
      GTEST_LOG_(INFO) << "fixed scalar MUSCL MAC-state-clamped seed=" << seed
          << " substeps=" << macCflLimitedSteps
          << " frame=" << frame << " positiveMass=" << clampedPositiveMass
          << " signedMass=" << clampedSignedMass
          << " negativeMass=" << clampedNegativeMass
          << " minimumDensity=" << clampedMinimum
          << " boundaryMassDelta=" << macClampedBoundaryMassDelta
          << " signedBalanceResidual=" << clampedSignedMass - macInitialMass -
                 macClampedBoundaryMassDelta << " peak=" << clampedPeak;
      ++macCapture;
    }
    ASSERT_EQ(macCapture, static_cast<int>(captureFrames.size()));

    for (std::size_t condition = 0; condition < substepCounts.size(); ++condition) {
      for (std::size_t capture = 0; capture < captureFrames.size(); ++capture) {
        GTEST_LOG_(INFO) << "fixed scalar substeps seed=" << seed
            << " count=" << substepCounts[condition]
            << " frame=" << captureFrames[capture]
            << " mass=" << masses[condition][capture]
            << " gradientRms=" << gradients[condition][capture]
            << " peak=" << peaks[condition][capture]
            << " maxFrameCflCells=" << maximumFrameCfl;
      }
    }
    for (std::size_t method = 0; method < 4; ++method) {
      const std::size_t row = method + 4;
      for (std::size_t capture = 0; capture < captureFrames.size(); ++capture) {
        GTEST_LOG_(INFO) << "fixed scalar " << (method < 2 ? "upwind" : "limited-MUSCL")
            << " seed=" << seed
            << " count=" << substepCounts[method % 2 + 2]
            << " frame=" << captureFrames[capture]
            << " mass=" << masses[row][capture]
            << " gradientRms=" << gradients[row][capture]
            << " peak=" << peaks[row][capture]
            << " maxFrameCflCells=" << maximumFrameCfl;
      }
    }
    for (std::size_t capture = 0; capture < captureFrames.size(); ++capture) {
      GTEST_LOG_(INFO) << "fixed scalar MUSCL frame-CFL-capped seed=" << seed
          << " substeps=" << cflLimitedSteps
          << " frame=" << captureFrames[capture]
          << " mass=" << masses[8][capture]
          << " gradientRms=" << gradients[8][capture]
          << " peak=" << peaks[8][capture];
      GTEST_LOG_(INFO) << "fixed scalar MUSCL MAC-curl seed=" << seed
          << " substeps=" << macCflLimitedSteps
          << " frame=" << captureFrames[capture]
          << " mass=" << masses[9][capture]
          << " gradientRms=" << gradients[9][capture]
          << " peak=" << peaks[9][capture];
    }
    const QString path = QDir(outputDirectory).filePath(
        QStringLiteral("fixed_velocity_scalar_substeps_seed_%1.png").arg(seed));
    ASSERT_TRUE(contact.save(path, "PNG")) << path.toStdString();
    const QString macPath = QDir(outputDirectory).filePath(
        QStringLiteral("fixed_velocity_scalar_mac_seed_%1.png").arg(seed));
    ASSERT_TRUE(contact.save(macPath, "PNG")) << macPath.toStdString();
  }
}

TEST(SmokeEmitterStandaloneTest,
     TemperatureMaskedCurlNoiseAddsDivergenceFreeVelocity) {
  constexpr int width = 80;
  constexpr int height = 64;
  ArtifactCore::FluidSolver2D solver(width, height);
  ArtifactCore::NamedVector<float> temperature;
  ArtifactCore::NamedVector<float> streamFunction;
  temperature.resize(static_cast<std::size_t>(width * height));
  streamFunction.resize(static_cast<std::size_t>(width * height));
  for (int y = 1; y < height - 1; ++y) {
    for (int x = 1; x < width - 1; ++x) {
      const float dx = static_cast<float>(x - 40) / 11.0f;
      const float dy = static_cast<float>(y - 40) / 9.0f;
      temperature[static_cast<std::size_t>(x + y * width)] =
          std::exp(-0.5f * (dx * dx + dy * dy));
    }
  }

  applyTemperatureCurlTurbulence(solver, temperature, 1.0f / 30.0f, 0.5f,
                                 5.0f, 2.5f, 1.73f, true,
                                 streamFunction);
  const auto velocity = solver.snapshot();
  double divergenceSquared = 0.0;
  double maximumAbsoluteDivergence = 0.0;
  std::size_t sampleCount = 0;
  double velocityEnergy = 0.0;
  for (int y = 3; y < height - 3; ++y) {
    for (int x = 3; x < width - 3; ++x) {
      const auto index = [width](const int px, const int py) {
        return static_cast<std::size_t>(px + py * width);
      };
      const float divergence = 0.5f * (
          velocity.velocityX[index(x + 1, y)] -
              velocity.velocityX[index(x - 1, y)] +
          velocity.velocityY[index(x, y + 1)] -
              velocity.velocityY[index(x, y - 1)]);
      divergenceSquared += static_cast<double>(divergence) * divergence;
      maximumAbsoluteDivergence = std::max(
          maximumAbsoluteDivergence, std::abs(static_cast<double>(divergence)));
      const auto cell = index(x, y);
      velocityEnergy += static_cast<double>(velocity.velocityX[cell]) *
              velocity.velocityX[cell] +
          static_cast<double>(velocity.velocityY[cell]) *
              velocity.velocityY[cell];
      ++sampleCount;
    }
  }
  const double divergenceRms = std::sqrt(
      divergenceSquared / static_cast<double>(sampleCount));
  EXPECT_GT(velocityEnergy, 1.0e-8);
  EXPECT_LT(divergenceRms, 1.0e-7);
  EXPECT_LT(maximumAbsoluteDivergence, 1.0e-6);
}

TEST(SmokeEmitterStandaloneTest,
     ComparesDensityDissipationAfterEqualSmokePulse) {
  constexpr int gridWidth = 72;
  constexpr int gridHeight = 88;
  constexpr int totalFrames = 60;
  constexpr int emissionFrames = 12;
  constexpr float dt = 1.0f / 30.0f;
  constexpr int imageScale = 4;
  constexpr int cellWidth = gridWidth * imageScale;
  constexpr int cellHeight = gridHeight * imageScale;
  constexpr std::array<int, 4> captureFrames{12, 24, 36, 60};
  constexpr std::array<float, 4> dissipationRates{0.1f, 0.5f, 1.0f, 2.5f};
  const QString outputDirectory =
      qEnvironmentVariable("ARTIFACT_SMOKE_OUTPUT_DIR");
  ASSERT_FALSE(outputDirectory.isEmpty())
      << "Set ARTIFACT_SMOKE_OUTPUT_DIR to an absolute output directory";
  ASSERT_TRUE(QDir().mkpath(outputDirectory.toUtf8().constData()));

  for (int seedIndex = 0; seedIndex < 2; ++seedIndex) {
    const int seed = seedIndex == 0 ? 17 : 71;
    QImage contact(cellWidth * 4, cellHeight * 4, QImage::Format_ARGB32);
    contact.fill(qRgba(8, 12, 18, 255));
    std::array<std::array<double, 4>, 4> masses{};
    std::array<std::array<double, 4>, 4> centroids{};

    for (std::size_t conditionIndex = 0;
         conditionIndex < dissipationRates.size(); ++conditionIndex) {
      ArtifactCore::FluidSolver2D solver(gridWidth, gridHeight);
      solver.setViscosity(0.00001f);
      solver.setDiffusion(0.00001f);
      solver.setBuoyancy(0.05f);
      solver.setVorticity(0.1f);
      ArtifactCore::SmokeEmitter2D emitter;
      emitter.x = seedIndex == 0 ? 0.47f : 0.53f;
      emitter.y = 0.82f;
      emitter.radius = 0.07f;
      emitter.density = 0.45f;
      emitter.velocityX = seedIndex == 0 ? -0.04f : 0.04f;
      emitter.velocityY = -0.55f;
      emitter.swirl = seedIndex == 0 ? 0.55f : -0.55f;

      int captureIndex = 0;
      for (int frame = 1; frame <= totalFrames; ++frame) {
        if (frame <= emissionFrames) {
          ArtifactCore::injectSmokeEmitter2D(solver, emitter);
        }
        solver.update(dt);
        solver.scaleDensity(std::exp(-dissipationRates[conditionIndex] * dt));
        if (captureIndex >= static_cast<int>(captureFrames.size()) ||
            frame != captureFrames[static_cast<std::size_t>(captureIndex)]) {
          continue;
        }
        const QImage sample = renderSmokeDensity(solver, imageScale);
        const int offsetX = captureIndex * cellWidth;
        const int offsetY = static_cast<int>(conditionIndex) * cellHeight;
        for (int y = 0; y < cellHeight; ++y) {
          auto* dst = reinterpret_cast<QRgb*>(
              contact.scanLine(offsetY + y)) + offsetX;
          const auto* src = reinterpret_cast<const QRgb*>(sample.constScanLine(y));
          std::copy(src, src + cellWidth, dst);
        }
        masses[conditionIndex][static_cast<std::size_t>(captureIndex)] =
            smokeMass(solver);
        centroids[conditionIndex][static_cast<std::size_t>(captureIndex)] =
            smokeCentroidY(solver);
        ++captureIndex;
      }
      ASSERT_EQ(captureIndex, static_cast<int>(captureFrames.size()));
      EXPECT_GT(masses[conditionIndex][3], 0.1)
          << "smoke vanished for dissipation rate "
          << dissipationRates[conditionIndex] << " seed " << seed;
    }

    for (std::size_t conditionIndex = 0;
         conditionIndex < dissipationRates.size(); ++conditionIndex) {
      for (std::size_t frameIndex = 0; frameIndex < captureFrames.size();
           ++frameIndex) {
        GTEST_LOG_(INFO) << "smoke dissipation seed=" << seed
                         << " rate-per-second="
                         << dissipationRates[conditionIndex]
                         << " frame=" << captureFrames[frameIndex]
                         << " density-mass="
                         << masses[conditionIndex][frameIndex]
                         << " density-centroid-y="
                         << centroids[conditionIndex][frameIndex];
      }
    }
    for (std::size_t conditionIndex = 0;
         conditionIndex + 1 < dissipationRates.size(); ++conditionIndex) {
      EXPECT_GT(masses[conditionIndex][3], masses[conditionIndex + 1][3])
          << "a lower dissipation rate should retain more smoke at frame 60";
    }

    const QString imagePath = QDir(outputDirectory).filePath(
        QStringLiteral("smoke_dissipation_response_seed_%1.png").arg(seed));
    ASSERT_TRUE(contact.save(imagePath, "PNG")) << imagePath.toStdString();
  }
}
