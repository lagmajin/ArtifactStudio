#include <gtest/gtest.h>

#include <QJsonObject>
#include <QString>

import Coordinate.Profile;

using namespace ArtifactCore;

namespace {
CoordinateValue coordinateValue(double value, CoordinateUnit unit,
                                const QString& expression = {})
{
    return CoordinateValue{value, unit, expression};
}
}

TEST(CoordinateProfileContractTest, CoordinateValueResolvesEachUnitAgainstItsReference)
{
    EXPECT_DOUBLE_EQ(coordinateValue(25.0, CoordinateUnit::Percent).resolve(200.0, 0.0, 0.0), 50.0);
    EXPECT_DOUBLE_EQ(coordinateValue(0.25, CoordinateUnit::Normalized).resolve(200.0, 0.0, 0.0), 50.0);
    EXPECT_DOUBLE_EQ(coordinateValue(3.0, CoordinateUnit::Grid).resolve(200.0, 0.0, 12.0), 36.0);
    EXPECT_DOUBLE_EQ(coordinateValue(7.5, CoordinateUnit::SafeArea).resolve(200.0, 40.0, 0.0), 47.5);
    EXPECT_DOUBLE_EQ(coordinateValue(12.0, CoordinateUnit::Pixels).resolve(200.0, 0.0, 0.0), 12.0);
}

TEST(CoordinateProfileContractTest, CoordinateSpaceUsesAxisSpecificCanvasSafeAndGridSizes)
{
    CoordinateSpace space;
    space.canvasWidth = 1920.0;
    space.canvasHeight = 1080.0;
    space.safeLeft = 120.0;
    space.safeTop = 54.0;
    space.gridSizeX = 80.0;
    space.gridSizeY = 45.0;

    const auto percent = coordinateValue(50.0, CoordinateUnit::Percent);
    const auto grid = coordinateValue(2.0, CoordinateUnit::Grid);
    const auto safeArea = coordinateValue(10.0, CoordinateUnit::SafeArea);
    EXPECT_DOUBLE_EQ(space.resolveX(percent), 960.0);
    EXPECT_DOUBLE_EQ(space.resolveY(percent), 540.0);
    EXPECT_DOUBLE_EQ(space.resolveX(grid), 160.0);
    EXPECT_DOUBLE_EQ(space.resolveY(grid), 90.0);
    EXPECT_DOUBLE_EQ(space.resolveX(safeArea), 130.0);
    EXPECT_DOUBLE_EQ(space.resolveY(safeArea), 64.0);
}

TEST(CoordinateProfileContractTest, CoordinateValueTextParserRecognizesSupportedSuffixes)
{
    const auto pixels = CoordinateValue::fromString(QStringLiteral(" 12.5 px "));
    const auto percent = CoordinateValue::fromString(QStringLiteral("25%"));
    const auto normalized = CoordinateValue::fromString(QStringLiteral("0.25"));
    const auto grid = CoordinateValue::fromString(QStringLiteral("GRID[3.5]"));
    const auto safe = CoordinateValue::fromString(QStringLiteral("Safe + 8px"));
    const auto safeRoundTrip = CoordinateValue::fromString(
        coordinateValue(8.0, CoordinateUnit::SafeArea).toString());

    EXPECT_EQ(pixels.unit, CoordinateUnit::Pixels);
    EXPECT_DOUBLE_EQ(pixels.value, 12.5);
    EXPECT_EQ(percent.unit, CoordinateUnit::Percent);
    EXPECT_DOUBLE_EQ(percent.value, 25.0);
    EXPECT_EQ(normalized.unit, CoordinateUnit::Normalized);
    EXPECT_DOUBLE_EQ(normalized.value, 0.25);
    EXPECT_EQ(grid.unit, CoordinateUnit::Grid);
    EXPECT_DOUBLE_EQ(grid.value, 3.5);
    EXPECT_EQ(safe.unit, CoordinateUnit::SafeArea);
    EXPECT_DOUBLE_EQ(safe.value, 8.0);
    EXPECT_EQ(safeRoundTrip.unit, CoordinateUnit::SafeArea);
    EXPECT_DOUBLE_EQ(safeRoundTrip.value, 8.0);
}

