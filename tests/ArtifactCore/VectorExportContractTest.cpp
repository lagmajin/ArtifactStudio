#include <gtest/gtest.h>

#include <memory>
#include <vector>

#include <QColor>
#include <QFile>
#include <QDir>
#include <QByteArray>
#include <QRectF>
#include <QSize>
#include <QStringList>
#include <QTemporaryDir>
#include <QXmlStreamReader>

import IO.VectorExport;
import Shape.Layer;

using namespace ArtifactCore;

TEST(VectorExportContractTest, HtmlPlayerEscapesTitleButPreservesMarkupPayload)
{
    const QString title = QStringLiteral("<Shot & \"look\">");
    const QString svg = QStringLiteral("<svg data-label=\"A&B\"></svg>");
    const QString html = HtmlPlayerWriter::generateFromSequence(
        title, QSize(640, 360), QStringLiteral(".test{color:red}"), svg);

    EXPECT_NE(html.indexOf(QStringLiteral("&lt;Shot &amp; &quot;look&quot;&gt;")), -1);
    EXPECT_NE(html.indexOf(QStringLiteral(".test{color:red}")), -1);
    EXPECT_NE(html.indexOf(svg), -1);
    EXPECT_NE(html.indexOf(QStringLiteral("width:min(100")), -1);
    EXPECT_NE(html.indexOf(QStringLiteral("640px")), -1);
    EXPECT_NE(html.indexOf(QStringLiteral("aspect-ratio:640/360")), -1);
}

TEST(VectorExportContractTest, EmptyTitleAndFrameListUseDocumentedFallbacks)
{
    const QString html = HtmlPlayerWriter::generateFrameSequencePlayer(
        QString(), QSize(320, 200), {}, 0.0);

    EXPECT_NE(html.indexOf(QStringLiteral("<title>Artifact Web Export</title>")), -1);
    EXPECT_NE(html.indexOf(QStringLiteral("No frames available")), -1);
    EXPECT_NE(html.indexOf(QStringLiteral("--frame-count:1")), -1);
    EXPECT_NE(html.indexOf(QStringLiteral("const rate=30.000;")), -1);
    EXPECT_NE(html.indexOf(QStringLiteral("max=\"0\"")), -1);
}

TEST(VectorExportContractTest, FrameSequenceEscapesFilenamesAndMarksFirstFrameActive)
{
    const QStringList frames{
        QStringLiteral("frame&one.png"),
        QStringLiteral("frame\"two.png")};
    const QString html = HtmlPlayerWriter::generateFrameSequencePlayer(
        QStringLiteral("Review"), QSize(1920, 1080), frames, 2.0);

    EXPECT_NE(html.indexOf(QStringLiteral("src=\"frame&amp;one.png\"")), -1);
    EXPECT_NE(html.indexOf(QStringLiteral("src=\"frame&quot;two.png\"")), -1);
    EXPECT_NE(html.indexOf(QStringLiteral("class=\"frame-0 is-active\"")), -1);
    EXPECT_NE(html.indexOf(QStringLiteral("class=\"frame-1\"")), -1);
    EXPECT_NE(html.indexOf(QStringLiteral("const rate=1.000;")), -1);
}

TEST(VectorExportContractTest, CssAnimationUsesDurationAndFrameTimesAsPercentages)
{
    CssAnimationData data;
    data.animationName = QStringLiteral("MoveLayer");
    data.durationSec = 1.0;
    data.properties.push_back({QStringLiteral("transform"),
        {{0, QStringLiteral("translate(0px, 0px)")},
         {500, QStringLiteral("translate(5px, 0px)")},
         {1000, QStringLiteral("translate(10px, 0px)")}}});

    const QString css = CssAnimationExporter::generateCss(data, QSize(800, 450));
    EXPECT_NE(css.indexOf(QStringLiteral("width: 800px;")), -1);
    EXPECT_NE(css.indexOf(QStringLiteral("height: 450px;")), -1);
    EXPECT_NE(css.indexOf(QStringLiteral("0.0% { transform: translate(0px, 0px); }")), -1);
    EXPECT_NE(css.indexOf(QStringLiteral("50.0% { transform: translate(5px, 0px); }")), -1);
    EXPECT_NE(css.indexOf(QStringLiteral("100.0% { transform: translate(10px, 0px); }")), -1);
    EXPECT_NE(css.indexOf(QStringLiteral("animation: MoveLayer 1.000s linear infinite;")), -1);
}

