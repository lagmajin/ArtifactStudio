#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <numeric>
#include <span>
#include <string>
#include <cstdint>
#include <chrono>
#include <vector>

import Core.Simulation.Pyro;
import Render.ImageBuffer;
import Render.Vector3D;
import Render.VolumeRenderer;

namespace {

struct RenderStats {
  std::size_t occupiedPixels = 0;
  double luminanceSum = 0.0;
  double luminanceSquaredSum = 0.0;
  double luminanceStdDev = 0.0;
  double centroidX = 0.0;
  double centroidY = 0.0;
  int maximumChannel = 0;
};

RenderStats measure(const ArtifactCore::RayTrace::ImageBuffer& image) {
  RenderStats stats;
  double weightedX = 0.0;
  double weightedY = 0.0;
  for (int y = 0; y < image.height; ++y) {
    for (int x = 0; x < image.width; ++x) {
      const auto index = static_cast<std::size_t>((y * image.width + x) * 3);
      const int red = image.pixels[index];
      const int green = image.pixels[index + 1];
      const int blue = image.pixels[index + 2];
      const double luminance =
          0.2126 * red + 0.7152 * green + 0.0722 * blue;
      stats.maximumChannel = std::max(
          stats.maximumChannel, std::max({red, green, blue}));
      // Background luminance is about 5.0 in this 8-bit output. Count only
      // pixels that have received a measurable volume contribution.
      if (luminance > 10.0) {
        ++stats.occupiedPixels;
        stats.luminanceSum += luminance;
        stats.luminanceSquaredSum += luminance * luminance;
        weightedX += luminance * x;
        weightedY += luminance * y;
      }
    }
  }
  if (stats.luminanceSum > 0.0) {
    const double mean = stats.luminanceSum /
        static_cast<double>(stats.occupiedPixels);
    stats.luminanceStdDev = std::sqrt(std::max(0.0,
        stats.luminanceSquaredSum /
            static_cast<double>(stats.occupiedPixels) - mean * mean));
    stats.centroidX = weightedX / stats.luminanceSum;
    stats.centroidY = weightedY / stats.luminanceSum;
  }
  return stats;
}

struct FieldTemporalStats {
  double cosineSum = 0.0;
  double relativeL1Sum = 0.0;
  std::size_t comparisons = 0;
};

void updateTemporalStats(const std::vector<float>& current,
                         std::vector<float>& previous,
                         FieldTemporalStats& stats) {
  if (!previous.empty() && previous.size() == current.size()) {
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
    const double denominator = std::sqrt(previousSquared * currentSquared);
    if (denominator > 0.0) stats.cosineSum += dot / denominator;
    stats.relativeL1Sum += absoluteDelta / std::max(previousMass, 1e-12);
    ++stats.comparisons;
  }
  previous = current;
}

double densityLaplacianRms(const std::vector<float>& density,
                           int width, int height, int depth) {
  double squaredSum = 0.0;
  std::size_t sampleCount = 0;
  const auto indexOf = [width, height](int x, int y, int z) {
    return static_cast<std::size_t>(x + width * (y + height * z));
  };
  for (int z = 1; z + 1 < depth; ++z) {
    for (int y = 1; y + 1 < height; ++y) {
      for (int x = 1; x + 1 < width; ++x) {
        const auto i = indexOf(x, y, z);
        const float laplacian = 6.0f * density[i]
            - density[indexOf(x - 1, y, z)]
            - density[indexOf(x + 1, y, z)]
            - density[indexOf(x, y - 1, z)]
            - density[indexOf(x, y + 1, z)]
            - density[indexOf(x, y, z - 1)]
            - density[indexOf(x, y, z + 1)];
        squaredSum += static_cast<double>(laplacian) * laplacian;
        ++sampleCount;
      }
    }
  }
  return sampleCount == 0 ? 0.0 : std::sqrt(squaredSum / sampleCount);
}

double densityHighFrequencyRms(const std::vector<float>& density,
                               int width, int height, int depth) {
  double squaredSum = 0.0;
  std::size_t count = 0;
  const auto indexOf = [width, height](int x, int y, int z) {
    return static_cast<std::size_t>(x + width * (y + height * z));
  };
  for (int z = 1; z + 1 < depth; ++z)
    for (int y = 1; y + 1 < height; ++y)
      for (int x = 1; x + 1 < width; ++x) {
        const auto index = indexOf(x, y, z);
        const float smooth = (density[indexOf(x - 1, y, z)] +
            density[indexOf(x + 1, y, z)] + density[indexOf(x, y - 1, z)] +
            density[indexOf(x, y + 1, z)] + density[indexOf(x, y, z - 1)] +
            density[indexOf(x, y, z + 1)]) / 6.0f;
        const double high = density[index] - smooth;
        squaredSum += high * high;
        ++count;
      }
  return count ? std::sqrt(squaredSum / count) : 0.0;
}

double sinusoidAmplitude(const std::vector<float>& values, int width, int height,
                         int depth, int axis, int wavelength) {
  double cosine = 0.0;
  double sine = 0.0;
  std::size_t count = 0;
  for (int z = 2; z < depth - 2; ++z)
    for (int y = 2; y < height - 2; ++y)
      for (int x = 2; x < width - 2; ++x) {
        const int sampleCoordinate = axis == 0 ? x : axis == 1 ? y : z;
        const float phase = 6.28318530718f * sampleCoordinate / wavelength;
        const double sample = values[static_cast<std::size_t>(x + width * (y + height * z))];
        cosine += (sample - 0.5) * std::cos(phase);
        sine += (sample - 0.5) * std::sin(phase);
        ++count;
      }
  return count ? 2.0 * std::sqrt(cosine * cosine + sine * sine) / count : 0.0;
}

double velocityDivergenceRms(const ArtifactCore::PyroSimulation& simulation) {
  const auto velocity = simulation.fields().velocityView();
  const auto r = velocity.resolution;
  if (r.width < 3 || r.height < 3 || r.depth < 3) return 0.0;
  double sumSquared = 0.0;
  std::size_t count = 0;
  for (int z = 1; z + 1 < r.depth; ++z) {
    for (int y = 1; y + 1 < r.height; ++y) {
      for (int x = 1; x + 1 < r.width; ++x) {
        const float divergence =
            (velocity.at(x + 1, y, z).x - velocity.at(x - 1, y, z).x +
             velocity.at(x, y + 1, z).y - velocity.at(x, y - 1, z).y +
             velocity.at(x, y, z + 1).z - velocity.at(x, y, z - 1).z) * 0.5f;
        sumSquared += static_cast<double>(divergence) * divergence;
        ++count;
      }
    }
  }
  return count ? std::sqrt(sumSquared / count) : 0.0;
}

double gridDivergenceRms(const std::vector<ArtifactCore::PyroVec3>& velocity,
                         int width, int height, int depth) {
  const auto index = [width, height](int x, int y, int z) {
    return static_cast<std::size_t>(x + width * (y + height * z));
  };
  double sumSquared = 0.0;
  std::size_t count = 0;
  for (int z = 1; z + 1 < depth; ++z)
    for (int y = 1; y + 1 < height; ++y)
      for (int x = 1; x + 1 < width; ++x) {
        const float divergence = 0.5f * (
            velocity[index(x + 1, y, z)].x - velocity[index(x - 1, y, z)].x +
            velocity[index(x, y + 1, z)].y - velocity[index(x, y - 1, z)].y +
            velocity[index(x, y, z + 1)].z - velocity[index(x, y, z - 1)].z);
        sumSquared += static_cast<double>(divergence) * divergence;
        ++count;
      }
  return count ? std::sqrt(sumSquared / count) : 0.0;
}

std::vector<ArtifactCore::PyroVec3> projectTestVelocity(
    const std::vector<ArtifactCore::PyroVec3>& input,
    int width, int height, int depth, int iterations, bool previousFormula) {
  using ArtifactCore::PyroVec3;
  const auto index = [width, height](int x, int y, int z) {
    return static_cast<std::size_t>(x + width * (y + height * z));
  };
  const auto clampX = [width](int x) { return std::clamp(x, 0, width - 1); };
  const auto clampY = [height](int y) { return std::clamp(y, 0, height - 1); };
  const auto clampZ = [depth](int z) { return std::clamp(z, 0, depth - 1); };
  std::vector<float> divergence(input.size(), 0.0f);
  std::vector<float> pressure(input.size(), 0.0f);
  std::vector<float> nextPressure(input.size(), 0.0f);
  for (int z = 0; z < depth; ++z)
    for (int y = 0; y < height; ++y)
      for (int x = 0; x < width; ++x)
        divergence[index(x, y, z)] = 0.5f * (
            input[index(clampX(x + 1), y, z)].x - input[index(clampX(x - 1), y, z)].x +
            input[index(x, clampY(y + 1), z)].y - input[index(x, clampY(y - 1), z)].y +
            input[index(x, y, clampZ(z + 1))].z - input[index(x, y, clampZ(z - 1))].z);
  for (int iteration = 0; iteration < iterations; ++iteration) {
    for (int z = 0; z < depth; ++z) {
      for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
          float neighborSum = 0.0f;
          const int offset = previousFormula ? 1 : 2;
          neighborSum += pressure[index(clampX(x - offset), y, z)];
          neighborSum += pressure[index(clampX(x + offset), y, z)];
          neighborSum += pressure[index(x, clampY(y - offset), z)];
          neighborSum += pressure[index(x, clampY(y + offset), z)];
          neighborSum += pressure[index(x, y, clampZ(z - offset))];
          neighborSum += pressure[index(x, y, clampZ(z + offset))];
          nextPressure[index(x, y, z)] = previousFormula
              ? (divergence[index(x, y, z)] + neighborSum) / 6.0f
              : (neighborSum - 4.0f * divergence[index(x, y, z)]) / 6.0f;
        }
      }
    }
    pressure.swap(nextPressure);
  }
  std::vector<PyroVec3> output = input;
  for (int z = 0; z < depth; ++z)
    for (int y = 0; y < height; ++y)
      for (int x = 0; x < width; ++x) {
        auto& v = output[index(x, y, z)];
        v.x -= 0.5f * (pressure[index(clampX(x + 1), y, z)] - pressure[index(clampX(x - 1), y, z)]);
        v.y -= 0.5f * (pressure[index(x, clampY(y + 1), z)] - pressure[index(x, clampY(y - 1), z)]);
        v.z -= 0.5f * (pressure[index(x, y, clampZ(z + 1))] - pressure[index(x, y, clampZ(z - 1))]);
      }
  return output;
}

ArtifactCore::RayTrace::CPUVolumeRenderer makeRenderer(
    const ArtifactCore::PyroFrameSnapshot& snapshot,
    const ArtifactCore::PyroDomain& domain,
    const ArtifactCore::RayTrace::Point3& cameraPosition) {
  using namespace ArtifactCore::RayTrace;
  CPUVolumeRenderer renderer;
  renderer.bounds.min = {domain.min.x, domain.min.y, domain.min.z};
  renderer.bounds.max = {domain.max.x, domain.max.y, domain.max.z};
  VolumeFieldSet fields;
  fields.density = {
      const_cast<float*>(snapshot.density.values.data()),
      {snapshot.resolution.width, snapshot.resolution.height,
       snapshot.resolution.depth}};
  fields.temperature = {
      const_cast<float*>(snapshot.temperature.values.data()),
      {snapshot.resolution.width, snapshot.resolution.height,
       snapshot.resolution.depth}};
  renderer.setVolumeData(renderer.bounds, fields);
  renderer.settings.stepSize = 0.018f;
  renderer.settings.maxSteps = 256;
  renderer.settings.densityScale = 1.8f;
  renderer.settings.lightIntensity = 0.0f;
  renderer.settings.backgroundColor = {0.015f, 0.02f, 0.03f};
  renderer.settings.multipleScatteringStrength = 0.0f;
  renderer.camera = Camera(
      1.0f, 36.0f, 2.2f, 0.0f, cameraPosition, {0.0f, 0.55f, 0.0f},
      {0.0f, 1.0f, 0.0f});
  return renderer;
}

void applyMultiscaleCurl(ArtifactCore::PyroSimulation& simulation,
                        const ArtifactCore::PyroDomain& domain,
                        float timeSeconds,
                        float strengthScale,
                        float wavelengthScale) {
  const auto resolution = domain.resolution();
  const auto density = simulation.fields().densityView();
  auto velocity = simulation.fields().velocityView();
  constexpr float twoPi = 6.28318530718f;
  constexpr float wavelengths[] = {0.42f, 0.21f};
  constexpr float amplitudes[] = {0.025f, 0.008f};

  for (int z = 0; z < resolution.depth; ++z) {
    for (int y = 0; y < resolution.height; ++y) {
      for (int x = 0; x < resolution.width; ++x) {
        const float rho = std::max(0.0f, density.at(x, y, z));
        if (rho < 0.015f) continue;
        const float xw = domain.min.x +
            (static_cast<float>(x) + 0.5f) * domain.voxelSize;
        const float yw = domain.min.y +
            (static_cast<float>(y) + 0.5f) * domain.voxelSize;
        const float zw = domain.min.z +
            (static_cast<float>(z) + 0.5f) * domain.voxelSize;
        float ax = 0.0f, ay = 0.0f, az = 0.0f;
        float curlX = 0.0f, curlY = 0.0f, curlZ = 0.0f;
        for (int band = 0; band < 2; ++band) {
          const float k = twoPi / (wavelengths[band] * wavelengthScale);
          const float phase = timeSeconds * (0.8f + band * 1.1f);
          const float sx = std::sin(k * xw + phase);
          const float cx = std::cos(k * xw + phase);
          const float sy = std::sin(k * yw - phase * 0.7f);
          const float cy = std::cos(k * yw - phase * 0.7f);
          const float sz = std::sin(k * zw + phase * 0.43f);
          const float cz = std::cos(k * zw + phase * 0.43f);
          const float a = amplitudes[band] * strengthScale;
          ax += a * sy * sz;
          ay += a * sz * sx;
          az += a * sx * sy;
          curlX += a * k * (sx * cy - cz * sx);
          curlY += a * k * (sy * cz - cx * sy);
          curlZ += a * k * (cx * sz - cy * sz);
        }

        const auto sampleDensity = [&](int sx, int sy, int sz) {
          return std::max(0.0f, density.at(
              std::clamp(sx, 0, resolution.width - 1),
              std::clamp(sy, 0, resolution.height - 1),
              std::clamp(sz, 0, resolution.depth - 1)));
        };
        const float invTwoVoxel = 0.5f / domain.voxelSize;
        const float derivative = 0.12f / ((rho + 0.12f) * (rho + 0.12f));
        const float gradX = derivative *
            (sampleDensity(x + 1, y, z) - sampleDensity(x - 1, y, z)) * invTwoVoxel;
        const float gradY = derivative *
            (sampleDensity(x, y + 1, z) - sampleDensity(x, y - 1, z)) * invTwoVoxel;
        const float gradZ = derivative *
            (sampleDensity(x, y, z + 1) - sampleDensity(x, y, z - 1)) * invTwoVoxel;
        const float envelope = rho / (rho + 0.12f);
        velocity.at(x, y, z).x += envelope * curlX + gradY * az - gradZ * ay;
        velocity.at(x, y, z).y += envelope * curlY + gradZ * ax - gradX * az;
        velocity.at(x, y, z).z += envelope * curlZ + gradX * ay - gradY * ax;
      }
    }
  }
}

void applyTemporalCurlNoise(ArtifactCore::PyroSimulation& simulation,
                            const ArtifactCore::PyroDomain& domain,
                            float timeSeconds,
                            std::uint32_t seed,
                            float strength) {
  using ArtifactCore::PyroVec3;
  const auto resolution = domain.resolution();
  auto velocity = simulation.fields().velocityView();
  const float seedPhase = static_cast<float>(seed) * 0.173f;
  constexpr float twoPi = 6.28318530718f;
  constexpr float wavelengths[] = {0.36f, 0.18f, 0.09f};
  constexpr float weights[] = {1.0f, 0.55f, 0.25f};
  const float timePhase = timeSeconds * 0.9f;
  const auto densityField = simulation.fields().densityView();
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
      const float amount = 0.11f * strength * weights[band];
      result.x += amount * std::sin(k * (0.31f * xw + yw + 0.19f * zw) + phase);
      result.y += amount * std::sin(k * (0.23f * xw + 0.37f * yw + zw) - phase * 0.7f);
      result.z += amount * std::sin(k * (xw - 0.29f * yw + 0.41f * zw) + phase * 0.43f);
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
        const PyroVec3 kick{
            (ayp.z - aym.z - azp.y + azm.y) * invTwoVoxel,
            (azp.x - azm.x - axp.z + axm.z) * invTwoVoxel,
            (axp.y - axm.y - ayp.x + aym.x) * invTwoVoxel};
        velocity.at(x, y, z).x += kick.x;
        velocity.at(x, y, z).y += kick.y;
        velocity.at(x, y, z).z += kick.z;
      }
}
struct VelocityRotationResult {
  double relativeSpeedError = 0.0;
  double affectedSpeed = 0.0;
};

VelocityRotationResult applyLocalVelocityRotation(
    ArtifactCore::PyroSimulation& simulation,
    const ArtifactCore::PyroDomain& domain, float timeSeconds,
    std::uint32_t seed, float angleScale) {
  using ArtifactCore::PyroVec3;
  const auto resolution = domain.resolution();
  const auto density = simulation.fields().densityView();
  auto velocity = simulation.fields().velocityView();
  constexpr float twoPi = 6.28318530718f;
  const float phase = static_cast<float>(seed) * 0.173f + timeSeconds * 0.9f;
  double errorSquared = 0.0;
  double speedSquared = 0.0;
  std::size_t count = 0;
  for (int z = 1; z + 1 < resolution.depth; ++z) {
    for (int y = 1; y + 1 < resolution.height; ++y) {
      for (int x = 1; x + 1 < resolution.width; ++x) {
        const float rho = std::max(0.0f, density.at(x, y, z));
        if (rho < 0.015f) continue;
        const float xw = domain.min.x + (x + 0.5f) * domain.voxelSize;
        const float yw = domain.min.y + (y + 0.5f) * domain.voxelSize;
        const float zw = domain.min.z + (z + 0.5f) * domain.voxelSize;
        const float envelope = rho / (rho + 0.12f);
        const float angle = angleScale * envelope * (
            0.58f * std::sin(twoPi * (xw / 0.36f + yw / 0.18f) + phase) +
            0.30f * std::sin(twoPi * (yw / 0.18f + zw / 0.09f) - phase * 0.7f) +
            0.12f * std::sin(twoPi * (zw / 0.09f - xw / 0.36f) + phase * 0.43f));
        const float axisX = std::sin(twoPi * (yw / 0.36f + phase * 0.23f));
        const float axisY = std::sin(twoPi * (zw / 0.21f - phase * 0.31f));
        const float axisZ = std::sin(twoPi * (xw / 0.42f + phase * 0.17f));
        const float axisLength = std::sqrt(axisX * axisX + axisY * axisY + axisZ * axisZ);
        if (axisLength < 1e-5f) continue;
        const PyroVec3 axis{axisX / axisLength, axisY / axisLength, axisZ / axisLength};
        const PyroVec3 before = velocity.at(x, y, z);
        const float cosine = std::cos(angle);
        const float sine = std::sin(angle);
        const float dot = before.x * axis.x + before.y * axis.y + before.z * axis.z;
        const PyroVec3 cross{
            axis.y * before.z - axis.z * before.y,
            axis.z * before.x - axis.x * before.z,
            axis.x * before.y - axis.y * before.x};
        const PyroVec3 after{
            before.x * cosine + cross.x * sine + axis.x * dot * (1.0f - cosine),
            before.y * cosine + cross.y * sine + axis.y * dot * (1.0f - cosine),
            before.z * cosine + cross.z * sine + axis.z * dot * (1.0f - cosine)};
        const double oldSpeed2 = static_cast<double>(before.x) * before.x +
            static_cast<double>(before.y) * before.y + static_cast<double>(before.z) * before.z;
        const double newSpeed2 = static_cast<double>(after.x) * after.x +
            static_cast<double>(after.y) * after.y + static_cast<double>(after.z) * after.z;
        errorSquared += (std::sqrt(newSpeed2) - std::sqrt(oldSpeed2)) *
            (std::sqrt(newSpeed2) - std::sqrt(oldSpeed2));
        speedSquared += oldSpeed2;
        ++count;
        velocity.at(x, y, z) = after;
      }
    }
  }
  return {speedSquared > 0.0 ? std::sqrt(errorSquared / speedSquared) : 0.0,
          std::sqrt(speedSquared / std::max<std::size_t>(1, count))};
}

VelocityRotationResult applyVorticityAxisRotation(
    ArtifactCore::PyroSimulation& simulation,
    const ArtifactCore::PyroDomain& domain, float angleScale) {
  using ArtifactCore::PyroVec3;
  const auto resolution = domain.resolution();
  const auto density = simulation.fields().densityView();
  auto velocity = simulation.fields().velocityView();
  const float invTwoVoxel = 0.5f / domain.voxelSize;
  double errorSquared = 0.0;
  double speedSquared = 0.0;
  std::size_t count = 0;
  for (int z = 1; z + 1 < resolution.depth; ++z)
    for (int y = 1; y + 1 < resolution.height; ++y)
      for (int x = 1; x + 1 < resolution.width; ++x) {
        const float rho = std::max(0.0f, density.at(x, y, z));
        if (rho < 0.015f) continue;
        const auto xp = velocity.at(x + 1, y, z);
        const auto xm = velocity.at(x - 1, y, z);
        const auto yp = velocity.at(x, y + 1, z);
        const auto ym = velocity.at(x, y - 1, z);
        const auto zp = velocity.at(x, y, z + 1);
        const auto zm = velocity.at(x, y, z - 1);
        PyroVec3 axis{
            (yp.z - ym.z - zp.y + zm.y) * invTwoVoxel,
            (zp.x - zm.x - xp.z + xm.z) * invTwoVoxel,
            (xp.y - xm.y - yp.x + ym.x) * invTwoVoxel};
        const float curlLength = std::sqrt(axis.x * axis.x + axis.y * axis.y + axis.z * axis.z);
        if (curlLength < 1e-5f) continue;
        axis.x /= curlLength;
        axis.y /= curlLength;
        axis.z /= curlLength;
        const float envelope = rho / (rho + 0.12f);
        const float angle = angleScale * envelope * std::min(1.0f, curlLength * 0.02f);
        const PyroVec3 before = velocity.at(x, y, z);
        const float cosine = std::cos(angle);
        const float sine = std::sin(angle);
        const float dot = before.x * axis.x + before.y * axis.y + before.z * axis.z;
        const PyroVec3 cross{
            axis.y * before.z - axis.z * before.y,
            axis.z * before.x - axis.x * before.z,
            axis.x * before.y - axis.y * before.x};
        const PyroVec3 after{
            before.x * cosine + cross.x * sine + axis.x * dot * (1.0f - cosine),
            before.y * cosine + cross.y * sine + axis.y * dot * (1.0f - cosine),
            before.z * cosine + cross.z * sine + axis.z * dot * (1.0f - cosine)};
        const double oldSpeed = std::sqrt(static_cast<double>(before.x) * before.x +
            static_cast<double>(before.y) * before.y + static_cast<double>(before.z) * before.z);
        const double newSpeed = std::sqrt(static_cast<double>(after.x) * after.x +
            static_cast<double>(after.y) * after.y + static_cast<double>(after.z) * after.z);
        errorSquared += (newSpeed - oldSpeed) * (newSpeed - oldSpeed);
        speedSquared += oldSpeed * oldSpeed;
        ++count;
        velocity.at(x, y, z) = after;
      }
  return {speedSquared > 0.0 ? std::sqrt(errorSquared / speedSquared) : 0.0,
          std::sqrt(speedSquared / std::max<std::size_t>(1, count))};
}

void applyCanonicalVorticityConfinement(
    ArtifactCore::PyroSimulation& simulation,
    const ArtifactCore::PyroDomain& domain, float epsilon, float deltaSeconds,
    std::vector<ArtifactCore::PyroVec3>& velocitySnapshot,
    std::vector<float>& curlMagnitude) {
  using ArtifactCore::PyroVec3;
  const auto resolution = domain.resolution();
  auto velocity = simulation.fields().velocityView();
  const auto density = simulation.fields().densityView();
  const auto index = [resolution](int x, int y, int z) {
    return static_cast<std::size_t>(x + resolution.width * (y + resolution.height * z));
  };
  std::copy(velocity.values.begin(), velocity.values.end(), velocitySnapshot.begin());
  std::fill(curlMagnitude.begin(), curlMagnitude.end(), 0.0f);
  const float derivativeScale = 0.5f / domain.voxelSize;
  const auto curlAt = [&](int x, int y, int z) {
    const auto xp = velocitySnapshot[index(x + 1, y, z)];
    const auto xm = velocitySnapshot[index(x - 1, y, z)];
    const auto yp = velocitySnapshot[index(x, y + 1, z)];
    const auto ym = velocitySnapshot[index(x, y - 1, z)];
    const auto zp = velocitySnapshot[index(x, y, z + 1)];
    const auto zm = velocitySnapshot[index(x, y, z - 1)];
    return PyroVec3{(yp.z - ym.z - zp.y + zm.y) * derivativeScale,
                    (zp.x - zm.x - xp.z + xm.z) * derivativeScale,
                    (xp.y - xm.y - yp.x + ym.x) * derivativeScale};
  };
  for (int z = 1; z + 1 < resolution.depth; ++z)
    for (int y = 1; y + 1 < resolution.height; ++y)
      for (int x = 1; x + 1 < resolution.width; ++x) {
        const auto omega = curlAt(x, y, z);
        curlMagnitude[index(x, y, z)] = std::sqrt(omega.x * omega.x +
            omega.y * omega.y + omega.z * omega.z);
      }
  for (int z = 1; z + 1 < resolution.depth; ++z)
    for (int y = 1; y + 1 < resolution.height; ++y)
      for (int x = 1; x + 1 < resolution.width; ++x) {
        if (density.at(x, y, z) < 0.015f) continue;
        const float gx = (curlMagnitude[index(x + 1, y, z)] -
                          curlMagnitude[index(x - 1, y, z)]) * derivativeScale;
        const float gy = (curlMagnitude[index(x, y + 1, z)] -
                          curlMagnitude[index(x, y - 1, z)]) * derivativeScale;
        const float gz = (curlMagnitude[index(x, y, z + 1)] -
                          curlMagnitude[index(x, y, z - 1)]) * derivativeScale;
        const float gradientLength = std::sqrt(gx * gx + gy * gy + gz * gz);
        if (gradientLength < 1e-6f) continue;
        const auto omega = curlAt(x, y, z);
        const float scale = epsilon * domain.voxelSize * deltaSeconds / gradientLength;
        velocity.at(x, y, z).x += (gy * omega.z - gz * omega.y) * scale;
        velocity.at(x, y, z).y += (gz * omega.x - gx * omega.z) * scale;
        velocity.at(x, y, z).z += (gx * omega.y - gy * omega.x) * scale;
      }
}

struct MatchedCurlAdapterStats {
  double meanDeltaFraction = 0.0;
  double meanSpeedChangeFraction = 0.0;
  double discreteCurlDivergenceRms = 0.0;
  std::size_t affectedCells = 0;
};

MatchedCurlAdapterStats applyMatchedCurlTurbulenceOrShredding(
    ArtifactCore::PyroSimulation& simulation,
    const ArtifactCore::PyroDomain& domain, float timeSeconds,
    std::uint32_t seed, float targetSpeedFraction, bool preserveSpeed,
    float wavelengthScale = 1.0f, float temporalFrequencyScale = 1.0f,
    int firstBand = 0, int bandEnd = 3) {
  using ArtifactCore::PyroVec3;
  const auto resolution = domain.resolution();
  auto velocity = simulation.fields().velocityView();
  const auto density = simulation.fields().densityView();
  const auto index = [resolution](int x, int y, int z) {
    return static_cast<std::size_t>(x + resolution.width * (y + resolution.height * z));
  };
  constexpr float twoPi = 6.28318530718f;
  constexpr float baseWavelengths[] = {0.36f, 0.18f, 0.09f};
  constexpr float weights[] = {1.0f, 0.55f, 0.25f};
  const float seedPhase = static_cast<float>(seed) * 0.173f;
  const auto potential = [&](int x, int y, int z) {
    x = std::clamp(x, 0, resolution.width - 1);
    y = std::clamp(y, 0, resolution.height - 1);
    z = std::clamp(z, 0, resolution.depth - 1);
    const float xw = domain.min.x + (x + 0.5f) * domain.voxelSize;
    const float yw = domain.min.y + (y + 0.5f) * domain.voxelSize;
    const float zw = domain.min.z + (z + 0.5f) * domain.voxelSize;
    PyroVec3 result{};
    for (int band = firstBand; band < std::min(3, bandEnd); ++band) {
      const float k = twoPi / (baseWavelengths[band] * wavelengthScale);
      const float phase = seedPhase + timeSeconds * temporalFrequencyScale *
          (0.55f + band * 0.3f);
      result.x += weights[band] * std::sin(k * (0.31f * xw + yw + 0.19f * zw) + phase);
      result.y += weights[band] * std::sin(k * (0.23f * xw + 0.37f * yw + zw) - phase * 0.7f);
      result.z += weights[band] * std::sin(k * (xw - 0.29f * yw + 0.41f * zw) + phase * 0.43f);
    }
    const float rho = std::max(0.0f, density.at(x, y, z));
    const float envelope = rho / (rho + 0.12f);
    result.x *= envelope; result.y *= envelope; result.z *= envelope;
    return result;
  };
  const float derivativeScale = 0.5f / domain.voxelSize;
  MatchedCurlAdapterStats stats{};
  double deltaFractionSum = 0.0;
  double speedChangeFractionSum = 0.0;
  for (int z = 1; z + 1 < resolution.depth; ++z)
    for (int y = 1; y + 1 < resolution.height; ++y)
      for (int x = 1; x + 1 < resolution.width; ++x) {
        if (density.at(x, y, z) < 0.015f) continue;
        const float xw = domain.min.x + (x + 0.5f) * domain.voxelSize;
        const float yw = domain.min.y + (y + 0.5f) * domain.voxelSize;
        const float zw = domain.min.z + (z + 0.5f) * domain.voxelSize;
        const auto axp = potential(x + 1, y, z), axm = potential(x - 1, y, z);
        const auto ayp = potential(x, y + 1, z), aym = potential(x, y - 1, z);
        const auto azp = potential(x, y, z + 1), azm = potential(x, y, z - 1);
        const PyroVec3 curl{
            (ayp.z - aym.z - azp.y + azm.y) * derivativeScale,
            (azp.x - azm.x - axp.z + axm.z) * derivativeScale,
            (axp.y - axm.y - ayp.x + aym.x) * derivativeScale};
        const float curlLength = std::sqrt(curl.x * curl.x + curl.y * curl.y + curl.z * curl.z);
        const auto before = velocity.at(x, y, z);
        const float speed = std::sqrt(before.x * before.x + before.y * before.y + before.z * before.z);
        if (curlLength < 1e-6f || speed < 1e-4f) continue;
        const float invSpeed = 1.0f / speed;
        const PyroVec3 direction{curl.x / curlLength, curl.y / curlLength, curl.z / curlLength};
        PyroVec3 after = before;
        if (!preserveSpeed) {
          const float kick = targetSpeedFraction * speed;
          after.x += direction.x * kick;
          after.y += direction.y * kick;
          after.z += direction.z * kick;
        } else {
          const float along = direction.x * before.x * invSpeed +
              direction.y * before.y * invSpeed + direction.z * before.z * invSpeed;
          PyroVec3 tangent{direction.x - along * before.x * invSpeed,
                           direction.y - along * before.y * invSpeed,
                           direction.z - along * before.z * invSpeed};
          const float tangentLength = std::sqrt(tangent.x * tangent.x +
              tangent.y * tangent.y + tangent.z * tangent.z);
          if (tangentLength < 1e-5f) continue;
          tangent.x /= tangentLength; tangent.y /= tangentLength; tangent.z /= tangentLength;
          const PyroVec3 axis{
              before.y * tangent.z - before.z * tangent.y,
              before.z * tangent.x - before.x * tangent.z,
              before.x * tangent.y - before.y * tangent.x};
          const float angle = 2.0f * std::asin(std::clamp(targetSpeedFraction * 0.5f, 0.0f, 0.99f));
          const float c = std::cos(angle), s = std::sin(angle);
          const PyroVec3 cross{
              axis.y * before.z - axis.z * before.y,
              axis.z * before.x - axis.x * before.z,
              axis.x * before.y - axis.y * before.x};
          after = {before.x * c + cross.x * s / speed,
                   before.y * c + cross.y * s / speed,
                   before.z * c + cross.z * s / speed};
        }
        const float dx = after.x - before.x, dy = after.y - before.y, dz = after.z - before.z;
        const float newSpeed = std::sqrt(after.x * after.x + after.y * after.y + after.z * after.z);
        deltaFractionSum += std::sqrt(dx * dx + dy * dy + dz * dz) / speed;
        speedChangeFractionSum += std::abs(newSpeed - speed) / speed;
        ++stats.affectedCells;
        velocity.at(x, y, z) = after;
        (void)xw; (void)yw; (void)zw;
      }
  if (stats.affectedCells) {
    stats.meanDeltaFraction = deltaFractionSum / stats.affectedCells;
    stats.meanSpeedChangeFraction = speedChangeFractionSum / stats.affectedCells;
  }
  return stats;
}

