#include <gtest/gtest.h>

#include <QDir>
#include <QFile>
#include <QJsonObject>
#include <QJsonValue>
#include <QFileInfo>
#include <QProcess>
#include <QString>
#include <QTemporaryDir>

import Artifact.Layer.Video;
import Image.ImageF32x4_RGBA;
import Proxy.Service;

TEST(VideoProxyQualityLodContractTest, PreservesEverySupportedProxyScale)
{
    Artifact::ArtifactVideoLayer layer;
    const Artifact::ProxyQuality qualities[] = {
        Artifact::ProxyQuality::None,
        Artifact::ProxyQuality::Eighth,
        Artifact::ProxyQuality::Quarter,
        Artifact::ProxyQuality::Half,
        Artifact::ProxyQuality::Full,
    };

    for (const auto quality : qualities) {
        layer.setProxyQuality(quality);
        EXPECT_EQ(layer.proxyQuality(), quality);
    }
}

TEST(VideoProxyQualityLodContractTest, ClampsUnknownProxyQualityValues)
{
    Artifact::ArtifactVideoLayer layer;

    layer.setProxyQuality(static_cast<Artifact::ProxyQuality>(-1));
    EXPECT_EQ(layer.proxyQuality(), Artifact::ProxyQuality::None);

    layer.setProxyQuality(static_cast<Artifact::ProxyQuality>(100));
    EXPECT_EQ(layer.proxyQuality(), Artifact::ProxyQuality::Eighth);
}

TEST(VideoProxyQualityLodContractTest,
     InvalidReducedProxyFallsBackToSourceAndFullSkipsProxyOpen)
{
    QTemporaryDir temporaryDirectory;
    ASSERT_TRUE(temporaryDirectory.isValid());

    const QString proxyPath = QDir(temporaryDirectory.path()).filePath(
        QStringLiteral("invalid_proxy_eighth.mp4"));
    QFile proxyFile(proxyPath);
    ASSERT_TRUE(proxyFile.open(QIODevice::WriteOnly));
    ASSERT_GT(proxyFile.write("not a video"), 0);
    proxyFile.close();

    Artifact::ArtifactVideoLayer layer;
    layer.setProxyPath(proxyPath);
    ASSERT_TRUE(layer.hasProxy());

    layer.setProxyQuality(Artifact::ProxyQuality::Eighth);
    EXPECT_EQ(layer.proxyQuality(), Artifact::ProxyQuality::None)
        << "a reduced proxy that cannot be opened should fall back to source";
    EXPECT_EQ(layer.proxyPath(), proxyPath)
        << "proxy metadata should remain available for regeneration or retry";

    layer.setProxyQuality(Artifact::ProxyQuality::Full);
    EXPECT_EQ(layer.proxyQuality(), Artifact::ProxyQuality::Full)
        << "Full quality should use the source without opening a proxy file";
}

