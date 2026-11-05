#include <gtest/gtest.h>

#include <QByteArray>
#include <QColor>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QVariant>

#include <algorithm>
#include <memory>

import Artifact.Layer.Text;
import Image.ImageF32x4_RGBA;
import Memory.SharedPtr;
import Undo.UndoManager;
import Utils.String.UniString;

namespace {

class OffscreenGuiApplication final : public testing::Environment {
public:
    void SetUp() override
    {
        qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
        static int argc = 1;
        static char applicationName[] = "ArtifactTextAnimatorLayerContractTest";
        static char* argv[] = {applicationName, nullptr};
        application_ = new QGuiApplication(argc, argv);
    }

    void TearDown() override
    {
        delete application_;
        application_ = nullptr;
    }

private:
    QGuiApplication* application_ = nullptr;
};

testing::Environment* const guiApplication =
    testing::AddGlobalTestEnvironment(new OffscreenGuiApplication());

void configureText(Artifact::ArtifactTextLayer& layer, const QString& text)
{
    layer.setText(ArtifactCore::UniString(text));
    layer.setFontSize(48.0f);
}

QJsonObject animatorAt(const QJsonArray& stack, const int index)
{
    return stack.at(index).toObject();
}

QJsonObject rangeAt(const QJsonArray& stack, const int index)
{
    return animatorAt(stack, index).value(QStringLiteral("range")).toObject();
}

QJsonObject propertiesAt(const QJsonArray& stack, const int index)
{
    return animatorAt(stack, index)
        .value(QStringLiteral("properties"))
        .toObject();
}

struct AlphaSummary {
    double alpha = 0.0;
    double weightedX = 0.0;
};

AlphaSummary summarizeAlpha(const Artifact::ArtifactTextLayer& layer)
{
    const auto& image = layer.currentFrameBuffer();
    AlphaSummary summary;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const double alpha = std::clamp(
                static_cast<double>(image.getPixel(x, y).a()), 0.0, 1.0);
            summary.alpha += alpha;
            summary.weightedX += alpha * x;
        }
    }
    return summary;
}

bool setAnimator(Artifact::ArtifactTextLayer& layer, const int index,
                 const QString& suffix, const QVariant& value)
{
    return layer.setLayerPropertyValue(
        QStringLiteral("text.animators.%1.%2").arg(index).arg(suffix), value);
}

} // namespace

TEST(TextAnimatorLayerContractTest, AddAnimatorPropertyRejectsUnsupportedIds)
{
    Artifact::ArtifactTextLayer layer;
    configureText(layer, QStringLiteral("Animator"));

    EXPECT_TRUE(layer.addAnimatorProperty(QStringLiteral("position")));
    EXPECT_TRUE(layer.addAnimatorProperty(QStringLiteral("fillColor")));
    EXPECT_FALSE(layer.addAnimatorProperty(QStringLiteral("unknown")));
    EXPECT_FALSE(layer.addAnimatorProperty(QString()));
    EXPECT_EQ(layer.animatorCount(), 2);

    const QJsonArray stack = layer.textAnimatorStackSnapshot();
    ASSERT_EQ(stack.size(), 2);
    EXPECT_EQ(animatorAt(stack, 0).value(QStringLiteral("name")).toString(),
              QStringLiteral("Position"));
    EXPECT_EQ(animatorAt(stack, 1).value(QStringLiteral("name")).toString(),
              QStringLiteral("Fill Color"));
    EXPECT_TRUE(propertiesAt(stack, 1)
                    .value(QStringLiteral("colorEnabled"))
                    .toBool());
}

