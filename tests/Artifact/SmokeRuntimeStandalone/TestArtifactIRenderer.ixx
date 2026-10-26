module;

#include <utility>
#include <cstddef>

export module Artifact.Render.IRenderer;

import Graphics.ParticleData;

export namespace Artifact {

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

private:
  ArtifactCore::ParticleRenderData lastParticleData_;
  int particleDrawCount_ = 0;
};

} // namespace Artifact
