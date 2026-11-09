#include <gtest/gtest.h>

#include <QPointF>

#include <algorithm>
#include <array>
#include <cstddef>
#include <tuple>
#include <vector>

import Core.Mask.RotoMask;

using namespace ArtifactCore;

TEST(RotoMaskContractTest, VertexIdsRemainUniqueAfterReverseAndSubsequentEdits)
{
    RotoMask mask;
    const auto first = mask.addVertex(QPointF(1.0, 2.0), QPointF(-1.0, 0.0),
                                      QPointF(1.0, 0.0));
    const auto second = mask.addVertex(QPointF(5.0, 2.0), QPointF(-1.0, 0.0),
                                       QPointF(1.0, 0.0));
    const auto third = mask.addVertex(QPointF(5.0, 6.0));

    mask.reverse();
    const auto reversedIds = mask.vertexIDs();
    ASSERT_EQ(reversedIds.size(), 3u);
    EXPECT_NE(reversedIds[0], third);
    EXPECT_NE(reversedIds[1], second);
    EXPECT_NE(reversedIds[2], first);
    EXPECT_EQ(mask.vertexPosition(reversedIds[0], 0.0), QPointF(5.0, 6.0));
    EXPECT_EQ(mask.vertexPosition(reversedIds[1], 0.0), QPointF(5.0, 2.0));
    EXPECT_EQ(mask.vertexPosition(reversedIds[2], 0.0), QPointF(1.0, 2.0));

    const auto addedAfterReverse = mask.addVertex(QPointF(9.0, 10.0));
    const auto idsAfterAdd = mask.vertexIDs();
    EXPECT_EQ(idsAfterAdd.size(), 4u);
    EXPECT_EQ(std::count(idsAfterAdd.begin(), idsAfterAdd.end(), addedAfterReverse), 1);
    EXPECT_EQ(mask.vertexPosition(first, 0.0), QPointF(1.0, 2.0));
    EXPECT_EQ(mask.vertexPosition(second, 0.0), QPointF(5.0, 2.0));
    EXPECT_EQ(mask.vertexPosition(third, 0.0), QPointF(5.0, 6.0));
}

TEST(RotoMaskContractTest, ReversePreservesAnimatedGeometryAndSwapsTangentsAtEveryKey)
{
    RotoMask mask;
    const auto first = mask.addVertex(QPointF(0.0, 0.0), QPointF(-1.0, 0.0),
                                      QPointF(2.0, 0.0));
    const auto second = mask.addVertex(QPointF(10.0, 0.0), QPointF(-2.0, 1.0),
                                       QPointF(1.0, 1.0));
    const auto third = mask.addVertex(QPointF(10.0, 10.0), QPointF(0.0, -1.0),
                                      QPointF(0.0, 2.0));
    mask.setVertexPosition(first, QPointF(1.0, 2.0), 10.0);
    mask.setVertexPosition(second, QPointF(13.0, 1.0), 10.0);
    mask.setVertexPosition(third, QPointF(12.0, 14.0), 10.0);
    mask.setInTangent(first, QPointF(-3.0, 1.0), 10.0);
    mask.setOutTangent(first, QPointF(4.0, 2.0), 10.0);
    mask.setInTangent(second, QPointF(-1.0, 3.0), 10.0);
    mask.setOutTangent(second, QPointF(2.0, 4.0), 10.0);
    mask.setInTangent(third, QPointF(1.0, -2.0), 10.0);
    mask.setOutTangent(third, QPointF(3.0, 1.0), 10.0);
    for (const auto vertex : {first, second, third}) {
        mask.setPositionInterpolation(vertex, RotoMask::Interpolation::Linear);
    }

    const auto originalAtStart = mask.sampleVertices(0.0);
    const auto originalAtMiddle = mask.sampleVertices(5.0);
    const auto originalAtEnd = mask.sampleVertices(10.0);
    mask.reverse();

    const auto expectReversed = [&mask](double time,
                                        const std::vector<RotoVertex>& original) {
        const auto reversed = mask.sampleVertices(time);
        ASSERT_EQ(reversed.size(), original.size());
        for (std::size_t index = 0; index < original.size(); ++index) {
            const auto& before = original[original.size() - index - 1];
            EXPECT_EQ(reversed[index].position, before.position);
            EXPECT_EQ(reversed[index].inTangent, before.outTangent);
            EXPECT_EQ(reversed[index].outTangent, before.inTangent);
        }
    };
    expectReversed(0.0, originalAtStart);
    expectReversed(5.0, originalAtMiddle);
    expectReversed(10.0, originalAtEnd);
}