TEST(TextAnimatorLayerContractTest, StrokeWidthAndZAreReachableFromAddProperty)
{
    Artifact::ArtifactTextLayer layer;
    configureText(layer, QStringLiteral("Animator"));

    ASSERT_TRUE(layer.addAnimatorProperty(QStringLiteral("strokeWidth")));
    ASSERT_TRUE(layer.addAnimatorProperty(QStringLiteral("z")));
    EXPECT_EQ(layer.animatorCount(), 2);

    const QJsonArray stack = layer.textAnimatorStackSnapshot();
    ASSERT_EQ(stack.size(), 2);
    EXPECT_EQ(animatorAt(stack, 0).value(QStringLiteral("name")).toString(),
              QStringLiteral("Stroke Width"));
    EXPECT_TRUE(propertiesAt(stack, 0)
                    .value(QStringLiteral("strokeEnabled"))
                    .toBool());
    EXPECT_DOUBLE_EQ(
        propertiesAt(stack, 0).value(QStringLiteral("strokeWidth")).toDouble(),
        0.0);
    EXPECT_EQ(animatorAt(stack, 1).value(QStringLiteral("name")).toString(),
              QStringLiteral("Z"));
    EXPECT_DOUBLE_EQ(propertiesAt(stack, 1).value(QStringLiteral("z")).toDouble(),
                     0.0);

    EXPECT_TRUE(setAnimator(layer, 0, QStringLiteral("strokeWidth"), QVariant(3.0)));
    EXPECT_TRUE(setAnimator(layer, 1, QStringLiteral("z"), QVariant(12.0)));
    const QJsonArray updated = layer.textAnimatorStackSnapshot();
    EXPECT_DOUBLE_EQ(
        propertiesAt(updated, 0).value(QStringLiteral("strokeWidth")).toDouble(),
        3.0);
    EXPECT_DOUBLE_EQ(propertiesAt(updated, 1).value(QStringLiteral("z")).toDouble(),
                     12.0);
}

TEST(TextAnimatorLayerContractTest, BaselineShiftAndAnchorPointPersist)
{
    Artifact::ArtifactTextLayer layer;
    configureText(layer, QStringLiteral("Animator"));

    ASSERT_TRUE(layer.addAnimatorProperty(QStringLiteral("baselineShift")));
    ASSERT_TRUE(layer.addAnimatorProperty(QStringLiteral("anchorPoint")));
    EXPECT_EQ(layer.animatorCount(), 2);

    const QJsonArray added = layer.textAnimatorStackSnapshot();
    ASSERT_EQ(added.size(), 2);
    EXPECT_EQ(animatorAt(added, 0).value(QStringLiteral("name")).toString(),
              QStringLiteral("Baseline Shift"));
    EXPECT_EQ(animatorAt(added, 1).value(QStringLiteral("name")).toString(),
              QStringLiteral("Anchor Point"));
    EXPECT_DOUBLE_EQ(
        propertiesAt(added, 0)
            .value(QStringLiteral("baselineShift"))
            .toDouble(),
        0.0);
    EXPECT_DOUBLE_EQ(
        propertiesAt(added, 1)
            .value(QStringLiteral("anchorPointX"))
            .toDouble(),
        0.0);
    EXPECT_DOUBLE_EQ(
        propertiesAt(added, 1)
            .value(QStringLiteral("anchorPointY"))
            .toDouble(),
        0.0);

    EXPECT_TRUE(
        setAnimator(layer, 0, QStringLiteral("baselineShift"), QVariant(6.0)));
    EXPECT_TRUE(
        setAnimator(layer, 1, QStringLiteral("anchorPointX"), QVariant(3.0)));
    EXPECT_TRUE(
        setAnimator(layer, 1, QStringLiteral("anchorPointY"), QVariant(-2.0)));

    const QJsonArray snapshot = layer.textAnimatorStackSnapshot();
    EXPECT_DOUBLE_EQ(
        propertiesAt(snapshot, 0)
            .value(QStringLiteral("baselineShift"))
            .toDouble(),
        6.0);
    EXPECT_DOUBLE_EQ(
        propertiesAt(snapshot, 1)
            .value(QStringLiteral("anchorPointX"))
            .toDouble(),
        3.0);
    EXPECT_DOUBLE_EQ(
        propertiesAt(snapshot, 1)
            .value(QStringLiteral("anchorPointY"))
            .toDouble(),
        -2.0);

    Artifact::ArtifactTextLayer restored;
    configureText(restored, QStringLiteral("Animator"));
    restored.restoreTextAnimatorStack(snapshot);
    EXPECT_EQ(restored.textAnimatorStackSnapshot(), snapshot);
}

