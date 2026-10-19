#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QImage>
#include <QColor>
#include <QSize>
#include <QStandardPaths>
#include <QString>
#include <QTemporaryDir>
#include <QThread>
#include <QVariant>

#include <memory>
#include <utility>
#include <vector>

import Artifact.Composition.Abstract;
import Artifact.Composition.InitParams;
import Artifact.Layer.Abstract;
import Artifact.Layer.Factory;
import Artifact.Layer.Image;
import Artifact.Layers.SolidImage;
import Artifact.Layer.InitParams;
import Artifact.Render.DiligentDeviceManager;
import Artifact.Render.OffscreenComposition;
import Layer.State;
import Color.Float;
import Frame.Position;
import Physics.System;
import Utils.Id;
import Utils.String.UniString;

namespace {

constexpr int kWidth = 320;
constexpr int kHeight = 180;
constexpr int kTriggerFrame = 3;
constexpr int kFirstFrame = 1;
constexpr int kLastFrame = 10;

class FractureLayerRenderEnvironment final : public testing::Environment {
public:
    void SetUp() override
    {
        static int argc = 1;
        static char applicationName[] = "ArtifactFractureLayerRenderTest";
        static char* argv[] = {applicationName, nullptr};
        application_ = new QGuiApplication(argc, argv);
        // Diligent(Vulkan)はプロセス内で1デバイスのみ。同時複数生成は
        // 失敗するため、全テストで1つの共有レンダラーを使う。
        deviceManager_ = std::make_unique<Artifact::DiligentDeviceManager>();
        deviceManager_->initializeHeadless();
        if (deviceManager_->device()) {
            renderer_ = std::make_unique<Artifact::OffscreenCompositionRenderer>(
                deviceManager_->device(), deviceManager_->immediateContext(),
                kWidth, kHeight);
        }
    }

    void TearDown() override
    {
        renderer_.reset();
        deviceManager_.reset();
        delete application_;
        application_ = nullptr;
    }

    Artifact::OffscreenCompositionRenderer* renderer() const
    {
        return renderer_.get();
    }

private:
    QGuiApplication* application_ = nullptr;
    std::unique_ptr<Artifact::DiligentDeviceManager> deviceManager_;
    std::unique_ptr<Artifact::OffscreenCompositionRenderer> renderer_;
};

FractureLayerRenderEnvironment* const fractureLayerRenderEnvironment =
    new FractureLayerRenderEnvironment();
const bool fractureLayerRenderEnvironmentRegistered = [] {
    testing::AddGlobalTestEnvironment(fractureLayerRenderEnvironment);
    return true;
}();

class FracturePhysicsLodSettingsScope final {
public:
    FracturePhysicsLodSettingsScope()
        : system_(ArtifactCore::PhysicsSystem::instance())
        , previous_(system_.physicsLODSettings())
    {
    }

    ~FracturePhysicsLodSettingsScope()
    {
        system_.setPhysicsLODSettings(previous_);
    }

    ArtifactCore::PhysicsSystem& system() const { return system_; }

private:
    ArtifactCore::PhysicsSystem& system_;
    ArtifactCore::PhysicsLODSettings previous_;
};

QImage makeFourColorImage()
{
    QImage image(kWidth, kHeight, QImage::Format_RGBA8888);
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const bool left = x < kWidth / 2;
            const bool top = y < kHeight / 2;
            image.setPixelColor(
                x, y, top ? (left ? QColor(70, 130, 200, 255) : QColor(220, 90, 70, 255))
                          : (left ? QColor(80, 180, 110, 255) : QColor(230, 200, 80, 255)));
        }
    }
    return image;
}

int countDifferentPixels(const QImage& a, const QImage& b)
{
    if (a.size() != b.size() || a.isNull() || b.isNull()) {
        return -1;
    }
    const QImage left = a.convertToFormat(QImage::Format_ARGB32);
    const QImage right = b.convertToFormat(QImage::Format_ARGB32);
    int count = 0;
    for (int y = 0; y < left.height(); ++y) {
        for (int x = 0; x < left.width(); ++x) {
            if (left.pixel(x, y) != right.pixel(x, y)) {
                ++count;
            }
        }
    }
    return count;
}