TEST(RotoMaskContractTest, AnimationUsesSortedTimesAndClampsOutsideTheKeyframeRange)
{
    RotoMask mask;
    const auto vertex = mask.addVertex(QPointF(0.0, 0.0));
    mask.setVertexPosition(vertex, QPointF(20.0, 10.0), 10.0);
    mask.setVertexPosition(vertex, QPointF(0.0, 0.0), 0.0);
    mask.setPositionInterpolation(vertex, RotoMask::Interpolation::Linear);

    EXPECT_TRUE(mask.isAnimated());
    EXPECT_EQ(mask.startTime(), 0.0);
    EXPECT_EQ(mask.endTime(), 10.0);
    EXPECT_EQ(mask.keyframeTimes(), (std::vector<double>{0.0, 10.0}));
    EXPECT_EQ(mask.vertexPosition(vertex, -1.0), QPointF(0.0, 0.0));
    EXPECT_EQ(mask.vertexPosition(vertex, 5.0), QPointF(10.0, 5.0));
    EXPECT_EQ(mask.vertexPosition(vertex, 11.0), QPointF(20.0, 10.0));

    mask.removeKeyframe(10.0);
    EXPECT_FALSE(mask.isAnimated());
    EXPECT_EQ(mask.vertexPosition(vertex, 10.0), QPointF(0.0, 0.0));
}

TEST(RotoMaskContractTest,
     AddingStaticVertexAfterAnimatedSnapshotsPreservesExistingTracks)
{
    RotoMask mask;
    const auto animatedVertex = mask.addVertex(QPointF(0.0, 0.0));
    mask.setVertexPosition(animatedVertex, QPointF(0.0, 0.0), 0.0);
    mask.setVertexPosition(animatedVertex, QPointF(20.0, 10.0), 10.0);
    mask.setPositionInterpolation(animatedVertex, RotoMask::Interpolation::Linear);
    mask.setOpacity(0.2f, 0.0);
    mask.setOpacity(0.8f, 10.0);
    mask.addKeyframe(5.0);

    const auto staticVertex = mask.addVertex(QPointF(7.0, 3.0),
                                              QPointF(-1.0, 0.5),
                                              QPointF(2.0, -0.5));

    EXPECT_EQ(mask.keyframeTimes(), (std::vector<double>{0.0, 5.0, 10.0}));
    EXPECT_EQ(mask.vertexIDs(), (std::vector<RotoMask::VertexID>{
        animatedVertex, staticVertex}));
    EXPECT_EQ(mask.sampleVertices(0.0).size(), 2u);

    for (const auto [time, expectedPosition, expectedOpacity] :
         std::array<std::tuple<double, QPointF, float>, 3>{{
             {0.0, QPointF(0.0, 0.0), 0.2f},
             {5.0, QPointF(10.0, 5.0), 0.5f},
             {10.0, QPointF(20.0, 10.0), 0.8f},
         }}) {
        const auto vertices = mask.sampleVertices(time);
        ASSERT_EQ(vertices.size(), 2u);
        EXPECT_EQ(vertices[0].position, expectedPosition);
        EXPECT_EQ(vertices[1].position, QPointF(7.0, 3.0));
        EXPECT_EQ(vertices[1].inTangent, QPointF(-1.0, 0.5));
        EXPECT_EQ(vertices[1].outTangent, QPointF(2.0, -0.5));
        EXPECT_FLOAT_EQ(mask.opacity(time), expectedOpacity);
    }
}