TEST(VideoProxyQualityLodContractTest,
     SelectedProxyQualityRetriesWhenItsFileAppearsLater)
{
    const QString sourceDirectory =
        QFileInfo(QString::fromUtf8(__FILE__)).absolutePath();
    const QString fixturePath = QDir::cleanPath(QDir(sourceDirectory).filePath(
        QStringLiteral("../ArtifactCore/fixtures/walkway-orbit-cc0.mp4")));
    if (!QFileInfo(fixturePath).isFile()) {
        GTEST_SKIP() << "The existing video fixture is required to verify proxy retry";
    }

    QTemporaryDir temporaryDirectory;
    ASSERT_TRUE(temporaryDirectory.isValid());
    const QString futureProxyPath = QDir(temporaryDirectory.path()).filePath(
        QStringLiteral("future_proxy_quarter.mp4"));

    Artifact::ArtifactVideoLayer layer;
    layer.setProxyPath(futureProxyPath);
    layer.setProxyQuality(Artifact::ProxyQuality::Quarter);
    EXPECT_EQ(layer.proxyQuality(), Artifact::ProxyQuality::Quarter)
        << "the requested quality should remain selected while its file is missing";
    EXPECT_FALSE(layer.hasProxy());

    ASSERT_TRUE(QFile::copy(fixturePath, futureProxyPath));
    layer.setProxyPath(futureProxyPath);
    EXPECT_TRUE(layer.hasProxy());
    EXPECT_EQ(layer.proxyQuality(), Artifact::ProxyQuality::Quarter)
        << "setting the generated file path should retry and open the selected proxy";

    const QString regeneratedProxyPath = QDir(temporaryDirectory.path()).filePath(
        QStringLiteral("regenerated_proxy_quarter.mp4"));
    layer.setProxyPath(regeneratedProxyPath);
    EXPECT_FALSE(layer.hasProxy());
    EXPECT_EQ(layer.proxyQuality(), Artifact::ProxyQuality::Quarter)
        << "a missing replacement proxy should preserve the requested quality";

    ASSERT_TRUE(QFile::copy(fixturePath, regeneratedProxyPath));
    layer.setProxyPath(regeneratedProxyPath);
    EXPECT_TRUE(layer.hasProxy());
    EXPECT_EQ(layer.proxyQuality(), Artifact::ProxyQuality::Quarter)
        << "regenerating the same proxy should restore the selected quality";
}

TEST(VideoProxyQualityLodContractTest,
     ValidReducedProxyRemainsSelectedAndFullClearsTheProxyController)
{
    const QString sourceDirectory =
        QFileInfo(QString::fromUtf8(__FILE__)).absolutePath();
    const QString fixturePath = QDir::cleanPath(QDir(sourceDirectory).filePath(
        QStringLiteral("../ArtifactCore/fixtures/walkway-orbit-cc0.mp4")));
    const QFileInfo fixtureInfo(fixturePath);
    ASSERT_TRUE(fixtureInfo.isFile())
        << "The existing video fixture is required to exercise a valid proxy";

    Artifact::ArtifactVideoLayer layer;
    layer.setProxyPath(fixturePath);
    ASSERT_TRUE(layer.hasProxy());

    for (const Artifact::ProxyQuality quality : {
             Artifact::ProxyQuality::Eighth,
             Artifact::ProxyQuality::Quarter,
             Artifact::ProxyQuality::Half}) {
        layer.setProxyQuality(quality);
        EXPECT_EQ(layer.proxyQuality(), quality)
            << "a proxy that opens successfully should retain requested quality "
            << static_cast<int>(quality);
        EXPECT_EQ(layer.proxyPath(), fixturePath);

        layer.setProxyQuality(Artifact::ProxyQuality::Full);
        EXPECT_EQ(layer.proxyQuality(), Artifact::ProxyQuality::Full)
            << "Full quality should switch back to the source controller";
        EXPECT_EQ(layer.proxyPath(), fixturePath)
            << "changing quality should preserve proxy metadata";
    }

    layer.setProxyQuality(Artifact::ProxyQuality::Quarter);
    EXPECT_EQ(layer.proxyQuality(), Artifact::ProxyQuality::Quarter)
        << "a valid reduced proxy should remain selectable after cycling through Full";
    EXPECT_EQ(layer.proxyPath(), fixturePath)
        << "cycling quality should keep the proxy source available";
}