bool enableFracture(Artifact::ArtifactAbstractLayer* layer)
{
    const std::pair<const char*, QVariant> props[] = {
        {"fracture.enabled", QVariant(true)},
        // preGenerate=false: trigger前は無傷、trigger後に破片が生成・飛散する。
        // trueだと初フレームから亀裂表示になり前後比較できない。
        {"fracture.preGenerate", QVariant(false)},
        {"fracture.triggerFrame", QVariant(kTriggerFrame)},
        {"fracture.shardCount", QVariant(12)},
    };
    for (const auto& [path, value] : props) {
        if (!layer->setLayerPropertyValue(QString::fromLatin1(path), value)) {
            return false;
        }
    }
    return true;
}

// Viewport with the widget removed: OffscreenCompositionRenderer owns a
// headless ArtifactIRenderer; renderLayerToQImage() drives the production
// layer->draw() path (incl. drawFractureOverlay) per frame.
bool renderLayerFrameRange(Artifact::ArtifactAbstractComposition* composition,
                           Artifact::ArtifactAbstractLayer* layer,
                           const QString& tag,
                           int firstFrame,
                           int lastFrame,
                           std::vector<QImage>* outFrames)
{
    Artifact::OffscreenCompositionRenderer* renderer =
        fractureLayerRenderEnvironment->renderer();
    if (!renderer) {
        return false;
    }

    const QString previewRoot =
        qEnvironmentVariable("ARTIFACT_FRACTURE_LAYER_PREVIEW_DIR");
    if (!previewRoot.isEmpty()) {
        QDir().mkpath(previewRoot);
    }

    outFrames->clear();
    for (int frame = firstFrame; frame <= lastFrame; ++frame) {
        composition->goToFrame(frame);
        // Render Queueと同様にコンポーネントシミュレーションを評価する。
        composition->evaluateLayerComponentSimulation(
            ArtifactCore::FramePosition(frame), false);
        const QImage image = renderer->renderLayerToQImage(
            ArtifactCore::FramePosition(frame), layer);
        if (image.isNull()) {
            return false;
        }
        if (!previewRoot.isEmpty()) {
            image.save(QDir(previewRoot).filePath(
                QStringLiteral("%1_%2.png").arg(tag).arg(frame, 4, 10, QLatin1Char('0'))),
                "PNG");
        }
        outFrames->push_back(image);
        QCoreApplication::processEvents();
    }
    return !outFrames->empty();
}

bool renderLayerFrames(Artifact::ArtifactAbstractComposition* composition,
                       Artifact::ArtifactAbstractLayer* layer,
                       const QString& tag,
                       QImage* firstImage,
                       QImage* lastImage)
{
    std::vector<QImage> frames;
    if (!renderLayerFrameRange(composition, layer, tag, kFirstFrame, kLastFrame, &frames)) {
        return false;
    }
    *firstImage = frames.front();
    *lastImage = frames.back();
    return true;
}

Artifact::ArtifactCompositionInitParams makeCompositionParams(const QString& name)
{
    Artifact::ArtifactCompositionInitParams params(
        ArtifactCore::UniString(name),
        ArtifactCore::FloatColor(0.012f, 0.018f, 0.032f, 1.0f));
    params.setResolution(kWidth, kHeight);
    params.setFrameRate(30.0);
    params.setDurationFrames(32);
    return params;
}

bool waitForImageDecode(Artifact::ArtifactImageLayer* layer)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 10000) {
        QCoreApplication::processEvents();
        (void)layer->toQImage();
        if (layer->hasCurrentFrameBuffer()) {
            const auto& buffer = layer->currentFrameBuffer();
            if (buffer.width() == kWidth && buffer.height() == kHeight) {
                return true;
            }
        }
        QThread::msleep(5);
    }
    return false;
}

} // namespace