TEST(RotoMaskContractTest, StepInterpolationTakesTheNewValueAtAnInteriorKeyframe)
{
    RotoMask mask;
    const auto vertex = mask.addVertex(QPointF(0.0, 0.0));
    mask.setVertexPosition(vertex, QPointF(10.0, 20.0), 10.0);
    mask.setVertexPosition(vertex, QPointF(30.0, 40.0), 20.0);
    mask.setPositionInterpolation(vertex, RotoMask::Interpolation::Step);

    EXPECT_EQ(mask.vertexPosition(vertex, 9.999), QPointF(0.0, 0.0));
    EXPECT_EQ(mask.vertexPosition(vertex, 10.0), QPointF(10.0, 20.0));
    EXPECT_EQ(mask.vertexPosition(vertex, 10.001), QPointF(10.0, 20.0));
    EXPECT_EQ(mask.vertexPosition(vertex, 19.999), QPointF(10.0, 20.0));
    EXPECT_EQ(mask.vertexPosition(vertex, 20.0), QPointF(30.0, 40.0));
}

TEST(RotoMaskContractTest, AnimatedAttributesClampAndInterpolateAtTheirOwnTimes)
{
    RotoMask mask;
    mask.setOpacity(-2.0f, 0.0);
    mask.setOpacity(2.0f, 10.0);
    mask.setFeather(-3.0f, 0.0);
    mask.setFeather(8.0f, 10.0);
    mask.setExpansion(-4.0f, 0.0);
    mask.setExpansion(6.0f, 10.0);

    EXPECT_FLOAT_EQ(mask.opacity(0.0), 0.0f);
    EXPECT_FLOAT_EQ(mask.opacity(5.0), 0.5f);
    EXPECT_FLOAT_EQ(mask.opacity(10.0), 1.0f);
    EXPECT_FLOAT_EQ(mask.feather(0.0), 0.0f);
    EXPECT_FLOAT_EQ(mask.feather(5.0), 4.0f);
    EXPECT_FLOAT_EQ(mask.expansion(5.0), 1.0f);

    mask.setMode(RotoMaskMode::Subtract);
    mask.setClosed(false);
    mask.setInverted(true);
    mask.setName(QStringLiteral("Mask A"));
    EXPECT_EQ(mask.mode(), RotoMaskMode::Subtract);
    EXPECT_FALSE(mask.isClosed());
    EXPECT_TRUE(mask.isInverted());
    EXPECT_EQ(mask.name(), QStringLiteral("Mask A"));
}

TEST(RotoMaskContractTest, AddKeyframeSnapshotsInterpolatedValuesAcrossAllProperties)
{
    RotoMask mask;
    const auto vertex = mask.addVertex(QPointF(0.0, 0.0));
    mask.setVertexPosition(vertex, QPointF(0.0, 0.0), 0.0);
    mask.setVertexPosition(vertex, QPointF(20.0, 10.0), 10.0);
    mask.setPositionInterpolation(vertex, RotoMask::Interpolation::Linear);
    mask.setOpacity(0.2f, 0.0);
    mask.setOpacity(0.8f, 10.0);
    mask.setFeather(2.0f, -5.0);
    mask.setFeather(12.0f, 15.0);
    mask.setExpansion(-4.0f, 0.0);
    mask.setExpansion(6.0f, 10.0);

    mask.addKeyframe(5.0);

    EXPECT_EQ(mask.vertexPosition(vertex, 5.0), QPointF(10.0, 5.0));
    EXPECT_FLOAT_EQ(mask.opacity(5.0), 0.5f);
    EXPECT_FLOAT_EQ(mask.feather(5.0), 7.0f);
    EXPECT_FLOAT_EQ(mask.expansion(5.0), 1.0f);
    EXPECT_EQ(mask.keyframeTimes(),
              (std::vector<double>{-5.0, 0.0, 5.0, 10.0, 15.0}));

    // Existing keys after the inserted snapshot remain intact.
    EXPECT_EQ(mask.vertexPosition(vertex, 10.0), QPointF(20.0, 10.0));
    EXPECT_FLOAT_EQ(mask.opacity(10.0), 0.8f);
    EXPECT_FLOAT_EQ(mask.feather(15.0), 12.0f);
    EXPECT_FLOAT_EQ(mask.expansion(10.0), 6.0f);
}

