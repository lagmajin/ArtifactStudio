#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

import Procedural3DGenerators;

using namespace ArtifactCore;

namespace {

PathTubeSettings makeTubeSettings() {
    PathTubeSettings settings;
    settings.pathSource = ProceduralPathSource::Parametric;
    settings.profile = ProceduralPathProfile::Tube;
    settings.pathSamples = 32;
    settings.sides = 4;
    settings.radius = 0.15f;
    settings.noiseAmplitude = 0.0f;
    settings.quality = Procedural3DQuality::Draft;
    return settings;
}

void zBounds(const Procedural3DMeshData& mesh, float& outMin, float& outMax) {
    ASSERT_FALSE(mesh.vertices.empty());
    outMin = mesh.vertices[0].pz;
    outMax = mesh.vertices[0].pz;
    for (const auto& v : mesh.vertices) {
        outMin = std::min(outMin, v.pz);
        outMax = std::max(outMax, v.pz);
    }
}

} // namespace

TEST(PathTubeTrimTest, FullRangeIsDefault) {
    const auto full = Procedural3DGenerators::generatePathTube(makeTubeSettings());
    ASSERT_FALSE(full.vertices.empty());
    PathTubeSettings trimmed = makeTubeSettings();
    trimmed.trimStart = 0.0f;
    trimmed.trimEnd = 1.0f;
    const auto same = Procedural3DGenerators::generatePathTube(trimmed);
    EXPECT_EQ(same.vertices.size(), full.vertices.size());
}

TEST(PathTubeTrimTest, SubRangeShrinksBounds) {
    const auto full = Procedural3DGenerators::generatePathTube(makeTubeSettings());
    PathTubeSettings trimmed = makeTubeSettings();
    trimmed.trimStart = 0.25f;
    trimmed.trimEnd = 0.75f;
    const auto sub = Procedural3DGenerators::generatePathTube(trimmed);
    ASSERT_FALSE(sub.vertices.empty());
    EXPECT_EQ(sub.vertices.size(), full.vertices.size());
    float fullMin = 0.0f, fullMax = 0.0f, subMin = 0.0f, subMax = 0.0f;
    zBounds(full, fullMin, fullMax);
    zBounds(sub, subMin, subMax);
    // Parametric path z spans (t - 0.5) * 1.2 over the trimmed range.
    EXPECT_GT(subMin, fullMin);
    EXPECT_LT(subMax, fullMax);
}

TEST(PathTubeTrimTest, InvertedTrimYieldsEmptyMesh) {
    PathTubeSettings trimmed = makeTubeSettings();
    trimmed.trimStart = 0.75f;
    trimmed.trimEnd = 0.25f;
    const auto empty = Procedural3DGenerators::generatePathTube(trimmed);
    EXPECT_TRUE(empty.vertices.empty());
}

TEST(PathTubeTrimTest, OutOfRangeTrimIsClamped) {
    PathTubeSettings trimmed = makeTubeSettings();
    trimmed.trimStart = -2.0f;
    trimmed.trimEnd = 5.0f;
    const auto full = Procedural3DGenerators::generatePathTube(makeTubeSettings());
    const auto clamped = Procedural3DGenerators::generatePathTube(trimmed);
    EXPECT_EQ(clamped.vertices.size(), full.vertices.size());
}