TEST(ArtifactFractureLayerRenderTest, ImageLayerShattersAfterTriggerFrame)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    QCoreApplication::setApplicationName(QStringLiteral("ArtifactFractureImageTest"));
    QStandardPaths::setTestModeEnabled(true);

    const QString sourcePath = dir.filePath(QStringLiteral("fracture_source.png"));
    ASSERT_TRUE(makeFourColorImage().save(sourcePath, "PNG"));

    Artifact::ArtifactAbstractComposition composition(
        ArtifactCore::CompositionID(QStringLiteral("fracture-image-render")),
        makeCompositionParams(QStringLiteral("FractureImage")));

    Artifact::ArtifactLayerFactory layerFactory;
    Artifact::ArtifactImageInitParams imageParams(QStringLiteral("FractureImage"));
    imageParams.setImagePath(sourcePath);
    const auto layerResult = layerFactory.createLayer(imageParams);
    ASSERT_TRUE(layerResult.success);
    ASSERT_NE(layerResult.layer.get(), nullptr);
    auto* imageLayer =
        dynamic_cast<Artifact::ArtifactImageLayer*>(layerResult.layer.get());
    ASSERT_NE(imageLayer, nullptr);
    ASSERT_TRUE(waitForImageDecode(imageLayer));
    ASSERT_TRUE(enableFracture(imageLayer));
    ASSERT_TRUE(composition.appendLayerTop(layerResult.layer).success);

    QImage before, after;
    ASSERT_TRUE(renderLayerFrames(&composition, imageLayer,
                                  QStringLiteral("fracture_image"), &before, &after));

    const int changed = countDifferentPixels(before, after);
    EXPECT_GT(changed, 2000)
        << "frame " << kLastFrame << " looks identical to pre-trigger frame "
        << kFirstFrame << " (trigger did not fire visibly)";
}

TEST(ArtifactFractureLayerRenderTest, SolidPlaneLayerShattersAfterTriggerFrame)
{
    QCoreApplication::setApplicationName(QStringLiteral("ArtifactFractureSolidTest"));
    QStandardPaths::setTestModeEnabled(true);

    Artifact::ArtifactAbstractComposition composition(
        ArtifactCore::CompositionID(QStringLiteral("fracture-solid-render")),
        makeCompositionParams(QStringLiteral("FractureSolid")));

    Artifact::ArtifactLayerFactory layerFactory;
    Artifact::ArtifactSolidLayerInitParams solidParams(QStringLiteral("FractureSolid"));
    solidParams.setWidth(kWidth);
    solidParams.setHeight(kHeight);
    solidParams.setColor(ArtifactCore::FloatColor(0.95f, 0.45f, 0.10f, 1.0f));
    const auto layerResult = layerFactory.createLayer(solidParams);
    ASSERT_TRUE(layerResult.success);
    ASSERT_NE(layerResult.layer.get(), nullptr);
    auto* solidLayer = dynamic_cast<Artifact::ArtifactSolidImageLayer*>(
        layerResult.layer.get());
    ASSERT_NE(solidLayer, nullptr);
    ASSERT_TRUE(enableFracture(solidLayer));
    ASSERT_TRUE(composition.appendLayerTop(layerResult.layer).success);

    QImage before, after;
    ASSERT_TRUE(renderLayerFrames(&composition, solidLayer,
                                  QStringLiteral("fracture_solid"), &before, &after));

    const int changed = countDifferentPixels(before, after);
    EXPECT_GT(changed, 2000)
        << "frame " << kLastFrame << " looks identical to pre-trigger frame "
        << kFirstFrame << " (trigger did not fire visibly)";
}