TEST(TextAnimatorLayerContractTest, AnimatorPropertyPathsClampToAuthoredRanges)
{
    Artifact::ArtifactTextLayer layer;
    configureText(layer, QStringLiteral("Animator"));
    ASSERT_TRUE(layer.addAnimatorProperty(QStringLiteral("opacity")));

    EXPECT_TRUE(setAnimator(layer, 0, QStringLiteral("units"), QVariant(1)));
    EXPECT_TRUE(setAnimator(layer, 0, QStringLiteral("end"), QVariant(0.0)));
    EXPECT_TRUE(setAnimator(layer, 0, QStringLiteral("opacity"), QVariant(2.5)));
    EXPECT_TRUE(setAnimator(layer, 0, QStringLiteral("blur"), QVariant(20000.0)));
    EXPECT_TRUE(
        setAnimator(layer, 0, QStringLiteral("strokeWidth"), QVariant(300.0)));
    EXPECT_TRUE(setAnimator(layer, 0, QStringLiteral("scale"), QVariant(12.0)));
    EXPECT_TRUE(setAnimator(layer, 0, QStringLiteral("scaleX"), QVariant(-5.0)));

    const QJsonArray stack = layer.textAnimatorStackSnapshot();
    ASSERT_EQ(stack.size(), 1);
    EXPECT_EQ(rangeAt(stack, 0).value(QStringLiteral("units")).toInt(), 1);
    EXPECT_DOUBLE_EQ(
        rangeAt(stack, 0).value(QStringLiteral("end")).toDouble(), 0.0);
    const QJsonObject properties = propertiesAt(stack, 0);
    EXPECT_DOUBLE_EQ(properties.value(QStringLiteral("opacity")).toDouble(), 1.0);
    EXPECT_DOUBLE_EQ(properties.value(QStringLiteral("blur")).toDouble(), 10000.0);
    EXPECT_DOUBLE_EQ(
        properties.value(QStringLiteral("strokeWidth")).toDouble(), 256.0);
    EXPECT_DOUBLE_EQ(properties.value(QStringLiteral("scale")).toDouble(), 8.0);
    EXPECT_DOUBLE_EQ(properties.value(QStringLiteral("scaleX")).toDouble(), -1.0);
}

TEST(TextAnimatorLayerContractTest, OutOfRangeIndexAndUnknownSuffixDoNotMutate)
{
    Artifact::ArtifactTextLayer layer;
    configureText(layer, QStringLiteral("Animator"));
    ASSERT_TRUE(layer.addAnimatorProperty(QStringLiteral("position")));
    const QJsonArray before = layer.textAnimatorStackSnapshot();

    EXPECT_FALSE(setAnimator(layer, 3, QStringLiteral("positionX"), QVariant(5.0)));
    EXPECT_FALSE(setAnimator(layer, -1, QStringLiteral("positionX"), QVariant(5.0)));
    (void)setAnimator(layer, 0, QStringLiteral("unknownField"), QVariant(5.0));

    EXPECT_EQ(layer.textAnimatorStackSnapshot(), before);
}