TEST(VideoProxyQualityLodContractTest,
     GeneratedProxyDimensionsFollowEveryQualityScale)
{
    QProcess encoderProbe;
    encoderProbe.start(QStringLiteral("ffmpeg"), {QStringLiteral("-version")});
    if (!encoderProbe.waitForStarted(1000)) {
        GTEST_SKIP() << "The ffmpeg executable required for proxy generation is unavailable";
    }
    ASSERT_TRUE(encoderProbe.waitForFinished(10000));
    ASSERT_EQ(encoderProbe.exitCode(), 0);

    const QString sourceDirectory =
        QFileInfo(QString::fromUtf8(__FILE__)).absolutePath();
    const QString fixturePath = QDir::cleanPath(QDir(sourceDirectory).filePath(
        QStringLiteral("../ArtifactCore/fixtures/walkway-orbit-cc0.mp4")));
    ASSERT_TRUE(QFileInfo(fixturePath).isFile())
        << "The existing video fixture is required to exercise generated proxies";

    QTemporaryDir temporaryDirectory;
    ASSERT_TRUE(temporaryDirectory.isValid());
    const QString sourcePath = QDir(temporaryDirectory.path()).filePath(
        QStringLiteral("proxy-lod-source.mp4"));
    ASSERT_TRUE(QFile::copy(fixturePath, sourcePath));

    Artifact::ArtifactVideoLayer source;
    ASSERT_TRUE(source.loadFromPath(sourcePath));
    const int sourceWidth = source.streamInfo().width;
    const int sourceHeight = source.streamInfo().height;
    ASSERT_GT(sourceWidth, 0);
    ASSERT_GT(sourceHeight, 0);

    struct ProxyDimensionCase {
        Artifact::ProxyQuality quality;
        double scale;
    };
    constexpr ProxyDimensionCase cases[]{
        {Artifact::ProxyQuality::Eighth, 0.125},
        {Artifact::ProxyQuality::Quarter, 0.25},
        {Artifact::ProxyQuality::Half, 0.5},
        {Artifact::ProxyQuality::Full, 1.0},
    };
    const auto expectedDimension = [](const int sourceDimension,
                                      const double scale) {
        return static_cast<int>(std::trunc(
            static_cast<double>(sourceDimension) * scale / 2.0) * 2.0);
    };

    auto* manager = Artifact::ArtifactProxyManager::instance();
    ASSERT_NE(manager, nullptr);
    for (const ProxyDimensionCase& testCase : cases) {
        const QString proxyPath = manager->generateProxy(
            sourcePath, static_cast<Artifact::ProxyServiceQuality>(
                            testCase.quality));
        ASSERT_FALSE(proxyPath.isEmpty())
            << "proxy generation failed at quality "
            << static_cast<int>(testCase.quality);
        ASSERT_TRUE(QFileInfo(proxyPath).isFile());

        Artifact::ArtifactVideoLayer proxy;
        ASSERT_TRUE(proxy.loadFromPath(proxyPath))
            << "generated proxy could not be reopened at quality "
            << static_cast<int>(testCase.quality);
        EXPECT_EQ(proxy.streamInfo().width,
                  expectedDimension(sourceWidth, testCase.scale))
            << "quality=" << static_cast<int>(testCase.quality);
        EXPECT_EQ(proxy.streamInfo().height,
                  expectedDimension(sourceHeight, testCase.scale))
            << "quality=" << static_cast<int>(testCase.quality);
        const ArtifactCore::ImageF32x4_RGBA decodedFrame =
            proxy.decodeFrameToImageBuffer(0);
        ASSERT_FALSE(decodedFrame.isEmpty())
            << "generated proxy should decode a frame at quality "
            << static_cast<int>(testCase.quality);
        EXPECT_EQ(decodedFrame.width(),
                  expectedDimension(sourceWidth, testCase.scale));
        EXPECT_EQ(decodedFrame.height(),
                  expectedDimension(sourceHeight, testCase.scale));
        EXPECT_TRUE(manager->clearProxy(proxyPath));
    }
}