MatchedCurlAdapterStats applyDivergenceFreeCurlTurbulence(
    ArtifactCore::PyroSimulation& simulation,
    const ArtifactCore::PyroDomain& domain, float timeSeconds,
    std::uint32_t seed, float targetSpeedFraction,
    float wavelengthScale, float temporalFrequencyScale) {
  using ArtifactCore::PyroVec3;
  const auto resolution = domain.resolution();
  auto velocity = simulation.fields().velocityView();
  const auto density = simulation.fields().densityView();
  const auto index = [resolution](int x, int y, int z) {
    return static_cast<std::size_t>(x + resolution.width *
        (y + resolution.height * z));
  };
  constexpr float twoPi = 6.28318530718f;
  constexpr std::array<float, 3> wavelengths{0.36f, 0.18f, 0.09f};
  constexpr std::array<float, 3> weights{1.0f, 0.55f, 0.25f};
  const float seedPhase = static_cast<float>(seed) * 0.173f;
  const auto potential = [&](int x, int y, int z) {
    x = std::clamp(x, 0, resolution.width - 1);
    y = std::clamp(y, 0, resolution.height - 1);
    z = std::clamp(z, 0, resolution.depth - 1);
    const float xw = domain.min.x + (x + 0.5f) * domain.voxelSize;
    const float yw = domain.min.y + (y + 0.5f) * domain.voxelSize;
    const float zw = domain.min.z + (z + 0.5f) * domain.voxelSize;
    PyroVec3 result{};
    for (int band = 0; band < 2; ++band) {
      const float k = twoPi / (wavelengths[band] * wavelengthScale);
      const float phase = seedPhase + timeSeconds * temporalFrequencyScale *
          (0.55f + band * 0.3f);
      result.x += weights[band] * std::sin(
          k * (0.31f * xw + yw + 0.19f * zw) + phase);
      result.y += weights[band] * std::sin(
          k * (0.23f * xw + 0.37f * yw + zw) - phase * 0.7f);
      result.z += weights[band] * std::sin(
          k * (xw - 0.29f * yw + 0.41f * zw) + phase * 0.43f);
    }
    const float rho = std::max(0.0f, density.at(x, y, z));
    const float envelope = rho / (rho + 0.12f);
    result.x *= envelope;
    result.y *= envelope;
    result.z *= envelope;
    return result;
  };
  const float derivativeScale = 0.5f / domain.voxelSize;
  const auto cellCount = static_cast<std::size_t>(resolution.width) *
      resolution.height * resolution.depth;
  std::vector<PyroVec3> curlField(cellCount);
  MatchedCurlAdapterStats stats{};
  for (int z = 1; z + 1 < resolution.depth; ++z)
    for (int y = 1; y + 1 < resolution.height; ++y)
      for (int x = 1; x + 1 < resolution.width; ++x) {
        const auto axp = potential(x + 1, y, z);
        const auto axm = potential(x - 1, y, z);
        const auto ayp = potential(x, y + 1, z);
        const auto aym = potential(x, y - 1, z);
        const auto azp = potential(x, y, z + 1);
        const auto azm = potential(x, y, z - 1);
        curlField[index(x, y, z)] = {
            (ayp.z - aym.z - azp.y + azm.y) * derivativeScale,
            (azp.x - azm.x - axp.z + axm.z) * derivativeScale,
            (axp.y - axm.y - ayp.x + aym.x) * derivativeScale};
      }

  double divergenceSquaredSum = 0.0;
  std::size_t divergenceSampleCount = 0;
  for (int z = 2; z + 2 < resolution.depth; ++z)
    for (int y = 2; y + 2 < resolution.height; ++y)
      for (int x = 2; x + 2 < resolution.width; ++x) {
        const auto xp = curlField[index(x + 1, y, z)];
        const auto xm = curlField[index(x - 1, y, z)];
        const auto yp = curlField[index(x, y + 1, z)];
        const auto ym = curlField[index(x, y - 1, z)];
        const auto zp = curlField[index(x, y, z + 1)];
        const auto zm = curlField[index(x, y, z - 1)];
        const double divergence = (xp.x - xm.x + yp.y - ym.y +
            zp.z - zm.z) * derivativeScale;
        divergenceSquaredSum += divergence * divergence;
        ++divergenceSampleCount;
      }
  if (divergenceSampleCount > 0)
    stats.discreteCurlDivergenceRms = std::sqrt(divergenceSquaredSum /
        static_cast<double>(divergenceSampleCount));

  double curlToSpeedSum = 0.0;
  for (int z = 1; z + 1 < resolution.depth; ++z)
    for (int y = 1; y + 1 < resolution.height; ++y)
      for (int x = 1; x + 1 < resolution.width; ++x) {
        if (density.at(x, y, z) < 0.015f) continue;
        const auto cell = index(x, y, z);
        const auto curl = curlField[cell];
        const auto before = velocity.values[cell];
        const float curlLength = std::sqrt(curl.x * curl.x + curl.y * curl.y +
            curl.z * curl.z);
        const float speed = std::sqrt(before.x * before.x + before.y * before.y +
            before.z * before.z);
        if (curlLength < 1e-6f || speed < 1e-4f) continue;
        curlToSpeedSum += curlLength / speed;
        ++stats.affectedCells;
      }
  if (stats.affectedCells == 0 || curlToSpeedSum <= 1e-9) return stats;

  const double curlScale = targetSpeedFraction * stats.affectedCells /
      curlToSpeedSum;
  double deltaFractionSum = 0.0;
  double speedChangeFractionSum = 0.0;
  for (int z = 1; z + 1 < resolution.depth; ++z)
    for (int y = 1; y + 1 < resolution.height; ++y)
      for (int x = 1; x + 1 < resolution.width; ++x) {
        if (density.at(x, y, z) < 0.015f) continue;
        const auto cell = index(x, y, z);
        const auto curl = curlField[cell];
        const auto before = velocity.values[cell];
        const float speed = std::sqrt(before.x * before.x + before.y * before.y +
            before.z * before.z);
        if (speed < 1e-4f) continue;
        const PyroVec3 delta{
            static_cast<float>(curl.x * curlScale),
            static_cast<float>(curl.y * curlScale),
            static_cast<float>(curl.z * curlScale)};
        const PyroVec3 after{before.x + delta.x, before.y + delta.y,
            before.z + delta.z};
        const float newSpeed = std::sqrt(after.x * after.x + after.y * after.y +
            after.z * after.z);
        deltaFractionSum += std::sqrt(delta.x * delta.x + delta.y * delta.y +
            delta.z * delta.z) / speed;
        speedChangeFractionSum += std::abs(newSpeed - speed) / speed;
        velocity.values[cell] = after;
      }
  stats.meanDeltaFraction = deltaFractionSum / stats.affectedCells;
  stats.meanSpeedChangeFraction = speedChangeFractionSum / stats.affectedCells;
  return stats;
}

double densityWorldGradientRms(const std::vector<float>& density,
                               int width, int height, int depth, float voxelSize) {
  const auto index = [width, height](int x, int y, int z) {
    return static_cast<std::size_t>(x + width * (y + height * z));
  };
  double squaredSum = 0.0;
  std::size_t count = 0;
  const float scale = 0.5f / voxelSize;
  for (int z = 1; z + 1 < depth; ++z)
    for (int y = 1; y + 1 < height; ++y)
      for (int x = 1; x + 1 < width; ++x) {
        const auto i = index(x, y, z);
        const double gx = (density[index(x + 1, y, z)] - density[index(x - 1, y, z)]) * scale;
        const double gy = (density[index(x, y + 1, z)] - density[index(x, y - 1, z)]) * scale;
        const double gz = (density[index(x, y, z + 1)] - density[index(x, y, z - 1)]) * scale;
        squaredSum += gx * gx + gy * gy + gz * gz;
        ++count;
      }
  return count ? std::sqrt(squaredSum / count) : 0.0;
}

struct SignedCurvatureRegion {
  int sign = 0;
  std::size_t cellCount = 0;
  ArtifactCore::PyroVec3 centroid{};
  ArtifactCore::PyroVec3 meanVelocity{};
};

struct DensityInterfaceVorticityStats {
  double meanTangentAlignment = 0.0;
  double meanNormalAlignment = 0.0;
  double curvatureRms = 0.0;
  std::size_t sampleCount = 0;
  std::size_t positiveCurvatureCells = 0;
  std::size_t negativeCurvatureCells = 0;
  std::size_t largestPositiveCurvatureRegion = 0;
  std::size_t largestNegativeCurvatureRegion = 0;
  std::vector<SignedCurvatureRegion> curvatureRegions;
  std::vector<std::int8_t> qualifyingCurvatureSignMask;
};

DensityInterfaceVorticityStats measureDensityInterfaceVorticity(
    const ArtifactCore::PyroSimulation& simulation,
    const ArtifactCore::PyroDomain& domain) {
  using ArtifactCore::PyroVec3;
  const auto density = simulation.fields().densityView();
  const auto velocity = simulation.fields().velocityView();
  const auto resolution = domain.resolution();
  const auto index = [resolution](int x, int y, int z) {
    return static_cast<std::size_t>(x + resolution.width *
        (y + resolution.height * z));
  };
  const auto gradientAt = [&](int x, int y, int z) {
    constexpr float inverseTwo = 0.5f;
    return PyroVec3{
        (density.at(x + 1, y, z) - density.at(x - 1, y, z)) * inverseTwo,
        (density.at(x, y + 1, z) - density.at(x, y - 1, z)) * inverseTwo,
        (density.at(x, y, z + 1) - density.at(x, y, z - 1)) * inverseTwo};
  };
  const auto normalizedGradientAt = [&](int x, int y, int z) {
    auto normal = gradientAt(x, y, z);
    const float length = std::sqrt(normal.x * normal.x + normal.y * normal.y +
        normal.z * normal.z);
    if (length <= 1e-6f) return PyroVec3{};
    normal.x /= length;
    normal.y /= length;
    normal.z /= length;
    return normal;
  };
  DensityInterfaceVorticityStats stats{};
  double tangentAlignmentSum = 0.0;
  double normalAlignmentSum = 0.0;
  double alignmentWeightSum = 0.0;
  double curvatureSquaredSum = 0.0;
  constexpr double foldCurvatureThreshold = 24.0;
  const auto cellCount = static_cast<std::size_t>(resolution.width) *
      resolution.height * resolution.depth;
  stats.qualifyingCurvatureSignMask.assign(cellCount, 0);
  std::vector<std::int8_t> curvatureSign(cellCount, 0);
  for (int z = 2; z + 2 < resolution.depth; ++z)
    for (int y = 2; y + 2 < resolution.height; ++y)
      for (int x = 2; x + 2 < resolution.width; ++x) {
        if (density.at(x, y, z) < 0.015f) continue;
        const auto gradient = gradientAt(x, y, z);
        const double gradientLength = std::sqrt(
            gradient.x * gradient.x + gradient.y * gradient.y +
            gradient.z * gradient.z);
        if (gradientLength < 1e-4) continue;
        const auto xp = velocity.values[index(x + 1, y, z)];
        const auto xm = velocity.values[index(x - 1, y, z)];
        const auto yp = velocity.values[index(x, y + 1, z)];
        const auto ym = velocity.values[index(x, y - 1, z)];
        const auto zp = velocity.values[index(x, y, z + 1)];
        const auto zm = velocity.values[index(x, y, z - 1)];
        const PyroVec3 curl{
            0.5f * ((yp.z - ym.z) - (zp.y - zm.y)),
            0.5f * ((zp.x - zm.x) - (xp.z - xm.z)),
            0.5f * ((xp.y - xm.y) - (yp.x - ym.x))};
        const double curlLength = std::sqrt(curl.x * curl.x + curl.y * curl.y +
            curl.z * curl.z);
        if (curlLength < 1e-5) continue;
        const double cosine = std::clamp(
            (curl.x * gradient.x + curl.y * gradient.y +
             curl.z * gradient.z) / (curlLength * gradientLength), -1.0, 1.0);
        const double normalAlignment = std::abs(cosine);
        const double tangentAlignment = std::sqrt(
            std::max(0.0, 1.0 - cosine * cosine));
        const double alignmentWeight = curlLength * gradientLength;
        tangentAlignmentSum += tangentAlignment * alignmentWeight;
        normalAlignmentSum += normalAlignment * alignmentWeight;
        alignmentWeightSum += alignmentWeight;

        const auto nxp = normalizedGradientAt(x + 1, y, z);
        const auto nxm = normalizedGradientAt(x - 1, y, z);
        const auto nyp = normalizedGradientAt(x, y + 1, z);
        const auto nym = normalizedGradientAt(x, y - 1, z);
        const auto nzp = normalizedGradientAt(x, y, z + 1);
        const auto nzm = normalizedGradientAt(x, y, z - 1);
        const double curvature = (
            nxp.x - nxm.x + nyp.y - nym.y + nzp.z - nzm.z) /
            (2.0 * domain.voxelSize);
        curvatureSquaredSum += curvature * curvature;
        const auto cell = index(x, y, z);
        if (curvature >= foldCurvatureThreshold) {
          curvatureSign[cell] = 1;
          ++stats.positiveCurvatureCells;
        } else if (curvature <= -foldCurvatureThreshold) {
          curvatureSign[cell] = -1;
          ++stats.negativeCurvatureCells;
        }
        ++stats.sampleCount;
      }
  std::vector<std::uint8_t> visited(cellCount, 0);
  std::vector<std::size_t> pending;
  pending.reserve(cellCount / 8);
  for (int z = 2; z + 2 < resolution.depth; ++z)
    for (int y = 2; y + 2 < resolution.height; ++y)
      for (int x = 2; x + 2 < resolution.width; ++x) {
        const auto first = index(x, y, z);
        if (curvatureSign[first] == 0 || visited[first] != 0) continue;
        const auto sign = curvatureSign[first];
        visited[first] = 1;
        pending.clear();
        pending.push_back(first);
        std::size_t cursor = 0;
        double sumX = 0.0;
        double sumY = 0.0;
        double sumZ = 0.0;
        double sumVelocityX = 0.0;
        double sumVelocityY = 0.0;
        double sumVelocityZ = 0.0;
        while (cursor < pending.size()) {
          const auto cell = pending[cursor++];
          const int cx = static_cast<int>(cell % resolution.width);
          const int cy = static_cast<int>(
              (cell / resolution.width) % resolution.height);
          const int cz = static_cast<int>(cell /
              (static_cast<std::size_t>(resolution.width) * resolution.height));
          const auto cellVelocity = velocity.values[cell];
          sumX += cx;
          sumY += cy;
          sumZ += cz;
          sumVelocityX += cellVelocity.x;
          sumVelocityY += cellVelocity.y;
          sumVelocityZ += cellVelocity.z;
          constexpr std::array<std::array<int, 3>, 6> offsets{{
              {{-1, 0, 0}}, {{1, 0, 0}}, {{0, -1, 0}},
              {{0, 1, 0}}, {{0, 0, -1}}, {{0, 0, 1}}}};
          for (const auto& offset : offsets) {
            const int nx = cx + offset[0];
            const int ny = cy + offset[1];
            const int nz = cz + offset[2];
            if (nx < 2 || nx + 2 >= resolution.width ||
                ny < 2 || ny + 2 >= resolution.height ||
                nz < 2 || nz + 2 >= resolution.depth)
              continue;
            const auto neighbor = index(nx, ny, nz);
            if (visited[neighbor] != 0 || curvatureSign[neighbor] != sign)
              continue;
            visited[neighbor] = 1;
            pending.push_back(neighbor);
          }
        }
        if (sign > 0)
          stats.largestPositiveCurvatureRegion = std::max(
              stats.largestPositiveCurvatureRegion, pending.size());
        else
          stats.largestNegativeCurvatureRegion = std::max(
              stats.largestNegativeCurvatureRegion, pending.size());
        const double inverseCount = 1.0 / static_cast<double>(pending.size());
        if (pending.size() >= 4) {
          for (const auto cell : pending)
            stats.qualifyingCurvatureSignMask[cell] = sign;
        }
        stats.curvatureRegions.push_back({
            sign,
            pending.size(),
            {static_cast<float>(sumX * inverseCount),
             static_cast<float>(sumY * inverseCount),
             static_cast<float>(sumZ * inverseCount)},
            {static_cast<float>(sumVelocityX * inverseCount),
             static_cast<float>(sumVelocityY * inverseCount),
             static_cast<float>(sumVelocityZ * inverseCount)}});
      }
  if (stats.sampleCount > 0) {
    if (alignmentWeightSum > 0.0) {
      stats.meanTangentAlignment = tangentAlignmentSum / alignmentWeightSum;
      stats.meanNormalAlignment = normalAlignmentSum / alignmentWeightSum;
    }
    stats.curvatureRms = std::sqrt(curvatureSquaredSum /
        static_cast<double>(stats.sampleCount));
  }
  return stats;
}

void saveIfRequested(const ArtifactCore::RayTrace::ImageBuffer& image,
                     const char* filename) {
  const char* outputDirectory = std::getenv("ARTIFACT_PYRO_VOLUME_OUTPUT_DIR");
  if (!outputDirectory || outputDirectory[0] == '\0') return;
  std::filesystem::create_directories(outputDirectory);
  const auto path = std::filesystem::path(outputDirectory) / filename;
  std::ofstream output(path, std::ios::binary);
  ASSERT_TRUE(output.good()) << path.string();
  output << "P6\n" << image.width << ' ' << image.height << "\n255\n";
  output.write(reinterpret_cast<const char*>(image.pixels.data()),
               static_cast<std::streamsize>(image.pixels.size()));
  ASSERT_TRUE(output.good()) << path.string();
}

} // namespace