TEST(ArtifactFractureLayerRenderTest,
     PhysicsLODScalesGeneratedShardAndDebrisCounts)
{
    if (!fractureLayerRenderEnvironment->renderer()) {
        GTEST_SKIP() << "Diligent headless rendering is unavailable on this host";
    }

    FracturePhysicsLodSettingsScope lodScope;
    struct FragmentCounts {
        int shards = 0;
        int debris = 0;
    };
    Artifact::ArtifactLayerFactory layerFactory;
    const auto evaluateFracture = [&](const QString& fixtureName,
                                      const ArtifactCore::PhysicsLODLevel level,
                                      FragmentCounts* outCounts) {
        if (!outCounts) return false;
        ArtifactCore::PhysicsLODSettings lodSettings;
        lodSettings.level = level;
        lodScope.system().setPhysicsLODSettings(lodSettings);

        Artifact::ArtifactAbstractComposition composition(
            ArtifactCore::CompositionID(fixtureName),
            makeCompositionParams(fixtureName));
        Artifact::ArtifactSolidLayerInitParams solidParams(fixtureName);
        solidParams.setWidth(kWidth);
        solidParams.setHeight(kHeight);
        solidParams.setColor(
            ArtifactCore::FloatColor(0.95f, 0.45f, 0.10f, 1.0f));
        const auto layerResult = layerFactory.createLayer(solidParams);
        if (!layerResult.success || !layerResult.layer) return false;

        auto* layer = layerResult.layer.get();
        const std::pair<const char*, QVariant> properties[] = {
            {"fracture.enabled", QVariant(true)},
            {"fracture.preGenerate", QVariant(true)},
            {"fracture.shardCount", QVariant(20)},
        };
        for (const auto& [path, value] : properties) {
            if (!layer->setLayerPropertyValue(QString::fromLatin1(path), value)) {
                return false;
            }
        }
        if (!composition.appendLayerTop(layerResult.layer).success) {
            return false;
        }

        composition.goToFrame(ArtifactCore::FramePosition(1));
        composition.evaluateLayerComponentSimulation(
            ArtifactCore::FramePosition(1), false);
        const QImage image = fractureLayerRenderEnvironment->renderer()
            ->renderLayerToQImage(ArtifactCore::FramePosition(1), layer);
        if (image.isNull()) return false;

        for (const auto& fragment : layer->layerEvaluationState().fragments) {
            if (fragment.debris) {
                ++outCounts->debris;
            } else {
                ++outCounts->shards;
            }
        }
        return true;
    };

    FragmentCounts full;
    FragmentCounts reduced;
    FragmentCounts minimal;
    ASSERT_TRUE(evaluateFracture(
        QStringLiteral("fracture-lod-full"),
        ArtifactCore::PhysicsLODLevel::Full, &full));
    ASSERT_TRUE(evaluateFracture(
        QStringLiteral("fracture-lod-reduced"),
        ArtifactCore::PhysicsLODLevel::Reduced, &reduced));
    ASSERT_TRUE(evaluateFracture(
        QStringLiteral("fracture-lod-minimal"),
        ArtifactCore::PhysicsLODLevel::Minimal, &minimal));

    EXPECT_GT(full.shards, 0);
    EXPECT_GT(full.debris, 0);
    EXPECT_EQ(reduced.shards, full.shards)
        << "the Reduced preset should retain the default fracture shard budget";
    EXPECT_EQ(reduced.debris, full.debris)
        << "the Reduced preset should retain the default fracture debris budget";
    EXPECT_LT(minimal.shards, full.shards)
        << "Minimal physics LOD should reduce generated fracture shards";
    EXPECT_LT(minimal.debris, full.debris)
        << "Minimal physics LOD should reduce generated fracture debris";

    // Reuse one layer across quality changes so reduced pre-generation cannot
    // leave a truncated shard budget behind after returning to Full.
    const QString transitionName = QStringLiteral("fracture-lod-transition");
    Artifact::ArtifactAbstractComposition transitionComposition(
        ArtifactCore::CompositionID(transitionName),
        makeCompositionParams(transitionName));
    Artifact::ArtifactSolidLayerInitParams transitionParams(transitionName);
    transitionParams.setWidth(kWidth);
    transitionParams.setHeight(kHeight);
    transitionParams.setColor(
        ArtifactCore::FloatColor(0.95f, 0.45f, 0.10f, 1.0f));
    const auto transitionResult = layerFactory.createLayer(transitionParams);
    ASSERT_TRUE(transitionResult.success);
    ASSERT_NE(transitionResult.layer.get(), nullptr);
    auto* transitionLayer = transitionResult.layer.get();
    const std::pair<const char*, QVariant> transitionProperties[] = {
        {"fracture.enabled", QVariant(true)},
        {"fracture.preGenerate", QVariant(true)},
        {"fracture.shardCount", QVariant(20)},
    };
    for (const auto& [path, value] : transitionProperties) {
        ASSERT_TRUE(transitionLayer->setLayerPropertyValue(
            QString::fromLatin1(path), value)) << path;
    }
    ASSERT_TRUE(transitionComposition.appendLayerTop(transitionResult.layer).success);

    const auto evaluateTransition = [&](const ArtifactCore::PhysicsLODLevel level) {
        ArtifactCore::PhysicsLODSettings settings;
        settings.level = level;
        lodScope.system().setPhysicsLODSettings(settings);
        transitionComposition.resetLayerComponentSimulation();
        transitionComposition.goToFrame(ArtifactCore::FramePosition(1));
        transitionComposition.evaluateLayerComponentSimulation(
            ArtifactCore::FramePosition(1), false);
        const QImage image = fractureLayerRenderEnvironment->renderer()
            ->renderLayerToQImage(ArtifactCore::FramePosition(1), transitionLayer);
        if (image.isNull()) return FragmentCounts{};
        FragmentCounts counts;
        for (const auto& fragment : transitionLayer->layerEvaluationState().fragments) {
            if (fragment.debris) ++counts.debris;
            else ++counts.shards;
        }
        return counts;
    };
    const auto evaluateCustomFractureScale = [&](const float shardScale,
                                                  const float debrisScale) {
        ArtifactCore::PhysicsLODSettings settings;
        settings.level = ArtifactCore::PhysicsLODLevel::Full;
        settings.fractureShardScale = shardScale;
        settings.fractureDebrisScale = debrisScale;
        lodScope.system().setPhysicsLODSettings(settings);
        transitionComposition.resetLayerComponentSimulation();
        transitionComposition.goToFrame(ArtifactCore::FramePosition(1));
        transitionComposition.evaluateLayerComponentSimulation(
            ArtifactCore::FramePosition(1), false);
        const QImage image = fractureLayerRenderEnvironment->renderer()
            ->renderLayerToQImage(ArtifactCore::FramePosition(1), transitionLayer);
        if (image.isNull()) return FragmentCounts{};
        FragmentCounts counts;
        for (const auto& fragment : transitionLayer->layerEvaluationState().fragments) {
            if (fragment.debris) ++counts.debris;
            else ++counts.shards;
        }
        return counts;
    };

    const FragmentCounts transitionFull = evaluateTransition(
        ArtifactCore::PhysicsLODLevel::Full);
    const FragmentCounts transitionMinimal = evaluateTransition(
        ArtifactCore::PhysicsLODLevel::Minimal);
    const FragmentCounts restoredFull = evaluateTransition(
        ArtifactCore::PhysicsLODLevel::Full);
    const FragmentCounts transitionReduced = evaluateTransition(
        ArtifactCore::PhysicsLODLevel::Reduced);
    const FragmentCounts restoredMinimal = evaluateTransition(
        ArtifactCore::PhysicsLODLevel::Minimal);
    const FragmentCounts restoredReduced = evaluateTransition(
        ArtifactCore::PhysicsLODLevel::Reduced);
    const FragmentCounts restoredFullAgain = evaluateTransition(
        ArtifactCore::PhysicsLODLevel::Full);
    const FragmentCounts shardScaledOnly = evaluateCustomFractureScale(
        0.25f, 1.0f);
    const FragmentCounts debrisScaledOnly = evaluateCustomFractureScale(
        1.0f, 0.25f);
    const FragmentCounts minimumScaled = evaluateCustomFractureScale(
        0.0f, 0.0f);
    const FragmentCounts aboveMaximumScaled = evaluateCustomFractureScale(
        2.0f, 2.0f);
    const FragmentCounts restoredFullAfterCustomScale = evaluateTransition(
        ArtifactCore::PhysicsLODLevel::Full);
    EXPECT_EQ(transitionFull.shards, full.shards);
    EXPECT_EQ(transitionFull.debris, full.debris);
    EXPECT_LT(transitionMinimal.shards, transitionFull.shards);
    EXPECT_LT(transitionMinimal.debris, transitionFull.debris);
    EXPECT_EQ(restoredFull.shards, transitionFull.shards)
        << "returning to Full should restore the original shard count";
    EXPECT_EQ(restoredFull.debris, transitionFull.debris)
        << "returning to Full should restore the original debris count";
    EXPECT_EQ(transitionReduced.shards, transitionFull.shards)
        << "Reduced should retain Full's configured shard budget";
    EXPECT_EQ(transitionReduced.debris, transitionFull.debris)
        << "Reduced should retain Full's configured debris budget";
    EXPECT_EQ(restoredMinimal.shards, transitionMinimal.shards)
        << "returning to Minimal should reproduce its prior shard budget";
    EXPECT_EQ(restoredMinimal.debris, transitionMinimal.debris)
        << "returning to Minimal should reproduce its prior debris budget";
    EXPECT_EQ(restoredReduced.shards, transitionReduced.shards)
        << "returning to Reduced should restore its prior shard budget";
    EXPECT_EQ(restoredReduced.debris, transitionReduced.debris)
        << "returning to Reduced should restore its prior debris budget";
    EXPECT_EQ(restoredFullAgain.shards, transitionFull.shards)
        << "repeated transitions should restore the Full shard budget";
    EXPECT_EQ(restoredFullAgain.debris, transitionFull.debris)
        << "repeated transitions should restore the Full debris budget";
    EXPECT_LT(shardScaledOnly.shards, transitionFull.shards)
        << "a custom shard scale should reduce only the shard budget";
    EXPECT_EQ(shardScaledOnly.debris, transitionFull.debris)
        << "a full debris scale should preserve the debris budget";
    EXPECT_EQ(debrisScaledOnly.shards, transitionFull.shards)
        << "a full shard scale should preserve the shard budget";
    EXPECT_LT(debrisScaledOnly.debris, transitionFull.debris)
        << "a custom debris scale should reduce only the debris budget";
    EXPECT_GT(minimumScaled.shards, 0)
        << "the minimum fracture LOD should retain at least one shard";
    EXPECT_LT(minimumScaled.shards, transitionFull.shards)
        << "the minimum fracture LOD should clamp shard generation down";
    EXPECT_EQ(minimumScaled.debris, 0)
        << "the minimum debris scale should allow the debris budget to reach zero";
    EXPECT_EQ(aboveMaximumScaled.shards, transitionFull.shards)
        << "scales above one should clamp to the Full shard budget";
    EXPECT_EQ(aboveMaximumScaled.debris, transitionFull.debris)
        << "scales above one should clamp to the Full debris budget";
    EXPECT_EQ(restoredFullAfterCustomScale.shards, transitionFull.shards)
        << "returning to Full should restore shards after a custom scale";
    EXPECT_EQ(restoredFullAfterCustomScale.debris, transitionFull.debris)
        << "returning to Full should restore debris after a custom scale";
}

