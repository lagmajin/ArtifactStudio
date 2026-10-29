#include <gtest/gtest.h>

#include <QLineF>
#include <QPainterPath>

import Core.Mask.PathMorph;

using ArtifactCore::MorphCorrespondence;
using ArtifactCore::PathMorphEngine;
using ArtifactCore::PathMorphSettings;

namespace {
QPainterPath rectangle(double x, double y, double width, double height)
{
    QPainterPath path;
    path.addRect(x, y, width, height);
    return path;
}

QPainterPath twoRectangles(double offset)
{
    QPainterPath path = rectangle(offset, 0.0, 10.0, 10.0);
    path.addRect(offset + 20.0, 0.0, 5.0, 5.0);
    return path;
}
}

TEST(PathMorphContractTest, SamplingEmptyPathIsDefinedAndHasNoSubpaths)
{
    const auto sample = PathMorphEngine::samplePath({}, 8);
    EXPECT_TRUE(sample.subpaths.empty());
    EXPECT_FALSE(sample.closed);
}

TEST(PathMorphContractTest, ClosedPathSamplingKeepsRequestedCountAndClosure)
{
    const auto sample = PathMorphEngine::samplePath(rectangle(0.0, 0.0, 8.0, 4.0), 12);
    ASSERT_EQ(sample.subpaths.size(), 1u);
    ASSERT_EQ(sample.subpaths.front().size(), 12u);
    EXPECT_TRUE(sample.closed);
    EXPECT_NEAR(QLineF(sample.subpaths.front().front(), sample.subpaths.front().back()).length(), 0.0, 1e-8);
}

TEST(PathMorphContractTest, SampleInterpolationClosesWhenEitherSampleIsClosed)
{
    ArtifactCore::MorphPathSample closed;
    closed.subpaths = {{QPointF(0.0, 0.0), QPointF(10.0, 0.0), QPointF(0.0, 0.0)}};
    ArtifactCore::MorphPathSample open;
    open.subpaths = {{QPointF(2.0, 2.0), QPointF(12.0, 2.0), QPointF(22.0, 2.0)}};

    const auto result = PathMorphEngine::interpolateSamples(closed, open, 0.5f);
    ASSERT_FALSE(result.isEmpty());
    EXPECT_EQ(result.elementAt(result.elementCount() - 1).type, QPainterPath::LineToElement);
    EXPECT_EQ(result.currentPosition(), result.elementAt(0));
}

TEST(PathMorphContractTest, ClosedPathSamplingClampsRequestedCountToThree)
{
    const auto sample = PathMorphEngine::samplePath(rectangle(0.0, 0.0, 8.0, 4.0), 1);
    ASSERT_EQ(sample.subpaths.size(), 1u);
    EXPECT_EQ(sample.subpaths.front().size(), 3u);
}

TEST(PathMorphContractTest, OpenPathSamplingPreservesEndpoints)
{
    QPainterPath path;
    path.moveTo(2.0, 3.0);
    path.lineTo(12.0, 3.0);

    const auto sample = PathMorphEngine::samplePath(path, 5);
    ASSERT_EQ(sample.subpaths.size(), 1u);
    ASSERT_EQ(sample.subpaths.front().size(), 5u);
    EXPECT_EQ(sample.subpaths.front().front(), QPointF(2.0, 3.0));
    EXPECT_EQ(sample.subpaths.front().back(), QPointF(12.0, 3.0));
    EXPECT_FALSE(sample.closed);
}

TEST(PathMorphContractTest, SamplingRetainsSubpathOrderAndSeparateContours)
{
    const auto sample = PathMorphEngine::samplePath(twoRectangles(0.0), 4);
    ASSERT_EQ(sample.subpaths.size(), 2u);
    EXPECT_EQ(sample.subpaths[0].front(), QPointF(0.0, 0.0));
    EXPECT_EQ(sample.subpaths[1].front(), QPointF(20.0, 0.0));
}

TEST(PathMorphContractTest, InterpolationReturnsExactInputsAtAndBeyondEndpoints)
{
    const auto first = rectangle(0.0, 0.0, 10.0, 10.0);
    const auto second = rectangle(20.0, 5.0, 6.0, 8.0);
    EXPECT_EQ(PathMorphEngine::interpolate(first, second, 0.0), first);
    EXPECT_EQ(PathMorphEngine::interpolate(first, second, -0.5f), first);
    EXPECT_EQ(PathMorphEngine::interpolate(first, second, 1.0), second);
    EXPECT_EQ(PathMorphEngine::interpolate(first, second, 1.5f), second);
}

TEST(PathMorphContractTest, EmptyInputIsIdentityForIntermediateInterpolation)
{
    const auto path = rectangle(3.0, 4.0, 5.0, 6.0);
    EXPECT_EQ(PathMorphEngine::interpolate({}, path, 0.5f), path);
    EXPECT_EQ(PathMorphEngine::interpolate(path, {}, 0.5f), path);
    EXPECT_TRUE(PathMorphEngine::interpolate({}, {}, 0.5f).isEmpty());
}

TEST(PathMorphContractTest, MidpointInterpolationMovesContourByHalfTheOffset)
{
    PathMorphSettings settings;
    settings.sampleCount = 9;
    settings.correspondence = MorphCorrespondence::Ordered;
    const auto result = PathMorphEngine::interpolate(
        rectangle(0.0, 0.0, 10.0, 10.0), rectangle(20.0, 0.0, 10.0, 10.0), 0.5f, settings);
    EXPECT_NEAR(result.boundingRect().left(), 10.0, 1e-6);
    EXPECT_NEAR(result.boundingRect().right(), 20.0, 1e-6);
    EXPECT_NEAR(result.boundingRect().top(), 0.0, 1e-6);
    EXPECT_NEAR(result.boundingRect().bottom(), 10.0, 1e-6);
}

TEST(PathMorphContractTest, SampleInterpolationUsesOnlyMatchingSubpathsAndPointCounts)
{
    ArtifactCore::MorphPathSample first;
    first.subpaths = {{QPointF(0.0, 0.0), QPointF(10.0, 0.0), QPointF(20.0, 0.0)},
                      {QPointF(100.0, 0.0), QPointF(110.0, 0.0)}};
    ArtifactCore::MorphPathSample second;
    second.subpaths = {{QPointF(2.0, 2.0), QPointF(12.0, 2.0)}};
    const auto result = PathMorphEngine::interpolateSamples(first, second, 0.5f);
    ASSERT_FALSE(result.isEmpty());
    EXPECT_EQ(result.elementCount(), 2);
    EXPECT_EQ(result.elementAt(0).x, 1.0);
    EXPECT_EQ(result.elementAt(0).y, 1.0);
    EXPECT_EQ(result.elementAt(1).x, 11.0);
    EXPECT_EQ(result.elementAt(1).y, 1.0);
}

TEST(PathMorphContractTest, FeatureInterpolationPairsContoursByAreaInsteadOfInputOrder)
{
    QPainterPath first = rectangle(0.0, 0.0, 20.0, 20.0);
    first.addRect(40.0, 0.0, 5.0, 5.0);
    QPainterPath second = rectangle(100.0, 0.0, 5.0, 5.0);
    second.addRect(60.0, 0.0, 20.0, 20.0);

    const auto result = PathMorphEngine::interpolateByFeature(first, second, 0.5f, 8);
    EXPECT_EQ(result.elementCount(), 18);
    EXPECT_NEAR(result.boundingRect().left(), 10.0, 1e-5);
    EXPECT_NEAR(result.boundingRect().right(), 70.0, 1e-5);
}
