#include <gtest/gtest.h>

#include <QColor>
#include <QPointF>
#include <QRectF>
#include <QString>

#include <memory>

import Shape.Layer;

namespace {

using ArtifactCore::FillSettings;
using ArtifactCore::LineCap;
using ArtifactCore::LineJoin;
using ArtifactCore::PathShape;
using ArtifactCore::ShapeLayer;
using ArtifactCore::ShapePath;
using ArtifactCore::StrokeSettings;

ShapeLayer makeStyledRectangle()
{
    ShapePath path;
    path.setRectangle(QRectF(4.0, 8.0, 24.0, 16.0));
    auto shape = std::make_unique<PathShape>(path);

    FillSettings fill(QColor(12, 34, 56, 78));
    StrokeSettings stroke(QColor(90, 80, 70, 60), 3.5);
    stroke.cap = LineCap::Round;
    stroke.join = LineJoin::Bevel;
    stroke.miterLimit = 7.25;
    stroke.dashPattern = {4.0, 2.0};
    shape->setFill(fill);
    shape->setStroke(stroke);

    ShapeLayer layer = ShapeLayer::createEmpty();
    layer.addShape(std::move(shape));
    return layer;
}

const PathShape* onlyPath(const ShapeLayer& layer)
{
    if (layer.shapeCount() != 1) {
        return nullptr;
    }
    return dynamic_cast<const PathShape*>(layer.content()->childAt(0));
}

} // namespace

TEST(ShapeLayerSvgContractTest, ExportImportRoundTripPreservesGeometryAndInlineStyles)
{
    const ShapeLayer source = makeStyledRectangle();
    const QString svg = source.toSvg();
    ASSERT_TRUE(svg.contains(QStringLiteral("style=\"")));

    const ShapeLayer restored = ShapeLayer::fromSvg(svg);
    ASSERT_EQ(restored.shapeCount(), 1);
    EXPECT_NEAR(restored.boundingRect().x(), 4.0, 1e-6);
    EXPECT_NEAR(restored.boundingRect().y(), 8.0, 1e-6);
    EXPECT_NEAR(restored.boundingRect().width(), 24.0, 1e-6);
    EXPECT_NEAR(restored.boundingRect().height(), 16.0, 1e-6);

    const PathShape* restoredPath = onlyPath(restored);
    ASSERT_NE(restoredPath, nullptr);
    EXPECT_TRUE(restoredPath->fill().enabled);
    EXPECT_EQ(restoredPath->fill().color, QColor(12, 34, 56, 78));
    EXPECT_TRUE(restoredPath->stroke().enabled);
    EXPECT_EQ(restoredPath->stroke().color, QColor(90, 80, 70, 60));
    EXPECT_NEAR(restoredPath->stroke().width, 3.5, 1e-6);
    EXPECT_EQ(restoredPath->stroke().cap, LineCap::Round);
    EXPECT_EQ(restoredPath->stroke().join, LineJoin::Bevel);
    EXPECT_NEAR(restoredPath->stroke().miterLimit, 7.25, 1e-6);
    ASSERT_EQ(restoredPath->stroke().dashPattern.size(), 2u);
    EXPECT_NEAR(restoredPath->stroke().dashPattern[0], 4.0, 1e-6);
    EXPECT_NEAR(restoredPath->stroke().dashPattern[1], 2.0, 1e-6);
}

TEST(ShapeLayerSvgContractTest, InlineDeclarationsOverridePresentationAttributes)
{
    const QString svg = QStringLiteral(
        "<svg xmlns=\"http://www.w3.org/2000/svg\">"
        "<path d=\"M 0 0 L 10 0 L 10 10 Z\" "
        "fill=\"#ffff0000\" stroke=\"#ffffffff\" stroke-width=\"5\" "
        "style=\" fill : #ff0000ff ; stroke: none; stroke-width: 0 \" />"
        "</svg>");

    const ShapeLayer imported = ShapeLayer::fromSvg(svg);
    const PathShape* path = onlyPath(imported);
    ASSERT_NE(path, nullptr);

    EXPECT_TRUE(path->fill().enabled);
    EXPECT_EQ(path->fill().color, QColor(0, 0, 255));
    EXPECT_FALSE(path->stroke().enabled);
    EXPECT_DOUBLE_EQ(path->stroke().width, 0.0);
    EXPECT_TRUE(imported.contains(QPointF(5.0, 2.0)));
}

TEST(ShapeLayerSvgContractTest, PresentationAttributesStillImportWithoutInlineStyle)
{
    const QString svg = QStringLiteral(
        "<svg xmlns=\"http://www.w3.org/2000/svg\">"
        "<rect x=\"2\" y=\"3\" width=\"8\" height=\"6\" "
        "fill=\"#ff336699\" stroke=\"none\" stroke-width=\"0\" />"
        "</svg>");

    const ShapeLayer imported = ShapeLayer::fromSvg(svg);
    const PathShape* path = onlyPath(imported);
    ASSERT_NE(path, nullptr);

    EXPECT_EQ(path->fill().color, QColor(0x33, 0x66, 0x99));
    EXPECT_FALSE(path->stroke().enabled);
    EXPECT_NEAR(imported.boundingRect().x(), 2.0, 1e-6);
    EXPECT_NEAR(imported.boundingRect().y(), 3.0, 1e-6);
    EXPECT_NEAR(imported.boundingRect().width(), 8.0, 1e-6);
    EXPECT_NEAR(imported.boundingRect().height(), 6.0, 1e-6);
}