TEST(ArtifactFractureLayerRenderTest, RewoundReplayReproducesSameShatter)
{
    QCoreApplication::setApplicationName(QStringLiteral("ArtifactFractureSeekTest"));
    QStandardPaths::setTestModeEnabled(true);

    Artifact::ArtifactAbstractComposition composition(
        ArtifactCore::CompositionID(QStringLiteral("fracture-seek-render")),
        makeCompositionParams(QStringLiteral("FractureSeek")));

    Artifact::ArtifactLayerFactory layerFactory;
    Artifact::ArtifactSolidLayerInitParams solidParams(QStringLiteral("FractureSeek"));
    solidParams.setWidth(kWidth);
    solidParams.setHeight(kHeight);
    solidParams.setColor(ArtifactCore::FloatColor(0.20f, 0.55f, 0.90f, 1.0f));
    const auto layerResult = layerFactory.createLayer(solidParams);
    ASSERT_TRUE(layerResult.success);
    auto* solidLayer = dynamic_cast<Artifact::ArtifactSolidImageLayer*>(
        layerResult.layer.get());
    ASSERT_NE(solidLayer, nullptr);
    ASSERT_TRUE(enableFracture(solidLayer));
    ASSERT_TRUE(composition.appendLayerTop(layerResult.layer).success);

    // 連続再生1周目と、巻き戻し後の2周目は同一絵になる。
    std::vector<QImage> firstPass;
    ASSERT_TRUE(renderLayerFrameRange(&composition, solidLayer,
                                      QStringLiteral("fracture_seek_a"),
                                      kFirstFrame, kLastFrame, &firstPass));
    ASSERT_EQ(firstPass.size(), static_cast<std::size_t>(kLastFrame - kFirstFrame + 1));
    std::vector<QImage> secondPass;
    ASSERT_TRUE(renderLayerFrameRange(&composition, solidLayer,
                                      QStringLiteral("fracture_seek_b"),
                                      kFirstFrame, kLastFrame, &secondPass));
    ASSERT_EQ(firstPass.size(), secondPass.size());
    for (std::size_t i = 0; i < firstPass.size(); ++i) {
        EXPECT_EQ(countDifferentPixels(firstPass[i], secondPass[i]), 0)
            << "rewound replay diverged at frame " << (kFirstFrame + static_cast<int>(i));
    }

    // 非連続seek: frame1の直後にframe10だけ描くと、実行状態は持ち越されず
    // trigger直後の静止破片になる (=連続再生のframe10とは異なる)。
    composition.goToFrame(kFirstFrame);
    std::vector<QImage> jumped;
    ASSERT_TRUE(renderLayerFrameRange(&composition, solidLayer,
                                      QStringLiteral("fracture_seek_jump"),
                                      kFirstFrame, kFirstFrame, &jumped));
    ASSERT_EQ(jumped.size(), 1u);
    std::vector<QImage> jumpTarget;
    ASSERT_TRUE(renderLayerFrameRange(&composition, solidLayer,
                                      QStringLiteral("fracture_seek_jump"),
                                      kLastFrame, kLastFrame, &jumpTarget));
    ASSERT_EQ(jumpTarget.size(), 1u);
    EXPECT_GT(countDifferentPixels(firstPass.back(), jumpTarget.front()), 0)
        << "discontinuous seek wrongly carried over continuous-run motion";
    // 同じjumpを繰り返すと同一絵 (決定論的リセット)。
    std::vector<QImage> jumpRepeat;
    composition.goToFrame(kFirstFrame);
    ASSERT_TRUE(renderLayerFrameRange(&composition, solidLayer,
                                      QStringLiteral("fracture_seek_jump"),
                                      kFirstFrame, kFirstFrame, &jumped));
    ASSERT_TRUE(renderLayerFrameRange(&composition, solidLayer,
                                      QStringLiteral("fracture_seek_jump"),
                                      kLastFrame, kLastFrame, &jumpRepeat));
    ASSERT_EQ(jumpRepeat.size(), 1u);
    EXPECT_EQ(countDifferentPixels(jumpTarget.front(), jumpRepeat.front()), 0)
        << "repeated discontinuous seek is not deterministic";
}