TEST(VideoProxyQualityLodContractTest,
     GeneratedProxiesKeepOddSourceDimensionsEvenAtEveryLOD)
{
    QProcess encoderProbe;
    encoderProbe.start(QStringLiteral("ffmpeg"), {QStringLiteral("-version")});
    if (!encoderProbe.waitForStarted(1000)) {
        GTEST_SKIP() << "The ffmpeg executable required for proxy generation is unavailable";
    }
    ASSERT_TRUE(encoderProbe.waitForFinished(10000));
    ASSERT_EQ(encoderProbe.exitCode(), 0);

    QTemporaryDir temporaryDirectory;
    ASSERT_TRUE(temporaryDirectory.isValid());
    constexpr int sourceWidth = 65;
    constexpr int sourceHeight = 35;
    const QString sourcePath = QDir(temporaryDirectory.path()).filePath(
        QStringLiteral("odd-dimension-lod-source.mkv"));

    QProcess sourceEncoder;
    sourceEncoder.start(QStringLiteral("ffmpeg"), {
        QStringLiteral("-y"), QStringLiteral("-f"), QStringLiteral("lavfi"),
        QStringLiteral("-i"), QStringLiteral("color=c=red:s=65x35:r=1,format=yuv444p"),
        QStringLiteral("-frames:v"), QStringLiteral("1"),
        QStringLiteral("-c:v"), QStringLiteral("ffv1"), sourcePath});
    ASSERT_TRUE(sourceEncoder.waitForStarted(1000));
    ASSERT_TRUE(sourceEncoder.waitForFinished(30000));
    ASSERT_EQ(sourceEncoder.exitCode(), 0)
        << sourceEncoder.readAllStandardError().toStdString();
    ASSERT_TRUE(QFileInfo(sourcePath).isFile());

    Artifact::ArtifactVideoLayer source;
    ASSERT_TRUE(source.loadFromPath(sourcePath));
    ASSERT_EQ(source.streamInfo().width, sourceWidth);
    ASSERT_EQ(source.streamInfo().height, sourceHeight);

    struct ProxyDimensionCase {
        Artifact::ProxyQuality quality;
        int width;
        int height;
    };
    constexpr ProxyDimensionCase cases[] = {
        {Artifact::ProxyQuality::Eighth, 8, 4},
        {Artifact::ProxyQuality::Quarter, 16, 8},
        {Artifact::ProxyQuality::Half, 32, 16},
        {Artifact::ProxyQuality::Full, 64, 34},
    };

    auto* manager = Artifact::ArtifactProxyManager::instance();
    ASSERT_NE(manager, nullptr);
    for (const ProxyDimensionCase& testCase : cases) {
        const QString proxyPath = manager->generateProxy(
            sourcePath, static_cast<Artifact::ProxyServiceQuality>(
                            testCase.quality));
        ASSERT_FALSE(proxyPath.isEmpty())
            << "proxy generation failed at quality "
            << static_cast<int>(testCase.quality);
        Artifact::ArtifactVideoLayer proxy;
        ASSERT_TRUE(proxy.loadFromPath(proxyPath));
        EXPECT_EQ(proxy.streamInfo().width, testCase.width)
            << "quality=" << static_cast<int>(testCase.quality);
        EXPECT_EQ(proxy.streamInfo().height, testCase.height)
            << "quality=" << static_cast<int>(testCase.quality);
        const ArtifactCore::ImageF32x4_RGBA decodedFrame =
            proxy.decodeFrameToImageBuffer(0);
        ASSERT_FALSE(decodedFrame.isEmpty())
            << "generated odd-dimension proxy should decode at quality "
            << static_cast<int>(testCase.quality);
        EXPECT_EQ(decodedFrame.width(), testCase.width);
        EXPECT_EQ(decodedFrame.height(), testCase.height);
        EXPECT_TRUE(manager->clearProxy(proxyPath));
    }
}

