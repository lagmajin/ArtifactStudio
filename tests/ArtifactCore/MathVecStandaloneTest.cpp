#include <gtest/gtest.h>

#include <type_traits>

#include <QPointF>
#include <QVector3D>

import Math.Vec;

using namespace ArtifactCore;
using namespace ArtifactCore::Coordinates;

TEST(MathVecStandaloneTest, ScreenPhysicalPointScalesByDevicePixelRatio)
{
    const auto physical = toScreenPhysical(ScreenLogicalPoint2{12.0f, -3.0f}, 1.5f);
    EXPECT_FLOAT_EQ(physical.x, 18.0f);
    EXPECT_FLOAT_EQ(physical.y, -4.5f);
}

TEST(MathVecStandaloneTest, ScreenPhysicalVectorScalesWithoutChangingItsType)
{
    const ScreenLogicalVector2 logical{4.0f, -2.0f};
    const ScreenPhysicalVector2 physical = toScreenPhysical(logical, 2.0f);
    static_assert(std::is_same_v<decltype(physical), const ScreenPhysicalVector2>);
    EXPECT_FLOAT_EQ(physical.x, 8.0f);
    EXPECT_FLOAT_EQ(physical.y, -4.0f);
}

TEST(MathVecStandaloneTest, ScreenPhysicalExtentScalesWidthAndHeight)
{
    const auto logical = screenLogicalExtent(640.0f, 360.0f);
    const auto physical = toScreenPhysical(logical, 1.25f);
    EXPECT_FLOAT_EQ(physical.width, 800.0f);
    EXPECT_FLOAT_EQ(physical.height, 450.0f);
}

TEST(MathVecStandaloneTest, ScreenLogicalExtentFactoryPreservesDimensions)
{
    const ScreenLogicalExtent2 extent = screenLogicalExtent(20.0f, 15.0f);
    EXPECT_FLOAT_EQ(extent.width, 20.0f);
    EXPECT_FLOAT_EQ(extent.height, 15.0f);
}

TEST(MathVecStandaloneTest, CompositionPointConvertsToAndFromQtPoint)
{
    const CompositionPoint2 point{2.5f, -7.25f};
    const QPointF qt = toQPointF(point);
    const auto restored = compositionPointFromQPointF(qt);
    EXPECT_FLOAT_EQ(restored.x, point.x);
    EXPECT_FLOAT_EQ(restored.y, point.y);
}

TEST(MathVecStandaloneTest, LayerParentPointCanBeNamedFromCompositionPoint)
{
    const CompositionPoint2 composition{11.0f, 13.0f};
    const LayerParentPoint2 parent = layerParentPoint2FromComposition(composition);
    EXPECT_FLOAT_EQ(parent.x, composition.x);
    EXPECT_FLOAT_EQ(parent.y, composition.y);
}

TEST(MathVecStandaloneTest, BoundsPointQtConversionRetainsDoublePrecision)
{
    const BoundsPoint2<SourcePixelSpace> point{123456.125, -0.000000123};
    const QPointF qt = toQPointF(point);
    EXPECT_DOUBLE_EQ(qt.x(), point.x);
    EXPECT_DOUBLE_EQ(qt.y(), point.y);
}

TEST(MathVecStandaloneTest, WorldVectorAndPointUseExplicitQtConversions)
{
    const WorldVector3 vector{1.0f, 2.0f, 3.0f};
    const WorldPoint3 point{4.0f, 5.0f, 6.0f};
    const QVector3D qtVector = toQVector3D(vector);
    const QVector3D qtPoint = toQVector3D(point);
    const auto vectorBack = worldVectorFromQVector3D(qtVector);
    const auto pointBack = worldPoint3FromQVector3D(qtPoint);
    EXPECT_FLOAT_EQ(vectorBack.x, vector.x);
    EXPECT_FLOAT_EQ(vectorBack.y, vector.y);
    EXPECT_FLOAT_EQ(vectorBack.z, vector.z);
    EXPECT_FLOAT_EQ(pointBack.x, point.x);
    EXPECT_FLOAT_EQ(pointBack.y, point.y);
    EXPECT_FLOAT_EQ(pointBack.z, point.z);
}

