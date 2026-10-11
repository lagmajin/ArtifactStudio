#include <gtest/gtest.h>

#include <QSize>

#include <type_traits>

import Size;

using namespace ArtifactCore;

namespace {
struct WidthHeightFields { long width; short height; };
struct WidthHeightMethods {
    long width() const { return 640; }
    short height() const { return 360; }
};
struct WHFields { int w; int h; };
struct ColumnRowFields { int cols; int rows; };
struct XYFields { float x; double y; };
struct PascalCaseFields { int Width; int Height; };
struct UnsupportedShape { int left; int top; };
}

TEST(Size2DStandaloneTest, DefaultConstructionCreatesEmptySize)
{
    const Size_2D size;
    EXPECT_EQ(size.width, 0);
    EXPECT_EQ(size.height, 0);
    EXPECT_TRUE(size.isEmpty());
}

TEST(Size2DStandaloneTest, ExplicitDimensionsArePreserved)
{
    const Size_2D size(1920, 1080);
    EXPECT_EQ(size.width, 1920);
    EXPECT_EQ(size.height, 1080);
    EXPECT_FALSE(size.isEmpty());
}

TEST(Size2DStandaloneTest, ConvertsQtSize)
{
    const Size_2D size(QSize(1280, 720));
    EXPECT_EQ(size.width, 1280);
    EXPECT_EQ(size.height, 720);
}

TEST(Size2DStandaloneTest, ConvertsWidthAndHeightFields)
{
    const Size_2D size(WidthHeightFields{640, 480});
    EXPECT_EQ(size.width, 640);
    EXPECT_EQ(size.height, 480);
}

TEST(Size2DStandaloneTest, ConvertsWidthAndHeightMethods)
{
    const Size_2D size(WidthHeightMethods{});
    EXPECT_EQ(size.width, 640);
    EXPECT_EQ(size.height, 360);
}

TEST(Size2DStandaloneTest, ConvertsWAndHFields)
{
    const Size_2D size(WHFields{320, 200});
    EXPECT_EQ(size.width, 320);
    EXPECT_EQ(size.height, 200);
}

TEST(Size2DStandaloneTest, ConvertsColumnsAndRows)
{
    const Size_2D size(ColumnRowFields{80, 25});
    EXPECT_EQ(size.width, 80);
    EXPECT_EQ(size.height, 25);
}

TEST(Size2DStandaloneTest, ConvertsXAndYCoordinates)
{
    const Size_2D size(XYFields{12.0f, 7.0});
    EXPECT_EQ(size.width, 12);
    EXPECT_EQ(size.height, 7);
}

TEST(Size2DStandaloneTest, ConvertsPascalCaseDimensions)
{
    const Size_2D size(PascalCaseFields{1024, 768});
    EXPECT_EQ(size.width, 1024);
    EXPECT_EQ(size.height, 768);
}

TEST(Size2DStandaloneTest, ConceptAcceptsSupportedShapesAndRejectsUnrelatedTypes)
{
    EXPECT_TRUE((HasWidthHeightLike<QSize>));
    EXPECT_TRUE((HasWidthHeightLike<WidthHeightFields>));
    EXPECT_TRUE((HasWidthHeightLike<WidthHeightMethods>));
    EXPECT_TRUE((HasWidthHeightLike<WHFields>));
    EXPECT_TRUE((HasWidthHeightLike<ColumnRowFields>));
    EXPECT_TRUE((HasWidthHeightLike<XYFields>));
    EXPECT_TRUE((HasWidthHeightLike<PascalCaseFields>));
    EXPECT_FALSE((HasWidthHeightLike<UnsupportedShape>));
}

TEST(Size2DStandaloneTest, EmptyMeansEitherDimensionIsNotPositive)
{
    EXPECT_TRUE(Size_2D(0, 10).isEmpty());
    EXPECT_TRUE(Size_2D(10, 0).isEmpty());
    EXPECT_TRUE(Size_2D(-1, 10).isEmpty());
    EXPECT_TRUE(Size_2D(10, -1).isEmpty());
    EXPECT_FALSE(Size_2D(1, 1).isEmpty());
}

TEST(Size2DStandaloneTest, EqualityAndInequalityCompareBothDimensions)
{
    EXPECT_EQ(Size_2D(10, 20), Size_2D(10, 20));
    EXPECT_NE(Size_2D(10, 20), Size_2D(20, 10));
    EXPECT_NE(Size_2D(10, 20), Size_2D(10, 21));
}

TEST(Size2DStandaloneTest, AreaAndAspectRatioHandleOrdinaryAndZeroHeightSizes)
{
    EXPECT_EQ(Size_2D(1920, 1080).area(), 1920 * 1080);
    EXPECT_FLOAT_EQ(Size_2D(16, 9).aspectRatio(), 16.0f / 9.0f);
    EXPECT_FLOAT_EQ(Size_2D(4, 0).aspectRatio(), 0.0f);
}