TEST(VideoProxyQualityLodContractTest,
     GeneratedProxyQualitiesPreserveClipTimingAndFrameOrder)
{
    QProcess encoderProbe;
    encoderProbe.start(QStringLiteral("ffmpeg"), {QStringLiteral("-version")});
    if (!encoderProbe.waitForStarted(1000)) {
        GTEST_SKIP() << "The ffmpeg executable required for proxy generation is unavailable";
    }
    ASSERT_TRUE(encoderProbe.waitForFinished(10000));
    ASSERT_EQ(encoderProbe.exitCode(), 0);

    QTemporaryDir temporaryDirectory;
    ASSERT_TRUE(temporaryDirectory.isValid());
    const QString sourcePath = QDir(temporaryDirectory.path()).filePath(
        QStringLiteral("timing-lod-source.mp4"));
    QProcess sourceEncoder;
    sourceEncoder.start(QStringLiteral("ffmpeg"), {
        QStringLiteral("-y"),
        QStringLiteral("-f"), QStringLiteral("lavfi"),
        QStringLiteral("-i"), QStringLiteral("color=c=red:s=64x48:r=2:d=1"),
        QStringLiteral("-f"), QStringLiteral("lavfi"),
        QStringLiteral("-i"), QStringLiteral("color=c=blue:s=64x48:r=2:d=1"),
        QStringLiteral("-filter_complex"),
        QStringLiteral("[0:v][1:v]concat=n=2:v=1:a=0[outv]"),
        QStringLiteral("-map"), QStringLiteral("[outv]"),
        QStringLiteral("-frames:v"), QStringLiteral("4"),
        QStringLiteral("-r"), QStringLiteral("2"),
        QStringLiteral("-c:v"), QStringLiteral("libx264"),
        QStringLiteral("-pix_fmt"), QStringLiteral("yuv420p"),
        sourcePath});
    ASSERT_TRUE(sourceEncoder.waitForStarted(1000));
    ASSERT_TRUE(sourceEncoder.waitForFinished(30000));
    ASSERT_EQ(sourceEncoder.exitCode(), 0)
        << sourceEncoder.readAllStandardError().toStdString();

    Artifact::ArtifactVideoLayer source;
    ASSERT_TRUE(source.loadFromPath(sourcePath));
    ASSERT_EQ(source.streamInfo().frameCount, 4);
    ASSERT_NEAR(source.streamInfo().frameRate, 2.0, 0.01);
    ASSERT_NEAR(source.streamInfo().duration, 2.0, 0.1);

    auto* manager = Artifact::ArtifactProxyManager::instance();
    ASSERT_NE(manager, nullptr);
    for (const Artifact::ProxyQuality quality : {
             Artifact::ProxyQuality::Eighth,
             Artifact::ProxyQuality::Quarter,
             Artifact::ProxyQuality::Half,
             Artifact::ProxyQuality::Full}) {
        const QString proxyPath = manager->generateProxy(
            sourcePath,
            static_cast<Artifact::ProxyServiceQuality>(quality));
        ASSERT_FALSE(proxyPath.isEmpty())
            << "proxy generation failed at quality " << static_cast<int>(quality);

        Artifact::ArtifactVideoLayer proxy;
        ASSERT_TRUE(proxy.loadFromPath(proxyPath));
        EXPECT_EQ(proxy.streamInfo().frameCount, source.streamInfo().frameCount)
            << "quality=" << static_cast<int>(quality);
        EXPECT_NEAR(proxy.streamInfo().frameRate, source.streamInfo().frameRate,
                    0.01)
            << "quality=" << static_cast<int>(quality);
        EXPECT_NEAR(proxy.streamInfo().duration, source.streamInfo().duration,
                    0.1)
            << "quality=" << static_cast<int>(quality);

        const auto firstFrame = proxy.decodeFrameToImageBuffer(0);
        const auto lastFrame = proxy.decodeFrameToImageBuffer(3);
        ASSERT_FALSE(firstFrame.isEmpty());
        ASSERT_FALSE(lastFrame.isEmpty());
        const auto firstPixel =
            firstFrame.getPixel(firstFrame.width() / 2,
                                firstFrame.height() / 2);
        const auto lastPixel =
            lastFrame.getPixel(lastFrame.width() / 2,
                               lastFrame.height() / 2);
        EXPECT_GT(firstPixel.r(), firstPixel.b())
            << "the first red frame should remain first at quality "
            << static_cast<int>(quality);
        EXPECT_GT(lastPixel.b(), lastPixel.r())
            << "the final blue frame should remain last at quality "
            << static_cast<int>(quality);
        EXPECT_TRUE(manager->clearProxy(proxyPath));
    }
}

