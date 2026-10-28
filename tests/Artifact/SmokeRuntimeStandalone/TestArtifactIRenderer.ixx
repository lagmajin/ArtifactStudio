module;

#include <utility>
#include <cstddef>
#include <QMatrix4x4>
#include <QRectF>

export module Artifact.Render.IRenderer;

import Color.Float;
import Graphics.ParticleData;

export namespace Artifact {

struct SmokeVolumeLook {
  ArtifactCore::FloatColor denseColor =
      ArtifactCore::FloatColor(0.42f, 0.72f, 1.0f, 1.0f);
  float densityRampStart = 0.12f;
  float densityRampEnd = 0.45f;
  float densityRampStrength = 0.0f;
  float scatteringBrightness = 1.0f;
  float shadowDensity = 0.8f;
  float detailFrequency = 0.42f;
  float detailContrast = 0.55f;
};

class ArtifactIRenderer {
public:
  void drawParticles(ArtifactCore::ParticleRenderData&& data) {
    auto recycledParticles = std::move(lastParticleData_.particles);
    const std::size_t submittedCapacity = data.particles.capacity();
    if (recycledParticles.capacity() < submittedCapacity) {
      recycledParticles.reserve(submittedCapacity);
    }
    lastParticleData_ = std::move(data);
    data.particles = std::move(recycledParticles);
    ++particleDrawCount_;
  }

  const ArtifactCore::ParticleRenderData& lastParticleData() const noexcept {
    return lastParticleData_;
  }

  int particleDrawCount() const noexcept { return particleDrawCount_; }

  bool drawSmokeVolumeField(
      const float*, const float*, int, int, const QRectF&,
      const QMatrix4x4&, const ArtifactCore::FloatColor&, float, float,
      float, const SmokeVolumeLook& = {}) noexcept {
    return false;
  }

private:
  ArtifactCore::ParticleRenderData lastParticleData_;
  int particleDrawCount_ = 0;
};

} // namespace Artifact