TEST(ArtifactFractureLayerRenderTest, ShardsBounceAndSettleOnFloor)
{
    QCoreApplication::setApplicationName(QStringLiteral("ArtifactFractureFloorTest"));
    QStandardPaths::setTestModeEnabled(true);

    Artifact::ArtifactAbstractComposition composition(
        ArtifactCore::CompositionID(QStringLiteral("fracture-floor-render")),
        makeCompositionParams(QStringLiteral("FractureFloor")));

    Artifact::ArtifactLayerFactory layerFactory;
    Artifact::ArtifactSolidLayerInitParams solidParams(QStringLiteral("FractureFloor"));
    solidParams.setWidth(kWidth);
    solidParams.setHeight(kHeight);
    solidParams.setColor(ArtifactCore::FloatColor(0.90f, 0.25f, 0.55f, 1.0f));
    const auto layerResult = layerFactory.createLayer(solidParams);
    ASSERT_TRUE(layerResult.success);
    auto* solidLayer = dynamic_cast<Artifact::ArtifactSolidImageLayer*>(
        layerResult.layer.get());
    ASSERT_NE(solidLayer, nullptr);
    ASSERT_TRUE(enableFracture(solidLayer));
    // 重力落下 (注: component.collision.enabled=true との併用では
    // 破片が一様に上方へ射出される製品不具合があるため、本テストでは
    // 衝突を有効化しない。詳細は最終報告の「既知の不具合」を参照)。
    ASSERT_TRUE(solidLayer->setLayerPropertyValue(
        QStringLiteral("fracture.shardGravity"), QVariant(1500.0)));
    ASSERT_TRUE(composition.appendLayerTop(layerResult.layer).success);

    constexpr int kSettleLast = 30;
    std::vector<QImage> frames;
    ASSERT_TRUE(renderLayerFrameRange(&composition, solidLayer,
                                      QStringLiteral("fracture_floor"),
                                      kFirstFrame, kSettleLast, &frames));
    ASSERT_EQ(frames.size(), static_cast<std::size_t>(kSettleLast - kFirstFrame + 1));
    const QImage& falling = frames[7];   // frame 8: 落下中
    const QImage& last = frames.back();  // frame 30
    // 落下で絵が大きく変わる (運動が発生した)。
    EXPECT_GT(countDifferentPixels(falling, last), 2000)
        << "shards never visibly fell";
    // 全 active 破片が有限値に留まる (NaN/発散なし)。
    const auto& fragments = solidLayer->layerEvaluationState().fragments;
    int activeCount = 0;
    for (std::size_t i = 0; i < fragments.size(); ++i) {
        const auto& fragment = fragments[i];
        if (!fragment.active || fragment.opacity <= 0.0f) {
            continue;
        }
        ++activeCount;
        const QVector3D center = fragment.transform.column(3).toVector3D();
        EXPECT_TRUE(std::isfinite(center.x()));
        EXPECT_TRUE(std::isfinite(center.y()));
        EXPECT_TRUE(std::isfinite(fragment.linearVelocity.x()));
        EXPECT_TRUE(std::isfinite(fragment.linearVelocity.y()));
    }
    EXPECT_GT(activeCount, 0) << "no active fragments left to inspect";
}