TEST(VideoProxyQualityLodContractTest,
     ProxyScaleCalculationKeepsOddAndTinyInputsAtValidEvenDimensions)
{
    struct DimensionCase {
        int source;
        double scale;
        int expected;
    };
    constexpr DimensionCase cases[] = {
        {1, 0.125, 2}, {1, 0.25, 2}, {1, 0.5, 2}, {1, 1.0, 2},
        {3, 0.125, 2}, {3, 0.25, 2}, {3, 0.5, 2}, {3, 1.0, 2},
        {17, 0.125, 2}, {17, 0.25, 4}, {17, 0.5, 8}, {17, 1.0, 16},
        {33, 0.125, 4}, {33, 0.25, 8}, {33, 0.5, 16}, {33, 1.0, 32},
    };

    const auto scaledEvenDimension = [](const int sourceDimension,
                                        const double scale) {
        const int scaled = static_cast<int>(std::trunc(
            static_cast<double>(sourceDimension) * scale / 2.0) * 2.0);
        return std::max(2, scaled);
    };

    for (const DimensionCase& testCase : cases) {
        EXPECT_EQ(scaledEvenDimension(testCase.source, testCase.scale),
                  testCase.expected)
            << "source=" << testCase.source
            << " scale=" << testCase.scale;
    }
}

TEST(VideoProxyQualityLodContractTest, ProxyServiceReportsExpectedScaleFactors)
{
    Artifact::ArtifactProxyManager* manager =
        Artifact::ArtifactProxyManager::instance();
    ASSERT_NE(manager, nullptr);

    struct QualityScaleCase {
        Artifact::ProxyQuality quality;
        float expectedScale;
    };
    const QualityScaleCase cases[] = {
        {Artifact::ProxyQuality::None, 1.0f},
        {Artifact::ProxyQuality::Eighth, 0.125f},
        {Artifact::ProxyQuality::Quarter, 0.25f},
        {Artifact::ProxyQuality::Half, 0.5f},
        {Artifact::ProxyQuality::Full, 1.0f},
    };
    const QString sourcePath = QStringLiteral("missing-lod-fixture.mp4");

    for (const auto& testCase : cases) {
        const auto info = manager->getProxyInfo(
            sourcePath, testCase.quality);
        EXPECT_EQ(info.sourcePath, sourcePath);
        EXPECT_EQ(info.quality, testCase.quality);
        EXPECT_FALSE(info.hasProxy);
        EXPECT_FLOAT_EQ(info.scaleFactor, testCase.expectedScale);
        EXPECT_EQ(info.proxyPath,
                  Artifact::ArtifactProxyManager::proxyFilePath(
                      sourcePath,
                      static_cast<Artifact::ProxyServiceQuality>(
                          testCase.quality)));
    }
}

TEST(VideoProxyQualityLodContractTest,
     UnknownServiceQualityUsesTheSameFallbackAsVideoLayerQuality)
{
    Artifact::ArtifactVideoLayer layer;
    layer.setProxyQuality(static_cast<Artifact::ProxyQuality>(100));
    ASSERT_EQ(layer.proxyQuality(), Artifact::ProxyQuality::Eighth);

    Artifact::ArtifactProxyManager* manager =
        Artifact::ArtifactProxyManager::instance();
    ASSERT_NE(manager, nullptr);
    const QString sourcePath = QStringLiteral("missing-unknown-lod-fixture.mp4");
    const auto info = manager->getProxyInfo(
        sourcePath, static_cast<Artifact::ProxyServiceQuality>(100));

    EXPECT_EQ(info.quality, layer.proxyQuality());
    EXPECT_EQ(info.proxyPath, Artifact::ArtifactProxyManager::proxyFilePath(
                                  sourcePath, layer.proxyQuality()));
    EXPECT_FLOAT_EQ(info.scaleFactor, 0.125f);
}