TEST(ArtifactCorePyroVolumeSmokeTest,
     RendersProductionPyroDensityThroughCpuVolumeRenderer) {
  using namespace ArtifactCore;
  using namespace ArtifactCore::RayTrace;

  PyroDomain domain;
  domain.min = {-0.5f, 0.0f, -0.5f};
  domain.max = {0.5f, 1.5f, 0.5f};
  domain.voxelSize = 1.0f / 32.0f;
  domain.fixedTimeStep = 1.0f / 60.0f;
  domain.boundaryMode = PyroBoundaryMode::Open;
  domain.memoryBudgetBytes = 16u * 1024u * 1024u;

  PyroSimulation simulation(domain);
  PyroSimulationSettings settings;
  settings.sourceDensity = 12.0f;
  settings.sourceTemperature = 4.0f;
  settings.sourceFuel = 0.0f;
  settings.dissipation = 0.035f;
  settings.coolingRate = 0.18f;
  settings.buoyancy = 0.7f;
  settings.vorticity = 0.3f;
  settings.pressureIterations = 8.0f;
  settings.advectionClamp = 1.0f;
  simulation.setSettings(settings);
  const auto resolution = domain.resolution();
  auto densityField = simulation.fields().densityView();
  auto temperatureField = simulation.fields().temperatureView();
  auto velocityField = simulation.fields().velocityView();
  for (int z = 0; z < resolution.depth; ++z) {
    for (int y = 0; y < resolution.height; ++y) {
      for (int x = 0; x < resolution.width; ++x) {
        const float worldX = domain.min.x +
            (static_cast<float>(x) + 0.5f) * domain.voxelSize;
        const float worldY = domain.min.y +
            (static_cast<float>(y) + 0.5f) * domain.voxelSize;
        const float worldZ = domain.min.z +
            (static_cast<float>(z) + 0.5f) * domain.voxelSize;
        const float dx = worldX / 0.15f;
        const float dy = (worldY - 0.62f) / 0.38f;
        const float dz = worldZ / 0.095f;
        const float envelope = std::exp(-0.5f *
            (dx * dx + dy * dy + dz * dz));
        const float structure = 0.78f + 0.22f * std::sin(
            19.0f * worldX + 11.0f * worldY + 23.0f * worldZ) *
            std::sin(13.0f * worldZ - 7.0f * worldX);
        densityField.at(x, y, z) = envelope * structure;
        temperatureField.at(x, y, z) = envelope * 0.35f;
        velocityField.at(x, y, z) = {
            0.3f * std::sin(5.0f * worldZ),
            3.2f + 0.4f * std::sin(7.0f * worldX),
            0.3f * std::sin(6.0f * worldX)};
      }
    }
  }
  for (int frame = 0; frame < 24; ++frame) {
    simulation.step(1.0 / 30.0);
  }

  const auto snapshot = simulation.snapshot();
  ASSERT_EQ(snapshot.backend, PyroBackendKind::CPUReference);
  ASSERT_EQ(snapshot.resolution.width, 32);
  ASSERT_EQ(snapshot.resolution.height, 48);
  ASSERT_EQ(snapshot.resolution.depth, 32);
  ASSERT_FALSE(snapshot.density.empty());
  const auto density = snapshot.density.values;
  float maximumDensity = 0.0f;
  double densityMass = 0.0;
  for (const float value : density) {
    EXPECT_TRUE(std::isfinite(value));
    maximumDensity = std::max(maximumDensity, value);
    densityMass += value;
  }
  ASSERT_GT(maximumDensity, 0.0f);
  ASSERT_GT(densityMass, 0.0);

  auto frontRenderer = makeRenderer(snapshot, domain, {0.0f, 0.75f, 2.4f});
  auto sideRenderer = makeRenderer(snapshot, domain, {2.4f, 0.75f, 0.0f});
  const auto frontImage = frontRenderer.render(160, 160);
  const auto sideImage = sideRenderer.render(160, 160);
  auto litRenderer = makeRenderer(snapshot, domain, {0.0f, 0.75f, 2.4f});
  litRenderer.settings.lightIntensity = 5.0f;
  litRenderer.settings.phaseAnisotropy = 0.25f;
  litRenderer.settings.multipleScatteringStrength = 0.45f;
  litRenderer.settings.shadowIntensity = 0.0f;
  litRenderer.settings.shadowStepSize = 0.04f;
  litRenderer.settings.shadowMaxSteps = 96;
  const auto litImage = litRenderer.render(160, 160);
  auto directLightRenderer = makeRenderer(
      snapshot, domain, {0.0f, 0.75f, 2.4f});
  directLightRenderer.settings.lightIntensity = 5.0f;
  directLightRenderer.settings.phaseAnisotropy = 0.0f;
  directLightRenderer.settings.multipleScatteringStrength = 0.0f;
  directLightRenderer.settings.shadowIntensity = 0.0f;
  const auto directLightImage = directLightRenderer.render(160, 160);
  auto shadowRenderer = makeRenderer(
      snapshot, domain, {0.0f, 0.75f, 2.4f});
  shadowRenderer.settings.lightIntensity = 5.0f;
  shadowRenderer.settings.phaseAnisotropy = 0.0f;
  shadowRenderer.settings.multipleScatteringStrength = 0.0f;
  shadowRenderer.settings.shadowIntensity = 0.8f;
  shadowRenderer.settings.shadowStepSize = 0.5f;
  shadowRenderer.settings.shadowMaxSteps = 96;
  shadowRenderer.settings.shadowDensityScale = 0.08f;
  const auto shadowImage = shadowRenderer.render(160, 160);
  auto shadowScatterRenderer = makeRenderer(
      snapshot, domain, {0.0f, 0.75f, 2.4f});
  shadowScatterRenderer.settings.lightIntensity = 5.0f;
  shadowScatterRenderer.settings.phaseAnisotropy = 0.55f;
  shadowScatterRenderer.settings.multipleScatteringStrength = 0.45f;
  shadowScatterRenderer.settings.shadowIntensity = 0.8f;
  shadowScatterRenderer.settings.shadowStepSize = 0.5f;
  shadowScatterRenderer.settings.shadowMaxSteps = 96;
  shadowScatterRenderer.settings.shadowDensityScale = 0.08f;
  const auto shadowScatterImage = shadowScatterRenderer.render(160, 160);
  auto softScatterRenderer = makeRenderer(
      snapshot, domain, {0.0f, 0.75f, 2.4f});
  softScatterRenderer.settings.lightIntensity = 5.0f;
  softScatterRenderer.settings.phaseAnisotropy = 0.35f;
  softScatterRenderer.settings.multipleScatteringStrength = 0.12f;
  softScatterRenderer.settings.shadowIntensity = 0.8f;
  softScatterRenderer.settings.shadowStepSize = 0.5f;
  softScatterRenderer.settings.shadowMaxSteps = 96;
  softScatterRenderer.settings.shadowDensityScale = 0.08f;
  const auto softScatterImage = softScatterRenderer.render(160, 160);

  auto topRenderer = makeRenderer(snapshot, domain, {0.0f, 2.4f, 0.0f});
  topRenderer.camera = Camera(1.0f, 36.0f, 2.2f, 0.0f,
      {0.0f, 2.4f, 0.0f}, {0.0f, 0.65f, 0.0f}, {0.0f, 0.0f, 1.0f});
  const std::array<Point3, 3> responseCameraPositions{{
      {0.0f, 0.75f, 2.4f}, {2.4f, 0.75f, 0.0f}, {0.0f, 2.4f, 0.0f}}};
  const std::array<const char*, 3> responseViewNames{"front", "side", "top"};
  std::array<ImageBuffer, 3> responseBaselineImages{
      frontImage, sideImage, topRenderer.render(160, 160)};
  const auto foregroundLuma = [](const ImageBuffer& image) {
    constexpr double backgroundLuma = 0.2126 * 3.0 + 0.7152 * 5.0 + 0.0722 * 7.0;
    double sum = 0.0;
    for (std::size_t i = 0; i + 2 < image.pixels.size(); i += 3) {
      const double luma = 0.2126 * image.pixels[i] +
          0.7152 * image.pixels[i + 1] + 0.0722 * image.pixels[i + 2];
      sum += std::max(0.0, luma - backgroundLuma);
    }
    return sum;
  };
  const auto highPassRms = [](const ImageBuffer& image) {
    double squared = 0.0;
    std::size_t count = 0;
    const auto luma = [&](int x, int y) {
      const auto i = static_cast<std::size_t>((y * image.width + x) * 3);
      return 0.2126 * image.pixels[i] + 0.7152 * image.pixels[i + 1] +
          0.0722 * image.pixels[i + 2];
    };
    for (int y = 1; y + 1 < image.height; ++y)
      for (int x = 1; x + 1 < image.width; ++x) {
        const double residual = luma(x, y) - 0.25 * (luma(x - 1, y) +
            luma(x + 1, y) + luma(x, y - 1) + luma(x, y + 1));
        squared += residual * residual;
        ++count;
      }
    return count ? std::sqrt(squared / count) : 0.0;
  };
  for (std::size_t view = 0; view < responseViewNames.size(); ++view) {
    const auto baselineStats = measure(responseBaselineImages[view]);
    GTEST_LOG_(INFO) << "Pyro fixed-field exposure-matched baseline view="
        << responseViewNames[view]
        << " foregroundLuma=" << foregroundLuma(responseBaselineImages[view])
        << " occupied/luma/stddev/hp=" << baselineStats.occupiedPixels << '/'
        << baselineStats.luminanceSum << '/' << baselineStats.luminanceStdDev
        << '/' << highPassRms(responseBaselineImages[view]);
    const std::string filename = std::string("pyro_transfer_exposure_baseline_") +
        responseViewNames[view] + ".ppm";
    saveIfRequested(responseBaselineImages[view], filename.c_str());
  }
  for (int response = 1; response <= 2; ++response) {
    for (std::size_t view = 0; view < responseViewNames.size(); ++view) {
      auto renderer = makeRenderer(snapshot, domain, responseCameraPositions[view]);
      if (view == 2) {
        renderer.camera = Camera(1.0f, 36.0f, 2.2f, 0.0f,
            responseCameraPositions[view], {0.0f, 0.65f, 0.0f},
            {0.0f, 0.0f, 1.0f});
      }
      TransferFunction transfer;
      transfer.kind = TransferFunctionKind::Custom;
      if (response == 1) {
        transfer.customCallback = [](float d, float) {
          TransferFunctionSample sample;
          sample.densityValue = d;
          sample.emission = {0.55f, 0.55f, 0.55f};
          sample.absorption = d * 30.0f;
          return sample;
        };
      } else {
        transfer.customCallback = [](float d, float) {
          TransferFunctionSample sample;
          sample.densityValue = d;
          const float lift = std::sqrt(std::clamp(d * 5.0f, 0.0f, 1.0f));
          sample.emission = {0.42f * lift, 0.42f * lift, 0.42f * lift};
          sample.absorption = d * 30.0f;
          return sample;
        };
      }
      renderer.setTransferFunction(transfer);
      auto image = renderer.render(160, 160);
      const double sourceLuma = foregroundLuma(image);
      const double targetLuma = foregroundLuma(responseBaselineImages[view]);
      const double exposureScale = sourceLuma > 0.0 ? targetLuma / sourceLuma : 1.0;
      constexpr std::array<double, 3> background{3.0, 5.0, 7.0};
      for (std::size_t i = 0; i + 2 < image.pixels.size(); i += 3)
        for (std::size_t channel = 0; channel < 3; ++channel) {
          const double contribution = std::max(
              0.0, static_cast<double>(image.pixels[i + channel]) - background[channel]);
          image.pixels[i + channel] = static_cast<std::uint8_t>(std::clamp(
              std::lround(background[channel] + contribution * exposureScale),
              0l, 255l));
        }
      const auto stats = measure(image);
      GTEST_LOG_(INFO) << "Pyro fixed-field exposure-matched response="
          << (response == 1 ? "constant-albedo" : "thin-smoke-lift")
          << " view=" << responseViewNames[view]
          << " exposureScale=" << exposureScale
          << " foregroundLuma=" << foregroundLuma(image)
          << " occupied/luma/stddev/hp=" << stats.occupiedPixels << '/'
          << stats.luminanceSum << '/' << stats.luminanceStdDev << '/'
          << highPassRms(image);
      const std::string filename = std::string("pyro_transfer_exposure_matched_") +
          (response == 1 ? "constant_albedo_" : "thin_smoke_lift_") +
          responseViewNames[view] + ".ppm";
      saveIfRequested(image, filename.c_str());
      EXPECT_GT(stats.occupiedPixels, 100u);
    }
  }

  std::vector<float> resampledDensity(48u * 72u * 48u, 0.0f);
  VolumeScalarField sourceDensityField{
      const_cast<float*>(snapshot.density.values.data()),
      {snapshot.resolution.width, snapshot.resolution.height,
       snapshot.resolution.depth}};
  for (int z = 0; z < 48; ++z) {
    for (int y = 0; y < 72; ++y) {
      for (int x = 0; x < 48; ++x) {
        const Vec3 sourceCoord{
            static_cast<float>(x) * (snapshot.resolution.width - 1) / 47.0f,
            static_cast<float>(y) * (snapshot.resolution.height - 1) / 71.0f,
            static_cast<float>(z) * (snapshot.resolution.depth - 1) / 47.0f};
        const auto index = static_cast<std::size_t>(x + 48 * (y + 72 * z));
        resampledDensity[index] = sourceDensityField.sampleTrilinear(sourceCoord);
      }
    }
  }
  PyroFrameSnapshot highResolutionSnapshot = snapshot;
  highResolutionSnapshot.resolution = {48, 72, 48};
  highResolutionSnapshot.density.values = std::span<const float>(resampledDensity);
  highResolutionSnapshot.temperature = {};
  PyroDomain highResolutionDomain = domain;
  highResolutionDomain.voxelSize = 1.0f / 48.0f;

  const auto densityMetrics = [](const std::vector<float>& values) {
    std::array<double, 5> result{}; // mass, max, centroid x/y/z
    constexpr int width = 48, height = 72, depth = 48;
    for (std::size_t index = 0; index < values.size(); ++index) {
      const double value = values[index];
      const auto x = index % width;
      const auto y = (index / width) % height;
      const auto z = index / (width * height);
      result[0] += value;
      result[1] = std::max(result[1], value);
      result[2] += value * x;
      result[3] += value * y;
      result[4] += value * z;
    }
    if (result[0] > 0.0) {
      result[2] /= result[0];
      result[3] /= result[0];
      result[4] /= result[0];
    }
    return result;
  };
  const auto makeDensityDetail = [&](float wavelength, float amplitude) {
    constexpr int width = 48, height = 72, depth = 48;
    constexpr float twoPi = 6.28318530718f;
    const float waveNumber = twoPi / wavelength;
    const std::size_t count = resampledDensity.size();
    std::vector<float> noise(count, 0.0f);
    double baseMass = 0.0;
    double weightedNoise = 0.0;
    for (int z = 0; z < depth; ++z) {
      for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
          const auto index = static_cast<std::size_t>(x + width * (y + height * z));
          const float xw = highResolutionDomain.min.x +
              (static_cast<float>(x) + 0.5f) * highResolutionDomain.voxelSize;
          const float yw = highResolutionDomain.min.y +
              (static_cast<float>(y) + 0.5f) * highResolutionDomain.voxelSize;
          const float zw = highResolutionDomain.min.z +
              (static_cast<float>(z) + 0.5f) * highResolutionDomain.voxelSize;
          const float n0 = std::sin(waveNumber *
              (xw + 0.31f * yw + 0.17f * zw) + 0.4f);
          const float n1 = std::sin(waveNumber *
              (-0.23f * xw + 0.91f * yw + 0.34f * zw) + 1.7f);
          const float n2 = std::sin(waveNumber *
              (0.19f * xw - 0.37f * yw + 0.89f * zw) - 0.8f);
          noise[index] = (n0 + n1 + n2) / 3.0f;
          baseMass += resampledDensity[index];
          weightedNoise += resampledDensity[index] * noise[index];
        }
      }
    }
    const float meanNoise = static_cast<float>(weightedNoise / baseMass);
    std::vector<float> detailed(count, 0.0f);
    double detailedMass = 0.0;
    for (std::size_t index = 0; index < count; ++index) {
      const float modulation = std::max(
          0.08f, 1.0f + amplitude * (noise[index] - meanNoise));
      detailed[index] = resampledDensity[index] * modulation;
      detailedMass += detailed[index];
    }
    const float massCorrection = static_cast<float>(baseMass / detailedMass);
    for (float& value : detailed) value *= massCorrection;
    return detailed;
  };
  const auto renderDensityDetail = [&](const std::vector<float>& values,
                                       const char* name) {
    PyroFrameSnapshot field = highResolutionSnapshot;
    field.density.values = std::span<const float>(values);
    auto renderer = makeRenderer(field, highResolutionDomain,
                                  {0.0f, 0.75f, 2.4f});
    renderer.settings.lightIntensity = 5.0f;
    renderer.settings.phaseAnisotropy = 0.35f;
    renderer.settings.multipleScatteringStrength = 0.12f;
    renderer.settings.shadowIntensity = 0.8f;
    renderer.settings.shadowStepSize = 0.75f;
    renderer.settings.shadowMaxSteps = 96;
    renderer.settings.shadowDensityScale = 0.12f;
    const auto image = renderer.render(160, 160);
    const auto stats = measure(image);
    const auto fieldStats = densityMetrics(values);
    GTEST_LOG_(INFO) << "static 3D density detail " << name
        << " mass/max=" << fieldStats[0] << '/' << fieldStats[1]
        << " centroid=" << fieldStats[2] << ',' << fieldStats[3] << ','
        << fieldStats[4] << " fieldLaplacianRms="
        << densityLaplacianRms(values, 48, 72, 48)
        << " image occupied/luma/stddev=" << stats.occupiedPixels << '/'
        << stats.luminanceSum << '/' << stats.luminanceStdDev;
    const std::string filename = std::string(name) + ".ppm";
    saveIfRequested(image, filename.c_str());
    EXPECT_GT(stats.occupiedPixels, 100u);
    EXPECT_GT(fieldStats[0], 0.0);
  };
  renderDensityDetail(resampledDensity, "pyro_density_detail_base");
  const auto broadDetail = makeDensityDetail(0.25f, 0.45f);
  renderDensityDetail(broadDetail, "pyro_density_detail_broad");
  const auto fineDetail = makeDensityDetail(0.125f, 0.45f);
  renderDensityDetail(fineDetail, "pyro_density_detail_fine");

  const auto renderShadowStepSweep = [&](const PyroFrameSnapshot& field,
                                         const PyroDomain& fieldDomain,
                                         float voxelRatio,
                                         const char* namePrefix) {
    constexpr float stepIn32Voxels[] = {0.25f, 0.5f, 1.0f};
    constexpr int maxSteps[] = {192, 96, 48};
    constexpr float densityScale[] = {0.04f, 0.08f, 0.16f};
    std::array<RenderStats, 3> sweepStats{};
    for (int i = 0; i < 3; ++i) {
      auto renderer = makeRenderer(field, fieldDomain,
                                   {0.0f, 0.75f, 2.4f});
      renderer.settings.lightIntensity = 5.0f;
      renderer.settings.phaseAnisotropy = 0.0f;
      renderer.settings.multipleScatteringStrength = 0.0f;
      renderer.settings.shadowIntensity = 0.8f;
      renderer.settings.shadowStepSize = stepIn32Voxels[i] * voxelRatio;
      renderer.settings.shadowMaxSteps = maxSteps[i];
      renderer.settings.shadowDensityScale = densityScale[i];
      const auto image = renderer.render(160, 160);
      const auto stats = measure(image);
      sweepStats[static_cast<std::size_t>(i)] = stats;
      GTEST_LOG_(INFO) << "shadow sweep " << namePrefix
          << " step32vox=" << stepIn32Voxels[i]
          << " effectiveStep=" << renderer.settings.shadowStepSize
          << " maxSteps=" << maxSteps[i]
          << " shadowDensityScale=" << densityScale[i]
          << " occupied/luma/stddev=" << stats.occupiedPixels << '/'
          << stats.luminanceSum << '/' << stats.luminanceStdDev;
      const std::string filename = std::string(namePrefix) +
          "_shadow_step_" + std::to_string(i) + ".ppm";
      saveIfRequested(image, filename.c_str());
    }
    EXPECT_LT(std::abs(sweepStats[2].luminanceSum -
                       sweepStats[0].luminanceSum) /
                  sweepStats[1].luminanceSum,
              0.02);
    EXPECT_GT(sweepStats[1].occupiedPixels, 20000u);
    return sweepStats;
  };
  const auto shadowStats32 = renderShadowStepSweep(
      snapshot, domain, 1.0f, "pyro_volume_shadow_32");
  const auto shadowStats48 = renderShadowStepSweep(
      highResolutionSnapshot, highResolutionDomain, 1.5f,
      "pyro_volume_shadow_48_resampled");
  EXPECT_LT(std::abs(shadowStats48[1].luminanceSum -
                     shadowStats32[1].luminanceSum) /
                shadowStats32[1].luminanceSum,
            0.025);
  const auto frontStats = measure(frontImage);
  const auto sideStats = measure(sideImage);
  const auto litStats = measure(litImage);
  const auto directLightStats = measure(directLightImage);
  const auto shadowStats = measure(shadowImage);
  const auto shadowScatterStats = measure(shadowScatterImage);
  const auto softScatterStats = measure(softScatterImage);
  GTEST_LOG_(INFO) << "Pyro3D volume frame=" << snapshot.frameIndex
      << " resolution=" << snapshot.resolution.width << 'x'
      << snapshot.resolution.height << 'x' << snapshot.resolution.depth
      << " densityMass=" << densityMass << " max=" << maximumDensity
      << " front occupied/luma/centroidY=" << frontStats.occupiedPixels << '/'
      << frontStats.luminanceSum << '/' << frontStats.centroidY
      << " side occupied/luma/centroidY=" << sideStats.occupiedPixels << '/'
      << sideStats.luminanceSum << '/' << sideStats.centroidY;
  GTEST_LOG_(INFO) << "lit front occupied/luma/centroidY="
      << litStats.occupiedPixels << '/' << litStats.luminanceSum << '/'
      << litStats.centroidY << " max=" << litStats.maximumChannel
      << " lumaStdDev=" << litStats.luminanceStdDev;
  GTEST_LOG_(INFO) << "fixed-field lighting A/B direct/shadow/shadow+scatter"
      << " occupied=" << directLightStats.occupiedPixels << '/'
      << shadowStats.occupiedPixels << '/' << shadowScatterStats.occupiedPixels
      << " luma=" << directLightStats.luminanceSum << '/'
      << shadowStats.luminanceSum << '/' << shadowScatterStats.luminanceSum
      << '/' << softScatterStats.luminanceSum
      << " stddev=" << directLightStats.luminanceStdDev << '/'
      << shadowStats.luminanceStdDev << '/' << shadowScatterStats.luminanceStdDev
      << '/' << softScatterStats.luminanceStdDev
      << " max=" << directLightStats.maximumChannel << '/'
      << shadowStats.maximumChannel << '/' << shadowScatterStats.maximumChannel
      << '/' << softScatterStats.maximumChannel
      << " soft occupied=" << softScatterStats.occupiedPixels;
  EXPECT_GT(frontStats.occupiedPixels, 100);
  EXPECT_GT(sideStats.occupiedPixels, 100);
  EXPECT_GT(frontStats.luminanceSum, 1000.0);
  EXPECT_GT(sideStats.luminanceSum, 1000.0);
  EXPECT_GT(litStats.occupiedPixels, 100);
  EXPECT_GT(litStats.luminanceSum, frontStats.luminanceSum);
  EXPECT_GT(directLightStats.luminanceSum, 1000.0);
  EXPECT_GT(shadowStats.luminanceSum, 1000.0);
  EXPECT_GT(shadowScatterStats.luminanceSum, 1000.0);
  EXPECT_GT(softScatterStats.luminanceSum, 1000.0);
  EXPECT_GT(frontStats.maximumChannel, 5);
  EXPECT_GT(sideStats.maximumChannel, 5);
  ASSERT_EQ(frontImage.pixels.size(), sideImage.pixels.size());
  std::size_t differentChannels = 0;
  for (std::size_t index = 0; index < frontImage.pixels.size(); ++index) {
    if (frontImage.pixels[index] != sideImage.pixels[index]) {
      ++differentChannels;
    }
  }
  EXPECT_GT(differentChannels, 1000u);

  saveIfRequested(frontImage, "pyro_volume_front.ppm");
  saveIfRequested(sideImage, "pyro_volume_side.ppm");
  saveIfRequested(litImage, "pyro_volume_front_lit.ppm");
  saveIfRequested(directLightImage, "pyro_volume_front_direct_light.ppm");
  saveIfRequested(shadowImage, "pyro_volume_front_shadow.ppm");
  saveIfRequested(shadowScatterImage, "pyro_volume_front_shadow_scatter.ppm");
  saveIfRequested(softScatterImage, "pyro_volume_front_shadow_soft_scatter.ppm");
}

TEST(ArtifactCorePyroVolumeSmokeTest,
     ComparesFixedDensitySmokeTransferResponses) {
  using namespace ArtifactCore::RayTrace;

  constexpr int width = 48;
  constexpr int height = 72;
  constexpr int depth = 48;
  constexpr float voxel = 1.0f / 48.0f;
  std::vector<float> density(static_cast<std::size_t>(width * height * depth), 0.0f);
  for (int z = 0; z < depth; ++z) {
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        const float wx = -0.5f + (static_cast<float>(x) + 0.5f) * voxel;
        const float wy = (static_cast<float>(y) + 0.5f) * voxel;
        const float wz = -0.5f + (static_cast<float>(z) + 0.5f) * voxel;
        const float centerX = 0.055f * std::sin(6.0f * wy) +
            0.025f * std::sin(17.0f * wy + 2.0f * wz);
        const float radius = 0.115f + 0.035f * std::sin(11.0f * wy + 4.0f * wz);
        const float radial = ((wx - centerX) * (wx - centerX) + wz * wz) /
            (radius * radius);
        const float cap = (wy - 0.82f) / 0.28f;
        const float envelope = std::exp(-1.7f * radial - 1.4f * cap * cap) *
            std::clamp((wy - 0.12f) / 0.16f, 0.0f, 1.0f);
        const float billow = 0.62f + 0.38f * std::sin(19.0f * wx + 9.0f * wy) *
            std::sin(16.0f * wz - 7.0f * wy);
        density[static_cast<std::size_t>(x + width * (y + height * z))] =
            envelope * billow;
      }
    }
  }

  VolumeFieldSet fields;
  fields.density = {density.data(), {width, height, depth}};
  VolumeAABB bounds;
  bounds.min = {-0.5f, 0.0f, -0.5f};
  bounds.max = {0.5f, 1.5f, 0.5f};
  const auto render = [&](int response, float stepSize) {
    CPUVolumeRenderer renderer;
    renderer.setVolumeData(bounds, fields);
    renderer.settings.stepSize = stepSize;
    renderer.settings.maxSteps = 512;
    renderer.settings.densityScale = 1.8f;
    renderer.settings.lightIntensity = 0.0f;
    renderer.settings.multipleScatteringStrength = 0.0f;
    renderer.settings.backgroundColor = {0.015f, 0.02f, 0.03f};
    renderer.camera = Camera(1.0f, 36.0f, 2.2f, 0.0f,
        {0.0f, 0.75f, 2.4f}, {0.0f, 0.65f, 0.0f}, {0.0f, 1.0f, 0.0f});
    if (response == 1) {
      TransferFunction tf;
      tf.kind = TransferFunctionKind::Custom;
      tf.customCallback = [](float d, float) {
        TransferFunctionSample sample;
        sample.densityValue = d;
        sample.emission = {0.55f, 0.55f, 0.55f};
        sample.absorption = d * 30.0f;
        return sample;
      };
      renderer.setTransferFunction(tf);
    } else if (response == 2) {
      TransferFunction tf;
      tf.kind = TransferFunctionKind::Custom;
      tf.customCallback = [](float d, float) {
        TransferFunctionSample sample;
        sample.densityValue = d;
        const float lowDensityLift = std::sqrt(std::clamp(d * 5.0f, 0.0f, 1.0f));
        sample.emission = {0.42f * lowDensityLift, 0.42f * lowDensityLift,
                           0.42f * lowDensityLift};
        sample.absorption = d * 30.0f;
        return sample;
      };
      renderer.setTransferFunction(tf);
    }
    return renderer.render(144, 144);
  };
  const auto imageStats = [](const ImageBuffer& image) {
    std::array<double, 2> stats{}; // high-pass RMS and highlight fraction
    double highPassSquared = 0.0;
    std::size_t count = 0;
    std::size_t highlights = 0;
    const auto luma = [&](int x, int y) {
      const auto i = static_cast<std::size_t>((y * image.width + x) * 3);
      return 0.2126 * image.pixels[i] + 0.7152 * image.pixels[i + 1] +
          0.0722 * image.pixels[i + 2];
    };
    for (int y = 1; y + 1 < image.height; ++y) {
      for (int x = 1; x + 1 < image.width; ++x) {
        const double center = luma(x, y);
        const double cross = 0.25 * (luma(x - 1, y) + luma(x + 1, y) +
                                      luma(x, y - 1) + luma(x, y + 1));
        const double residual = center - cross;
        highPassSquared += residual * residual;
        highlights += center >= 220.0;
        ++count;
      }
    }
    stats[0] = count ? std::sqrt(highPassSquared / count) : 0.0;
    stats[1] = count ? static_cast<double>(highlights) / count : 0.0;
    return stats;
  };
  const auto volumeLuminance = [](const ImageBuffer& image) {
    constexpr double backgroundLuma =
        0.2126 * 3.0 + 0.7152 * 5.0 + 0.0722 * 7.0;
    double sum = 0.0;
    for (std::size_t i = 0; i + 2 < image.pixels.size(); i += 3) {
      const double luma = 0.2126 * image.pixels[i] +
          0.7152 * image.pixels[i + 1] + 0.0722 * image.pixels[i + 2];
      sum += std::max(0.0, luma - backgroundLuma);
    }
    return sum;
  };
  const auto matchVolumeExposure = [&](ImageBuffer& image, double target) {
    constexpr std::array<double, 3> background{3.0, 5.0, 7.0};
    const double source = volumeLuminance(image);
    if (source <= 0.0) return 1.0;
    const double scale = target / source;
    for (std::size_t i = 0; i + 2 < image.pixels.size(); i += 3) {
      for (std::size_t channel = 0; channel < 3; ++channel) {
        const double contribution = std::max(
            0.0, static_cast<double>(image.pixels[i + channel]) - background[channel]);
        image.pixels[i + channel] = static_cast<std::uint8_t>(std::clamp(
            std::lround(background[channel] + contribution * scale), 0l, 255l));
      }
    }
    return scale;
  };

  const std::array<const char*, 3> names{"builtin", "constant-albedo", "thin-smoke-lift"};
  std::array<ImageBuffer, 3> images;
  std::array<std::array<double, 2>, 3> stats{};
  for (int response = 0; response < 3; ++response) {
    images[response] = render(response, 0.018f);
    stats[response] = imageStats(images[response]);
    const auto appearance = measure(images[response]);
    GTEST_LOG_(INFO) << "fixed-density transfer " << names[response]
        << " hp-rms=" << stats[response][0]
        << " highlight-fraction=" << stats[response][1]
        << " occupied=" << appearance.occupiedPixels
        << " luma-sum=" << appearance.luminanceSum
        << " luma-stddev=" << appearance.luminanceStdDev;
    saveIfRequested(images[response], (std::string("pyro_transfer_") +
        names[response] + ".ppm").c_str());
  }

  const double targetVolumeLuminance = volumeLuminance(images[0]);
  for (int response = 0; response < 3; ++response) {
    auto matched = images[response];
    const double exposureScale = matchVolumeExposure(matched, targetVolumeLuminance);
    const auto matchedStats = imageStats(matched);
    GTEST_LOG_(INFO) << "exposure-matched transfer " << names[response]
        << " scale=" << exposureScale
        << " volume-luma=" << volumeLuminance(matched)
        << " hp-rms=" << matchedStats[0]
        << " highlight-fraction=" << matchedStats[1];
    saveIfRequested(matched, (std::string("pyro_transfer_exposure_matched_") +
        names[response] + ".ppm").c_str());
  }

  const auto coarse = render(1, 0.04f);
  const auto medium = render(1, 0.02f);
  const auto fine = render(1, 0.01f);
  const auto coarseStats = measure(coarse);
  const auto mediumStats = measure(medium);
  const auto fineStats = measure(fine);
  GTEST_LOG_(INFO) << "fixed-density ray step 0.04/0.02/0.01 luma="
      << coarseStats.luminanceSum << '/' << mediumStats.luminanceSum << '/'
      << fineStats.luminanceSum << " hp-rms=" << imageStats(coarse)[0] << '/'
      << imageStats(medium)[0] << '/' << imageStats(fine)[0];
  EXPECT_GT(coarseStats.occupiedPixels, 100u);
  EXPECT_GT(mediumStats.occupiedPixels, 100u);
  EXPECT_GT(fineStats.occupiedPixels, 100u);
  EXPECT_GT(stats[0][0], 0.0);
  EXPECT_GT(stats[1][0], 0.0);
  EXPECT_GT(stats[2][0], 0.0);
}

TEST(ArtifactCorePyroVolumeSmokeTest,
     ComparesMultiscaleCurlOnRepeatedThreeDimensionalEmitter) {
  using namespace ArtifactCore;
  using namespace ArtifactCore::RayTrace;

  PyroDomain domain;
  domain.min = {-0.5f, 0.0f, -0.5f};
  domain.max = {0.5f, 1.5f, 0.5f};
  domain.voxelSize = 1.0f / 48.0f;
  domain.fixedTimeStep = 1.0f / 60.0f;
  domain.boundaryMode = PyroBoundaryMode::Open;
  domain.memoryBudgetBytes = 32u * 1024u * 1024u;

  PyroSimulation baseline(domain);
  PyroSimulation sourceSwirl(domain);
  PyroSimulation swirling(domain);
  PyroSimulation strongCurl(domain);
  PyroSimulation fineCurl(domain);
  PyroSimulationSettings settings;
  settings.dissipation = 0.035f;
  settings.coolingRate = 0.18f;
  settings.buoyancy = 0.7f;
  settings.vorticity = 0.45f;
  settings.pressureIterations = 8.0f;
  baseline.setSettings(settings);
  sourceSwirl.setSettings(settings);
  swirling.setSettings(settings);
  strongCurl.setSettings(settings);
  fineCurl.setSettings(settings);
  std::vector<float> previousBaselineDensity;
  std::vector<float> previousStrongCurlDensity;
  std::vector<float> previousFineCurlDensity;
  FieldTemporalStats baselineTemporal{};
  FieldTemporalStats strongCurlTemporal{};
  FieldTemporalStats fineCurlTemporal{};

  const auto makeSource = [](float x, float y, float z,
                             float density, float temperature,
                             float vx, float vy, float vz) {
    PyroSourceState source;
    source.position = {x, y, z};
    source.extent = {0.13f, 0.09f, 0.13f};
    source.velocity = {vx, vy, vz};
    source.density = density;
    source.temperature = temperature;
    source.fuel = 0.0f;
    source.enabled = true;
    return source;
  };
  constexpr float emitterY = 0.24f;
  const std::array<PyroSourceState, 1> centeredEmitter{
      makeSource(0.0f, emitterY, 0.0f, 4.0f, 1.5f, 0.0f, 240.0f, 0.0f)};
  const std::array<PyroSourceState, 4> rotatingEmitter{
      makeSource(0.045f, emitterY, 0.0f, 1.0f, 0.375f, 0.0f, 240.0f, 180.0f),
      makeSource(-0.045f, emitterY, 0.0f, 1.0f, 0.375f, 0.0f, 240.0f, -180.0f),
      makeSource(0.0f, emitterY, 0.045f, 1.0f, 0.375f, -180.0f, 240.0f, 0.0f),
      makeSource(0.0f, emitterY, -0.045f, 1.0f, 0.375f, 180.0f, 240.0f, 0.0f)};
  for (int frame = 0; frame < 36; ++frame) {
    for (int substep = 0; substep < 2; ++substep) {
      const float timeSeconds = static_cast<float>(frame * 2 + substep) / 60.0f;
      baseline.setSources(centeredEmitter);
      sourceSwirl.setSources(rotatingEmitter);
      swirling.setSources(centeredEmitter);
      strongCurl.setSources(centeredEmitter);
      fineCurl.setSources(centeredEmitter);
      applyMultiscaleCurl(swirling, domain, timeSeconds, 1.0f, 1.0f);
      applyMultiscaleCurl(strongCurl, domain, timeSeconds, 16.0f, 1.0f);
      applyMultiscaleCurl(fineCurl, domain, timeSeconds, 1.0f, 0.5f);
      baseline.step(1.0 / 60.0);
      sourceSwirl.step(1.0 / 60.0);
      swirling.step(1.0 / 60.0);
      strongCurl.step(1.0 / 60.0);
      fineCurl.step(1.0 / 60.0);
    }
    updateTemporalStats(baseline.fields().densityStorage(),
                        previousBaselineDensity, baselineTemporal);
    updateTemporalStats(strongCurl.fields().densityStorage(),
                        previousStrongCurlDensity, strongCurlTemporal);
    updateTemporalStats(fineCurl.fields().densityStorage(),
                        previousFineCurlDensity, fineCurlTemporal);
  }
  const auto baselineSnapshot = baseline.snapshot();
  const auto sourceSwirlSnapshot = sourceSwirl.snapshot();
  const auto swirlSnapshot = swirling.snapshot();
  const auto strongCurlSnapshot = strongCurl.snapshot();
  const auto fineCurlSnapshot = fineCurl.snapshot();
  ASSERT_FALSE(baselineSnapshot.density.empty());
  ASSERT_FALSE(sourceSwirlSnapshot.density.empty());
  ASSERT_FALSE(swirlSnapshot.density.empty());
  ASSERT_FALSE(strongCurlSnapshot.density.empty());
  ASSERT_FALSE(fineCurlSnapshot.density.empty());

  struct DensityStats {
    double mass = 0.0;
    float maximum = 0.0f;
    double centroidX = 0.0;
    double centroidY = 0.0;
    double centroidZ = 0.0;
  };
  const auto densityStats = [](const PyroFrameSnapshot& snapshot) {
    DensityStats stats{};
    const auto r = snapshot.resolution;
    for (std::size_t index = 0; index < snapshot.density.values.size(); ++index) {
      const float value = snapshot.density.values[index];
      EXPECT_TRUE(std::isfinite(value));
      const auto x = index % static_cast<std::size_t>(r.width);
      const auto y = (index / static_cast<std::size_t>(r.width)) %
          static_cast<std::size_t>(r.height);
      const auto z = index / (static_cast<std::size_t>(r.width) * r.height);
      stats.mass += value;
      stats.maximum = std::max(stats.maximum, value);
      stats.centroidX += value * static_cast<double>(x);
      stats.centroidY += value * static_cast<double>(y);
      stats.centroidZ += value * static_cast<double>(z);
    }
    if (stats.mass > 0.0) {
      stats.centroidX /= stats.mass;
      stats.centroidY /= stats.mass;
      stats.centroidZ /= stats.mass;
    }
    return stats;
  };
  const auto baselineDensity = densityStats(baselineSnapshot);
  const auto sourceSwirlDensity = densityStats(sourceSwirlSnapshot);
  const auto swirlDensity = densityStats(swirlSnapshot);
  const auto strongCurlDensity = densityStats(strongCurlSnapshot);
  const auto fineCurlDensity = densityStats(fineCurlSnapshot);
  ASSERT_GT(baselineTemporal.comparisons, 0u);
  ASSERT_GT(strongCurlTemporal.comparisons, 0u);
  ASSERT_GT(fineCurlTemporal.comparisons, 0u);
  ASSERT_GT(baselineDensity.mass, 1.0);
  ASSERT_GT(sourceSwirlDensity.mass, 1.0);
  ASSERT_GT(swirlDensity.mass, 1.0);
  ASSERT_GT(strongCurlDensity.mass, 1.0);
  ASSERT_GT(fineCurlDensity.mass, 1.0);

  const auto configureAppearance = [](CPUVolumeRenderer& renderer) {
    renderer.settings.lightIntensity = 5.0f;
    renderer.settings.phaseAnisotropy = 0.25f;
    renderer.settings.multipleScatteringStrength = 0.45f;
    renderer.settings.shadowIntensity = 0.0f;
    renderer.settings.densityScale = 0.7f;
    renderer.settings.stepSize = 0.012f;
  };
  auto baselineFront = makeRenderer(
      baselineSnapshot, domain, {0.0f, 0.75f, 2.4f});
  auto sourceSwirlFront = makeRenderer(
      sourceSwirlSnapshot, domain, {0.0f, 0.75f, 2.4f});
  auto swirlFront = makeRenderer(
      swirlSnapshot, domain, {0.0f, 0.75f, 2.4f});
  auto strongCurlFront = makeRenderer(
      strongCurlSnapshot, domain, {0.0f, 0.75f, 2.4f});
  auto fineCurlFront = makeRenderer(
      fineCurlSnapshot, domain, {0.0f, 0.75f, 2.4f});
  auto swirlSide = makeRenderer(
      swirlSnapshot, domain, {2.4f, 0.75f, 0.0f});
  configureAppearance(baselineFront);
  configureAppearance(sourceSwirlFront);
  configureAppearance(swirlFront);
  configureAppearance(strongCurlFront);
  configureAppearance(fineCurlFront);
  configureAppearance(swirlSide);
  const auto baselineImage = baselineFront.render(192, 192);
  const auto sourceSwirlImage = sourceSwirlFront.render(192, 192);
  const auto swirlImage = swirlFront.render(192, 192);
  const auto strongCurlImage = strongCurlFront.render(192, 192);
  const auto fineCurlImage = fineCurlFront.render(192, 192);
  const auto sideImage = swirlSide.render(192, 192);
  const auto baselineStats = measure(baselineImage);
  const auto sourceSwirlStats = measure(sourceSwirlImage);
  const auto swirlStats = measure(swirlImage);
  const auto strongCurlStats = measure(strongCurlImage);
  const auto fineCurlStats = measure(fineCurlImage);
  const auto sideStats = measure(sideImage);
  ASSERT_EQ(baselineImage.pixels.size(), swirlImage.pixels.size());
  std::size_t sourceSwirlDifferentChannels = 0;
  std::size_t curlDifferentChannels = 0;
  std::size_t strongCurlDifferentChannels = 0;
  std::size_t fineCurlDifferentChannels = 0;
  for (std::size_t index = 0; index < baselineImage.pixels.size(); ++index) {
    if (baselineImage.pixels[index] != sourceSwirlImage.pixels[index])
      ++sourceSwirlDifferentChannels;
    if (baselineImage.pixels[index] != swirlImage.pixels[index])
      ++curlDifferentChannels;
    if (baselineImage.pixels[index] != strongCurlImage.pixels[index])
      ++strongCurlDifferentChannels;
    if (baselineImage.pixels[index] != fineCurlImage.pixels[index])
      ++fineCurlDifferentChannels;
  }
  GTEST_LOG_(INFO) << "3D emitter curl A/B frames=36 resolution=48x72x48"
      << " baseline mass/max=" << baselineDensity.mass << '/'
      << baselineDensity.maximum << " centroid=" << baselineDensity.centroidX
      << ',' << baselineDensity.centroidY << ',' << baselineDensity.centroidZ
      << " source-swirl mass/max=" << sourceSwirlDensity.mass << '/'
      << sourceSwirlDensity.maximum << " centroid="
      << sourceSwirlDensity.centroidX << ',' << sourceSwirlDensity.centroidY
      << ',' << sourceSwirlDensity.centroidZ
      << " multiscale-curl mass/max=" << swirlDensity.mass << '/'
      << swirlDensity.maximum << " centroid=" << swirlDensity.centroidX << ','
      << swirlDensity.centroidY << ',' << swirlDensity.centroidZ
      << " strong-curl-16x mass/max=" << strongCurlDensity.mass << '/'
      << strongCurlDensity.maximum << " centroid="
      << strongCurlDensity.centroidX << ',' << strongCurlDensity.centroidY
      << ',' << strongCurlDensity.centroidZ
      << " fine-band-0p5 mass/max=" << fineCurlDensity.mass << '/'
      << fineCurlDensity.maximum << " centroid=" << fineCurlDensity.centroidX
      << ',' << fineCurlDensity.centroidY << ',' << fineCurlDensity.centroidZ
      << " front occupied=" << baselineStats.occupiedPixels << '/'
      << sourceSwirlStats.occupiedPixels << '/' << swirlStats.occupiedPixels
      << '/' << strongCurlStats.occupiedPixels
      << '/' << fineCurlStats.occupiedPixels
      << " luma=" << baselineStats.luminanceSum << '/'
      << sourceSwirlStats.luminanceSum << '/' << swirlStats.luminanceSum
      << '/' << strongCurlStats.luminanceSum
      << '/' << fineCurlStats.luminanceSum
      << " changed channels=" << sourceSwirlDifferentChannels << '/'
      << curlDifferentChannels << '/' << strongCurlDifferentChannels
      << '/' << fineCurlDifferentChannels
      << " temporal cosine baseline/16x="
      << baselineTemporal.cosineSum / baselineTemporal.comparisons << '/'
      << strongCurlTemporal.cosineSum / strongCurlTemporal.comparisons
      << " fine=" << fineCurlTemporal.cosineSum /
          fineCurlTemporal.comparisons
      << " relL1=" << baselineTemporal.relativeL1Sum /
          baselineTemporal.comparisons << '/'
      << strongCurlTemporal.relativeL1Sum /
          strongCurlTemporal.comparisons << '/'
      << fineCurlTemporal.relativeL1Sum / fineCurlTemporal.comparisons
      << " side occupied/luma=" << sideStats.occupiedPixels
      << '/' << sideStats.luminanceSum;
  EXPECT_GT(baselineStats.occupiedPixels, 100);
  EXPECT_GT(sourceSwirlStats.occupiedPixels, 100);
  EXPECT_GT(swirlStats.occupiedPixels, 100);
  EXPECT_GT(strongCurlStats.occupiedPixels, 100);
  EXPECT_GT(fineCurlStats.occupiedPixels, 100);
  EXPECT_GT(sideStats.occupiedPixels, 100);
  EXPECT_GT(sourceSwirlDifferentChannels, 100u);
  EXPECT_GT(curlDifferentChannels, 100u);
  EXPECT_GT(strongCurlDifferentChannels, 100u);
  EXPECT_GT(fineCurlDifferentChannels, 100u);
  saveIfRequested(baselineImage, "pyro_volume_curl_baseline_front.ppm");
  saveIfRequested(sourceSwirlImage, "pyro_volume_curl_source_swirl_front.ppm");
  saveIfRequested(swirlImage, "pyro_volume_curl_multiscale_front.ppm");
  saveIfRequested(strongCurlImage, "pyro_volume_curl_strong_front.ppm");
  saveIfRequested(fineCurlImage, "pyro_volume_curl_fine_front.ppm");
  saveIfRequested(sideImage, "pyro_volume_curl_multiscale_side.ppm");
}

