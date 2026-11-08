#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
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

TEST(PathTubeTrimTest,
     TrimMapsEveryRingAcrossRequestedRangeAndKeepsAdjacentRingTopology) {
    PathTubeSettings settings = makeTubeSettings();
    settings.trimStart = 0.2f;
    settings.trimEnd = 0.8f;
    const auto mesh = Procedural3DGenerators::generatePathTube(settings);

    constexpr std::size_t sides = 3;
    ASSERT_EQ(mesh.vertices.size() % sides, 0u);
    const std::size_t ringCount = mesh.vertices.size() / sides;
    ASSERT_GT(ringCount, 1u);
    ASSERT_EQ(mesh.indices.size(), (ringCount - 1u) * sides * 6u);

    for (std::size_t ring = 0; ring < ringCount; ++ring) {
        const float amount = static_cast<float>(ring) /
                             static_cast<float>(ringCount - 1u);
        const float expectedPathParameter =
            settings.trimStart + amount * (settings.trimEnd - settings.trimStart);
        for (std::size_t side = 0; side < sides; ++side) {
            EXPECT_NEAR(mesh.vertices[ring * sides + side].v,
                        expectedPathParameter, 1.0e-6f)
                << "ring=" << ring << " side=" << side;
        }
    }

    for (std::size_t ring = 0; ring + 1u < ringCount; ++ring) {
        const std::size_t firstVertex = ring * sides;
        const std::size_t afterNextRing = (ring + 2u) * sides;
        const std::size_t firstIndex = ring * sides * 6u;
        const std::size_t indexEnd = firstIndex + sides * 6u;
        for (std::size_t index = firstIndex; index < indexEnd; ++index) {
            const std::uint32_t vertex = mesh.indices[index];
            EXPECT_LT(vertex, mesh.vertices.size()) << "index=" << index;
            EXPECT_GE(vertex, firstVertex) << "ring=" << ring;
            EXPECT_LT(vertex, afterNextRing) << "ring=" << ring;
        }
    }
}

TEST(PathTubeTrimTest, TrimmedRingsKeepCenterlineAndEvaluateTaperAtPathParameter) {
    PathTubeSettings settings = makeTubeSettings();
    settings.trimStart = 0.2f;
    settings.trimEnd = 0.8f;
    settings.taperStart = 0.5f;
    settings.taperEnd = 1.5f;
    const auto mesh = Procedural3DGenerators::generatePathTube(settings);

    constexpr std::size_t sides = 3;
    ASSERT_EQ(mesh.vertices.size() % sides, 0u);
    const std::size_t ringCount = mesh.vertices.size() / sides;
    ASSERT_GT(ringCount, 2u);

    for (const std::size_t ring : {std::size_t{0}, ringCount / 2,
                                   ringCount - 1}) {
        const float ringAmount = static_cast<float>(ring) /
                                 static_cast<float>(ringCount - 1u);
        const float pathParameter = settings.trimStart + ringAmount *
            (settings.trimEnd - settings.trimStart);
        std::array<double, 3> center{};
        for (std::size_t side = 0; side < sides; ++side) {
            const auto& vertex = mesh.vertices[ring * sides + side];
            center[0] += vertex.px / sides;
            center[1] += vertex.py / sides;
            center[2] += vertex.pz / sides;
        }

        EXPECT_NEAR(center[2], (pathParameter - 0.5f) * 1.2f, 1.0e-5)
            << "ring=" << ring;
        const double expectedRadius = settings.radius *
            (settings.taperStart +
             (settings.taperEnd - settings.taperStart) * pathParameter);
        for (std::size_t side = 0; side < sides; ++side) {
            const auto& vertex = mesh.vertices[ring * sides + side];
            const double dx = vertex.px - center[0];
            const double dy = vertex.py - center[1];
            const double dz = vertex.pz - center[2];
            EXPECT_NEAR(std::sqrt(dx * dx + dy * dy + dz * dz), expectedRadius,
                        1.0e-5)
                << "ring=" << ring << " side=" << side;
        }
    }
}