TEST(TextAnimatorLayerContractTest, SnapshotRoundTripsEverySelectorField)
{
    Artifact::ArtifactTextLayer layer;
    configureText(layer, QStringLiteral("Animator"));
    ASSERT_TRUE(layer.addAnimatorProperty(QStringLiteral("position")));
    ASSERT_TRUE(layer.addAnimatorProperty(QStringLiteral("rotation")));

    EXPECT_TRUE(setAnimator(layer, 0, QStringLiteral("name"), QStringLiteral("Wave")));
    EXPECT_TRUE(setAnimator(layer, 0, QStringLiteral("combine"), QVariant(2)));
    EXPECT_TRUE(setAnimator(layer, 0, QStringLiteral("units"), QVariant(1)));
    EXPECT_TRUE(setAnimator(layer, 0, QStringLiteral("shape"), QVariant(3)));
    EXPECT_TRUE(setAnimator(layer, 0, QStringLiteral("order"), QVariant(4)));
    EXPECT_TRUE(setAnimator(layer, 0, QStringLiteral("anchorGrouping"), QVariant(5)));
    EXPECT_TRUE(setAnimator(layer, 0, QStringLiteral("start"), QVariant(1.0)));
    EXPECT_TRUE(setAnimator(layer, 0, QStringLiteral("end"), QVariant(3.0)));
    EXPECT_TRUE(setAnimator(layer, 0, QStringLiteral("offset"), QVariant(0.5)));
    EXPECT_TRUE(setAnimator(layer, 0, QStringLiteral("easeHigh"), QVariant(3.0)));
    EXPECT_TRUE(setAnimator(layer, 0, QStringLiteral("easeLow"), QVariant(2.0)));
    EXPECT_TRUE(
        setAnimator(layer, 0, QStringLiteral("selectorPattern"), QStringLiteral("A.")));
    EXPECT_TRUE(setAnimator(layer, 0, QStringLiteral("regexEnabled"), QVariant(true)));
    EXPECT_TRUE(setAnimator(layer, 0, QStringLiteral("wigglyEnabled"), QVariant(true)));
    EXPECT_TRUE(
        setAnimator(layer, 0, QStringLiteral("wigglesPerSecond"), QVariant(3.5)));
    EXPECT_TRUE(setAnimator(layer, 0, QStringLiteral("correlation"), QVariant(25.0)));
    EXPECT_TRUE(setAnimator(layer, 0, QStringLiteral("phase"), QVariant(0.5)));
    EXPECT_TRUE(setAnimator(layer, 0, QStringLiteral("seed"), QVariant(7)));
    EXPECT_TRUE(setAnimator(layer, 0, QStringLiteral("expression"),
                            QStringLiteral("textIndex / textTotal")));
    EXPECT_TRUE(setAnimator(layer, 0, QStringLiteral("expressionEnabled"), QVariant(true)));
    EXPECT_TRUE(setAnimator(layer, 0, QStringLiteral("expressionSeed"), QVariant(9)));
    EXPECT_TRUE(setAnimator(layer, 0, QStringLiteral("positionX"), QVariant(12.0)));
    EXPECT_TRUE(setAnimator(layer, 0, QStringLiteral("positionY"), QVariant(-4.0)));
    EXPECT_TRUE(setAnimator(layer, 0, QStringLiteral("scaleX"), QVariant(1.5)));
    EXPECT_TRUE(setAnimator(layer, 0, QStringLiteral("scaleY"), QVariant(0.75)));
    EXPECT_TRUE(setAnimator(layer, 0, QStringLiteral("skew"), QVariant(6.0)));
    EXPECT_TRUE(setAnimator(layer, 0, QStringLiteral("tracking"), QVariant(2.0)));
    EXPECT_TRUE(setAnimator(layer, 0, QStringLiteral("z"), QVariant(9.0)));
    EXPECT_TRUE(setAnimator(layer, 0, QStringLiteral("blur"), QVariant(1.5)));
    EXPECT_TRUE(
        setAnimator(layer, 0, QStringLiteral("fillColor"), QVariant(QColor(Qt::red))));
    EXPECT_TRUE(setAnimator(layer, 0, QStringLiteral("strokeWidth"), QVariant(2.0)));
    EXPECT_TRUE(
        setAnimator(layer, 0, QStringLiteral("strokeColor"), QVariant(QColor(Qt::green))));
    EXPECT_TRUE(setAnimator(layer, 1, QStringLiteral("enabled"), QVariant(false)));

    const QJsonArray snapshot = layer.textAnimatorStackSnapshot();
    ASSERT_EQ(snapshot.size(), 2);

    Artifact::ArtifactTextLayer restored;
    configureText(restored, QStringLiteral("Animator"));
    restored.restoreTextAnimatorStack(snapshot);

    EXPECT_EQ(restored.animatorCount(), 2);
    EXPECT_EQ(restored.textAnimatorStackSnapshot(), snapshot);
}