TEST(ArtifactCorePyroVolumeSmokeTest,
     ComparesTemporalCurlNoiseAcrossSeedsAndAdjacentFrames) {
  using namespace ArtifactCore;
  using namespace ArtifactCore::RayTrace;
  constexpr std::array<std::uint32_t, 3> seeds{7u, 29u, 83u};
  constexpr std::array<float, 4> strengths{0.0f, 0.03125f, 0.0625f, 0.125f};
  PyroDomain domain;
  domain.min = {-0.5f, 0.0f, -0.5f};
  domain.max = {0.5f, 1.5f, 0.5f};
  domain.voxelSize = 1.0f / 32.0f;
  domain.fixedTimeStep = 1.0f / 60.0f;
  domain.boundaryMode = PyroBoundaryMode::Open;
  domain.memoryBudgetBytes = 32u * 1024u * 1024u;
  PyroSimulationSettings settings;
  settings.sourceDensity = 1.8f;
  settings.sourceTemperature = 0.0f;
  settings.sourceFuel = 0.0f;
  settings.dissipation = 0.018f;
  settings.coolingRate = 0.0f;
  settings.buoyancy = 0.0f;
  settings.vorticity = 0.0f;
  settings.pressureIterations = 12.0f;
  std::array<std::array<double, 4>, 3> finalMass{};
  std::array<std::array<std::array<double, 3>, 4>, 3> finalCentroid{};
  std::array<std::array<double, 4>, 3> finalLaplacian{};
  std::array<std::array<double, 4>, 3> fieldCosine{};
  std::array<std::array<double, 4>, 3> frameResidual{};
  std::array<std::array<double, 4>, 3> frameBrightnessDelta{};
  std::vector<RenderStats> imageStats(36);
  std::vector<ArtifactCore::RayTrace::ImageBuffer> images(36);
  std::array<std::array<std::array<double, 5>, 4>, 3> massHistory{};
  std::array<std::array<std::array<double, 5>, 4>, 3> divergenceHistory{};
  std::array<std::array<double, 4>, 3> projectedDivergenceRms{};

  for (std::size_t seedIndex = 0; seedIndex < seeds.size(); ++seedIndex) {
    for (std::size_t condition = 0; condition < strengths.size(); ++condition) {
      PyroSimulation simulation(domain);
      simulation.setSettings(settings);
      auto density = simulation.fields().densityView();
      const auto resolution = domain.resolution();
      for (int z = 0; z < resolution.depth; ++z) {
        for (int y = 0; y < resolution.height; ++y) {
          for (int x = 0; x < resolution.width; ++x) {
            const float xw = domain.min.x +
                (static_cast<float>(x) + 0.5f) * domain.voxelSize;
            const float yw = domain.min.y +
                (static_cast<float>(y) + 0.5f) * domain.voxelSize;
            const float zw = domain.min.z +
                (static_cast<float>(z) + 0.5f) * domain.voxelSize;
            const float dx = xw / 0.18f;
            const float dy = (yw - 0.48f) / 0.26f;
            const float dz = zw / 0.18f;
            const float radius = std::sqrt(dx * dx + dy * dy + dz * dz);
            const float mottling = 0.78f + 0.22f * std::sin(
                13.0f * xw + 9.0f * yw + 17.0f * zw + seeds[seedIndex] * 0.11f) *
                std::sin(7.0f * zw - 11.0f * yw);
            density.at(x, y, z) = radius < 1.0f
                ? (1.0f - radius) * mottling * 0.72f : 0.0f;
          }
        }
      }
      std::vector<float> previousDensity;
      FieldTemporalStats temporal{};
      for (int frame = 0; frame < 24; ++frame) {
        for (int substep = 0; substep < 2; ++substep) {
          const float time = static_cast<float>(frame * 2 + substep) / 60.0f;
          if (strengths[condition] > 0.0f) {
            applyTemporalCurlNoise(simulation, domain, time,
                                   seeds[seedIndex], strengths[condition]);
          }
          simulation.step(1.0 / 60.0);
        }
        if (frame == 0 || frame == 3 || frame == 7 || frame == 15 || frame == 23) {
          const std::size_t sample = frame == 0 ? 0 : frame == 3 ? 1 :
              frame == 7 ? 2 : frame == 15 ? 3 : 4;
          double mass = 0.0;
          for (const float value : simulation.fields().densityView().values)
            mass += value;
          double divergenceSum = 0.0;
          for (const float value : simulation.fields().divergenceView().values)
            divergenceSum += static_cast<double>(value) * value;
          massHistory[seedIndex][condition][sample] = mass;
          divergenceHistory[seedIndex][condition][sample] =
              std::sqrt(divergenceSum / simulation.fields().divergenceView().size());
        }
        updateTemporalStats(simulation.fields().densityStorage(),
                            previousDensity, temporal);
        if (frame >= 21) {
          const int capture = frame - 21;
          const auto frameSnapshot = simulation.snapshot();
          auto renderer = makeRenderer(
              frameSnapshot, domain, {2.4f, 0.8f, 2.4f});
          renderer.camera = Camera(
              1.0f, 36.0f, 2.1f, 0.0f, {0.0f, 0.8f, 2.4f},
              {0.0f, 0.55f, 0.0f}, {0.0f, 1.0f, 0.0f});
          renderer.settings.stepSize = 0.022f;
          renderer.settings.maxSteps = 128;
          renderer.settings.densityScale = 1.8f;
          renderer.settings.lightIntensity = 5.0f;
          renderer.settings.phaseAnisotropy = 0.35f;
          renderer.settings.multipleScatteringStrength = 0.12f;
          renderer.settings.shadowIntensity = 0.0f;
          images[(seedIndex * 4 + condition) * 3 + capture] = renderer.render(160, 160);
          imageStats[(seedIndex * 4 + condition) * 3 + capture] =
              measure(images[(seedIndex * 4 + condition) * 3 + capture]);
          EXPECT_GT(imageStats[(seedIndex * 4 + condition) * 3 + capture].occupiedPixels, 20u);
          const auto stats = [&]() {
            std::array<double, 4> result{};
            const auto values = simulation.fields().densityView().values;
            for (std::size_t i = 0; i < values.size(); ++i) {
              const double value = values[i];
              const int x = static_cast<int>(i % resolution.width);
              const int y = static_cast<int>((i / resolution.width) % resolution.height);
              const int z = static_cast<int>(i /
                  (static_cast<std::size_t>(resolution.width) * resolution.height));
              result[0] += value;
              result[1] += value * x;
              result[2] += value * y;
              result[3] += value * z;
            }
            return result;
          }();
          if (capture == 2) {
            finalMass[seedIndex][condition] = stats[0];
            for (int axis = 0; axis < 3; ++axis)
              finalCentroid[seedIndex][condition][axis] =
                  stats[0] > 0.0 ? stats[axis + 1] / stats[0] : 0.0;
            finalLaplacian[seedIndex][condition] = densityLaplacianRms(
                simulation.fields().densityStorage(), resolution.width,
                resolution.height, resolution.depth);
            fieldCosine[seedIndex][condition] =
                temporal.cosineSum / std::max(1.0, static_cast<double>(temporal.comparisons));
            projectedDivergenceRms[seedIndex][condition] =
                velocityDivergenceRms(simulation);
          }
        }
      }
      for (int capture = 1; capture < 3; ++capture) {
        double residual = 0.0;
        const auto& a = images[(seedIndex * 4 + condition) * 3 + capture - 1].pixels;
        const auto& b = images[(seedIndex * 4 + condition) * 3 + capture].pixels;
        for (std::size_t i = 0; i < a.size(); ++i)
          residual += std::abs(static_cast<int>(a[i]) - b[i]) / 255.0;
        frameResidual[seedIndex][condition] += residual / a.size();
        const auto& previousStats = imageStats[(seedIndex * 4 + condition) * 3 + capture - 1];
        const auto& currentStats = imageStats[(seedIndex * 4 + condition) * 3 + capture];
        frameBrightnessDelta[seedIndex][condition] +=
            std::abs(currentStats.luminanceSum - previousStats.luminanceSum) /
            std::max(1.0, previousStats.luminanceSum);
      }
      frameResidual[seedIndex][condition] /= 2.0;
      frameBrightnessDelta[seedIndex][condition] /= 2.0;
    }
  }

  for (std::size_t seedIndex = 0; seedIndex < seeds.size(); ++seedIndex) {
    GTEST_LOG_(INFO) << "temporal curl-noise seed=" << seeds[seedIndex];
    for (std::size_t condition = 0; condition < 4; ++condition) {
      GTEST_LOG_(INFO) << "  strength=" << strengths[condition]
          << " mass=" << finalMass[seedIndex][condition]
          << " centroidXYZ=" << finalCentroid[seedIndex][condition][0] << ','
          << finalCentroid[seedIndex][condition][1] << ','
          << finalCentroid[seedIndex][condition][2]
          << " fieldLaplacian=" << finalLaplacian[seedIndex][condition]
          << " densityCosine=" << fieldCosine[seedIndex][condition]
          << " mass@0/4/8/16/24=" << massHistory[seedIndex][condition][0] << '/'
          << massHistory[seedIndex][condition][1] << '/'
          << massHistory[seedIndex][condition][2] << '/'
          << massHistory[seedIndex][condition][3] << '/'
          << massHistory[seedIndex][condition][4]
          << " divRms@24=" << divergenceHistory[seedIndex][condition][4]
          << " projectedDivRms=" << projectedDivergenceRms[seedIndex][condition]
          << " coverage=" << imageStats[(seedIndex * 4 + condition) * 3 + 2].occupiedPixels
          << " frameResidual=" << frameResidual[seedIndex][condition]
          << " frameLumaDelta=" << frameBrightnessDelta[seedIndex][condition];
      EXPECT_GT(finalMass[seedIndex][condition], 0.0);
      for (int capture = 0; capture < 3; ++capture) {
        const std::string filename = "temporal_curl_seed" +
            std::to_string(seeds[seedIndex]) + "_strength" +
            std::to_string(static_cast<int>(strengths[condition] * 1000.0f)) +
            "_frame" + std::to_string(capture) +
            ".ppm";
        saveIfRequested(images[(seedIndex * 4 + condition) * 3 + capture], filename.c_str());
      }
    }
  }
}