TEST(MathVecStandaloneTest, LayerLocalPointAndVectorRoundTripSeparately)
{
    const LayerLocalPoint3 point{2.0f, 3.0f, 4.0f};
    const auto pointBack = layerLocalPoint3FromQVector3D(toQVector3D(point));
    const auto vectorBack = layerLocalVector3FromQVector3D(QVector3D(-1.0f, 0.5f, 8.0f));
    EXPECT_FLOAT_EQ(pointBack.x, point.x);
    EXPECT_FLOAT_EQ(pointBack.y, point.y);
    EXPECT_FLOAT_EQ(pointBack.z, point.z);
    EXPECT_FLOAT_EQ(vectorBack.x, vector.x);
    EXPECT_FLOAT_EQ(vectorBack.y, vector.y);
    EXPECT_FLOAT_EQ(vectorBack.z, vector.z);
}

TEST(MathVecStandaloneTest, PointsTranslateByVectorsAndSubtractToVectors)
{
    const LayerLocalPoint2 origin{4.0f, 7.0f};
    const LayerLocalVector2 delta{2.0f, -3.0f};
    const LayerLocalPoint2 moved = origin + delta;
    const LayerLocalVector2 recovered = moved - origin;
    EXPECT_FLOAT_EQ(moved.x, 6.0f);
    EXPECT_FLOAT_EQ(moved.y, 4.0f);
    EXPECT_FLOAT_EQ(recovered.x, delta.x);
    EXPECT_FLOAT_EQ(recovered.y, delta.y);
}

TEST(MathVecStandaloneTest, ThreeDimensionalCrossProductUsesRightHandRule)
{
    const auto result = cross(WorldVector3{1.0f, 0.0f, 0.0f},
                              WorldVector3{0.0f, 1.0f, 0.0f});
    EXPECT_FLOAT_EQ(result.x, 0.0f);
    EXPECT_FLOAT_EQ(result.y, 0.0f);
    EXPECT_FLOAT_EQ(result.z, 1.0f);
}

TEST(MathVecStandaloneTest, SpaceTypedVectorsSupportComponentArithmetic)
{
    const CompositionVector2 a{3.0f, 4.0f};
    const CompositionVector2 b{1.0f, -2.0f};
    const auto result = (a + b) * 2.0f;
    EXPECT_FLOAT_EQ(result.x, 8.0f);
    EXPECT_FLOAT_EQ(result.y, 4.0f);
}

TEST(MathVecStandaloneTest, SpaceTypedThreeDimensionalVectorSupportsScaling)
{
    const WorldVector3 value{1.5f, -2.0f, 4.0f};
    const auto scaled = value * 0.5f;
    EXPECT_FLOAT_EQ(scaled.x, 0.75f);
    EXPECT_FLOAT_EQ(scaled.y, -1.0f);
    EXPECT_FLOAT_EQ(scaled.z, 2.0f);
}

TEST(MathVecStandaloneTest, BoundsAndSpaceAliasesRetainTheirDeclaredTypes)
{
    static_assert(std::is_same_v<CompositionBounds2, Bounds2<CompositionSpace>>);
    static_assert(std::is_same_v<SourcePixelBounds2, Bounds2<SourcePixelSpace>>);
    static_assert(std::is_same_v<ScreenPhysicalExtent2, Extent2<ScreenPhysicalSpace>>);
    EXPECT_DOUBLE_EQ(CompositionBounds2{}.minimum.x, 0.0);
    EXPECT_FLOAT_EQ(SourcePixelExtent2{}.width, 0.0f);
}

TEST(MathVecStandaloneTest, DistinctSpacesDoNotImplicitlyConvert)
{
    static_assert(!std::is_convertible_v<CompositionPoint2, LayerLocalPoint2>);
    static_assert(!std::is_convertible_v<WorldPoint3, LayerLocalPoint3>);
    static_assert(!std::is_convertible_v<WorldVector3, ViewVector3>);
    SUCCEED();
}
