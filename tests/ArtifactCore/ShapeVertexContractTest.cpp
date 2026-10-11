#include <gtest/gtest.h>

import ShapeVertex;

using ArtifactCore::ShapeVertex;

namespace {
void expectVec(ShapeVertex::Vec2 actual, float x, float y)
{
    EXPECT_FLOAT_EQ(actual.x, x);
    EXPECT_FLOAT_EQ(actual.y, y);
}
}

TEST(ShapeVertexContractTest, DefaultVertexHasZeroPositionAndHandles)
{
    const ShapeVertex vertex;
    expectVec(vertex.position(), 0.0f, 0.0f);
    expectVec(vertex.inHandle(), 0.0f, 0.0f);
    expectVec(vertex.outHandle(), 0.0f, 0.0f);
    EXPECT_FALSE(vertex.isCorner());
}

TEST(ShapeVertexContractTest, PositionConstructorSetsOnlyPosition)
{
    const ShapeVertex vertex({3.0f, -2.0f});
    expectVec(vertex.position(), 3.0f, -2.0f);
    expectVec(vertex.inHandle(), 0.0f, 0.0f);
    expectVec(vertex.outHandle(), 0.0f, 0.0f);
}

TEST(ShapeVertexContractTest, VecAdditionIsComponentWise)
{
    const ShapeVertex::Vec2 result = ShapeVertex::Vec2{2.0f, -4.0f} + ShapeVertex::Vec2{5.0f, 3.0f};
    expectVec(result, 7.0f, -1.0f);
}

TEST(ShapeVertexContractTest, VecSubtractionIsComponentWise)
{
    const ShapeVertex::Vec2 result = ShapeVertex::Vec2{2.0f, -4.0f} - ShapeVertex::Vec2{5.0f, 3.0f};
    expectVec(result, -3.0f, -7.0f);
}

TEST(ShapeVertexContractTest, ScalarMultiplicationScalesBothComponents)
{
    const ShapeVertex::Vec2 result = ShapeVertex::Vec2{2.5f, -3.0f} * 2.0f;
    expectVec(result, 5.0f, -6.0f);
}

TEST(ShapeVertexContractTest, LengthUsesEuclideanMagnitude)
{
    EXPECT_FLOAT_EQ((ShapeVertex::Vec2{3.0f, 4.0f}.length()), 5.0f);
    EXPECT_FLOAT_EQ(ShapeVertex::Vec2{}.length(), 0.0f);
}

TEST(ShapeVertexContractTest, NormalizeProducesUnitLength)
{
    ShapeVertex::Vec2 vector{3.0f, 4.0f};
    vector.normalize();
    EXPECT_NEAR(vector.x, 0.6f, 1e-6f);
    EXPECT_NEAR(vector.y, 0.8f, 1e-6f);
    EXPECT_NEAR(vector.length(), 1.0f, 1e-6f);
}

TEST(ShapeVertexContractTest, NormalizeLeavesZeroVectorUnchanged)
{
    ShapeVertex::Vec2 vector{};
    vector.normalize();
    expectVec(vector, 0.0f, 0.0f);
}

TEST(ShapeVertexContractTest, NormalizeLeavesVectorsAtThresholdUnchanged)
{
    ShapeVertex::Vec2 vector{1.0e-6f, 0.0f};
    vector.normalize();
    expectVec(vector, 1.0e-6f, 0.0f);
}

TEST(ShapeVertexContractTest, NormalizeBranchesImmediatelyAroundLengthThreshold)
{
    ShapeVertex::Vec2 below{0.999e-6f, 0.0f};
    ShapeVertex::Vec2 above{1.001e-6f, 0.0f};

    below.normalize();
    above.normalize();

    EXPECT_FLOAT_EQ(below.x, 0.999e-6f);
    EXPECT_FLOAT_EQ(below.y, 0.0f);
    EXPECT_FLOAT_EQ(above.x, 1.0f);
    EXPECT_FLOAT_EQ(above.y, 0.0f);
}

TEST(ShapeVertexContractTest, SettersReplacePositionAndHandles)
{
    ShapeVertex vertex({1.0f, 2.0f});
    vertex.setPosition({3.0f, 4.0f});
    vertex.setInHandle({-2.0f, 1.0f});
    vertex.setOutHandle({5.0f, -3.0f});
    expectVec(vertex.position(), 3.0f, 4.0f);
    expectVec(vertex.inHandle(), -2.0f, 1.0f);
    expectVec(vertex.outHandle(), 5.0f, -3.0f);
}

TEST(ShapeVertexContractTest, HandleWorldPositionsAddLocalOffsets)
{
    ShapeVertex vertex({10.0f, 20.0f});
    vertex.setInHandle({-2.0f, 3.0f});
    vertex.setOutHandle({4.0f, -5.0f});
    expectVec(vertex.inHandleWorld(), 8.0f, 23.0f);
    expectVec(vertex.outHandleWorld(), 14.0f, 15.0f);
}

TEST(ShapeVertexContractTest, MirrorHandlesNegatesIncomingOffset)
{
    ShapeVertex vertex;
    vertex.setInHandle({2.0f, -3.0f});
    vertex.setOutHandle({7.0f, 8.0f});
    vertex.mirrorHandles();
    expectVec(vertex.outHandle(), -2.0f, 3.0f);
}

TEST(ShapeVertexContractTest, MirrorHandlesCanPreserveExistingOutgoingLength)
{
    ShapeVertex vertex;
    vertex.setInHandle({3.0f, 4.0f});
    vertex.setOutHandle({0.0f, 2.0f});

    vertex.mirrorHandles(false);

    expectVec(vertex.outHandle(), -1.2f, -1.6f);
    EXPECT_NEAR(vertex.outHandle().length(), 2.0f, 1e-6f);
}

TEST(ShapeVertexContractTest, CornerFlagCanBeToggled)
{
    ShapeVertex vertex;
    vertex.setCorner(true);
    EXPECT_TRUE(vertex.isCorner());
    vertex.setCorner(false);
    EXPECT_FALSE(vertex.isCorner());
}