TEST(ArtifactCorePyroVolumeSmokeTest,
     ComparesForceCurlAgainstSpeedPreservingLocalRotation) {
  using namespace ArtifactCore;
  using namespace ArtifactCore::RayTrace;
  PyroDomain domain;
  domain.min = {-0.5f, 0.0f, -0.5f};
  domain.max = {0.5f, 1.5f, 0.5f};
  domain.voxelSize = 1.0f / 32.0f;
  domain.fixedTimeStep = 1.0 / 60.0;
  domain.boundaryMode = PyroBoundaryMode::Open;
  domain.memoryBudgetBytes = 24u * 1024u * 1024u;
  constexpr std::array<std::uint32_t, 2> seeds{17, 71};
  constexpr std::array<float, 3> strengths{0.0f, 0.02f, 0.15f};
  constexpr int frames = 24;
  PyroSimulationSettings settings;
  settings.dissipation = 0.035f;
  settings.coolingRate = 0.18f;
  settings.buoyancy = 0.7f;
  settings.vorticity = 0.45f;
  settings.pressureIterations = 12.0f;
  PyroSourceState source;
  source.position = {0.0f, 0.24f, 0.0f};
  source.extent = {0.13f, 0.09f, 0.13f};
  source.velocity = {0.0f, 240.0f, 0.0f};
  source.density = 4.0f;
  source.temperature = 1.5f;
  source.enabled = true;

  for (const std::uint32_t seed : seeds) {
    std::array<double, 3> mass{};
    std::array<std::array<double, 3>, 3> centroid{};
    std::array<double, 3> laplacian{};
    std::array<double, 3> highFrequency{};
    std::array<double, 3> adapterUsPerStep{};
    std::array<double, 3> simulationMilliseconds{};
    std::array<double, 3> renderMilliseconds{};
    std::array<double, 3> speedLengthError{};
    std::array<FieldTemporalStats, 3> temporal{};
    std::array<std::vector<float>, 3> previousDensity{};
    std::array<std::array<RenderStats, 3>, 3> imageStats{};
    std::array<std::array<ImageBuffer, 3>, 3> images{};
    for (std::size_t condition = 0; condition < strengths.size(); ++condition) {
      PyroSimulation simulation(domain);
      simulation.setSettings(settings);
      const auto simulationStart = std::chrono::steady_clock::now();
      for (int frame = 0; frame < frames; ++frame) {
        for (int substep = 0; substep < 2; ++substep) {
          const float time = static_cast<float>(frame * 2 + substep) / 60.0f;
          const auto start = std::chrono::steady_clock::now();
          if (condition == 1) {
            applyTemporalCurlNoise(simulation, domain, time, seed, strengths[condition]);
          } else if (condition == 2) {
            const auto result = applyVorticityAxisRotation(
                simulation, domain, strengths[condition]);
            speedLengthError[condition] = std::max(
                speedLengthError[condition], result.relativeSpeedError);
          }
          const auto end = std::chrono::steady_clock::now();
          adapterUsPerStep[condition] +=
              std::chrono::duration<double, std::micro>(end - start).count();
          simulation.setSources(std::span<const PyroSourceState>(&source, 1));
          simulation.step(1.0 / 60.0);
        }
        updateTemporalStats(simulation.fields().densityStorage(),
                            previousDensity[condition], temporal[condition]);
        if (frame >= frames - 3) {
          const int capture = frame - (frames - 3);
          const auto snapshot = simulation.snapshot();
          auto renderer = makeRenderer(snapshot, domain, {2.4f, 0.8f, 2.4f});
          renderer.camera = Camera(
              1.0f, 36.0f, 2.1f, 0.0f, {0.0f, 0.8f, 2.4f},
              {0.0f, 0.55f, 0.0f}, {0.0f, 1.0f, 0.0f});
          renderer.settings.stepSize = 0.025f;
          renderer.settings.maxSteps = 96;
          renderer.settings.densityScale = 2.2f;
          renderer.settings.lightIntensity = 5.0f;
          renderer.settings.phaseAnisotropy = 0.35f;
          renderer.settings.multipleScatteringStrength = 0.12f;
          renderer.settings.shadowIntensity = 0.0f;
          const auto renderStart = std::chrono::steady_clock::now();
          images[condition][capture] = renderer.render(144, 144);
          const auto renderEnd = std::chrono::steady_clock::now();
          renderMilliseconds[condition] +=
              std::chrono::duration<double, std::milli>(renderEnd - renderStart).count();
          imageStats[condition][capture] = measure(images[condition][capture]);
          if (capture == 2) {
            const auto field = simulation.fields().densityView();
            const auto r = domain.resolution();
            for (int z = 0; z < r.depth; ++z)
              for (int y = 0; y < r.height; ++y)
                for (int x = 0; x < r.width; ++x) {
                  const double value = field.at(x, y, z);
                  mass[condition] += value;
                  centroid[condition][0] += value * x;
                  centroid[condition][1] += value * y;
                  centroid[condition][2] += value * z;
                }
            if (mass[condition] > 0.0)
              for (int axis = 0; axis < 3; ++axis)
                centroid[condition][axis] /= mass[condition];
            laplacian[condition] = densityLaplacianRms(
                simulation.fields().densityStorage(), r.width, r.height, r.depth);
            highFrequency[condition] = densityHighFrequencyRms(
                simulation.fields().densityStorage(), r.width, r.height, r.depth);
          }
        }
      }
      const auto simulationEnd = std::chrono::steady_clock::now();
      simulationMilliseconds[condition] =
          std::chrono::duration<double, std::milli>(simulationEnd - simulationStart).count();
      adapterUsPerStep[condition] /= frames * 2;
    }
    GTEST_LOG_(INFO) << "force-curl vs vorticity-axis rotation seed=" << seed;
    for (std::size_t condition = 0; condition < strengths.size(); ++condition) {
      const int type = static_cast<int>(condition);
      for (int capture = 0; capture < 3; ++capture) {
        const std::string filename = "vorticity_compare_seed" +
            std::to_string(seed) + "_type" + std::to_string(type) +
            "_frame" + std::to_string(capture) + ".ppm";
        saveIfRequested(images[condition][capture], filename.c_str());
      }
      GTEST_LOG_(INFO) << "  type=" << type << " strength=" << strengths[condition]
          << " mass=" << mass[condition] << " centroid="
          << centroid[condition][0] << ',' << centroid[condition][1] << ','
          << centroid[condition][2] << " laplacian=" << laplacian[condition]
          << " densityHighFrequency=" << highFrequency[condition]
          << " densityCosine=" << temporal[condition].cosineSum /
              std::max<std::size_t>(1, temporal[condition].comparisons)
          << " speedLengthError=" << speedLengthError[condition]
          << " adapterUsPerStep=" << adapterUsPerStep[condition]
          << " simulationMs=" << simulationMilliseconds[condition]
          << " renderMs3Frames=" << renderMilliseconds[condition]
          << " coverage@21/22/23=" << imageStats[condition][0].occupiedPixels
          << '/' << imageStats[condition][1].occupiedPixels << '/'
          << imageStats[condition][2].occupiedPixels;
      EXPECT_GT(mass[condition], 1.0);
    }
  }
}
TEST(ArtifactCorePyroVolumeSmokeTest,
     RendersVelocityAdvectedThreeDimensionalDetailAtDoubleResolution) {
  using namespace ArtifactCore;
  using namespace ArtifactCore::RayTrace;
  PyroDomain domain;
  domain.min = {-0.5f, 0.0f, -0.5f};
  domain.max = {0.5f, 1.5f, 0.5f};
  domain.voxelSize = 1.0f / 32.0f;
  domain.fixedTimeStep = 1.0 / 60.0;
  domain.boundaryMode = PyroBoundaryMode::Open;
  domain.memoryBudgetBytes = 24u * 1024u * 1024u;
  constexpr int scale = 2;
  constexpr int frames = 16;
  constexpr std::uint32_t seed = 29;
  const auto low = domain.resolution();
  const int width = low.width * scale;
  const int height = low.height * scale;
  const int depth = low.depth * scale;
  const auto highIndex = [width, height](int x, int y, int z) {
    return static_cast<std::size_t>(x + width * (y + height * z));
  };
  const auto lowIndex = [low](int x, int y, int z) {
    return static_cast<std::size_t>(x + low.width * (y + low.height * z));
  };
  PyroSimulation simulation(domain);
  PyroSimulationSettings settings;
  settings.dissipation = 0.035f;
  settings.coolingRate = 0.18f;
  settings.buoyancy = 0.7f;
  settings.vorticity = 0.45f;
  settings.pressureIterations = 12.0f;
  simulation.setSettings(settings);
  PyroSourceState source;
  source.position = {0.0f, 0.24f, 0.0f};
  source.extent = {0.13f, 0.09f, 0.13f};
  source.velocity = {0.0f, 240.0f, 0.0f};
  source.density = 4.0f;
  source.temperature = 1.5f;
  source.enabled = true;
  std::vector<float> staticDetail(static_cast<std::size_t>(width * height * depth));
  std::vector<float> advectedDetail(staticDetail.size());
  std::vector<float> nextDetail(staticDetail.size());
  std::vector<float> generatedDetail(staticDetail.size());
  std::vector<float> nextGeneratedDetail(staticDetail.size());
  std::vector<float> generatedOtherSeed(staticDetail.size());
  std::vector<float> nextGeneratedOtherSeed(staticDetail.size());
  std::vector<float> curlOnlyDetail(staticDetail.size());
  std::vector<float> nextCurlOnlyDetail(staticDetail.size());
  std::vector<float> gradientOnlyDetail(staticDetail.size());
  std::vector<float> nextGradientOnlyDetail(staticDetail.size());
  std::vector<float> curlAlignedDetail(staticDetail.size());
  std::vector<float> nextCurlAlignedDetail(staticDetail.size());
  std::vector<float> tangentAlignedDetail(staticDetail.size());
  std::vector<float> nextTangentAlignedDetail(staticDetail.size());
  std::vector<float> multiTangentDetail(staticDetail.size());
  std::vector<float> nextMultiTangentDetail(staticDetail.size());
  constexpr float twoPi = 6.28318530718f;
  const float seedPhase = static_cast<float>(seed) * 0.137f;
  for (int z = 0; z < depth; ++z)
    for (int y = 0; y < height; ++y)
      for (int x = 0; x < width; ++x) {
        const float wx = (static_cast<float>(x) + 0.5f) / scale;
        const float wy = (static_cast<float>(y) + 0.5f) / scale;
        const float wz = (static_cast<float>(z) + 0.5f) / scale;
        const float broad = std::sin(twoPi * (wx / 11.0f + wy / 17.0f + wz / 13.0f) + seedPhase);
        const float mid = std::sin(twoPi * (wx / 5.0f - wy / 7.0f + wz / 6.0f) - seedPhase * 0.7f);
        const float fine = std::sin(twoPi * (wx / 2.7f + wy / 3.4f - wz / 2.9f) + seedPhase * 0.43f);
        staticDetail[highIndex(x, y, z)] = 0.5f * broad + 0.32f * mid + 0.18f * fine;
      }
  advectedDetail = staticDetail;
  generatedDetail = staticDetail;
  generatedOtherSeed = staticDetail;
  std::array<ImageBuffer, 16> finalImages{};
  std::array<double, 16> finalMass{};
  std::array<double, 16> finalMassBeforeCorrection{};
  std::array<double, 16> finalMassCorrection{};
  std::array<double, 16> finalDetailRms{};
  std::array<double, 16> finalDetailHighPass{};
  std::array<double, 16> finalImageHighPass{};
  std::array<std::size_t, 16> finalCoverage{};
  std::array<std::array<ImageBuffer, 3>, 12> multiViewImages{};
  std::array<std::array<ImageBuffer, 3>, 9> temporalImages{};
  double detailUpdateMilliseconds = 0.0;
  const auto advectDetailField = [&](const std::vector<float>& input,
                                     std::vector<float>& output,
                                     const auto& velocity) {
    for (int z = 0; z < depth; ++z)
      for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x) {
          const float lx = (static_cast<float>(x) + 0.5f) / scale - 0.5f;
          const float ly = (static_cast<float>(y) + 0.5f) / scale - 0.5f;
          const float lz = (static_cast<float>(z) + 0.5f) / scale - 0.5f;
          const auto v = simulation.fields().sampleVector(
              velocity, {lx, ly, lz}, PyroBoundaryMode::Open);
          const float sx = std::clamp(static_cast<float>(x) - v.x * scale / 60.0f,
                                      0.0f, static_cast<float>(width - 1));
          const float sy = std::clamp(static_cast<float>(y) - v.y * scale / 60.0f,
                                      0.0f, static_cast<float>(height - 1));
          const float sz = std::clamp(static_cast<float>(z) - v.z * scale / 60.0f,
                                      0.0f, static_cast<float>(depth - 1));
          const int x0 = static_cast<int>(std::floor(sx));
          const int y0 = static_cast<int>(std::floor(sy));
          const int z0 = static_cast<int>(std::floor(sz));
          const int x1 = std::min(x0 + 1, width - 1);
          const int y1 = std::min(y0 + 1, height - 1);
          const int z1 = std::min(z0 + 1, depth - 1);
          const float tx = sx - x0, ty = sy - y0, tz = sz - z0;
          const auto at = [&](int ix, int iy, int iz) { return input[highIndex(ix, iy, iz)]; };
          const float c00 = std::lerp(at(x0,y0,z0), at(x1,y0,z0), tx);
          const float c10 = std::lerp(at(x0,y1,z0), at(x1,y1,z0), tx);
          const float c01 = std::lerp(at(x0,y0,z1), at(x1,y0,z1), tx);
          const float c11 = std::lerp(at(x0,y1,z1), at(x1,y1,z1), tx);
          output[highIndex(x,y,z)] = std::lerp(
              std::lerp(c00,c10,ty), std::lerp(c01,c11,ty), tz);
        }
  };
  const auto renderHigh = [&](const std::vector<float>& baseDensity,
                              const std::vector<float>& detail,
                              bool useDetail, int variant,
                              float amplitude, float displacementWorld,
                              int frame) {
    double weightedNoise = 0.0;
    double baseMass = 0.0;
    std::vector<float> highDensity(staticDetail.size());
    const PyroConstScalarFieldView baseView{
        std::span<const float>(baseDensity), low};
    for (int z = 0; z < depth; ++z)
      for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x) {
          const float lx = (static_cast<float>(x) + 0.5f) / scale - 0.5f;
          const float ly = (static_cast<float>(y) + 0.5f) / scale - 0.5f;
          const float lz = (static_cast<float>(z) + 0.5f) / scale - 0.5f;
          const float rho = simulation.fields().sampleScalar(
              baseView, {lx, ly, lz}, PyroBoundaryMode::Open);
          const auto i = highIndex(x, y, z);
          if (useDetail && variant == 15) {
            // Stream displacement on density isosurfaces: cross(grad(rho), grad(psi)).
            constexpr float tau = 6.28318530718f;
            const PyroVec3 densityGradient{
                0.5f * (simulation.fields().sampleScalar(baseView,
                    {lx + 1.0f, ly, lz}, PyroBoundaryMode::Open) -
                    simulation.fields().sampleScalar(baseView,
                    {lx - 1.0f, ly, lz}, PyroBoundaryMode::Open)),
                0.5f * (simulation.fields().sampleScalar(baseView,
                    {lx, ly + 1.0f, lz}, PyroBoundaryMode::Open) -
                    simulation.fields().sampleScalar(baseView,
                    {lx, ly - 1.0f, lz}, PyroBoundaryMode::Open)),
                0.5f * (simulation.fields().sampleScalar(baseView,
                    {lx, ly, lz + 1.0f}, PyroBoundaryMode::Open) -
                    simulation.fields().sampleScalar(baseView,
                    {lx, ly, lz - 1.0f}, PyroBoundaryMode::Open))};
            const float phase = static_cast<float>(frame - (frames - 1)) * 0.12f;
            const float p0 = tau * (lx / 5.0f + ly / 7.0f - lz / 4.4f) + phase;
            const float p1 = tau * (lx / 2.8f - ly / 4.1f + lz / 6.2f) - phase * 0.73f + 1.31f;
            const float p2 = tau * (-lx / 3.7f + ly / 5.3f + lz / 3.4f) + phase * 0.41f + 2.17f;
            const PyroVec3 potentialGradient{
                (tau / 5.0f) * std::cos(p0) + 0.45f * (-tau / 2.8f) * std::cos(p1) +
                    0.20f * (-tau / 3.7f) * std::cos(p2),
                (tau / 7.0f) * std::cos(p0) + 0.45f * (-tau / 4.1f) * std::cos(p1) +
                    0.20f * (tau / 5.3f) * std::cos(p2),
                (-tau / 4.4f) * std::cos(p0) + 0.45f * (tau / 6.2f) * std::cos(p1) +
                    0.20f * (tau / 3.4f) * std::cos(p2)};
            constexpr float displacementScale = 2.0f;
            const PyroVec3 tangentCurl{
                densityGradient.y * potentialGradient.z - densityGradient.z * potentialGradient.y,
                densityGradient.z * potentialGradient.x - densityGradient.x * potentialGradient.z,
                densityGradient.x * potentialGradient.y - densityGradient.y * potentialGradient.x};
            highDensity[i] = simulation.fields().sampleScalar(baseView,
                {lx + tangentCurl.x * displacementScale,
                 ly + tangentCurl.y * displacementScale,
                 lz + tangentCurl.z * displacementScale}, PyroBoundaryMode::Open);
          } else if (useDetail && displacementWorld < 0.0f) {
            // Test-only divergence-free coordinate warp from a smooth vector potential.
            constexpr float tau = 6.28318530718f;
            const float frequency = -displacementWorld > 1.0f ? 2.0f : 1.0f;
            const float phaseRate = variant == 7 ? 0.9f : 0.18f;
            const float phase = static_cast<float>(frame - (frames - 1)) * phaseRate;
            const float px = tau * frequency * (ly / 8.0f + lz / 11.0f) + phase;
            const float py = tau * frequency * (lz / 7.0f + lx / 9.0f) - phase * 0.7f;
            const float pz = tau * frequency * (lx / 10.0f + ly / 6.0f) + phase * 0.43f;
            PyroVec3 curl{
                frequency * ((tau / 6.0f) * std::cos(pz) - (tau / 7.0f) * std::cos(py)),
                frequency * ((tau / 11.0f) * std::cos(px) - (tau / 10.0f) * std::cos(pz)),
                frequency * ((tau / 9.0f) * std::cos(py) - (tau / 8.0f) * std::cos(px))};
            const float length = std::sqrt(curl.x * curl.x + curl.y * curl.y + curl.z * curl.z);
            if (length > 1e-6f) {
              // Constant scaling preserves the curl field's zero divergence.
              const float shift = -displacementWorld * std::clamp(detail[i], -1.0f, 1.0f) / 1.6f;
              highDensity[i] = simulation.fields().sampleScalar(baseView,
                  {lx + curl.x * shift, ly + curl.y * shift, lz + curl.z * shift},
                  PyroBoundaryMode::Open);
            } else {
              highDensity[i] = rho;
            }
          } else if (useDetail && displacementWorld > 0.0f) {
            const float detailValue = std::clamp(detail[i], -1.0f, 1.0f);
            PyroVec3 normal{
                simulation.fields().sampleScalar(baseView, {lx + 1.0f, ly, lz}, PyroBoundaryMode::Open) -
                    simulation.fields().sampleScalar(baseView, {lx - 1.0f, ly, lz}, PyroBoundaryMode::Open),
                simulation.fields().sampleScalar(baseView, {lx, ly + 1.0f, lz}, PyroBoundaryMode::Open) -
                    simulation.fields().sampleScalar(baseView, {lx, ly - 1.0f, lz}, PyroBoundaryMode::Open),
                simulation.fields().sampleScalar(baseView, {lx, ly, lz + 1.0f}, PyroBoundaryMode::Open) -
                    simulation.fields().sampleScalar(baseView, {lx, ly, lz - 1.0f}, PyroBoundaryMode::Open)};
            const float gradientLength = std::sqrt(normal.x * normal.x +
                normal.y * normal.y + normal.z * normal.z);
            if (gradientLength > 1e-5f) {
              const float shift = displacementWorld / domain.voxelSize * detailValue;
              normal.x /= gradientLength;
              normal.y /= gradientLength;
              normal.z /= gradientLength;
              highDensity[i] = simulation.fields().sampleScalar(baseView,
                  {lx + normal.x * shift, ly + normal.y * shift,
                   lz + normal.z * shift}, PyroBoundaryMode::Open);
            } else {
              highDensity[i] = rho;
            }
          } else {
            highDensity[i] = rho;
          }
          weightedNoise += rho * detail[i];
          baseMass += rho;
        }
    const float mean = baseMass > 0.0 ? static_cast<float>(weightedNoise / baseMass) : 0.0f;
    double detailedMass = 0.0;
    for (std::size_t i = 0; i < highDensity.size(); ++i) {
      if (useDetail && displacementWorld == 0.0f) {
        const float perturbation = std::clamp(detail[i] - mean, -0.9f, 0.9f);
        highDensity[i] *= std::max(0.0f, 1.0f + amplitude * perturbation);
      }
      detailedMass += highDensity[i];
    }
    const float correction = detailedMass > 0.0
        ? static_cast<float>(baseMass / detailedMass) : 1.0f;
    finalMassBeforeCorrection[variant] = detailedMass;
    finalMassCorrection[variant] = correction;
    for (float& rho : highDensity) rho *= correction;
    finalMass[variant] = std::accumulate(highDensity.begin(), highDensity.end(), 0.0);
    double detailSquared = 0.0;
    double highPassSquared = 0.0;
    std::size_t detailCount = 0;
    for (int z = 1; z + 1 < depth; ++z)
      for (int y = 1; y + 1 < height; ++y)
        for (int x = 1; x + 1 < width; ++x) {
          const auto i = highIndex(x, y, z);
          const double value = detail[i];
          const float smooth = (detail[highIndex(x - 1,y,z)] + detail[highIndex(x + 1,y,z)] +
              detail[highIndex(x,y - 1,z)] + detail[highIndex(x,y + 1,z)] +
              detail[highIndex(x,y,z - 1)] + detail[highIndex(x,y,z + 1)]) / 6.0f;
          detailSquared += value * value;
          const double high = value - smooth;
          highPassSquared += high * high;
          ++detailCount;
        }
    finalDetailRms[variant] = detailCount ? std::sqrt(detailSquared / detailCount) : 0.0;
    finalDetailHighPass[variant] = detailCount ? std::sqrt(highPassSquared / detailCount) : 0.0;
    CPUVolumeRenderer renderer;
    renderer.bounds.min = {domain.min.x, domain.min.y, domain.min.z};
    renderer.bounds.max = {domain.max.x, domain.max.y, domain.max.z};
    VolumeFieldSet fields;
    fields.density = {highDensity.data(), {width, height, depth}};
    renderer.setVolumeData(renderer.bounds, fields);
    renderer.settings.stepSize = 0.0125f;
    renderer.settings.maxSteps = 144;
    renderer.settings.densityScale = 1.6f;
    renderer.settings.lightIntensity = 5.0f;
    renderer.settings.phaseAnisotropy = 0.35f;
    renderer.settings.multipleScatteringStrength = 0.12f;
    renderer.settings.shadowIntensity = 0.0f;
    renderer.settings.backgroundColor = {0.015f, 0.02f, 0.03f};
    renderer.camera = Camera(1.0f, 36.0f, 2.2f, 0.0f,
        {0.0f, 0.8f, 2.4f}, {0.0f, 0.55f, 0.0f}, {0.0f, 1.0f, 0.0f});
    auto image = renderer.render(160, 160);
    auto& imageOut = finalImages[variant];
    imageOut = std::move(image);
    if (variant == 0 || variant == 4 || variant == 5 || variant == 6 ||
        variant == 8 || variant == 9 || variant == 10 || variant == 11 ||
        variant == 12 || variant == 13 || variant == 14 || variant == 15) {
      int condition = 0;
      if (variant == 4) condition = 1;
      else if (variant == 5) condition = 2;
      else if (variant == 6) condition = 3;
      else if (variant == 8) condition = 4;
      else if (variant == 9) condition = 5;
      else if (variant == 10) condition = 6;
      else if (variant == 11) condition = 7;
      else if (variant == 12) condition = 8;
      else if (variant == 13) condition = 9;
      else if (variant == 14) condition = 10;
      else if (variant == 15) condition = 11;
      multiViewImages[condition][0] = imageOut;
      renderer.camera = Camera(1.0f, 36.0f, 2.2f, 0.0f,
          {2.4f, 0.8f, 0.0f}, {0.0f, 0.55f, 0.0f}, {0.0f, 1.0f, 0.0f});
      multiViewImages[condition][1] = renderer.render(160, 160);
      renderer.camera = Camera(1.0f, 36.0f, 2.2f, 0.0f,
          {0.0f, 2.4f, 0.0f}, {0.0f, 0.55f, 0.0f}, {0.0f, 0.0f, 1.0f});
      multiViewImages[condition][2] = renderer.render(160, 160);
    }
    const auto stats = measure(imageOut);
    finalCoverage[variant] = stats.occupiedPixels;
    double imageHighPass = 0.0;
    std::size_t imageSamples = 0;
    for (int y = 1; y + 1 < imageOut.height; ++y)
      for (int x = 1; x + 1 < imageOut.width; ++x) {
        const auto luminanceAt = [&](int px, int py) {
          const auto i = static_cast<std::size_t>((py * imageOut.width + px) * 3);
          return 0.2126 * imageOut.pixels[i] + 0.7152 * imageOut.pixels[i + 1] +
              0.0722 * imageOut.pixels[i + 2];
        };
        const double center = luminanceAt(x, y);
        const double neighbors = 0.25 * (luminanceAt(x - 1, y) + luminanceAt(x + 1, y) +
            luminanceAt(x, y - 1) + luminanceAt(x, y + 1));
        const double residual = center - neighbors;
        imageHighPass += residual * residual;
        ++imageSamples;
      }
    finalImageHighPass[variant] = imageSamples ? std::sqrt(imageHighPass / imageSamples) : 0.0;
  };

  for (int frame = 0; frame < frames; ++frame) {
    for (int substep = 0; substep < 2; ++substep) {
      simulation.setSources(std::span<const PyroSourceState>(&source, 1));
      simulation.step(1.0 / 60.0);
      const auto velocity = static_cast<const PyroSimulation&>(simulation).fields().velocityView();
      const auto detailStart = std::chrono::steady_clock::now();
      advectDetailField(advectedDetail, nextDetail, velocity);
      advectedDetail.swap(nextDetail);
      advectDetailField(generatedDetail, nextGeneratedDetail, velocity);
      generatedDetail.swap(nextGeneratedDetail);
      advectDetailField(generatedOtherSeed, nextGeneratedOtherSeed, velocity);
      generatedOtherSeed.swap(nextGeneratedOtherSeed);
      advectDetailField(curlOnlyDetail, nextCurlOnlyDetail, velocity);
      curlOnlyDetail.swap(nextCurlOnlyDetail);
      advectDetailField(gradientOnlyDetail, nextGradientOnlyDetail, velocity);
      gradientOnlyDetail.swap(nextGradientOnlyDetail);
      advectDetailField(curlAlignedDetail, nextCurlAlignedDetail, velocity);
      curlAlignedDetail.swap(nextCurlAlignedDetail);
      advectDetailField(tangentAlignedDetail, nextTangentAlignedDetail, velocity);
      tangentAlignedDetail.swap(nextTangentAlignedDetail);
      advectDetailField(multiTangentDetail, nextMultiTangentDetail, velocity);
      multiTangentDetail.swap(nextMultiTangentDetail);
      const auto coarseDensity = simulation.fields().densityView();
      const auto coarseVelocity = simulation.fields().velocityView();
      const auto sampleVelocity = [&](int x, int y, int z) {
        const auto cx = std::clamp(x, 0, low.width - 1);
        const auto cy = std::clamp(y, 0, low.height - 1);
        const auto cz = std::clamp(z, 0, low.depth - 1);
        return coarseVelocity.values[lowIndex(cx, cy, cz)];
      };
      const float generationTime = static_cast<float>(frame * 2 + substep) / 60.0f;
      for (int z = 0; z < depth; ++z)
        for (int y = 0; y < height; ++y)
          for (int x = 0; x < width; ++x) {
            const float lx = (static_cast<float>(x) + 0.5f) / scale - 0.5f;
            const float ly = (static_cast<float>(y) + 0.5f) / scale - 0.5f;
            const float lz = (static_cast<float>(z) + 0.5f) / scale - 0.5f;
            const int ix = std::clamp(static_cast<int>(std::lround(lx)), 1, low.width - 2);
            const int iy = std::clamp(static_cast<int>(std::lround(ly)), 1, low.height - 2);
            const int iz = std::clamp(static_cast<int>(std::lround(lz)), 1, low.depth - 2);
            const auto xm = sampleVelocity(ix - 1, iy, iz), xp = sampleVelocity(ix + 1, iy, iz);
            const auto ym = sampleVelocity(ix, iy - 1, iz), yp = sampleVelocity(ix, iy + 1, iz);
            const auto zm = sampleVelocity(ix, iy, iz - 1), zp = sampleVelocity(ix, iy, iz + 1);
            const float invDx = 0.5f / domain.voxelSize;
            PyroVec3 curl{
                ((yp.z - ym.z) - (zp.y - zm.y)) * invDx,
                ((zp.x - zm.x) - (xp.z - xm.z)) * invDx,
                ((xp.y - xm.y) - (yp.x - ym.x)) * invDx};
            const float curlMagnitude = std::sqrt(curl.x * curl.x + curl.y * curl.y + curl.z * curl.z);
            const float rho = coarseDensity.values[lowIndex(ix, iy, iz)];
            const float gx = (coarseDensity.values[lowIndex(ix + 1, iy, iz)] -
                              coarseDensity.values[lowIndex(ix - 1, iy, iz)]) * invDx;
            const float gy = (coarseDensity.values[lowIndex(ix, iy + 1, iz)] -
                              coarseDensity.values[lowIndex(ix, iy - 1, iz)]) * invDx;
            const float gz = (coarseDensity.values[lowIndex(ix, iy, iz + 1)] -
                              coarseDensity.values[lowIndex(ix, iy, iz - 1)]) * invDx;
            const float surfaceBand = std::clamp(rho * 4.0f, 0.0f, 1.0f) *
                std::clamp((0.55f - rho) * 3.0f, 0.0f, 1.0f) *
                std::clamp(std::sqrt(gx * gx + gy * gy + gz * gz) * 0.12f, 0.0f, 1.0f);
            const float noise = std::sin(twoPi * (lx / 3.2f + ly / 4.7f - lz / 3.8f) +
                generationTime * 2.4f + seedPhase);
            const float generated = 0.28f * surfaceBand *
                std::tanh(curlMagnitude * 0.025f) * noise;
            auto& value = generatedDetail[highIndex(x, y, z)];
            value = std::clamp(value + generated, -1.5f, 1.5f);
            const float otherSeedNoise = std::sin(
                twoPi * (lx / 3.2f + ly / 4.7f - lz / 3.8f) +
                generationTime * 2.4f + 83.0f * 0.137f);
            auto& otherSeedValue = generatedOtherSeed[highIndex(x, y, z)];
            otherSeedValue = std::clamp(otherSeedValue + 0.28f * surfaceBand *
                std::tanh(curlMagnitude * 0.025f) * otherSeedNoise, -1.5f, 1.5f);
            const float densityBand = std::clamp(rho * 4.0f, 0.0f, 1.0f) *
                std::clamp((0.55f - rho) * 3.0f, 0.0f, 1.0f);
            const float curlGate = std::tanh(curlMagnitude * 0.025f);
            auto& curlOnly = curlOnlyDetail[highIndex(x, y, z)];
            curlOnly = std::clamp(curlOnly + 0.28f * densityBand * curlGate * noise,
                                  -1.5f, 1.5f);
            auto& gradientOnly = gradientOnlyDetail[highIndex(x, y, z)];
            gradientOnly = std::clamp(gradientOnly + 0.28f * surfaceBand * noise,
                                      -1.5f, 1.5f);
            const float curlLength = std::sqrt(curl.x * curl.x + curl.y * curl.y + curl.z * curl.z);
            float alignedNoise = noise;
            if (curlLength > 1e-5f) {
              const float alongCurl = (curl.x * lx + curl.y * ly + curl.z * lz) / curlLength;
              alignedNoise = std::sin(twoPi * alongCurl / 3.8f +
                  generationTime * 2.4f + seedPhase);
            }
            auto& curlAligned = curlAlignedDetail[highIndex(x, y, z)];
            curlAligned = std::clamp(curlAligned + 0.28f * surfaceBand *
                std::tanh(curlLength * 0.025f) * alignedNoise, -1.5f, 1.5f);
            const float gradientLength = std::sqrt(gx * gx + gy * gy + gz * gz);
            float tangentNoise = noise;
            if (gradientLength > 1e-5f) {
              const float nx = gx / gradientLength;
              const float ny = gy / gradientLength;
              const float nz = gz / gradientLength;
              constexpr float kx = 1.0f / 3.2f;
              constexpr float ky = 1.0f / 4.7f;
              constexpr float kz = -1.0f / 3.8f;
              const float normalProjection = kx * nx + ky * ny + kz * nz;
              const float tx = kx - normalProjection * nx;
              const float ty = ky - normalProjection * ny;
              const float tz = kz - normalProjection * nz;
              const float tangentRatio = std::sqrt(tx * tx + ty * ty + tz * tz) /
                  std::sqrt(kx * kx + ky * ky + kz * kz);
              tangentNoise = std::sin(twoPi * (tx * lx + ty * ly + tz * lz) +
                  generationTime * 2.4f + seedPhase) * tangentRatio;
            }
            auto& tangentAligned = tangentAlignedDetail[highIndex(x, y, z)];
            tangentAligned = std::clamp(tangentAligned + 0.28f * surfaceBand *
                std::tanh(curlLength * 0.025f) * tangentNoise, -1.5f, 1.5f);
            float multiTangentNoise = noise;
            if (gradientLength > 1e-5f) {
              const float nx = gx / gradientLength;
              const float ny = gy / gradientLength;
              const float nz = gz / gradientLength;
              const auto projectedBand = [&](float kx, float ky, float kz, float phase) {
                const float normalProjection = kx * nx + ky * ny + kz * nz;
                const float tx = kx - normalProjection * nx;
                const float ty = ky - normalProjection * ny;
                const float tz = kz - normalProjection * nz;
                const float ratio = std::sqrt(tx * tx + ty * ty + tz * tz) /
                    std::sqrt(kx * kx + ky * ky + kz * kz);
                return ratio * std::sin(twoPi * (tx * lx + ty * ly + tz * lz) +
                    generationTime * 2.4f + phase);
              };
              multiTangentNoise = 0.55f * projectedBand(1.0f / 3.2f,
                  1.0f / 4.7f, -1.0f / 3.8f, seedPhase) +
                  0.30f * projectedBand(1.0f / 5.4f, -1.0f / 3.6f,
                      1.0f / 4.2f, seedPhase * 0.73f + 1.31f) +
                  0.15f * projectedBand(-1.0f / 2.8f, 1.0f / 6.3f,
                      1.0f / 3.1f, seedPhase * 0.41f + 2.17f);
            }
            auto& multiTangent = multiTangentDetail[highIndex(x, y, z)];
            multiTangent = std::clamp(multiTangent + 0.28f * surfaceBand *
                std::tanh(curlLength * 0.025f) * multiTangentNoise, -1.5f, 1.5f);
          }
      const auto detailEnd = std::chrono::steady_clock::now();
      detailUpdateMilliseconds +=
          std::chrono::duration<double, std::milli>(detailEnd - detailStart).count();
    }
    if (frame >= frames - 3) {
      const auto density = simulation.fields().densityStorage();
      renderHigh(density, staticDetail, false, 0, 0.0f, 0.0f, frame);
      renderHigh(density, advectedDetail, true, 2, 1.5f, 0.0f, frame);
      renderHigh(density, advectedDetail, true, 6, 0.0f, -1.5f, frame);
      renderHigh(density, advectedDetail, true, 7, 0.0f, -1.5f, frame);
      renderHigh(density, generatedDetail, true, 8, 1.5f, 0.0f, frame);
      renderHigh(density, generatedOtherSeed, true, 9, 1.5f, 0.0f, frame);
      renderHigh(density, tangentAlignedDetail, true, 13, 1.5f, 0.0f, frame);
      renderHigh(density, multiTangentDetail, true, 14, 1.5f, 0.0f, frame);
      renderHigh(density, advectedDetail, true, 15, 0.0f, -3.0f, frame);
      const int capture = frame - (frames - 3);
      temporalImages[0][capture] = finalImages[0];
      temporalImages[1][capture] = finalImages[2];
      temporalImages[2][capture] = finalImages[6];
      temporalImages[3][capture] = finalImages[7];
      temporalImages[4][capture] = finalImages[8];
      temporalImages[5][capture] = finalImages[9];
      temporalImages[6][capture] = finalImages[13];
      temporalImages[7][capture] = finalImages[14];
      temporalImages[8][capture] = finalImages[15];
      if (frame == frames - 1) {
        renderHigh(density, staticDetail, true, 1, 1.5f, 0.0f, frame);
        renderHigh(density, advectedDetail, true, 3, 0.0f, 0.02f, frame);
        renderHigh(density, advectedDetail, true, 4, 0.0f, 0.04f, frame);
        renderHigh(density, advectedDetail, true, 5, 0.0f, -0.75f, frame);
        renderHigh(density, curlOnlyDetail, true, 10, 1.5f, 0.0f, frame);
        renderHigh(density, gradientOnlyDetail, true, 11, 1.5f, 0.0f, frame);
        renderHigh(density, curlAlignedDetail, true, 12, 1.5f, 0.0f, frame);
      }
    }
  }
  for (int variant = 0; variant < 16; ++variant) {
    ASSERT_FALSE(finalImages[variant].pixels.empty());
    const auto stats = measure(finalImages[variant]);
    GTEST_LOG_(INFO) << "2x volume detail variant=" << variant
        << " occupied=" << stats.occupiedPixels << " luma=" << stats.luminanceSum
        << " mass=" << finalMass[variant]
        << " rawMass=" << finalMassBeforeCorrection[variant]
        << " massCorrection=" << finalMassCorrection[variant]
        << " detailRms=" << finalDetailRms[variant]
        << " detailHighPass=" << finalDetailHighPass[variant]
        << " imageHighPass=" << finalImageHighPass[variant];
  }
  GTEST_LOG_(INFO) << "2x detail field cells=" << staticDetail.size()
      << " bytes=" << staticDetail.size() * sizeof(float) * 17
      << " updateMs16frames=" << detailUpdateMilliseconds;
  const auto differentPixels = [](const ImageBuffer& a, const ImageBuffer& b) {
    std::size_t count = 0;
    for (std::size_t i = 0; i < std::min(a.pixels.size(), b.pixels.size()); ++i)
      if (a.pixels[i] != b.pixels[i]) ++count;
    return count;
  };
  EXPECT_GT(differentPixels(finalImages[0], finalImages[1]), 100u);
  EXPECT_GT(differentPixels(finalImages[1], finalImages[2]), 50u);
  EXPECT_GT(differentPixels(finalImages[2], finalImages[3]), 50u);
  EXPECT_GT(differentPixels(finalImages[3], finalImages[4]), 50u);
  EXPECT_GT(differentPixels(finalImages[0], finalImages[5]), 50u);
  EXPECT_GT(differentPixels(finalImages[0], finalImages[6]), 50u);
  EXPECT_GT(differentPixels(finalImages[0], finalImages[7]), 50u);
  EXPECT_GT(differentPixels(finalImages[2], finalImages[8]), 50u);
  EXPECT_GT(differentPixels(finalImages[8], finalImages[9]), 50u);
  EXPECT_GT(differentPixels(finalImages[8], finalImages[10]), 50u);
  EXPECT_GT(differentPixels(finalImages[8], finalImages[11]), 50u);
  EXPECT_GT(differentPixels(finalImages[8], finalImages[12]), 50u);
  EXPECT_GT(differentPixels(finalImages[8], finalImages[13]), 50u);
  EXPECT_GT(differentPixels(finalImages[13], finalImages[14]), 50u);
  EXPECT_GT(differentPixels(finalImages[8], finalImages[15]), 50u);
  const auto temporalImageResidual = [](const auto& sequence) {
    double sum = 0.0;
    std::size_t count = 0;
    for (int frame = 1; frame < 3; ++frame) {
      const auto& previous = sequence[frame - 1].pixels;
      const auto& current = sequence[frame].pixels;
      for (std::size_t i = 0; i < std::min(previous.size(), current.size()); ++i) {
        sum += std::abs(static_cast<int>(previous[i]) - current[i]) / 255.0;
        ++count;
      }
    }
    return count ? sum / count : 0.0;
  };
  for (int condition = 0; condition < 9; ++condition) {
    double centroidMotion = 0.0;
    for (int capture = 1; capture < 3; ++capture) {
      const auto previous = measure(temporalImages[condition][capture - 1]);
      const auto current = measure(temporalImages[condition][capture]);
      const double dx = current.centroidX - previous.centroidX;
      const double dy = current.centroidY - previous.centroidY;
      centroidMotion += std::sqrt(dx * dx + dy * dy) / 2.0;
    }
    GTEST_LOG_(INFO) << "detail temporal image residual condition=" << condition
        << " adjacentMeanAbs=" << temporalImageResidual(temporalImages[condition])
        << " centroidMotionPixels=" << centroidMotion;
    for (int capture = 0; capture < 3; ++capture) {
      const std::string filename = "pyro_volume_detail_temporal_condition" +
          std::to_string(condition) + "_frame" + std::to_string(capture) + ".ppm";
      saveIfRequested(temporalImages[condition][capture], filename.c_str());
    }
  }
  for (int view = 0; view < 3; ++view) {
    const auto baseStats = measure(multiViewImages[0][view]);
    const auto displacedStats = measure(multiViewImages[1][view]);
    GTEST_LOG_(INFO) << "normal displacement multiview=" << view
        << " occupied base/displaced=" << baseStats.occupiedPixels << '/'
        << displacedStats.occupiedPixels << " luma=" << baseStats.luminanceSum
        << '/' << displacedStats.luminanceSum << " changedPixels="
        << differentPixels(multiViewImages[0][view], multiViewImages[1][view]);
    EXPECT_GT(baseStats.occupiedPixels, 100u);
    EXPECT_GT(displacedStats.occupiedPixels, 100u);
    const auto curlStats = measure(multiViewImages[2][view]);
    const auto fineCurlStats = measure(multiViewImages[3][view]);
    GTEST_LOG_(INFO) << "curl warp multiview=" << view
        << " occupied base/warped=" << baseStats.occupiedPixels << '/'
        << curlStats.occupiedPixels << " luma=" << baseStats.luminanceSum
        << '/' << curlStats.luminanceSum << " changedPixels="
        << differentPixels(multiViewImages[0][view], multiViewImages[2][view]);
    GTEST_LOG_(INFO) << "fine curl warp multiview=" << view
        << " occupied base/warped=" << baseStats.occupiedPixels << '/'
        << fineCurlStats.occupiedPixels << " luma=" << baseStats.luminanceSum
        << '/' << fineCurlStats.luminanceSum << " changedPixels="
        << differentPixels(multiViewImages[0][view], multiViewImages[3][view]);
    saveIfRequested(multiViewImages[0][view],
        ("pyro_volume_detail_multiview_base_" + std::to_string(view) + ".ppm").c_str());
    saveIfRequested(multiViewImages[1][view],
        ("pyro_volume_detail_multiview_displaced_" + std::to_string(view) + ".ppm").c_str());
    saveIfRequested(multiViewImages[2][view],
        ("pyro_volume_detail_curl_warp_" + std::to_string(view) + ".ppm").c_str());
    saveIfRequested(multiViewImages[3][view],
        ("pyro_volume_detail_fine_curl_warp_" + std::to_string(view) + ".ppm").c_str());
    const auto generatedStats = measure(multiViewImages[4][view]);
    GTEST_LOG_(INFO) << "vorticity-generated detail multiview=" << view
        << " occupied=" << generatedStats.occupiedPixels
        << " luma=" << generatedStats.luminanceSum;
    saveIfRequested(multiViewImages[4][view],
        ("pyro_volume_detail_vorticity_generated_" + std::to_string(view) + ".ppm").c_str());
    const auto otherSeedStats = measure(multiViewImages[5][view]);
    GTEST_LOG_(INFO) << "vorticity-generated seed83 multiview=" << view
        << " occupied=" << otherSeedStats.occupiedPixels
        << " luma=" << otherSeedStats.luminanceSum;
    saveIfRequested(multiViewImages[5][view],
        ("pyro_volume_detail_vorticity_generated_seed83_" +
         std::to_string(view) + ".ppm").c_str());
    const auto curlOnlyStats = measure(multiViewImages[6][view]);
    const auto gradientOnlyStats = measure(multiViewImages[7][view]);
    GTEST_LOG_(INFO) << "generation ablation view=" << view
        << " curlOnlyCoverage=" << curlOnlyStats.occupiedPixels
        << " gradientOnlyCoverage=" << gradientOnlyStats.occupiedPixels
        << " curlOnlyLuma=" << curlOnlyStats.luminanceSum
        << " gradientOnlyLuma=" << gradientOnlyStats.luminanceSum;
    saveIfRequested(multiViewImages[6][view],
        ("pyro_volume_detail_curl_only_" + std::to_string(view) + ".ppm").c_str());
    saveIfRequested(multiViewImages[7][view],
        ("pyro_volume_detail_gradient_only_" + std::to_string(view) + ".ppm").c_str());
    const auto curlAlignedStats = measure(multiViewImages[8][view]);
    GTEST_LOG_(INFO) << "curl-aligned generation view=" << view
        << " coverage=" << curlAlignedStats.occupiedPixels
        << " luma=" << curlAlignedStats.luminanceSum;
    saveIfRequested(multiViewImages[8][view],
        ("pyro_volume_detail_curl_aligned_" + std::to_string(view) + ".ppm").c_str());
    const auto tangentStats = measure(multiViewImages[9][view]);
    GTEST_LOG_(INFO) << "tangent-aligned generation view=" << view
        << " coverage=" << tangentStats.occupiedPixels
        << " luma=" << tangentStats.luminanceSum;
    saveIfRequested(multiViewImages[9][view],
        ("pyro_volume_detail_tangent_aligned_" + std::to_string(view) + ".ppm").c_str());
    const auto multiTangentStats = measure(multiViewImages[10][view]);
    GTEST_LOG_(INFO) << "multiband tangent generation view=" << view
        << " coverage=" << multiTangentStats.occupiedPixels
        << " luma=" << multiTangentStats.luminanceSum;
    saveIfRequested(multiViewImages[10][view],
        ("pyro_volume_detail_multiband_tangent_" + std::to_string(view) + ".ppm").c_str());
    const auto tangentCurlStats = measure(multiViewImages[11][view]);
    GTEST_LOG_(INFO) << "surface curl-potential warp view=" << view
        << " coverage=" << tangentCurlStats.occupiedPixels
        << " luma=" << tangentCurlStats.luminanceSum;
    saveIfRequested(multiViewImages[11][view],
        ("pyro_volume_detail_surface_curl_potential_" +
         std::to_string(view) + ".ppm").c_str());
  }
  saveIfRequested(finalImages[0], "pyro_volume_detail_2x_base.ppm");
  saveIfRequested(finalImages[1], "pyro_volume_detail_2x_static.ppm");
  saveIfRequested(finalImages[2], "pyro_volume_detail_2x_advected.ppm");
  saveIfRequested(finalImages[3], "pyro_volume_detail_2x_displaced_020.ppm");
  saveIfRequested(finalImages[4], "pyro_volume_detail_2x_displaced_040.ppm");
  saveIfRequested(finalImages[5], "pyro_volume_detail_2x_curl_warp.ppm");
  saveIfRequested(finalImages[6], "pyro_volume_detail_2x_fine_curl_warp.ppm");
  saveIfRequested(finalImages[7], "pyro_volume_detail_2x_fast_curl_warp.ppm");
  saveIfRequested(finalImages[8], "pyro_volume_detail_2x_vorticity_generated.ppm");
  saveIfRequested(finalImages[9], "pyro_volume_detail_2x_vorticity_generated_seed83.ppm");
  saveIfRequested(finalImages[10], "pyro_volume_detail_2x_curl_only.ppm");
  saveIfRequested(finalImages[11], "pyro_volume_detail_2x_gradient_only.ppm");
  saveIfRequested(finalImages[12], "pyro_volume_detail_2x_curl_aligned.ppm");
  saveIfRequested(finalImages[13], "pyro_volume_detail_2x_tangent_aligned.ppm");
  saveIfRequested(finalImages[14], "pyro_volume_detail_2x_multiband_tangent.ppm");
  saveIfRequested(finalImages[15], "pyro_volume_detail_2x_surface_curl_potential.ppm");
}
TEST(ArtifactCorePyroVolumeSmokeTest,
     MeasuresSemiLagrangianDensityDetailDecayByWavelength) {
  using namespace ArtifactCore;
  constexpr int width = 64;
  constexpr int height = 32;
  constexpr int depth = 32;
  constexpr std::array<int, 4> wavelengths{16, 8, 4, 2};
  constexpr std::array<float, 3> displacements{0.25f, 0.5f, 1.0f};
  constexpr int steps = 8;
  PyroDomain domain;
  domain.min = {0.0f, 0.0f, 0.0f};
  domain.max = {static_cast<float>(width), static_cast<float>(height), static_cast<float>(depth)};
  domain.voxelSize = 1.0f;
  domain.boundaryMode = PyroBoundaryMode::Closed;
  domain.memoryBudgetBytes = 32u * 1024u * 1024u;
  PyroSimulation sampler(domain);
  std::vector<float> input(static_cast<std::size_t>(width * height * depth));
  std::vector<float> output(input.size());
  const auto index = [](int x, int y, int z) {
    return static_cast<std::size_t>(x + width * (y + height * z));
  };
  for (const float displacement : displacements) {
    for (const int wavelength : wavelengths) {
      for (int z = 0; z < depth; ++z)
        for (int y = 0; y < height; ++y)
          for (int x = 0; x < width; ++x) {
            const float wave = std::sin(
                6.28318530718f * (x + 0.25f) / wavelength);
            const float envelope = std::sin(3.14159265359f * x / (width - 1)) *
                std::sin(3.14159265359f * y / (height - 1)) *
                std::sin(3.14159265359f * z / (depth - 1));
            input[index(x, y, z)] = 0.5f + 0.4f * wave * envelope;
          }
      const double initialAmplitude = sinusoidAmplitude(
          input, width, height, depth, 0, wavelength);
      double customAmplitude = initialAmplitude;
      std::vector<float> field = input;
      for (int iteration = 0; iteration < steps; ++iteration) {
        for (int z = 0; z < depth; ++z)
          for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x) {
              const float sx = std::clamp(
                  static_cast<float>(x) - displacement, 0.0f,
                  static_cast<float>(width - 1));
              const int x0 = static_cast<int>(std::floor(sx));
              const int x1 = std::min(x0 + 1, width - 1);
              const float tx = sx - x0;
              output[index(x, y, z)] = field[index(x0, y, z)] * (1.0f - tx) +
                  field[index(x1, y, z)] * tx;
            }
        field.swap(output);
        customAmplitude = sinusoidAmplitude(field, width, height, depth, 0, wavelength);
      }

      std::copy(input.begin(), input.end(), sampler.fields().densityStorage().begin());
      double productionAmplitude = initialAmplitude;
      std::vector<float> current = input;
      std::vector<float> next(input.size());
      for (int iteration = 0; iteration < steps; ++iteration) {
        std::copy(current.begin(), current.end(), sampler.fields().densityStorage().begin());
        const auto view = static_cast<const PyroFieldSet&>(sampler.fields()).densityView();
        for (int z = 0; z < depth; ++z)
          for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x) {
              const PyroVec3 backtrace{static_cast<float>(x) - displacement,
                                       static_cast<float>(y), static_cast<float>(z)};
              next[index(x, y, z)] = sampler.fields().sampleScalar(
                  view, backtrace, PyroBoundaryMode::Closed);
            }
        current.swap(next);
        productionAmplitude = sinusoidAmplitude(current, width, height, depth, 0, wavelength);
      }
      const double customRetention = customAmplitude / std::max(initialAmplitude, 1e-12);
      const double productionRetention = productionAmplitude / std::max(initialAmplitude, 1e-12);
      GTEST_LOG_(INFO) << "semi-Lagrangian detail decay wavelength=" << wavelength
          << " cells displacement=" << displacement << " steps=" << steps
          << " customRetention=" << customRetention
          << " PyroFieldSetSampleRetention=" << productionRetention;
      EXPECT_GT(initialAmplitude, 0.01);
      EXPECT_GE(customRetention, 0.0);
      EXPECT_GE(productionRetention, 0.0);
    }
  }
}
TEST(ArtifactCorePyroVolumeSmokeTest,
     ComparesLimitedAndUnlimitedBfeccAdvection) {
  using namespace ArtifactCore;
  constexpr int width = 64;
  constexpr int height = 32;
  constexpr int depth = 32;
  constexpr std::array<int, 3> wavelengths{8, 4, 2};
  constexpr float displacement = 0.25f;
  constexpr int steps = 8;
  const auto index = [](int x, int y, int z) {
    return static_cast<std::size_t>(x + width * (y + height * z));
  };
  const auto advect = [&](const std::vector<float>& input,
                          std::vector<float>& output, float dx) {
    for (int z = 0; z < depth; ++z)
      for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x) {
          const float sx = std::clamp(static_cast<float>(x) - dx, 0.0f,
                                      static_cast<float>(width - 1));
          const int x0 = static_cast<int>(std::floor(sx));
          const int x1 = std::min(x0 + 1, width - 1);
          const float tx = sx - x0;
          output[index(x, y, z)] = input[index(x0, y, z)] * (1.0f - tx) +
              input[index(x1, y, z)] * tx;
        }
  };
  for (const int wavelength : wavelengths) {
    std::vector<float> original(static_cast<std::size_t>(width * height * depth));
    for (int z = 0; z < depth; ++z)
      for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x) {
          const float envelope = std::sin(3.14159265359f * x / (width - 1)) *
              std::sin(3.14159265359f * y / (height - 1)) *
              std::sin(3.14159265359f * z / (depth - 1));
          original[index(x, y, z)] = 0.5f + 0.4f * envelope *
              std::sin(6.28318530718f * (x + 0.25f) / wavelength);
        }
    std::vector<float> forward(original.size()), backward(original.size());
    std::vector<float> corrected(original.size()), next(original.size());
    std::vector<float> linear(original.size()), linearNext(original.size());
    std::vector<float> limited(original.size()), unlimited(original.size());
    std::copy(original.begin(), original.end(), limited.begin());
    std::copy(original.begin(), original.end(), unlimited.begin());
    std::copy(original.begin(), original.end(), linear.begin());
    for (int step = 0; step < steps; ++step) {
      advect(linear, linearNext, displacement);
      linear.swap(linearNext);
      auto bfecc = [&](std::vector<float>& state, bool useLimiter) {
        advect(state, forward, displacement);
        advect(forward, backward, -displacement);
        for (std::size_t i = 0; i < state.size(); ++i)
          corrected[i] = forward[i] + 0.5f * (state[i] - backward[i]);
        advect(corrected, next, displacement);
        for (int z = 0; z < depth; ++z)
          for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x) {
              const auto i = index(x, y, z);
              if (useLimiter) {
                const float sx = std::clamp(static_cast<float>(x) - displacement,
                                            0.0f, static_cast<float>(width - 1));
                const int x0 = static_cast<int>(std::floor(sx));
                const int x1 = std::min(x0 + 1, width - 1);
                const float lo = std::min(state[index(x0, y, z)], state[index(x1, y, z)]);
                const float hi = std::max(state[index(x0, y, z)], state[index(x1, y, z)]);
                next[i] = std::clamp(next[i], lo, hi);
              }
            }
        state.swap(next);
      };
      bfecc(limited, true);
      bfecc(unlimited, false);
    }
    const auto stats = [&](const std::vector<float>& values) {
      double mass = 0.0;
      float minimum = std::numeric_limits<float>::max();
      float maximum = std::numeric_limits<float>::lowest();
      for (const float value : values) {
        mass += value;
        minimum = std::min(minimum, value);
        maximum = std::max(maximum, value);
      }
      return std::array<double, 3>{mass, minimum, maximum};
    };
    const auto originalStats = stats(original);
    const auto limitedStats = stats(limited);
    const auto unlimitedStats = stats(unlimited);
    const double initialAmplitude = sinusoidAmplitude(original, width, height, depth, 0, wavelength);
    const double linearAmplitude = sinusoidAmplitude(linear, width, height, depth, 0, wavelength);
    const double limitedAmplitude = sinusoidAmplitude(limited, width, height, depth, 0, wavelength);
    const double unlimitedAmplitude = sinusoidAmplitude(unlimited, width, height, depth, 0, wavelength);
    GTEST_LOG_(INFO) << "BFECC density R&D wavelength=" << wavelength
        << " linearRetention=" << linearAmplitude / initialAmplitude
        << " limitedRetention=" << limitedAmplitude / initialAmplitude
        << " unlimitedRetention=" << unlimitedAmplitude / initialAmplitude
        << " massDeltaLimited=" << limitedStats[0] - originalStats[0]
        << " massDeltaUnlimited=" << unlimitedStats[0] - originalStats[0]
        << " minLimited=" << limitedStats[1] << " maxLimited=" << limitedStats[2]
        << " minUnlimited=" << unlimitedStats[1] << " maxUnlimited=" << unlimitedStats[2];
    EXPECT_GE(limitedStats[1], 0.0);
    EXPECT_GT(limitedAmplitude, 0.0);
  }
}
TEST(ArtifactCorePyroVolumeSmokeTest,
     ComparesPressureProjectionOperatorsOnKnownVelocityField) {
  constexpr int width = 24;
  constexpr int height = 24;
  constexpr int depth = 24;
  constexpr float twoPi = 6.28318530718f;
  const auto index = [](int x, int y, int z) {
    return static_cast<std::size_t>(x + width * (y + height * z));
  };
  std::vector<ArtifactCore::PyroVec3> velocity(
      static_cast<std::size_t>(width * height * depth));
  for (int z = 0; z < depth; ++z) {
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        const float nx = static_cast<float>(x) / (width - 1);
        const float ny = static_cast<float>(y) / (height - 1);
        const float nz = static_cast<float>(z) / (depth - 1);
        velocity[index(x, y, z)] = {
            0.8f * std::sin(twoPi * nx) * std::cos(twoPi * ny),
            0.6f * std::sin(twoPi * ny) * std::cos(twoPi * nz),
            0.7f * std::sin(twoPi * nz) * std::cos(twoPi * nx)};
      }
    }
  }
  const double inputDivergence =
      gridDivergenceRms(velocity, width, height, depth);
  ASSERT_GT(inputDivergence, 0.01);
  const std::array<int, 3> iterationCounts{8, 32, 96};
  for (const int iterations : iterationCounts) {
    const auto production = projectTestVelocity(
        velocity, width, height, depth, iterations, true);
    const auto matchedStencil = projectTestVelocity(
        velocity, width, height, depth, iterations, false);
    const double productionDivergence = gridDivergenceRms(
        production, width, height, depth);
    const double matchedDivergence = gridDivergenceRms(
        matchedStencil, width, height, depth);
    GTEST_LOG_(INFO) << "projection R&D iterations=" << iterations
        << " div RMS input/previous-formula/matched-stencil="
        << inputDivergence << '/' << productionDivergence << '/'
        << matchedDivergence;
    EXPECT_LT(matchedDivergence, inputDivergence);
    EXPECT_GT(productionDivergence, inputDivergence);
  }
}