TEST(RotoMaskContractTest, CopyCloneAndShiftedKeyframeCopyAreIndependent)
{
    RotoMask source;
    const auto sourceVertex = source.addVertex(QPointF(2.0, 3.0),
                                               QPointF(-1.0, 0.0),
                                               QPointF(1.0, 0.0));
    source.setVertexPosition(sourceVertex, QPointF(2.0, 3.0), 0.0);
    source.setVertexPosition(sourceVertex, QPointF(12.0, 13.0), 10.0);
    source.setInTangent(sourceVertex, QPointF(-1.0, 0.0), 0.0);
    source.setInTangent(sourceVertex, QPointF(-3.0, 2.0), 10.0);
    source.setOutTangent(sourceVertex, QPointF(1.0, 0.0), 0.0);
    source.setOutTangent(sourceVertex, QPointF(4.0, 2.0), 10.0);
    source.setOpacity(0.25f, 0.0);
    source.setOpacity(0.75f, 10.0);
    source.setFeather(2.0f, 0.0);
    source.setFeather(10.0f, 10.0);
    source.setExpansion(-2.0f, 0.0);
    source.setExpansion(4.0f, 10.0);
    source.setName(QStringLiteral("Source"));
    source.setMode(RotoMaskMode::Difference);

    RotoMask copied = source;
    RotoMask cloned = source.clone();
    RotoMask shifted;
    shifted.copyKeyframesFrom(source, 5.0);

    const auto copiedVertex = copied.vertexIDs().front();
    const auto clonedVertex = cloned.vertexIDs().front();
    const auto shiftedVertex = shifted.vertexIDs().front();
    EXPECT_EQ(copied.vertexPosition(copiedVertex, 5.0), QPointF(7.0, 8.0));
    EXPECT_EQ(cloned.vertexPosition(clonedVertex, 5.0), QPointF(7.0, 8.0));
    EXPECT_EQ(shifted.vertexPosition(shiftedVertex, 10.0), QPointF(7.0, 8.0));
    EXPECT_EQ(shifted.inTangent(shiftedVertex, 10.0), QPointF(-2.0, 1.0));
    EXPECT_EQ(shifted.outTangent(shiftedVertex, 10.0), QPointF(2.5, 1.0));
    EXPECT_FLOAT_EQ(shifted.opacity(10.0), 0.5f);
    EXPECT_FLOAT_EQ(shifted.feather(10.0), 6.0f);
    EXPECT_FLOAT_EQ(shifted.expansion(10.0), 1.0f);
    EXPECT_EQ(shifted.startTime(), 5.0);
    EXPECT_EQ(shifted.endTime(), 15.0);
    EXPECT_EQ(shifted.name(), QStringLiteral("Source"));
    EXPECT_EQ(shifted.mode(), RotoMaskMode::Difference);

    copied.setVertexPosition(copiedVertex, QPointF(99.0, 99.0), 5.0);
    EXPECT_EQ(source.vertexPosition(sourceVertex, 5.0), QPointF(7.0, 8.0));
    EXPECT_EQ(cloned.vertexPosition(clonedVertex, 5.0), QPointF(7.0, 8.0));

    shifted.removeKeyframe(15.0);
    EXPECT_EQ(shifted.keyframeTimes(), (std::vector<double>{5.0}));
    EXPECT_EQ(shifted.endTime(), 5.0);
    EXPECT_EQ(shifted.vertexPosition(shiftedVertex, 15.0), QPointF(2.0, 3.0));
    EXPECT_EQ(shifted.inTangent(shiftedVertex, 15.0), QPointF(-1.0, 0.0));
    EXPECT_EQ(shifted.outTangent(shiftedVertex, 15.0), QPointF(1.0, 0.0));
    EXPECT_FLOAT_EQ(shifted.opacity(15.0), 0.25f);
    EXPECT_FLOAT_EQ(shifted.feather(15.0), 2.0f);
    EXPECT_FLOAT_EQ(shifted.expansion(15.0), -2.0f);
}