TEST(VideoProxyQualityLodContractTest, UsesDistinctProxyPathsForEachQuality)
{
    const QString sourcePath = QStringLiteral("/media/shot.v2.mp4");
    const QString expectedDirectory = QStringLiteral("/media/.proxy");
    const struct ProxyPathCase {
        Artifact::ProxyQuality quality;
        QString fileName;
    } cases[] = {
        {Artifact::ProxyQuality::Eighth,
         QStringLiteral("shot.v2_proxy_eighth.mp4")},
        {Artifact::ProxyQuality::Quarter,
         QStringLiteral("shot.v2_proxy_quarter.mp4")},
        {Artifact::ProxyQuality::Half,
         QStringLiteral("shot.v2_proxy_half.mp4")},
        {Artifact::ProxyQuality::Full,
         QStringLiteral("shot.v2_proxy_full.mp4")},
    };

    QString previousPath;
    for (const auto& testCase : cases) {
        const QString proxyPath = Artifact::ArtifactProxyManager::proxyFilePath(
            sourcePath, static_cast<Artifact::ProxyServiceQuality>(testCase.quality));
        EXPECT_EQ(QFileInfo(proxyPath).absolutePath(), expectedDirectory);
        EXPECT_EQ(QFileInfo(proxyPath).fileName(), testCase.fileName);
        EXPECT_NE(proxyPath, previousPath);
        previousPath = proxyPath;
    }

    EXPECT_TRUE(Artifact::ArtifactProxyManager::proxyFilePath(
                    sourcePath, Artifact::ProxyServiceQuality::None).isEmpty());
    EXPECT_TRUE(Artifact::ArtifactProxyManager::proxyFilePath(
                    QString(), Artifact::ProxyServiceQuality::Half).isEmpty());
}

TEST(VideoProxyQualityLodContractTest,
     ReportsOnlyTheExistingProxyAtEachQualityPath)
{
    QTemporaryDir temporaryDirectory;
    ASSERT_TRUE(temporaryDirectory.isValid());

    const QString sourcePath = QDir(temporaryDirectory.path()).filePath(
        QStringLiteral("shot.mp4"));
    QFile sourceFile(sourcePath);
    ASSERT_TRUE(sourceFile.open(QIODevice::WriteOnly));
    ASSERT_GT(sourceFile.write("fixture"), 0);
    sourceFile.close();

    Artifact::ArtifactProxyManager* manager =
        Artifact::ArtifactProxyManager::instance();
    ASSERT_NE(manager, nullptr);

    struct ExistingProxyCase {
        Artifact::ProxyServiceQuality quality;
        float expectedScale;
    };
    const ExistingProxyCase cases[] = {
        {Artifact::ProxyServiceQuality::Eighth, 0.125f},
        {Artifact::ProxyServiceQuality::Quarter, 0.25f},
        {Artifact::ProxyServiceQuality::Half, 0.5f},
        {Artifact::ProxyServiceQuality::Full, 1.0f},
    };

    for (const ExistingProxyCase& testCase : cases) {
        const QString expectedPath = Artifact::ArtifactProxyManager::proxyFilePath(
            sourcePath, testCase.quality);
        ASSERT_FALSE(expectedPath.isEmpty());
        EXPECT_FALSE(QFileInfo::exists(expectedPath));
        EXPECT_FALSE(manager->getProxyInfo(sourcePath, testCase.quality).hasProxy)
            << "a missing quality-specific file should not report a proxy";

        ASSERT_TRUE(QDir().mkpath(QFileInfo(expectedPath).absolutePath()));
        QFile proxyFile(expectedPath);
        ASSERT_TRUE(proxyFile.open(QIODevice::WriteOnly));
        ASSERT_GT(proxyFile.write("proxy"), 0);
        proxyFile.close();

        const Artifact::ProxyServiceInfo info =
            manager->getProxyInfo(sourcePath, testCase.quality);
        EXPECT_TRUE(info.hasProxy);
        EXPECT_EQ(info.proxyPath, expectedPath);
        EXPECT_EQ(info.quality, testCase.quality);
        EXPECT_FLOAT_EQ(info.scaleFactor, testCase.expectedScale);
    }

    const QString quarterPath = Artifact::ArtifactProxyManager::proxyFilePath(
        sourcePath, Artifact::ProxyServiceQuality::Quarter);
    EXPECT_TRUE(manager->clearProxy(quarterPath));
    EXPECT_FALSE(manager->hasProxy(
        sourcePath, Artifact::ProxyServiceQuality::Quarter));
    EXPECT_FALSE(manager->getProxyInfo(
        sourcePath, Artifact::ProxyServiceQuality::Quarter).hasProxy);
    for (const Artifact::ProxyServiceQuality unaffectedQuality : {
             Artifact::ProxyServiceQuality::Eighth,
             Artifact::ProxyServiceQuality::Half,
             Artifact::ProxyServiceQuality::Full}) {
        EXPECT_TRUE(manager->hasProxy(sourcePath, unaffectedQuality))
            << "clearing Quarter should leave other quality files intact";
    }
    EXPECT_TRUE(manager->clearProxy(quarterPath))
        << "clearing an already missing proxy should be idempotent";
}