TEST(ArtifactCorePyroVolumeSmokeTest,
     MeasuresSourceAndDissipationPersistence) {
  using namespace ArtifactCore;
  PyroDomain domain;
  domain.min = {0.0f, 0.0f, 0.0f};
  domain.max = {1.0f, 1.0f, 1.0f};
  domain.voxelSize = 0.25f;
  domain.fixedTimeStep = 1.0f / 60.0f;
  domain.boundaryMode = PyroBoundaryMode::Open;
  domain.memoryBudgetBytes = 1u * 1024u * 1024u;
  PyroSimulationSettings settings;
  settings.dissipation = 0.6f;
  settings.coolingRate = 0.0f;
  settings.buoyancy = 0.0f;
  settings.vorticity = 0.0f;
  settings.pressureIterations = 2.0f;

  PyroSimulation decaying(domain);
  decaying.setSettings(settings);
  double initialMass = 0.0;
  for (float& value : decaying.fields().densityStorage()) {
    value = 1.0f;
    initialMass += value;
  }
  decaying.step(1.0 / 60.0);
  double afterDissipationMass = 0.0;
  for (const float value : decaying.fields().densityStorage())
    afterDissipationMass += value;

  PyroSimulation emitting(domain);
  settings.dissipation = 0.0f;
  settings.sourceDensity = 60.0f;
  emitting.setSettings(settings);
  const std::array<PyroSourceState, 1> source{{
      PyroSourceState{.position = {0.5f, 0.5f, 0.5f},
                      .extent = {0.3f, 0.3f, 0.3f},
                      .density = 1.0f}}};
  emitting.setSources(source);
  emitting.step(1.0 / 60.0);
  double emittedMass = 0.0;
  for (const float value : emitting.fields().densityStorage())
    emittedMass += value;

  GTEST_LOG_(INFO) << "source/dissipation persistence R&D initial/after-decay="
      << initialMass << '/' << afterDissipationMass
      << " source-only final mass=" << emittedMass;
  EXPECT_GT(initialMass, 0.0);
  EXPECT_NEAR(afterDissipationMass, initialMass - 0.6 / 60.0 * 64.0, 1e-4);
  EXPECT_GT(emittedMass, 1.0);
}

TEST(ArtifactCorePyroVolumeSmokeTest,
     ProductionStepReducesVelocityDivergence) {
  using namespace ArtifactCore;
  PyroDomain domain;
  domain.min = {-0.5f, -0.5f, -0.5f};
  domain.max = {0.5f, 0.5f, 0.5f};
  domain.voxelSize = 1.0f / 24.0f;
  domain.fixedTimeStep = 1.0 / 60.0;
  domain.boundaryMode = PyroBoundaryMode::Open;
  domain.memoryBudgetBytes = 16u * 1024u * 1024u;
  PyroSimulationSettings settings;
  settings.dissipation = 0.0f;
  settings.coolingRate = 0.0f;
  settings.buoyancy = 0.0f;
  settings.vorticity = 0.0f;
  constexpr std::array<int, 3> iterationCounts{8, 20, 32};
  constexpr float twoPi = 6.28318530718f;
  for (const int iterations : iterationCounts) {
    PyroSimulation simulation(domain);
    settings.pressureIterations = static_cast<float>(iterations);
    simulation.setSettings(settings);
    auto velocity = simulation.fields().velocityView();
    const auto resolution = domain.resolution();
    for (int z = 0; z < resolution.depth; ++z)
      for (int y = 0; y < resolution.height; ++y)
        for (int x = 0; x < resolution.width; ++x) {
          const float nx = static_cast<float>(x) / (resolution.width - 1);
          const float ny = static_cast<float>(y) / (resolution.height - 1);
          const float nz = static_cast<float>(z) / (resolution.depth - 1);
          velocity.at(x, y, z) = {
              0.8f * std::sin(twoPi * nx) * std::cos(twoPi * ny),
              0.6f * std::sin(twoPi * ny) * std::cos(twoPi * nz),
              0.7f * std::sin(twoPi * nz) * std::cos(twoPi * nx)};
        }
    const double before = velocityDivergenceRms(simulation);
    simulation.step(1.0 / 60.0);
    const double after = velocityDivergenceRms(simulation);
    GTEST_LOG_(INFO) << "production Pyro step projection iterations="
        << iterations << " divergence RMS before/after="
        << before << '/' << after;
    EXPECT_GT(before, 0.01);
    EXPECT_LT(after, before);
  }
}

TEST(ArtifactCorePyroVolumeSmokeTest,
     ComparesPressureProjectionAcrossGridScales) {
  constexpr float twoPi = 6.28318530718f;
  for (const int resolution : {12, 20, 32}) {
    const int width = resolution;
    const int height = resolution;
    const int depth = resolution;
    const auto index = [width, height](int x, int y, int z) {
      return static_cast<std::size_t>(x + width * (y + height * z));
    };
    std::vector<ArtifactCore::PyroVec3> velocity(
        static_cast<std::size_t>(width * height * depth));
    for (int z = 0; z < depth; ++z) {
      for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
          const float nx = static_cast<float>(x) / (width - 1);
          const float ny = static_cast<float>(y) / (height - 1);
          const float nz = static_cast<float>(z) / (depth - 1);
          velocity[index(x, y, z)] = {
              0.7f * std::sin(twoPi * nx) * std::cos(4.0f * twoPi * ny),
              0.5f * std::sin(3.0f * twoPi * ny) * std::cos(twoPi * nz),
              0.8f * std::sin(2.0f * twoPi * nz) * std::cos(3.0f * twoPi * nx)};
        }
      }
    }
    const double inputDivergence =
        gridDivergenceRms(velocity, width, height, depth);
    const auto production = projectTestVelocity(
        velocity, width, height, depth, 32, true);
    const auto matchedStencil = projectTestVelocity(
        velocity, width, height, depth, 32, false);
    const double productionDivergence = gridDivergenceRms(
        production, width, height, depth);
    const double matchedDivergence = gridDivergenceRms(
        matchedStencil, width, height, depth);
    GTEST_LOG_(INFO) << "projection grid sweep resolution=" << resolution
        << " div RMS input/previous-formula/matched-stencil="
        << inputDivergence << '/' << productionDivergence << '/'
        << matchedDivergence;
    EXPECT_GT(inputDivergence, 0.01);
    EXPECT_GT(productionDivergence, inputDivergence);
    EXPECT_LT(matchedDivergence, inputDivergence);
  }
}

TEST(ArtifactCorePyroVolumeSmokeTest,
     ComparesCanonicalVorticityConfinementAgainstCurrentImplementation) {
  using namespace ArtifactCore;
  using namespace ArtifactCore::RayTrace;
  PyroDomain domain;
  domain.min = {-0.5f, 0.0f, -0.5f};
  domain.max = {0.5f, 1.5f, 0.5f};
  domain.voxelSize = 1.0f / 32.0f;
  domain.fixedTimeStep = 1.0 / 60.0;
  domain.boundaryMode = PyroBoundaryMode::Open;
  domain.memoryBudgetBytes = 24u * 1024u * 1024u;
  constexpr std::array<float, 7> epsilon{0.0f, -1.0f, 1.0f, 2.0f, 4.0f, 8.0f, 16.0f};
  constexpr int frames = 16;
  PyroSimulationSettings baseSettings;
  baseSettings.dissipation = 0.035f;
  baseSettings.coolingRate = 0.18f;
  baseSettings.buoyancy = 0.7f;
  baseSettings.pressureIterations = 12.0f;
  PyroSourceState source;
  source.position = {0.0f, 0.24f, 0.0f};
  source.extent = {0.13f, 0.09f, 0.13f};
  source.velocity = {0.0f, 240.0f, 0.0f};
  source.density = 4.0f;
  source.temperature = 1.5f;
  source.enabled = true;

  for (std::size_t condition = 0; condition < epsilon.size(); ++condition) {
    PyroSimulation simulation(domain);
    auto settings = baseSettings;
    settings.vorticity = epsilon[condition] == -1.0f ? 0.45f : 0.0f;
    simulation.setSettings(settings);
    const auto resolution = domain.resolution();
    const auto cellCount = static_cast<std::size_t>(resolution.width) *
        resolution.height * resolution.depth;
    std::vector<PyroVec3> velocitySnapshot(cellCount);
    std::vector<float> curlMagnitude(cellCount);
    double densityMass = 0.0;
    FieldTemporalStats temporal{};
    std::vector<float> previousDensity;
    std::array<ImageBuffer, 3> frontFrames{};
    ImageBuffer sideImage{};
    ImageBuffer topImage{};
    for (int frame = 0; frame < frames; ++frame) {
      for (int substep = 0; substep < 2; ++substep) {
        if (epsilon[condition] > 0.0f)
          applyCanonicalVorticityConfinement(simulation, domain, epsilon[condition],
              1.0f / 60.0f, velocitySnapshot, curlMagnitude);
        simulation.setSources(std::span<const PyroSourceState>(&source, 1));
        simulation.step(1.0 / 60.0);
      }
      const auto densityStorage = simulation.fields().densityStorage();
      updateTemporalStats(densityStorage, previousDensity, temporal);
      if (frame < frames - 3) continue;
      const int capture = frame - (frames - 3);
      const auto snapshot = simulation.snapshot();
      auto front = makeRenderer(snapshot, domain, {2.4f, 0.8f, 2.4f});
      front.camera = Camera(1.0f, 36.0f, 2.1f, 0.0f,
          {0.0f, 0.8f, 2.4f}, {0.0f, 0.55f, 0.0f}, {0.0f, 1.0f, 0.0f});
      front.settings.stepSize = 0.025f;
      front.settings.maxSteps = 96;
      front.settings.densityScale = 2.2f;
      front.settings.lightIntensity = 5.0f;
      front.settings.phaseAnisotropy = 0.35f;
      front.settings.multipleScatteringStrength = 0.12f;
      front.settings.shadowIntensity = 0.0f;
      frontFrames[capture] = front.render(144, 144);
      if (capture != 2) continue;
      densityMass = std::accumulate(densityStorage.begin(), densityStorage.end(), 0.0);
      auto side = makeRenderer(snapshot, domain, {2.4f, 0.8f, 2.4f});
      side.camera = Camera(1.0f, 36.0f, 2.1f, 0.0f,
          {2.4f, 0.8f, 0.0f}, {0.0f, 0.55f, 0.0f}, {0.0f, 1.0f, 0.0f});
      side.settings = front.settings;
      sideImage = side.render(144, 144);
      auto top = makeRenderer(snapshot, domain, {2.4f, 0.8f, 2.4f});
      top.camera = Camera(1.0f, 36.0f, 2.1f, 0.0f,
          {0.0f, 2.4f, 0.0f}, {0.0f, 0.55f, 0.0f}, {0.0f, 0.0f, 1.0f});
      top.settings = front.settings;
      topImage = top.render(144, 144);
    }
    double frameResidual = 0.0;
    for (int i = 1; i < 3; ++i) {
      const auto a = measure(frontFrames[i - 1]);
      const auto b = measure(frontFrames[i]);
      frameResidual += std::abs(a.luminanceSum - b.luminanceSum) /
          std::max(1.0, a.luminanceSum);
    }
    frameResidual *= 0.5;
    const std::string label = condition == 1 ? "current" :
        "canonical_epsilon_" + std::to_string(static_cast<int>(epsilon[condition] * 100.0f));
    const auto frontStats = measure(frontFrames[2]);
    const auto sideStats = measure(sideImage);
    const auto topStats = measure(topImage);
    saveIfRequested(frontFrames[2], ("vorticity_" + label + "_front.ppm").c_str());
    saveIfRequested(sideImage, ("vorticity_" + label + "_side.ppm").c_str());
    saveIfRequested(topImage, ("vorticity_" + label + "_top.ppm").c_str());
    GTEST_LOG_(INFO) << "vorticity condition=" << label
        << " mass=" << densityMass
        << " divergenceRms=" << velocityDivergenceRms(simulation)
        << " temporalCosine=" << temporal.cosineSum /
            std::max<std::size_t>(1, temporal.comparisons)
        << " frontCoverage=" << frontStats.occupiedPixels
        << " sideCoverage=" << sideStats.occupiedPixels
        << " topCoverage=" << topStats.occupiedPixels
        << " frameLuminanceResidual=" << frameResidual;
    EXPECT_GT(densityMass, 1.0);
  }
}