TEST(CoordinateProfileContractTest, CoordinateValueTextFormatsPreserveUnitIdentity)
{
    EXPECT_EQ(coordinateValue(12.345, CoordinateUnit::Pixels).toString(), QStringLiteral("12.35px"));
    EXPECT_EQ(coordinateValue(25.0, CoordinateUnit::Percent).toString(), QStringLiteral("25.00%"));
    EXPECT_EQ(coordinateValue(0.125, CoordinateUnit::Normalized).toString(), QStringLiteral("0.1250"));
    EXPECT_EQ(coordinateValue(2.5, CoordinateUnit::Grid).toString(), QStringLiteral("grid[2.5]"));
    EXPECT_EQ(coordinateValue(8.0, CoordinateUnit::SafeArea).toString(), QStringLiteral("safe + 8.00px"));
}

TEST(CoordinateProfileContractTest, CoordinateValueJsonRoundTripPreservesValueUnitAndExpression)
{
    const CoordinateValue original = coordinateValue(
        12.75, CoordinateUnit::SafeArea, QStringLiteral("safe + 12.75px"));
    const CoordinateValue restored = CoordinateValue::fromJson(original.toJson());

    EXPECT_DOUBLE_EQ(restored.value, original.value);
    EXPECT_EQ(restored.unit, original.unit);
    EXPECT_EQ(restored.expression, original.expression);
}

TEST(CoordinateProfileContractTest, CoordinateProfileJsonRoundTripPreservesFlagsAndUnits)
{
    CoordinateProfile original;
    original.id = QStringLiteral("custom.profile");
    original.name = QStringLiteral("Custom");
    original.baseUnit = CoordinateUnit::Normalized;
    original.displayUnit = CoordinateUnit::Grid;
    original.enablePercent = false;
    original.enableNormalized = true;
    original.enableGrid = false;
    const CoordinateProfile restored = CoordinateProfile::fromJson(original.toJson());

    EXPECT_EQ(restored.id, original.id);
    EXPECT_EQ(restored.name, original.name);
    EXPECT_EQ(restored.baseUnit, original.baseUnit);
    EXPECT_EQ(restored.displayUnit, original.displayUnit);
    EXPECT_EQ(restored.enablePercent, original.enablePercent);
    EXPECT_EQ(restored.enableNormalized, original.enableNormalized);
    EXPECT_EQ(restored.enableGrid, original.enableGrid);
}

TEST(CoordinateProfileContractTest, ResolverConvertsThroughCanvasPixelsAndFormatsDisplayUnit)
{
    CoordinateProfile profile = CoordinateProfile::defaultPercent();
    profile.displayUnit = CoordinateUnit::Grid;
    CoordinateResolver resolver(profile);
    CoordinateSpace space;
    space.canvasWidth = 1600.0;
    space.gridSizeX = 100.0;

    const CoordinateValue converted = resolver.convert(
        coordinateValue(25.0, CoordinateUnit::Percent), CoordinateUnit::Pixels, space);
    EXPECT_DOUBLE_EQ(converted.value, 400.0);
    EXPECT_EQ(converted.unit, CoordinateUnit::Pixels);
    EXPECT_EQ(resolver.displayText(coordinateValue(25.0, CoordinateUnit::Percent), space),
              QStringLiteral("grid[4.0]"));
}

TEST(CoordinateProfileContractTest, ExpressionResolverSupportsPixelsPercentAndCanvasDimensions)
{
    CoordinateSpace space;
    space.canvasWidth = 1920.0;
    space.canvasHeight = 1080.0;

    EXPECT_DOUBLE_EQ(CoordinateResolver::resolveExpression(QStringLiteral("37px"), space), 37.0);
    EXPECT_DOUBLE_EQ(CoordinateResolver::resolveExpression(QStringLiteral("25%"), space), 480.0);
    EXPECT_DOUBLE_EQ(CoordinateResolver::resolveExpression(QStringLiteral("w-10"), space), 1910.0);
    EXPECT_DOUBLE_EQ(CoordinateResolver::resolveExpression(QStringLiteral("h+20"), space), 1100.0);
}
