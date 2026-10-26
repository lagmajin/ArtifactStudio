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
#include <limits>

import Physics.Fluid;

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
                          const int scale = 4) {
  QImage image(solver.width() * scale, solver.height() * scale,
               QImage::Format_ARGB32);
  image.fill(qRgba(8, 12, 18, 255));
  for (int y = 1; y < solver.height() - 1; ++y) {
    for (int x = 1; x < solver.width() - 1; ++x) {
      const float density = std::clamp(solver.getDensity(x, y), 0.0f, 1.0f);
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

}  // namespace

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