TEST(ArtifactCorePyroVolumeSmokeTest,
     ComparesEqualStrengthCurlTurbulenceAndSpeedPreservingShredding) {
  using namespace ArtifactCore;
  using namespace ArtifactCore::RayTrace;
  PyroDomain domain;
  domain.min = {-0.5f, 0.0f, -0.5f};
  domain.max = {0.5f, 1.5f, 0.5f};
  domain.voxelSize = 1.0f / 32.0f;
  domain.fixedTimeStep = 1.0 / 60.0;
  domain.boundaryMode = PyroBoundaryMode::Open;
  domain.memoryBudgetBytes = 24u * 1024u * 1024u;
  constexpr std::array<std::uint32_t, 2> seeds{17, 71};
  constexpr std::array<float, 3> strengths{0.0f, 0.05f, 0.15f};
  constexpr std::array<float, 3> wavelengthScales{0.75f, 1.0f, 2.0f};
  constexpr std::array<float, 3> temporalFrequencyScales{0.5f, 1.0f, 2.0f};
  constexpr int frames = 16;
  PyroSimulationSettings settings;
  settings.dissipation = 0.035f;
  settings.coolingRate = 0.18f;
  settings.buoyancy = 0.7f;
  settings.vorticity = 0.0f;
  settings.pressureIterations = 12.0f;
  PyroSourceState source;
  source.position = {0.0f, 0.24f, 0.0f};
  source.extent = {0.13f, 0.09f, 0.13f};
  source.velocity = {0.0f, 240.0f, 0.0f};
  source.density = 4.0f;
  source.temperature = 1.5f;
  source.enabled = true;
  for (const auto seed : seeds) {
    for (int strengthIndex = 0; strengthIndex < 3; ++strengthIndex) {
      for (int method = 0; method < (strengthIndex == 0 ? 1 : 2); ++method) {
        for (int wavelengthIndex = 0;
             wavelengthIndex < (strengthIndex == 0 ? 1 : 3);
             ++wavelengthIndex) {
          for (int temporalIndex = 0;
               temporalIndex < (strengthIndex == 0 ? 1 : 3);
               ++temporalIndex) {
            PyroSimulation simulation(domain);
            simulation.setSettings(settings);
            double densityMass = 0.0;
            double densityHighFrequency = 0.0;
            FieldTemporalStats temporal{};
            std::vector<float> previousDensity;
            std::array<ImageBuffer, 3> frontFrames{};
            std::array<ImageBuffer, 3> sideFrames{};
            std::array<ImageBuffer, 3> topFrames{};
            std::array<std::size_t, frames> positiveFoldRegions{};
            std::array<std::size_t, frames> negativeFoldRegions{};
            std::array<std::vector<SignedCurvatureRegion>, frames>
                curvatureRegionsPerFrame{};
            std::vector<std::int8_t> previousCurvatureSignMask;
            std::array<std::size_t, 2> curvatureMaskSamplesBySign{};
            std::array<double, 2> backtracedCurvatureMatchesBySign{};
            std::array<double, 2> stationaryCurvatureMatchesBySign{};
            ImageBuffer sideImage{}, topImage{};
            MatchedCurlAdapterStats adapterStats{};
            const bool trackTemporalStructure = strengthIndex == 0 ||
                (strengthIndex == 2 && wavelengthIndex == 1 && temporalIndex == 1);
            for (int frame = 0; frame < frames; ++frame) {
              for (int substep = 0; substep < 2; ++substep) {
                if (strengthIndex > 0) {
                  const float time = static_cast<float>(frame * 2 + substep) / 60.0f;
                  adapterStats = applyMatchedCurlTurbulenceOrShredding(simulation, domain,
                      time, seed, strengths[strengthIndex], method == 1,
                      wavelengthScales[wavelengthIndex],
                      temporalFrequencyScales[temporalIndex]);
                }
                simulation.setSources(std::span<const PyroSourceState>(&source, 1));
                simulation.step(1.0 / 60.0);
              }
              const auto densityStorage = simulation.fields().densityStorage();
              updateTemporalStats(densityStorage, previousDensity, temporal);
              if (trackTemporalStructure) {
                const auto frameInterface = measureDensityInterfaceVorticity(
                    simulation, domain);
                if (!previousCurvatureSignMask.empty()) {
                  const auto velocityView = simulation.fields().velocityView();
                  const auto resolution = domain.resolution();
                  const auto index = [resolution](int x, int y, int z) {
                    return static_cast<std::size_t>(x + resolution.width *
                        (y + resolution.height * z));
                  };
                  const auto samplePreviousSign = [&](double sampleX,
                                                       double sampleY,
                                                       double sampleZ,
                                                       std::int8_t sign) {
                    const int x0 = static_cast<int>(std::floor(sampleX));
                    const int y0 = static_cast<int>(std::floor(sampleY));
                    const int z0 = static_cast<int>(std::floor(sampleZ));
                    const int x1 = x0 + 1;
                    const int y1 = y0 + 1;
                    const int z1 = z0 + 1;
                    if (x0 < 2 || x1 + 2 >= resolution.width ||
                        y0 < 2 || y1 + 2 >= resolution.height ||
                        z0 < 2 || z1 + 2 >= resolution.depth)
                      return -1.0;
                    const double tx = sampleX - x0;
                    const double ty = sampleY - y0;
                    const double tz = sampleZ - z0;
                    double occupancy = 0.0;
                    for (int dz = 0; dz <= 1; ++dz)
                      for (int dy = 0; dy <= 1; ++dy)
                        for (int dx = 0; dx <= 1; ++dx) {
                          const double weight = (dx == 0 ? 1.0 - tx : tx) *
                              (dy == 0 ? 1.0 - ty : ty) *
                              (dz == 0 ? 1.0 - tz : tz);
                          if (previousCurvatureSignMask[index(
                                  dx == 0 ? x0 : x1,
                                  dy == 0 ? y0 : y1,
                                  dz == 0 ? z0 : z1)] == sign)
                            occupancy += weight;
                        }
                    return occupancy;
                  };
                  constexpr double frameDeltaSeconds = 2.0 / 60.0;
                  for (int z = 2; z + 2 < resolution.depth; ++z)
                    for (int y = 2; y + 2 < resolution.height; ++y)
                      for (int x = 2; x + 2 < resolution.width; ++x) {
                        const auto currentCell = index(x, y, z);
                        const auto sign =
                            frameInterface.qualifyingCurvatureSignMask[currentCell];
                        if (sign == 0) continue;
                        const auto signIndex = sign > 0 ? 1u : 0u;
                        const auto velocity = velocityView.values[currentCell];
                        const double previousOccupancy = samplePreviousSign(
                            x - velocity.x * frameDeltaSeconds,
                            y - velocity.y * frameDeltaSeconds,
                            z - velocity.z * frameDeltaSeconds, sign);
                        const double stationaryOccupancy = samplePreviousSign(
                            x, y, z, sign);
                        if (previousOccupancy < 0.0 || stationaryOccupancy < 0.0)
                          continue;
                        ++curvatureMaskSamplesBySign[signIndex];
                        backtracedCurvatureMatchesBySign[signIndex] +=
                            previousOccupancy;
                        stationaryCurvatureMatchesBySign[signIndex] +=
                            stationaryOccupancy;
                      }
                }
                previousCurvatureSignMask =
                    frameInterface.qualifyingCurvatureSignMask;
                positiveFoldRegions[static_cast<std::size_t>(frame)] =
                    frameInterface.largestPositiveCurvatureRegion;
                negativeFoldRegions[static_cast<std::size_t>(frame)] =
                    frameInterface.largestNegativeCurvatureRegion;
                curvatureRegionsPerFrame[static_cast<std::size_t>(frame)] =
                    frameInterface.curvatureRegions;
              }
              if (frame < frames - 3) continue;
              const int capture = frame - (frames - 3);
              const auto snapshot = simulation.snapshot();
              auto renderer = makeRenderer(snapshot, domain, {2.4f, 0.8f, 2.4f});
              renderer.camera = Camera(1.0f, 36.0f, 2.1f, 0.0f,
                  {0.0f, 0.8f, 2.4f}, {0.0f, 0.55f, 0.0f}, {0.0f, 1.0f, 0.0f});
              renderer.settings.stepSize = 0.025f;
              renderer.settings.maxSteps = 96;
              renderer.settings.densityScale = 2.2f;
              renderer.settings.lightIntensity = 5.0f;
              renderer.settings.phaseAnisotropy = 0.35f;
              renderer.settings.multipleScatteringStrength = 0.12f;
              renderer.settings.shadowIntensity = 0.0f;
              frontFrames[capture] = renderer.render(144, 144);
              if (capture == 2) {
                densityMass = std::accumulate(
                    densityStorage.begin(), densityStorage.end(), 0.0);
                const auto resolution = domain.resolution();
                densityHighFrequency = densityHighFrequencyRms(densityStorage,
                    resolution.width, resolution.height, resolution.depth);
              }
              if (capture != 2 && !trackTemporalStructure) continue;
              auto side = makeRenderer(snapshot, domain, {2.4f, 0.8f, 2.4f});
              side.camera = Camera(1.0f, 36.0f, 2.1f, 0.0f,
                  {2.4f, 0.8f, 0.0f}, {0.0f, 0.55f, 0.0f}, {0.0f, 1.0f, 0.0f});
              side.settings = renderer.settings;
              sideFrames[capture] = side.render(144, 144);
              if (capture == 2) sideImage = sideFrames[capture];
              auto top = makeRenderer(snapshot, domain, {2.4f, 0.8f, 2.4f});
              top.camera = Camera(1.0f, 36.0f, 2.1f, 0.0f,
                  {0.0f, 2.4f, 0.0f}, {0.0f, 0.55f, 0.0f}, {0.0f, 0.0f, 1.0f});
              top.settings = renderer.settings;
              topFrames[capture] = top.render(144, 144);
              if (capture == 2) topImage = topFrames[capture];
            }
            const auto interfaceStats = measureDensityInterfaceVorticity(
                simulation, domain);
            const double cosine = temporal.cosineSum /
                std::max<std::size_t>(1, temporal.comparisons);
            const double highFrequency = densityHighFrequency;
            std::size_t coexistingFoldFrames = 0;
            std::size_t longestCoexistingFoldRun = 0;
            std::size_t currentCoexistingFoldRun = 0;
            std::uint32_t coexistingFoldFrameMask = 0;
            std::size_t maxPositiveFoldRegion = 0;
            std::size_t maxNegativeFoldRegion = 0;
            std::size_t advectedRegionPairs = 0;
            double advectedCentroidErrorSum = 0.0;
            double stationaryCentroidErrorSum = 0.0;
            std::array<std::size_t, 2> advectedRegionPairsBySign{};
            std::array<double, 2> advectedCentroidErrorBySign{};
            std::array<double, 2> stationaryCentroidErrorBySign{};
            for (int frame = 0; frame < frames; ++frame) {
              const auto frameIndex = static_cast<std::size_t>(frame);
              maxPositiveFoldRegion = std::max(maxPositiveFoldRegion,
                  positiveFoldRegions[frameIndex]);
              maxNegativeFoldRegion = std::max(maxNegativeFoldRegion,
                  negativeFoldRegions[frameIndex]);
              if (trackTemporalStructure && positiveFoldRegions[frameIndex] >= 4 &&
                  negativeFoldRegions[frameIndex] >= 4) {
                ++coexistingFoldFrames;
                coexistingFoldFrameMask |= 1u << frame;
                ++currentCoexistingFoldRun;
                longestCoexistingFoldRun = std::max(longestCoexistingFoldRun,
                    currentCoexistingFoldRun);
              } else {
                currentCoexistingFoldRun = 0;
              }
            }
            if (trackTemporalStructure) {
              const double frameDeltaSeconds = 2.0 / 60.0;
              for (int frame = 0; frame + 1 < frames; ++frame) {
                const auto& previousRegions =
                    curvatureRegionsPerFrame[static_cast<std::size_t>(frame)];
                const auto& currentRegions =
                    curvatureRegionsPerFrame[static_cast<std::size_t>(frame + 1)];
                std::vector<std::uint8_t> matchedCurrent(currentRegions.size(), 0);
                for (const auto& previousRegion : previousRegions) {
                  if (previousRegion.cellCount < 4) continue;
                  const auto predictedX = previousRegion.centroid.x +
                      previousRegion.meanVelocity.x * frameDeltaSeconds;
                  const auto predictedY = previousRegion.centroid.y +
                      previousRegion.meanVelocity.y * frameDeltaSeconds;
                  const auto predictedZ = previousRegion.centroid.z +
                      previousRegion.meanVelocity.z * frameDeltaSeconds;
                  std::size_t closestIndex = currentRegions.size();
                  double closestDistance = std::numeric_limits<double>::max();
                  for (std::size_t candidateIndex = 0;
                       candidateIndex < currentRegions.size(); ++candidateIndex) {
                    const auto& candidate = currentRegions[candidateIndex];
                    if (matchedCurrent[candidateIndex] != 0 ||
                        candidate.sign != previousRegion.sign ||
                        candidate.cellCount < 4)
                      continue;
                    const double dx = candidate.centroid.x - predictedX;
                    const double dy = candidate.centroid.y - predictedY;
                    const double dz = candidate.centroid.z - predictedZ;
                    const double distance = std::sqrt(dx * dx + dy * dy + dz * dz);
                    if (distance < closestDistance) {
                      closestDistance = distance;
                      closestIndex = candidateIndex;
                    }
                  }
                  if (closestIndex == currentRegions.size() ||
                      closestDistance > 5.0)
                    continue;
                  matchedCurrent[closestIndex] = 1;
                  const auto& matchedRegion = currentRegions[closestIndex];
                  const double stationaryX = matchedRegion.centroid.x -
                      previousRegion.centroid.x;
                  const double stationaryY = matchedRegion.centroid.y -
                      previousRegion.centroid.y;
                  const double stationaryZ = matchedRegion.centroid.z -
                      previousRegion.centroid.z;
                  const double predictedXError = matchedRegion.centroid.x - predictedX;
                  const double predictedYError = matchedRegion.centroid.y - predictedY;
                  const double predictedZError = matchedRegion.centroid.z - predictedZ;
                  stationaryCentroidErrorSum += std::sqrt(
                      stationaryX * stationaryX + stationaryY * stationaryY +
                      stationaryZ * stationaryZ);
                  advectedCentroidErrorSum += std::sqrt(
                      predictedXError * predictedXError +
                      predictedYError * predictedYError +
                      predictedZError * predictedZError);
                  ++advectedRegionPairs;
                  const auto signIndex = previousRegion.sign > 0 ? 1u : 0u;
                  advectedCentroidErrorBySign[signIndex] += std::sqrt(
                      predictedXError * predictedXError +
                      predictedYError * predictedYError +
                      predictedZError * predictedZError);
                  stationaryCentroidErrorBySign[signIndex] += std::sqrt(
                      stationaryX * stationaryX + stationaryY * stationaryY +
                      stationaryZ * stationaryZ);
                  ++advectedRegionPairsBySign[signIndex];
                }
              }
            }
            const double inverseAdvectedPairs = advectedRegionPairs > 0
                ? 1.0 / static_cast<double>(advectedRegionPairs) : 0.0;
            const double inverseNegativePairs = advectedRegionPairsBySign[0] > 0
                ? 1.0 / static_cast<double>(advectedRegionPairsBySign[0]) : 0.0;
            const double inversePositivePairs = advectedRegionPairsBySign[1] > 0
                ? 1.0 / static_cast<double>(advectedRegionPairsBySign[1]) : 0.0;
            const double inverseNegativeMaskSamples =
                curvatureMaskSamplesBySign[0] > 0
                ? 1.0 / static_cast<double>(curvatureMaskSamplesBySign[0]) : 0.0;
            const double inversePositiveMaskSamples =
                curvatureMaskSamplesBySign[1] > 0
                ? 1.0 / static_cast<double>(curvatureMaskSamplesBySign[1]) : 0.0;
            const int methodIndex = strengthIndex == 0 ? 0 : method + 1;
            const std::string methodName = methodIndex == 0 ? "control" :
                methodIndex == 1 ? "curl" : "shred";
            const float wavelengthScale = strengthIndex == 0 ? 1.0f :
                wavelengthScales[wavelengthIndex];
            const float temporalFrequencyScale = strengthIndex == 0 ? 1.0f :
                temporalFrequencyScales[temporalIndex];
            const std::string tag = "matched_seed" + std::to_string(seed) + "_" +
                methodName + "_" + std::to_string(static_cast<int>(strengths[strengthIndex] * 100.0f));
            const std::string wavelengthTag = strengthIndex == 0 ? "_wl100" :
                "_wl" + std::to_string(static_cast<int>(
                    wavelengthScale * 100.0f));
            const std::string temporalTag = strengthIndex == 0 ? "_tf100" :
                "_tf" + std::to_string(static_cast<int>(
                    temporalFrequencyScale * 100.0f));
            saveIfRequested(frontFrames[2], (tag + wavelengthTag + temporalTag + "_front.ppm").c_str());
            saveIfRequested(sideImage, (tag + wavelengthTag + temporalTag + "_side.ppm").c_str());
            saveIfRequested(topImage, (tag + wavelengthTag + temporalTag + "_top.ppm").c_str());
            if (trackTemporalStructure) {
              for (int capture = 0; capture < 3; ++capture) {
                const auto frameTag = "_frame" + std::to_string(
                    frames - 3 + capture);
                saveIfRequested(frontFrames[capture], (tag + wavelengthTag +
                    temporalTag + frameTag + "_front.ppm").c_str());
                saveIfRequested(sideFrames[capture], (tag + wavelengthTag +
                    temporalTag + frameTag + "_side.ppm").c_str());
                saveIfRequested(topFrames[capture], (tag + wavelengthTag +
                    temporalTag + frameTag + "_top.ppm").c_str());
              }
            }
            GTEST_LOG_(INFO) << "matched curl adapter seed=" << seed
                << " method=" << methodName << " fraction=" << strengths[strengthIndex]
                << " wavelengthScale=" << wavelengthScale
                << " temporalFrequencyScale=" << temporalFrequencyScale
                << " affectedCells=" << adapterStats.affectedCells
                << " inputDeltaFraction=" << adapterStats.meanDeltaFraction
                << " inputSpeedChange=" << adapterStats.meanSpeedChangeFraction
                << " mass=" << densityMass << " densityHighFrequency=" << highFrequency
                << " temporalCosine=" << cosine
                << " front/side/topCoverage=" << measure(frontFrames[2]).occupiedPixels
                << '/' << measure(sideImage).occupiedPixels
                << '/' << measure(topImage).occupiedPixels
                << " interfaceSamples=" << interfaceStats.sampleCount
                << " vorticityTangent/normal=" << interfaceStats.meanTangentAlignment
                << '/' << interfaceStats.meanNormalAlignment
                << " interfaceCurvatureRms=" << interfaceStats.curvatureRms
                << " foldCurvatureCells+/-=" << interfaceStats.positiveCurvatureCells
                << '/' << interfaceStats.negativeCurvatureCells
                << " largestFoldRegion+/-=" << interfaceStats.largestPositiveCurvatureRegion
                << '/' << interfaceStats.largestNegativeCurvatureRegion
                << " temporalFoldRegionMax+/-=" << maxPositiveFoldRegion
                << '/' << maxNegativeFoldRegion
                << " coexistingFoldFrames/run=" << coexistingFoldFrames
                << '/' << longestCoexistingFoldRun
                << " coexistingFoldFrameMask=" << coexistingFoldFrameMask
                << " trackedCurvatureRegions=" << advectedRegionPairs
                << " centroidErrorAdvected/stationary="
                << advectedCentroidErrorSum * inverseAdvectedPairs << '/'
                << stationaryCentroidErrorSum * inverseAdvectedPairs
                << " negativeCentroidErrorAdvected/stationary="
                << advectedCentroidErrorBySign[0] * inverseNegativePairs << '/'
                << stationaryCentroidErrorBySign[0] * inverseNegativePairs
                << " positiveCentroidErrorAdvected/stationary="
                << advectedCentroidErrorBySign[1] * inversePositivePairs << '/'
                << stationaryCentroidErrorBySign[1] * inversePositivePairs
                << " negativeMaskMatchAdvected/stationary="
                << backtracedCurvatureMatchesBySign[0] * inverseNegativeMaskSamples
                << '/' << stationaryCurvatureMatchesBySign[0] *
                    inverseNegativeMaskSamples
                << " positiveMaskMatchAdvected/stationary="
                << backtracedCurvatureMatchesBySign[1] * inversePositiveMaskSamples
                << '/' << stationaryCurvatureMatchesBySign[1] *
                    inversePositiveMaskSamples
                << " divergence=" << velocityDivergenceRms(simulation);
            EXPECT_GT(densityMass, 1.0);
            EXPECT_GT(interfaceStats.sampleCount, 100u);
            EXPECT_GE(interfaceStats.meanTangentAlignment, 0.0);
            EXPECT_LE(interfaceStats.meanTangentAlignment, 1.0);
            EXPECT_GE(interfaceStats.meanNormalAlignment, 0.0);
            EXPECT_LE(interfaceStats.meanNormalAlignment, 1.0);
            if (strengthIndex > 0) {
              EXPECT_GT(adapterStats.affectedCells, 100u);
              EXPECT_NEAR(adapterStats.meanDeltaFraction, strengths[strengthIndex], 1e-5);
            }
            if (method == 1 && strengthIndex > 0)
              EXPECT_NEAR(adapterStats.meanSpeedChangeFraction, 0.0, 1e-5);
          }
        }
      }
    }
  }
}

TEST(ArtifactCorePyroVolumeSmokeTest,
     ComparesSeparatedDccStyleSwirlAndShredding) {
  using namespace ArtifactCore;
  using namespace ArtifactCore::RayTrace;
  struct Variant {
    const char* name;
    bool largeSwirl;
    bool fineShredding;
    int substepsPerFrame;
  };
  constexpr std::array<Variant, 5> variants{{
      {"baseline", false, false, 2},
      {"large_swirl", true, false, 2},
      {"fine_shred", false, true, 2},
      {"combined", true, true, 2},
      {"combined_substeps4", true, true, 4}}};
  constexpr std::array<std::uint32_t, 2> seeds{17, 71};
  constexpr int frames = 16;
  constexpr double frameDeltaSeconds = 1.0 / 30.0;
  PyroSimulationSettings settings;
  settings.dissipation = 0.035f;
  settings.coolingRate = 0.18f;
  settings.buoyancy = 0.7f;
  settings.vorticity = 0.0f;
  settings.pressureIterations = 12.0f;
  PyroSourceState source;
  source.position = {0.0f, 0.24f, 0.0f};
  source.extent = {0.13f, 0.09f, 0.13f};
  source.velocity = {0.0f, 240.0f, 0.0f};
  source.density = 4.0f;
  source.temperature = 1.5f;
  source.enabled = true;

  for (const auto seed : seeds) {
    for (const auto& variant : variants) {
      PyroDomain domain;
      domain.min = {-0.5f, 0.0f, -0.5f};
      domain.max = {0.5f, 1.5f, 0.5f};
      domain.voxelSize = 1.0f / 32.0f;
      domain.fixedTimeStep = frameDeltaSeconds / variant.substepsPerFrame;
      domain.boundaryMode = PyroBoundaryMode::Open;
      domain.memoryBudgetBytes = 24u * 1024u * 1024u;
      PyroSimulation simulation(domain);
      simulation.setSettings(settings);
      FieldTemporalStats temporal{};
      std::vector<float> previousDensity;
      MatchedCurlAdapterStats largeSwirlStats{};
      MatchedCurlAdapterStats shreddingStats{};
      double densityMass = 0.0;
      double densityHighFrequency = 0.0;

      for (int frame = 0; frame < frames; ++frame) {
        for (int substep = 0; substep < variant.substepsPerFrame; ++substep) {
          const double deltaSeconds =
              frameDeltaSeconds / variant.substepsPerFrame;
          const float timeSeconds = static_cast<float>(
              frame * frameDeltaSeconds + substep * deltaSeconds);
          const float stepScale = static_cast<float>(deltaSeconds / (1.0 / 60.0));
          if (variant.largeSwirl) {
            largeSwirlStats = applyDivergenceFreeCurlTurbulence(simulation,
                domain, timeSeconds, seed, 0.08f * stepScale, 1.5f, 0.5f);
          }
          if (variant.fineShredding) {
            shreddingStats = applyMatchedCurlTurbulenceOrShredding(simulation,
                domain, timeSeconds, seed, 0.04f * stepScale, true, 0.75f,
                6.0f, 2, 3);
          }
          simulation.setSources(std::span<const PyroSourceState>(&source, 1));
          simulation.step(deltaSeconds);
        }
        const auto densityStorage = simulation.fields().densityStorage();
        updateTemporalStats(densityStorage, previousDensity, temporal);
        if (frame == frames - 1) {
          densityMass = std::accumulate(densityStorage.begin(),
              densityStorage.end(), 0.0);
          const auto resolution = domain.resolution();
          densityHighFrequency = densityHighFrequencyRms(densityStorage,
              resolution.width, resolution.height, resolution.depth);
        }
      }

      const auto snapshot = simulation.snapshot();
      auto frontRenderer = makeRenderer(snapshot, domain, {2.4f, 0.8f, 2.4f});
      frontRenderer.camera = Camera(1.0f, 36.0f, 2.1f, 0.0f,
          {0.0f, 0.8f, 2.4f}, {0.0f, 0.55f, 0.0f}, {0.0f, 1.0f, 0.0f});
      frontRenderer.settings.stepSize = 0.025f;
      frontRenderer.settings.maxSteps = 96;
      frontRenderer.settings.densityScale = 2.2f;
      frontRenderer.settings.lightIntensity = 5.0f;
      frontRenderer.settings.phaseAnisotropy = 0.35f;
      frontRenderer.settings.multipleScatteringStrength = 0.12f;
      frontRenderer.settings.shadowIntensity = 0.0f;
      const auto frontImage = frontRenderer.render(144, 144);
      auto sideRenderer = makeRenderer(snapshot, domain, {2.4f, 0.8f, 2.4f});
      sideRenderer.camera = Camera(1.0f, 36.0f, 2.1f, 0.0f,
          {2.4f, 0.8f, 0.0f}, {0.0f, 0.55f, 0.0f}, {0.0f, 1.0f, 0.0f});
      sideRenderer.settings = frontRenderer.settings;
      const auto sideImage = sideRenderer.render(144, 144);
      auto topRenderer = makeRenderer(snapshot, domain, {2.4f, 0.8f, 2.4f});
      topRenderer.camera = Camera(1.0f, 36.0f, 2.1f, 0.0f,
          {0.0f, 2.4f, 0.0f}, {0.0f, 0.55f, 0.0f}, {0.0f, 0.0f, 1.0f});
      topRenderer.settings = frontRenderer.settings;
      const auto topImage = topRenderer.render(144, 144);
      const auto interfaceStats = measureDensityInterfaceVorticity(
          simulation, domain);
      const double densityCosine = temporal.cosineSum /
          std::max<std::size_t>(1, temporal.comparisons);
      const std::string prefix = "dcc_shaping_seed" + std::to_string(seed) +
          "_" + variant.name;
      saveIfRequested(frontImage, (prefix + "_front.ppm").c_str());
      saveIfRequested(sideImage, (prefix + "_side.ppm").c_str());
      saveIfRequested(topImage, (prefix + "_top.ppm").c_str());
      GTEST_LOG_(INFO) << "DCC-shaping seed=" << seed
          << " variant=" << variant.name
          << " substeps=" << variant.substepsPerFrame
          << " mass=" << densityMass
          << " densityHighFrequency=" << densityHighFrequency
          << " temporalCosine=" << densityCosine
          << " curvatureRms=" << interfaceStats.curvatureRms
          << " front/side/topCoverage=" << measure(frontImage).occupiedPixels
          << '/' << measure(sideImage).occupiedPixels
          << '/' << measure(topImage).occupiedPixels
          << " divergence=" << velocityDivergenceRms(simulation)
          << " largeSwirlCells/delta/speedChange/curlDiv="
          << largeSwirlStats.affectedCells << '/'
          << largeSwirlStats.meanDeltaFraction << '/'
          << largeSwirlStats.meanSpeedChangeFraction << '/'
          << largeSwirlStats.discreteCurlDivergenceRms
          << " fineShredCells/delta/speedChange="
          << shreddingStats.affectedCells << '/'
          << shreddingStats.meanDeltaFraction << '/'
          << shreddingStats.meanSpeedChangeFraction;
      EXPECT_GT(densityMass, 1.0);
      EXPECT_GT(interfaceStats.sampleCount, 100u);
      if (variant.largeSwirl) {
        EXPECT_GT(largeSwirlStats.affectedCells, 100u);
        EXPECT_NEAR(largeSwirlStats.meanDeltaFraction,
            0.08 * (2.0 / variant.substepsPerFrame), 1e-4);
        EXPECT_LT(largeSwirlStats.discreteCurlDivergenceRms, 1e-4);
      }
      if (variant.fineShredding) {
        EXPECT_GT(shreddingStats.affectedCells, 100u);
        EXPECT_NEAR(shreddingStats.meanDeltaFraction,
            0.04 * (2.0 / variant.substepsPerFrame), 1e-4);
        EXPECT_NEAR(shreddingStats.meanSpeedChangeFraction, 0.0, 1e-5);
      }
    }
  }
}

TEST(ArtifactCorePyroVolumeSmokeTest,
     SeparatesGridResolutionFromFixedSubstepSizeForSmoke) {
  using namespace ArtifactCore;
  using namespace ArtifactCore::RayTrace;
  struct Condition { float voxelSize; double fixedStep; const char* label; };
  constexpr std::array<Condition, 4> conditions{{
      {1.0f / 32.0f, 1.0 / 60.0, "v32_dt60"},
      {1.0f / 32.0f, 1.0 / 120.0, "v32_dt120"},
      {1.0f / 48.0f, 1.0 / 60.0, "v48_dt60"},
      {1.0f / 48.0f, 1.0 / 120.0, "v48_dt120"}}};
  constexpr int frames = 24;
  PyroSimulationSettings settings;
  settings.dissipation = 0.035f;
  settings.coolingRate = 0.18f;
  settings.buoyancy = 0.7f;
  settings.vorticity = 0.0f;
  PyroSourceState source;
  source.position = {0.0f, 0.24f, 0.0f};
  source.extent = {0.13f, 0.09f, 0.13f};
  source.velocity = {0.0f, 240.0f, 0.0f};
  source.density = 4.0f;
  source.temperature = 1.5f;
  source.enabled = true;

  for (const auto& condition : conditions) {
    PyroDomain domain;
    domain.min = {-0.5f, 0.0f, -0.5f};
    domain.max = {0.5f, 1.5f, 0.5f};
    domain.voxelSize = condition.voxelSize;
    domain.fixedTimeStep = condition.fixedStep;
    domain.boundaryMode = PyroBoundaryMode::Open;
    domain.memoryBudgetBytes = 64u * 1024u * 1024u;
    PyroSimulation simulation(domain);
    simulation.setFixedTimeStep(condition.fixedStep);
    const int gridScale = static_cast<int>(std::lround((1.0f / 32.0f) / condition.voxelSize));
    settings.pressureIterations = 12.0f * gridScale;
    simulation.setSettings(settings);
    FieldTemporalStats temporal{};
    std::vector<float> previousDensity;
    std::array<ImageBuffer, 3> frontFrames{};
    ImageBuffer sideImage{}, topImage{};
    double densitySum = 0.0;
    double worldMass = 0.0;
    double gradientRms = 0.0;
    for (int frame = 0; frame < frames; ++frame) {
      simulation.setSources(std::span<const PyroSourceState>(&source, 1));
      simulation.step(1.0 / 60.0);
      const auto densityStorage = simulation.fields().densityStorage();
      updateTemporalStats(densityStorage, previousDensity, temporal);
      if (frame < frames - 3) continue;
      const int capture = frame - (frames - 3);
      const auto snapshot = simulation.snapshot();
      auto front = makeRenderer(snapshot, domain, {2.4f, 0.8f, 2.4f});
      front.camera = Camera(1.0f, 36.0f, 2.1f, 0.0f,
          {0.0f, 0.8f, 2.4f}, {0.0f, 0.55f, 0.0f}, {0.0f, 1.0f, 0.0f});
      front.settings.stepSize = 0.0125f;
      front.settings.maxSteps = 192;
      front.settings.densityScale = 2.2f;
      front.settings.lightIntensity = 5.0f;
      front.settings.phaseAnisotropy = 0.35f;
      front.settings.multipleScatteringStrength = 0.12f;
      front.settings.shadowIntensity = 0.0f;
      frontFrames[capture] = front.render(160, 160);
      if (capture != 2) continue;
      densitySum = std::accumulate(densityStorage.begin(), densityStorage.end(), 0.0);
      worldMass = densitySum * condition.voxelSize * condition.voxelSize * condition.voxelSize;
      const auto resolution = domain.resolution();
      gradientRms = densityWorldGradientRms(densityStorage, resolution.width,
          resolution.height, resolution.depth, condition.voxelSize);
      auto side = makeRenderer(snapshot, domain, {2.4f, 0.8f, 2.4f});
      side.camera = Camera(1.0f, 36.0f, 2.1f, 0.0f,
          {2.4f, 0.8f, 0.0f}, {0.0f, 0.55f, 0.0f}, {0.0f, 1.0f, 0.0f});
      side.settings = front.settings;
      sideImage = side.render(160, 160);
      auto top = makeRenderer(snapshot, domain, {2.4f, 0.8f, 2.4f});
      top.camera = Camera(1.0f, 36.0f, 2.1f, 0.0f,
          {0.0f, 2.4f, 0.0f}, {0.0f, 0.55f, 0.0f}, {0.0f, 0.0f, 1.0f});
      top.settings = front.settings;
      topImage = top.render(160, 160);
    }
    const auto r = domain.resolution();
    const double cosine = temporal.cosineSum /
        std::max<std::size_t>(1, temporal.comparisons);
    saveIfRequested(frontFrames[2], (std::string("grid_") + condition.label + "_front.ppm").c_str());
    saveIfRequested(sideImage, (std::string("grid_") + condition.label + "_side.ppm").c_str());
    saveIfRequested(topImage, (std::string("grid_") + condition.label + "_top.ppm").c_str());
    GTEST_LOG_(INFO) << "grid/substep condition=" << condition.label
        << " resolution=" << r.width << 'x' << r.height << 'x' << r.depth
        << " fixedStep=" << condition.fixedStep
        << " pressureIterations=" << settings.pressureIterations
        << " voxelMassSum=" << densitySum << " worldMass=" << worldMass
        << " worldGradientRms=" << gradientRms
        << " densityHighFrequency=" << densityHighFrequencyRms(
            simulation.fields().densityStorage(), r.width, r.height, r.depth)
        << " temporalCosine=" << cosine
        << " coverage(front/side/top)=" << measure(frontFrames[2]).occupiedPixels
        << '/' << measure(sideImage).occupiedPixels << '/' << measure(topImage).occupiedPixels
        << " divergence=" << velocityDivergenceRms(simulation);
    EXPECT_GT(densitySum, 1.0);
    EXPECT_GT(worldMass, 0.0);
  }
}