TEST(RotoMaskContractTest, RasterizeAppliesOpacityToTheInvertedMask)
{
    RotoMask mask;
    mask.addVertex(QPointF(1.0, 1.0));
    mask.addVertex(QPointF(6.0, 1.0));
    mask.addVertex(QPointF(6.0, 6.0));
    mask.addVertex(QPointF(1.0, 6.0));
    mask.setOpacity(0.5f, 0.0);
    mask.setInverted(true);

    std::vector<float> pixels(8u * 8u, -1.0f);
    mask.rasterize(0.0, 8, 8, pixels.data());

    EXPECT_FLOAT_EQ(pixels[3u * 8u + 3u], 0.0f);
    EXPECT_FLOAT_EQ(pixels[0], 0.5f);
    EXPECT_FLOAT_EQ(pixels[7u * 8u + 7u], 0.5f);
}

TEST(RotoMaskContractTest,
     RasterizeComposesFeatherExpansionOpacityAndInversionInOrder)
{
    constexpr int width = 7;
    constexpr int height = 7;
    constexpr int pixelCount = width * height;
    RotoMask mask;
    mask.addVertex(QPointF(1.0, 1.0));
    mask.addVertex(QPointF(5.0, 1.0));
    mask.addVertex(QPointF(5.0, 5.0));
    mask.addVertex(QPointF(1.0, 5.0));
    mask.setFeather(1.0f, 0.0);
    mask.setExpansion(1.0f, 0.0);
    mask.setOpacity(0.5f, 0.0);
    mask.setInverted(true);

    std::vector<float> actual(pixelCount, -1.0f);
    mask.rasterize(0.0, width, height, actual.data());

    // The rectangle covers integer samples [1,5) in each dimension.
    std::array<float, pixelCount> polygon{};
    for (int y = 1; y < 5; ++y) {
        for (int x = 1; x < 5; ++x) {
            polygon[static_cast<std::size_t>(y * width + x)] = 1.0f;
        }
    }

    // Independent separable 3x3 box feather, with edge clamping.
    std::array<float, pixelCount> horizontal{};
    std::array<float, pixelCount> feathered{};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            float sum = 0.0f;
            for (int dx = -1; dx <= 1; ++dx) {
                const int sampleX = std::clamp(x + dx, 0, width - 1);
                sum += polygon[static_cast<std::size_t>(y * width + sampleX)];
            }
            horizontal[static_cast<std::size_t>(y * width + x)] = sum / 3.0f;
        }
    }
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            float sum = 0.0f;
            for (int dy = -1; dy <= 1; ++dy) {
                const int sampleY = std::clamp(y + dy, 0, height - 1);
                sum += horizontal[static_cast<std::size_t>(sampleY * width + x)];
            }
            feathered[static_cast<std::size_t>(y * width + x)] = sum / 3.0f;
        }
    }

    // Expansion is a square-neighborhood dilation applied after feathering.
    std::array<float, pixelCount> expected{};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            float value = 0.0f;
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    const int sampleX = std::clamp(x + dx, 0, width - 1);
                    const int sampleY = std::clamp(y + dy, 0, height - 1);
                    value = std::max(value, feathered[
                        static_cast<std::size_t>(sampleY * width + sampleX)]);
                }
            }
            expected[static_cast<std::size_t>(y * width + x)] = 1.0f - value * 0.5f;
        }
    }

    ASSERT_EQ(actual.size(), expected.size());
    for (std::size_t pixel = 0; pixel < expected.size(); ++pixel) {
        EXPECT_NEAR(actual[pixel], expected[pixel], 1.0e-6f)
            << "pixel=" << pixel;
    }
}
