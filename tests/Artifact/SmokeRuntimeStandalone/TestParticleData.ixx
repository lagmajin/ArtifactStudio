module;

#include <cstdint>
#include <utility>
#include <vector>

export module Graphics.ParticleData;

export namespace ArtifactCore {

struct ParticleVertex {
  float px = 0.0f;
  float py = 0.0f;
  float pz = 0.0f;
  float pad0 = 0.0f;
  float vx = 0.0f;
  float vy = 0.0f;
  float vz = 0.0f;
  float pad1 = 0.0f;
  float r = 1.0f;
  float g = 1.0f;
  float b = 1.0f;
  float a = 1.0f;
  float size = 1.0f;
  float stretch = 1.0f;
  float rotation = 0.0f;
  float age = 0.0f;
  float lifetime = 1.0f;
  int spriteFrame = 0;
  int spriteRows = 1;
  int spriteCols = 1;
  float ppx = 0.0f;
  float ppy = 0.0f;
  float ppz = 0.0f;
  float pad2 = 0.0f;
};
static_assert(sizeof(ParticleVertex) == 96,
              "test ParticleVertex layout must match the production ABI");

enum class ParticleBlendPolicy : std::uint32_t {
  Additive = 0,
  Subtractive = 1,
  Alpha = 2,
  Screen = 3,
  Multiply = 4
};

enum class ParticleBillboardPolicy : std::uint32_t {
  None = 0,
  ScreenAligned = 1,
  ViewPlane = 2,
  VelocityAligned = 3
};

enum class ParticleFalloffMode : std::uint32_t {
  SoftCircle = 0,
  GaussianDensity = 1
};

struct ParticleRenderOptions {
  ParticleBlendPolicy blend = ParticleBlendPolicy::Additive;
  ParticleBillboardPolicy billboard = ParticleBillboardPolicy::ScreenAligned;
  bool depthTest = false;
  bool depthWrite = false;
  ParticleFalloffMode falloff = ParticleFalloffMode::SoftCircle;
};

struct ParticleRenderData {
  std::vector<ParticleVertex> particles;
  std::int64_t frameNumber = 0;
  ParticleRenderOptions options;
};

} // namespace ArtifactCore