TEST(TextAnimatorLayerContractTest, ProjectJsonRoundTripPreservesAnimatorStack)
{
    Artifact::ArtifactTextLayer layer;
    configureText(layer, QStringLiteral("Animator"));
    ASSERT_TRUE(layer.addAnimatorProperty(QStringLiteral("rotation")));
    EXPECT_TRUE(setAnimator(layer, 0, QStringLiteral("rotation"), QVariant(30.0)));
    EXPECT_TRUE(setAnimator(layer, 0, QStringLiteral("units"), QVariant(1)));
    EXPECT_TRUE(setAnimator(layer, 0, QStringLiteral("end"), QVariant(0.0)));

    const QJsonObject json = layer.toJson();
    Artifact::ArtifactTextLayer restored;
    restored.fromJsonProperties(json);

    EXPECT_EQ(restored.animatorCount(), 1);
    EXPECT_EQ(restored.textAnimatorStackSnapshot(),
              layer.textAnimatorStackSnapshot());
}

TEST(TextAnimatorLayerContractTest, StackCommandUndoRedoRestoresAnimatorStack)
{
    auto layer = ArtifactCore::makeShared<Artifact::ArtifactTextLayer>();
    configureText(*layer, QStringLiteral("Animator"));
    ASSERT_TRUE(layer->addAnimatorProperty(QStringLiteral("position")));
    const QJsonArray before = layer->textAnimatorStackSnapshot();

    auto desired = ArtifactCore::makeShared<Artifact::ArtifactTextLayer>();
    configureText(*desired, QStringLiteral("Animator"));
    ASSERT_TRUE(desired->addAnimatorProperty(QStringLiteral("position")));
    ASSERT_TRUE(desired->addAnimatorProperty(QStringLiteral("opacity")));
    ASSERT_TRUE(
        setAnimator(*desired, 1, QStringLiteral("opacity"), QVariant(0.25)));
    const QJsonArray after = desired->textAnimatorStackSnapshot();

    Artifact::UndoManager manager;
    ASSERT_TRUE(manager.push(std::make_unique<Artifact::SetTextAnimatorStackCommand>(
        layer, before, after)));
    EXPECT_EQ(layer->animatorCount(), 2);
    EXPECT_EQ(layer->textAnimatorStackSnapshot(), after);

    manager.undo();
    EXPECT_EQ(layer->animatorCount(), 1);
    EXPECT_EQ(layer->textAnimatorStackSnapshot(), before);

    manager.redo();
    EXPECT_EQ(layer->animatorCount(), 2);
    EXPECT_EQ(layer->textAnimatorStackSnapshot(), after);
}

TEST(TextAnimatorLayerContractTest, LayerRenderReflectsAnimatorStack)
{
    Artifact::ArtifactTextLayer layer;
    configureText(layer, QStringLiteral("Animator"));
    layer.updateImage();
    if (!layer.hasCurrentFrameBuffer()) {
        GTEST_SKIP() << "text layer produced no frame buffer";
    }
    const AlphaSummary neutral = summarizeAlpha(layer);
    if (neutral.alpha <= 0.0) {
        GTEST_SKIP() << "host font produced no rasterized glyphs";
    }

    ASSERT_TRUE(layer.addAnimatorProperty(QStringLiteral("position")));
    ASSERT_TRUE(setAnimator(layer, 0, QStringLiteral("units"), QVariant(1)));
    ASSERT_TRUE(setAnimator(layer, 0, QStringLiteral("end"), QVariant(0.0)));
    ASSERT_TRUE(setAnimator(layer, 0, QStringLiteral("positionX"), QVariant(24.0)));
    layer.updateImage();
    const AlphaSummary moved = summarizeAlpha(layer);
    ASSERT_GT(moved.alpha, 0.0);
    EXPECT_GT(moved.weightedX / moved.alpha,
              neutral.weightedX / neutral.alpha + 0.5);

    ASSERT_TRUE(layer.addAnimatorProperty(QStringLiteral("opacity")));
    ASSERT_TRUE(setAnimator(layer, 1, QStringLiteral("units"), QVariant(1)));
    ASSERT_TRUE(setAnimator(layer, 1, QStringLiteral("end"), QVariant(0.0)));
    ASSERT_TRUE(setAnimator(layer, 1, QStringLiteral("opacity"), QVariant(0.25)));
    layer.updateImage();
    const AlphaSummary faded = summarizeAlpha(layer);
    EXPECT_LT(faded.alpha, moved.alpha - 0.1);
}