TEST(ArtifactCorePyroVolumeSmokeTest,
     TracksSmokeAfterEqualIntegratedEmitterPulsesStop) {
  using namespace ArtifactCore;
  using namespace ArtifactCore::RayTrace;
  struct PulseCondition { int emissionFrames; float rateScale; const char* label; };
  constexpr std::array<PulseCondition, 3> conditions{{
      {6, 2.0f, "pulse_06f"}, {12, 1.0f, "pulse_12f"},
      {24, 0.5f, "pulse_24f"}}};
  constexpr std::array<int, 5> captureFrames{6, 12, 24, 48, 72};
  constexpr int totalFrames = 72;
  PyroDomain domain;
  domain.min = {-0.5f, 0.0f, -0.5f};
  domain.max = {0.5f, 1.5f, 0.5f};
  domain.voxelSize = 1.0f / 32.0f;
  domain.fixedTimeStep = 1.0 / 60.0;
  domain.boundaryMode = PyroBoundaryMode::Open;
  domain.memoryBudgetBytes = 24u * 1024u * 1024u;
  PyroSimulationSettings settings;
  settings.dissipation = 0.035f;
  settings.coolingRate = 0.18f;
  settings.buoyancy = 0.7f;
  settings.vorticity = 0.0f;
  settings.pressureIterations = 12.0f;
  const PyroSourceState baseSource{{0.0f, 0.24f, 0.0f},
      {0.13f, 0.09f, 0.13f}, {0.0f, 240.0f, 0.0f}, 4.0f, 1.5f, 0.0f, true};
  const auto resolution = domain.resolution();
  for (const auto& condition : conditions) {
    PyroSimulation simulation(domain);
    simulation.setFixedTimeStep(1.0 / 60.0);
    simulation.setSettings(settings);
    std::array<std::array<ImageBuffer, 3>, 5> images{};
    FieldTemporalStats temporal{};
    std::vector<float> previousDensity;
    std::array<double, 5> worldMass{};
    std::array<std::array<double, 3>, 5> centroid{};
    std::array<double, 5> divergence{};
    std::array<double, 5> worldGradient{};
    std::array<double, 5> injectedRateIntegral{};
    std::size_t captureIndex = 0;
    for (int frame = 0; frame < totalFrames; ++frame) {
      if (frame < condition.emissionFrames) {
        auto source = baseSource;
        source.density *= condition.rateScale;
        source.temperature *= condition.rateScale;
        source.velocity.x *= condition.rateScale;
        source.velocity.y *= condition.rateScale;
        source.velocity.z *= condition.rateScale;
        simulation.setSources(std::span<const PyroSourceState>(&source, 1));
        injectedRateIntegral[captureIndex] += condition.rateScale / 60.0;
      } else {
        simulation.setSources(std::span<const PyroSourceState>{});
      }
      simulation.step(1.0 / 60.0);
      const auto densityStorage = simulation.fields().densityStorage();
      updateTemporalStats(densityStorage, previousDensity, temporal);
      if (captureIndex >= captureFrames.size() || frame + 1 != captureFrames[captureIndex]) continue;
      worldMass[captureIndex] = std::accumulate(densityStorage.begin(), densityStorage.end(), 0.0) *
          domain.voxelSize * domain.voxelSize * domain.voxelSize;
      double weightedX = 0.0, weightedY = 0.0, weightedZ = 0.0, densitySum = 0.0;
      for (int z = 0; z < resolution.depth; ++z)
        for (int y = 0; y < resolution.height; ++y)
          for (int x = 0; x < resolution.width; ++x) {
            const double rho = densityStorage[static_cast<std::size_t>(
                x + resolution.width * (y + resolution.height * z))];
            densitySum += rho;
            weightedX += rho * (domain.min.x + (x + 0.5) * domain.voxelSize);
            weightedY += rho * (domain.min.y + (y + 0.5) * domain.voxelSize);
            weightedZ += rho * (domain.min.z + (z + 0.5) * domain.voxelSize);
          }
      if (densitySum > 0.0) {
        centroid[captureIndex] = {weightedX / densitySum, weightedY / densitySum, weightedZ / densitySum};
      }
      divergence[captureIndex] = velocityDivergenceRms(simulation);
      worldGradient[captureIndex] = densityWorldGradientRms(densityStorage,
          resolution.width, resolution.height, resolution.depth, domain.voxelSize);
      const auto snapshot = simulation.snapshot();
      auto front = makeRenderer(snapshot, domain, {2.4f, 0.8f, 2.4f});
      front.camera = Camera(1.0f, 36.0f, 2.1f, 0.0f,
          {0.0f, 0.8f, 2.4f}, {0.0f, 0.55f, 0.0f}, {0.0f, 1.0f, 0.0f});
      front.settings.stepSize = 0.0125f;
      front.settings.maxSteps = 192;
      front.settings.densityScale = 2.2f;
      front.settings.lightIntensity = 5.0f;
      front.settings.phaseAnisotropy = 0.35f;
      front.settings.multipleScatteringStrength = 0.12f;
      front.settings.shadowIntensity = 0.0f;
      images[captureIndex][0] = front.render(160, 160);
      if (captureIndex == captureFrames.size() - 1) {
        auto side = makeRenderer(snapshot, domain, {2.4f, 0.8f, 2.4f});
        side.camera = Camera(1.0f, 36.0f, 2.1f, 0.0f,
            {2.4f, 0.8f, 0.0f}, {0.0f, 0.55f, 0.0f}, {0.0f, 1.0f, 0.0f});
        side.settings = front.settings;
        images[captureIndex][1] = side.render(160, 160);
        auto top = makeRenderer(snapshot, domain, {2.4f, 0.8f, 2.4f});
        top.camera = Camera(1.0f, 36.0f, 2.1f, 0.0f,
            {0.0f, 2.4f, 0.0f}, {0.0f, 0.55f, 0.0f}, {0.0f, 0.0f, 1.0f});
        top.settings = front.settings;
        images[captureIndex][2] = top.render(160, 160);
      }
      const auto stats = measure(images[captureIndex][0]);
      GTEST_LOG_(INFO) << "emitter pulse=" << condition.label
          << " time=" << captureFrames[captureIndex] / 60.0
          << " sourceActive=" << (frame < condition.emissionFrames)
          << " injectedRateIntegral=" << injectedRateIntegral[captureIndex]
          << " worldMass=" << worldMass[captureIndex]
          << " centroid=" << centroid[captureIndex][0] << ','
          << centroid[captureIndex][1] << ',' << centroid[captureIndex][2]
          << " worldGradient=" << worldGradient[captureIndex]
          << " divergence=" << divergence[captureIndex]
          << " frontCoverage=" << stats.occupiedPixels
          << " frontLuma=" << stats.luminanceSum;
      saveIfRequested(images[captureIndex][0], (std::string(condition.label) +
          "_capture" + std::to_string(captureIndex) + "_front.ppm").c_str());
      if (captureIndex == captureFrames.size() - 1) {
        saveIfRequested(images[captureIndex][1], (std::string(condition.label) + "_final_side.ppm").c_str());
        saveIfRequested(images[captureIndex][2], (std::string(condition.label) + "_final_top.ppm").c_str());
      }
      ++captureIndex;
    }
    GTEST_LOG_(INFO) << "emitter pulse summary=" << condition.label
        << " temporalCosine=" << temporal.cosineSum /
            std::max<std::size_t>(1, temporal.comparisons)
        << " temporalRelativeL1=" << temporal.relativeL1Sum /
            std::max<std::size_t>(1, temporal.comparisons);
    ASSERT_EQ(captureIndex, captureFrames.size());
    EXPECT_NEAR(condition.emissionFrames * condition.rateScale / 60.0, 0.2, 1e-6);
    EXPECT_GT(worldMass.back(), 0.0);
  }
}

TEST(ArtifactCorePyroVolumeSmokeTest,
     SeparatesSmokeMassLossByTraceDistanceAndBoundaryMode) {
  using namespace ArtifactCore;
  using namespace ArtifactCore::RayTrace;
  struct Condition { PyroBoundaryMode boundary; float sourceSpeed; const char* label; };
  constexpr std::array<Condition, 6> conditions{{
      {PyroBoundaryMode::Open, 60.0f, "open_v60"},
      {PyroBoundaryMode::Open, 120.0f, "open_v120"},
      {PyroBoundaryMode::Open, 240.0f, "open_v240"},
      {PyroBoundaryMode::Closed, 60.0f, "closed_v60"},
      {PyroBoundaryMode::Closed, 120.0f, "closed_v120"},
      {PyroBoundaryMode::Closed, 240.0f, "closed_v240"}}};
  constexpr int emissionFrames = 12;
  constexpr int totalFrames = 72;
  constexpr float voxelSize = 1.0f / 32.0f;
  constexpr double dt = 1.0 / 60.0;
  PyroSimulationSettings settings;
  settings.dissipation = 0.035f;
  settings.coolingRate = 0.18f;
  settings.buoyancy = 0.7f;
  settings.vorticity = 0.0f;
  settings.pressureIterations = 12.0f;
  PyroSourceState source;
  source.position = {0.0f, 0.24f, 0.0f};
  source.extent = {0.13f, 0.09f, 0.13f};
  source.density = 4.0f;
  source.temperature = 1.5f;
  source.enabled = true;
  const std::array<int, 2> captureFrames{emissionFrames, totalFrames};
  for (const auto& condition : conditions) {
    PyroDomain domain;
    domain.min = {-0.5f, 0.0f, -0.5f};
    domain.max = {0.5f, 1.5f, 0.5f};
    domain.voxelSize = voxelSize;
    domain.fixedTimeStep = dt;
    domain.boundaryMode = condition.boundary;
    domain.memoryBudgetBytes = 24u * 1024u * 1024u;
    PyroSimulation simulation(domain);
    simulation.setFixedTimeStep(dt);
    settings.advectionClamp = 1.0f;
    simulation.setSettings(settings);
    const auto resolution = domain.resolution();
    std::array<double, 2> worldMass{};
    std::array<double, 2> boundaryMassFraction{};
    std::array<double, 2> maximumTraceCells{};
    std::array<double, 2> densityWeightedTraceCells{};
    std::array<double, 2> divergence{};
    std::vector<float> finalDensity;
    for (int frame = 0; frame < totalFrames; ++frame) {
      if (frame < emissionFrames) {
        source.velocity = {0.0f, condition.sourceSpeed, 0.0f};
        simulation.setSources(std::span<const PyroSourceState>(&source, 1));
      } else {
        simulation.setSources(std::span<const PyroSourceState>{});
      }
      simulation.step(dt);
      if (frame + 1 != emissionFrames && frame + 1 != totalFrames) continue;
      const int capture = frame + 1 == emissionFrames ? 0 : 1;
      const auto density = simulation.fields().densityView();
      const auto velocity = simulation.fields().velocityView();
      double sum = 0.0, shellSum = 0.0, weightedTrace = 0.0;
      for (int z = 0; z < resolution.depth; ++z)
        for (int y = 0; y < resolution.height; ++y)
          for (int x = 0; x < resolution.width; ++x) {
            const float rho = density.at(x, y, z);
            const auto v = velocity.at(x, y, z);
            const double trace = std::sqrt(static_cast<double>(v.x) * v.x +
                static_cast<double>(v.y) * v.y + static_cast<double>(v.z) * v.z) * dt;
            sum += rho;
            weightedTrace += rho * trace;
            maximumTraceCells[capture] = std::max(maximumTraceCells[capture], trace);
            if (x < 2 || x >= resolution.width - 2 ||
                y < 2 || y >= resolution.height - 2 ||
                z < 2 || z >= resolution.depth - 2) shellSum += rho;
          }
      worldMass[capture] = sum * voxelSize * voxelSize * voxelSize;
      boundaryMassFraction[capture] = sum > 0.0 ? shellSum / sum : 0.0;
      densityWeightedTraceCells[capture] = sum > 0.0 ? weightedTrace / sum : 0.0;
      divergence[capture] = velocityDivergenceRms(simulation);
      if (capture == 1) finalDensity = simulation.fields().densityStorage();
    }
    const auto snapshot = simulation.snapshot();
    auto renderer = makeRenderer(snapshot, domain, {2.4f, 0.8f, 2.4f});
    renderer.camera = Camera(1.0f, 36.0f, 2.1f, 0.0f,
        {0.0f, 0.8f, 2.4f}, {0.0f, 0.55f, 0.0f}, {0.0f, 1.0f, 0.0f});
    renderer.settings.stepSize = 0.0125f;
    renderer.settings.maxSteps = 192;
    renderer.settings.densityScale = 2.2f;
    renderer.settings.lightIntensity = 5.0f;
    renderer.settings.phaseAnisotropy = 0.35f;
    renderer.settings.multipleScatteringStrength = 0.12f;
    renderer.settings.shadowIntensity = 0.0f;
    const auto front = renderer.render(160, 160);
    auto side = makeRenderer(snapshot, domain, {2.4f, 0.8f, 2.4f});
    side.camera = Camera(1.0f, 36.0f, 2.1f, 0.0f,
        {2.4f, 0.8f, 0.0f}, {0.0f, 0.55f, 0.0f}, {0.0f, 1.0f, 0.0f});
    side.settings = renderer.settings;
    const auto sideImage = side.render(160, 160);
    auto top = makeRenderer(snapshot, domain, {2.4f, 0.8f, 2.4f});
    top.camera = Camera(1.0f, 36.0f, 2.1f, 0.0f,
        {0.0f, 2.4f, 0.0f}, {0.0f, 0.55f, 0.0f}, {0.0f, 0.0f, 1.0f});
    top.settings = renderer.settings;
    const auto topImage = top.render(160, 160);
    saveIfRequested(front, (std::string("trace_") + condition.label + "_front.ppm").c_str());
    saveIfRequested(sideImage, (std::string("trace_") + condition.label + "_side.ppm").c_str());
    saveIfRequested(topImage, (std::string("trace_") + condition.label + "_top.ppm").c_str());
    const double retainedMass = worldMass[1] / std::max(worldMass[0], 1e-15);
    GTEST_LOG_(INFO) << "trace/boundary condition=" << condition.label
        << " massAtPulseEnd/1.2s=" << worldMass[0] << '/' << worldMass[1]
        << " retainedMass=" << retainedMass
        << " maxTraceCellsAtPulseEnd/final=" << maximumTraceCells[0] << '/' << maximumTraceCells[1]
        << " densityWeightedTraceCells=" << densityWeightedTraceCells[0] << '/'
            << densityWeightedTraceCells[1]
        << " boundaryShellMassFraction=" << boundaryMassFraction[0] << '/'
            << boundaryMassFraction[1]
        << " divergence=" << divergence[0] << '/' << divergence[1]
        << " finalCoverage(front/side/top)=" << measure(front).occupiedPixels
        << '/' << measure(sideImage).occupiedPixels << '/' << measure(topImage).occupiedPixels;
    EXPECT_GT(worldMass[0], 0.0);
    EXPECT_GT(worldMass[1], 0.0);
    EXPECT_LT(boundaryMassFraction[1], 0.1);
    EXPECT_EQ(finalDensity.size(), static_cast<std::size_t>(
        resolution.width * resolution.height * resolution.depth));
  }
}

TEST(ArtifactCorePyroVolumeSmokeTest,
     ComparesFixedScalarAdvectionWithPressureProjectedVelocity) {
  using namespace ArtifactCore;
  using namespace ArtifactCore::RayTrace;
  constexpr float voxelSize = 1.0f / 32.0f;
  constexpr double dt = 1.0 / 60.0;
  constexpr int pulseFrames = 12;
  constexpr int transportFrames = 60;
  PyroDomain domain;
  domain.min = {-0.5f, 0.0f, -0.5f};
  domain.max = {0.5f, 1.5f, 0.5f};
  domain.voxelSize = voxelSize;
  domain.fixedTimeStep = dt;
  domain.boundaryMode = PyroBoundaryMode::Open;
  domain.memoryBudgetBytes = 24u * 1024u * 1024u;
  PyroSimulationSettings settings;
  settings.dissipation = 0.035f;
  settings.coolingRate = 0.18f;
  settings.buoyancy = 0.7f;
  settings.vorticity = 0.0f;
  settings.pressureIterations = 12.0f;
  PyroSourceState source;
  source.position = {0.0f, 0.24f, 0.0f};
  source.extent = {0.13f, 0.09f, 0.13f};
  source.velocity = {0.0f, 240.0f, 0.0f};
  source.density = 4.0f;
  source.temperature = 1.5f;
  source.enabled = true;
  PyroSimulation production(domain);
  production.setFixedTimeStep(dt);
  production.setSettings(settings);
  for (int frame = 0; frame < pulseFrames; ++frame) {
    production.setSources(std::span<const PyroSourceState>(&source, 1));
    production.step(dt);
  }
  production.setSources(std::span<const PyroSourceState>{});
  const auto resolution = domain.resolution();
  const auto initialDensity = production.fields().densityStorage();
  const auto initialVelocity = production.fields().velocityStorage();
  const auto densityMass = [&](const std::vector<float>& values) {
    return std::accumulate(values.begin(), values.end(), 0.0) *
        voxelSize * voxelSize * voxelSize;
  };
  const auto index = [resolution](int x, int y, int z) {
    return static_cast<std::size_t>(x + resolution.width * (y + resolution.height * z));
  };
  PyroSimulation sampler(domain);
  std::array<double, 4> massRetention{};
  constexpr std::array<int, 4> iterations{0, 12, 24, 48};
  std::array<ImageBuffer, 4> finalImages{};
  for (std::size_t condition = 0; condition < iterations.size(); ++condition) {
    std::vector<PyroVec3> flow = iterations[condition] == 0
        ? initialVelocity
        : projectTestVelocity(initialVelocity, resolution.width, resolution.height,
            resolution.depth, iterations[condition], false);
    std::vector<float> density = initialDensity;
    std::vector<float> next(density.size());
    const PyroConstVectorFieldView flowView{std::span<const PyroVec3>(flow), resolution};
    for (int step = 0; step < transportFrames; ++step) {
      const PyroConstScalarFieldView densityView{std::span<const float>(density), resolution};
      for (int z = 0; z < resolution.depth; ++z)
        for (int y = 0; y < resolution.height; ++y)
          for (int x = 0; x < resolution.width; ++x) {
            const auto v = flow[index(x, y, z)];
            const float stepDt = static_cast<float>(dt);
            const PyroVec3 backtrace{static_cast<float>(x) - v.x * stepDt,
                static_cast<float>(y) - v.y * stepDt,
                static_cast<float>(z) - v.z * stepDt};
            next[index(x, y, z)] = sampler.fields().sampleScalar(
                densityView, backtrace, domain.boundaryMode);
          }
      density.swap(next);
    }
    massRetention[condition] = densityMass(density) / densityMass(initialDensity);
    PyroFrameSnapshot snapshot = production.snapshot();
    snapshot.density = {std::span<const float>(density), resolution};
    auto renderer = makeRenderer(snapshot, domain, {2.4f, 0.8f, 2.4f});
    renderer.camera = Camera(1.0f, 36.0f, 2.1f, 0.0f,
        {0.0f, 0.8f, 2.4f}, {0.0f, 0.55f, 0.0f}, {0.0f, 1.0f, 0.0f});
    renderer.settings.stepSize = 0.0125f;
    renderer.settings.maxSteps = 192;
    renderer.settings.densityScale = 2.2f;
    renderer.settings.lightIntensity = 5.0f;
    renderer.settings.phaseAnisotropy = 0.35f;
    renderer.settings.multipleScatteringStrength = 0.12f;
    renderer.settings.shadowIntensity = 0.0f;
    finalImages[condition] = renderer.render(160, 160);
    const double divergence = gridDivergenceRms(
        flow, resolution.width, resolution.height, resolution.depth);
    GTEST_LOG_(INFO) << "fixed-velocity scalar advection projectionIterations="
        << iterations[condition] << " flowDivergence=" << divergence
        << " initialMass=" << densityMass(initialDensity)
        << " retainedMass=" << massRetention[condition]
        << " finalCoverage=" << measure(finalImages[condition]).occupiedPixels;
    saveIfRequested(finalImages[condition], (std::string("scalar_projection_") +
        std::to_string(iterations[condition]) + ".ppm").c_str());
    EXPECT_GT(massRetention[condition], 0.0);
  }
  PyroSimulationSettings passive = settings;
  passive.dissipation = 0.0f;
  passive.coolingRate = 0.0f;
  passive.buoyancy = 0.0f;
  production.setSettings(passive);
  std::fill(production.fields().temperatureView().values.begin(),
            production.fields().temperatureView().values.end(), 0.0f);
  std::fill(production.fields().fuelView().values.begin(),
            production.fields().fuelView().values.end(), 0.0f);
  for (int frame = 0; frame < transportFrames; ++frame) production.step(dt);
  const double fullPyroRetention = densityMass(production.fields().densityStorage()) /
      densityMass(initialDensity);
  GTEST_LOG_(INFO) << "full Pyro after pulse retainedMass=" << fullPyroRetention
      << " divergence=" << velocityDivergenceRms(production);
  EXPECT_GT(fullPyroRetention, 0.0);
}

TEST(ArtifactCorePyroVolumeSmokeTest,
     MeasuresScalarMassLossForUniformAndAnalyticDivergenceFreeFlows) {
  using namespace ArtifactCore;
  constexpr float dt = 1.0f / 60.0f;
  constexpr int steps = 60;
  PyroDomain domain;
  domain.min = {-1.0f, 0.0f, -1.0f};
  domain.max = {1.0f, 2.0f, 1.0f};
  domain.voxelSize = 1.0f / 32.0f;
  domain.fixedTimeStep = dt;
  domain.boundaryMode = PyroBoundaryMode::Closed;
  domain.memoryBudgetBytes = 24u * 1024u * 1024u;
  PyroSimulation sampler(domain);
  const auto resolution = domain.resolution();
  const auto index = [resolution](int x, int y, int z) {
    return static_cast<std::size_t>(x + resolution.width *
        (y + resolution.height * z));
  };
  std::vector<float> initialDensity(static_cast<std::size_t>(
      resolution.width * resolution.height * resolution.depth));
  constexpr float sigma = 4.0f;
  for (int z = 0; z < resolution.depth; ++z)
    for (int y = 0; y < resolution.height; ++y)
      for (int x = 0; x < resolution.width; ++x) {
        const float dx = static_cast<float>(x) - 32.0f;
        const float dy = static_cast<float>(y) - 32.0f;
        const float dz = static_cast<float>(z) - 32.0f;
        initialDensity[index(x, y, z)] = std::exp(
            -(dx * dx + dy * dy + dz * dz) / (2.0f * sigma * sigma));
      }
  const double initialMass = std::accumulate(
      initialDensity.begin(), initialDensity.end(), 0.0);
  const auto advectScalar = [&](const std::vector<float>& input,
                                std::vector<float>& output,
                                const std::vector<PyroVec3>& flow,
                                float direction) {
    const PyroConstScalarFieldView inputView{
        std::span<const float>(input), resolution};
    const PyroConstVectorFieldView flowView{
        std::span<const PyroVec3>(flow), resolution};
    for (int z = 0; z < resolution.depth; ++z)
      for (int y = 0; y < resolution.height; ++y)
        for (int x = 0; x < resolution.width; ++x) {
          const auto velocity = flowView.at(x, y, z);
          const PyroVec3 backtrace{
              static_cast<float>(x) - direction * velocity.x * dt,
              static_cast<float>(y) - direction * velocity.y * dt,
              static_cast<float>(z) - direction * velocity.z * dt};
          output[index(x, y, z)] = sampler.fields().sampleScalar(
              inputView, backtrace, PyroBoundaryMode::Closed);
        }
  };
  const auto advectWithMode = [&](const std::vector<PyroVec3>& flow,
                                  int mode,
                                  std::vector<float>* finalDensity) {
    std::vector<float> density = initialDensity;
    std::vector<float> next(density.size());
    std::vector<float> forward(density.size());
    std::vector<float> backward(density.size());
    std::vector<float> corrected(density.size());
    for (int step = 0; step < steps; ++step) {
      if (mode == 0) {
        advectScalar(density, next, flow, 1.0f);
      } else {
        advectScalar(density, forward, flow, 1.0f);
        advectScalar(forward, backward, flow, -1.0f);
        for (std::size_t i = 0; i < density.size(); ++i)
          corrected[i] = density[i] + 0.5f * (density[i] - backward[i]);
        advectScalar(corrected, next, flow, 1.0f);
        if (mode == 2) {
          const PyroConstVectorFieldView flowView{
              std::span<const PyroVec3>(flow), resolution};
          for (int z = 0; z < resolution.depth; ++z)
            for (int y = 0; y < resolution.height; ++y)
              for (int x = 0; x < resolution.width; ++x) {
                const auto velocity = flowView.at(x, y, z);
                const float sx = std::clamp(
                    static_cast<float>(x) - velocity.x * dt, 0.0f,
                    static_cast<float>(resolution.width - 1));
                const float sy = std::clamp(
                    static_cast<float>(y) - velocity.y * dt, 0.0f,
                    static_cast<float>(resolution.height - 1));
                const float sz = std::clamp(
                    static_cast<float>(z) - velocity.z * dt, 0.0f,
                    static_cast<float>(resolution.depth - 1));
                const int x0 = static_cast<int>(std::floor(sx));
                const int y0 = static_cast<int>(std::floor(sy));
                const int z0 = static_cast<int>(std::floor(sz));
                const int x1 = std::min(x0 + 1, resolution.width - 1);
                const int y1 = std::min(y0 + 1, resolution.height - 1);
                const int z1 = std::min(z0 + 1, resolution.depth - 1);
                float lo = std::numeric_limits<float>::max();
                float hi = std::numeric_limits<float>::lowest();
                for (const int cz : {z0, z1})
                  for (const int cy : {y0, y1})
                    for (const int cx : {x0, x1}) {
                      const float value = density[index(cx, cy, cz)];
                      lo = std::min(lo, value);
                      hi = std::max(hi, value);
                    }
                next[index(x, y, z)] = std::clamp(
                    next[index(x, y, z)], lo, hi);
              }
        }
      }
      density.swap(next);
    }
    double mass = std::accumulate(density.begin(), density.end(), 0.0);
    if (finalDensity) *finalDensity = density;
    const auto [minimum, maximum] = std::minmax_element(
        density.begin(), density.end());
    return std::array<double, 4>{mass / initialMass, *minimum, *maximum,
        densityHighFrequencyRms(density, resolution.width,
            resolution.height, resolution.depth)};
  };

  std::vector<PyroVec3> uniformFlow(initialDensity.size(), {12.0f, 0.0f, 0.0f});
  constexpr float streamfunctionAmplitude = 400.0f;
  std::vector<float> streamfunction(initialDensity.size());
  for (int z = 0; z < resolution.depth; ++z)
    for (int y = 0; y < resolution.height; ++y)
      for (int x = 0; x < resolution.width; ++x) {
        const float sx = std::sin(3.14159265359f * x /
            static_cast<float>(resolution.width - 1));
        const float sy = std::sin(3.14159265359f * y /
            static_cast<float>(resolution.height - 1));
        streamfunction[index(x, y, z)] = streamfunctionAmplitude * sx * sx * sy * sy;
      }
  std::vector<PyroVec3> vortexFlow(initialDensity.size());
  const auto clampX = [resolution](int x) {
    return std::clamp(x, 0, resolution.width - 1);
  };
  const auto clampY = [resolution](int y) {
    return std::clamp(y, 0, resolution.height - 1);
  };
  for (int z = 0; z < resolution.depth; ++z)
    for (int y = 0; y < resolution.height; ++y)
      for (int x = 0; x < resolution.width; ++x) {
        const float dPsiDy = 0.5f * (
            streamfunction[index(x, clampY(y + 1), z)] -
            streamfunction[index(x, clampY(y - 1), z)]);
        const float dPsiDx = 0.5f * (
            streamfunction[index(clampX(x + 1), y, z)] -
            streamfunction[index(clampX(x - 1), y, z)]);
        vortexFlow[index(x, y, z)] = {dPsiDy, -dPsiDx, 0.0f};
      }

  std::vector<float> vortexSemiLagrangianDensity;
  std::vector<float> vortexBfeccDensity;
  std::vector<float> vortexLimitedBfeccDensity;
  const auto uniformSemiLagrangian = advectWithMode(
      uniformFlow, 0, nullptr);
  const auto vortexSemiLagrangian = advectWithMode(
      vortexFlow, 0, &vortexSemiLagrangianDensity);
  const auto vortexBfecc = advectWithMode(vortexFlow, 1, &vortexBfeccDensity);
  const auto vortexLimitedBfecc = advectWithMode(
      vortexFlow, 2, &vortexLimitedBfeccDensity);
  const double uniformDivergence = gridDivergenceRms(uniformFlow,
      resolution.width, resolution.height, resolution.depth);
  const double vortexDivergence = gridDivergenceRms(vortexFlow,
      resolution.width, resolution.height, resolution.depth);
  const auto report = [&](const char* label,
                          const std::array<double, 4>& stats) {
    GTEST_LOG_(INFO) << label << " massRetention=" << stats[0]
        << " min/max=" << stats[1] << '/' << stats[2]
        << " highFrequencyRms=" << stats[3];
  };
  report("uniform semi-Lagrangian", uniformSemiLagrangian);
  report("vortex semi-Lagrangian", vortexSemiLagrangian);
  report("vortex unlimited BFECC", vortexBfecc);
  report("vortex limited BFECC", vortexLimitedBfecc);
  const auto renderFinalDensity = [&](const std::vector<float>& density,
                                      const char* filename) {
    PyroFrameSnapshot snapshot = sampler.snapshot();
    snapshot.density = {std::span<const float>(density), resolution};
    auto renderer = makeRenderer(snapshot, domain, {0.0f, 1.0f, 3.2f});
    renderer.settings.stepSize = 0.025f;
    renderer.settings.maxSteps = 120;
    renderer.settings.densityScale = 2.2f;
    renderer.settings.lightIntensity = 4.0f;
    renderer.settings.phaseAnisotropy = 0.35f;
    renderer.settings.multipleScatteringStrength = 0.1f;
    renderer.settings.shadowIntensity = 0.0f;
    const auto image = renderer.render(128, 128);
    const auto imageStats = measure(image);
    GTEST_LOG_(INFO) << filename << " rendered coverage="
        << imageStats.occupiedPixels << " luminance="
        << imageStats.luminanceSum << " lumaStdDev="
        << imageStats.luminanceStdDev;
    saveIfRequested(image, filename);
  };
  renderFinalDensity(vortexSemiLagrangianDensity,
      "transport_vortex_semi_lagrangian.ppm");
  renderFinalDensity(vortexBfeccDensity, "transport_vortex_bfecc.ppm");
  renderFinalDensity(vortexLimitedBfeccDensity,
      "transport_vortex_limited_bfecc.ppm");
  GTEST_LOG_(INFO) << "discrete divergence RMS (uniform/vortex)="
      << uniformDivergence << '/' << vortexDivergence
      << " after " << steps << " fixed-flow steps";
  EXPECT_NEAR(uniformDivergence, 0.0, 1e-6);
  EXPECT_NEAR(vortexDivergence, 0.0, 1e-6);
  EXPECT_GT(uniformSemiLagrangian[0], 0.0);
  EXPECT_GT(vortexSemiLagrangian[0], 0.0);
  EXPECT_TRUE(std::isfinite(vortexBfecc[0]));
  EXPECT_TRUE(std::isfinite(vortexLimitedBfecc[0]));
}