TEST(VideoProxyQualityLodContractTest, PersistsEveryProxyQualityInLayerJson)
{
    const Artifact::ProxyQuality qualities[] = {
        Artifact::ProxyQuality::None,
        Artifact::ProxyQuality::Eighth,
        Artifact::ProxyQuality::Quarter,
        Artifact::ProxyQuality::Half,
        Artifact::ProxyQuality::Full,
    };

    for (const auto quality : qualities) {
        Artifact::ArtifactVideoLayer source;
        source.setProxyQuality(quality);
        const QJsonObject json = source.toJson();

        EXPECT_EQ(json.value(QStringLiteral("video.proxyQuality")).toInt(),
                  static_cast<int>(quality));

        const auto restored = Artifact::ArtifactVideoLayer::fromJson(json);
        ASSERT_TRUE(restored);
        EXPECT_EQ(restored->proxyQuality(), quality);
    }
}

TEST(VideoProxyQualityLodContractTest, RestoresLegacyAndClampsUnknownSerializedQuality)
{
    const auto legacy = Artifact::ArtifactVideoLayer::fromJson(QJsonObject{
        {QStringLiteral("proxyQuality"),
         static_cast<int>(Artifact::ProxyQuality::Eighth)}});
    ASSERT_TRUE(legacy);
    EXPECT_EQ(legacy->proxyQuality(), Artifact::ProxyQuality::Eighth);

    const auto unknown = Artifact::ArtifactVideoLayer::fromJson(QJsonObject{
        {QStringLiteral("video.proxyQuality"), 100}});
    ASSERT_TRUE(unknown);
    EXPECT_EQ(unknown->proxyQuality(), Artifact::ProxyQuality::Eighth);
}

TEST(VideoProxyQualityLodContractTest,
     CurrentSerializedQualityOverridesLegacyAndMalformedCurrentValuesFallBack)
{
    const auto currentOverridesLegacy = Artifact::ArtifactVideoLayer::fromJson(
        QJsonObject{
            {QStringLiteral("video.proxyQuality"),
             static_cast<int>(Artifact::ProxyQuality::Eighth)},
            {QStringLiteral("proxyQuality"),
             static_cast<int>(Artifact::ProxyQuality::Half)}});
    ASSERT_TRUE(currentOverridesLegacy);
    EXPECT_EQ(currentOverridesLegacy->proxyQuality(),
              Artifact::ProxyQuality::Eighth);

    for (const QJsonValue malformed : {
             QJsonValue(QStringLiteral("Half")), QJsonValue(2.5),
             QJsonValue(true), QJsonValue(QJsonValue::Null)}) {
        const auto restored = Artifact::ArtifactVideoLayer::fromJson(
            QJsonObject{
                {QStringLiteral("video.proxyQuality"), malformed},
                {QStringLiteral("proxyQuality"),
                 static_cast<int>(Artifact::ProxyQuality::Half)}});
        ASSERT_TRUE(restored);
        EXPECT_EQ(restored->proxyQuality(), Artifact::ProxyQuality::Half);
    }

    const auto malformedWithoutLegacy = Artifact::ArtifactVideoLayer::fromJson(
        QJsonObject{{QStringLiteral("video.proxyQuality"),
                     QJsonValue(2.5)}});
    ASSERT_TRUE(malformedWithoutLegacy);
    EXPECT_EQ(malformedWithoutLegacy->proxyQuality(),
              Artifact::ProxyQuality::None);
}