TEST(VectorExportContractTest, SvgSequenceCreatesDirectoryAndZeroPadsFrameNumbers)
{
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());
    const QString outputDir = tempDir.filePath(QStringLiteral("nested/frames"));
    const std::vector<VectorExportFrame> frames{
        {7, QStringLiteral("<svg>first</svg>\n")},
        {12003, QStringLiteral("<svg>last</svg>\n")}};

    ASSERT_TRUE(SvgFrameExporter::writeSvgSequence(
        frames, outputDir, QStringLiteral("shot")));

    QFile first(QDir(outputDir).filePath(QStringLiteral("shot_0007.svg")));
    QFile last(QDir(outputDir).filePath(QStringLiteral("shot_12003.svg")));
    ASSERT_TRUE(first.open(QIODevice::ReadOnly | QIODevice::Text));
    ASSERT_TRUE(last.open(QIODevice::ReadOnly | QIODevice::Text));
    EXPECT_EQ(first.readAll(), QByteArray("<svg>first</svg>\n"));
    EXPECT_EQ(last.readAll(), QByteArray("<svg>last</svg>\n"));
}

TEST(VectorExportContractTest, CompositionFrameExportsVisibleShapeGeometryAndTransform)
{
    ShapeLayer visibleLayer;
    visibleLayer.setName(QStringLiteral("VisibleRectangle"));
    visibleLayer.transform().position = Point2DValue(12.0, 7.0);
    auto rectangle = std::make_unique<RectanglePathShape>(QRectF(1.0, 2.0, 8.0, 6.0));
    rectangle->fill().color = QColor(255, 0, 0);
    rectangle->stroke().enabled = false;
    visibleLayer.addShape(std::move(rectangle));

    ShapeLayer hiddenLayer;
    hiddenLayer.setName(QStringLiteral("HiddenRectangle"));
    hiddenLayer.setVisible(false);
    auto hiddenRectangle =
        std::make_unique<RectanglePathShape>(QRectF(20.0, 20.0, 5.0, 5.0));
    hiddenRectangle->fill().color = QColor(0, 0, 255);
    hiddenRectangle->stroke().enabled = false;
    hiddenLayer.addShape(std::move(hiddenRectangle));

    const std::vector<const ShapeLayer*> layers{&visibleLayer, &hiddenLayer};
    const QString svg = SvgFrameExporter::exportCompositionFrame(
        layers, QSize(320, 180), 24);

    QXmlStreamReader xml(svg);
    bool foundSvgRoot = false;
    int pathElementCount = 0;
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement()) {
            if (!foundSvgRoot) {
                foundSvgRoot = true;
                EXPECT_EQ(xml.name(), QStringLiteral("svg"));
                EXPECT_EQ(xml.namespaceUri().toString(),
                          QStringLiteral("http://www.w3.org/2000/svg"));
            }
            if (xml.name() == QStringLiteral("path")) {
                ++pathElementCount;
                EXPECT_FALSE(xml.attributes().value(QStringLiteral("d")).isEmpty());
            }
        }
    }
    EXPECT_FALSE(xml.hasError()) << xml.errorString().toStdString();
    EXPECT_TRUE(foundSvgRoot);
    EXPECT_EQ(pathElementCount, 1);

    EXPECT_NE(svg.indexOf(QStringLiteral("width=\"320\" height=\"180\"")), -1);
    EXPECT_NE(svg.indexOf(QStringLiteral("<!-- Frame 24 -->")), -1);
    EXPECT_NE(svg.indexOf(QStringLiteral("<!-- Layer: VisibleRectangle -->")), -1);
    EXPECT_EQ(svg.indexOf(QStringLiteral("HiddenRectangle")), -1);
    EXPECT_NE(svg.indexOf(QStringLiteral("fill:#ffff0000")), -1);
    EXPECT_EQ(svg.indexOf(QStringLiteral("fill:#ff0000ff")), -1);
    EXPECT_NE(svg.indexOf(QStringLiteral(
        "matrix(1.000000,0.000000,0.000000,1.000000,12.000000,7.000000)")), -1);
    EXPECT_EQ(svg.count(QStringLiteral("<path d=\"")), 1);
}
