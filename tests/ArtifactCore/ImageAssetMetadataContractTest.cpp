#include <gtest/gtest.h>

#include <QByteArray>
#include <QColor>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDevice>
#include <QFileInfo>
#include <QIODevice>
#include <QImage>
#include <QTemporaryDir>
#include <QString>
#include <QStringList>
#include <QSize>

import ImageAsset;
import Image.Raw;
import IO.ImageImporter;
import Media.ImageSequenceSource;
import Media.ISource;
import Utils.String.UniString;

using namespace ArtifactCore;

namespace {

bool writeFile(const QString& path, const QByteArray& bytes)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate) &&
           file.write(bytes) == bytes.size();
}

QByteArray checkerPng()
{
    return QByteArray::fromBase64(QByteArrayLiteral(
        "iVBORw0KGgoAAAANSUhEUgAAAIAAAACACAYAAADDPmHLAAABqElEQVR4nO3asQnAMBAEQZXz/VehruwihBFmJ/h4JNjw1sw8J7f3Pjr+XX/dfgBfAHwB8AXAFwBfAHwB8AXAFwBfAHwB8AXAFwBfAHwB8D8J4O8f4J/5Aoj7Aoj7Aoj7Aoj7Aoj7Aoj7BiFxXwBxXwBxXwBxXwBxXwBxXwBxXwBxXwBxXwBxXwBxXwBxXwBxXwBxXwBx3yAk7gsg7gsg7gsg7gsg7gsg7gsg7gsg7gsg7gsg7gsg7gsg7gsg7gsg7huExH0BxH0BxH0BxH0BxH0BxH0BxH0BxH0BxH0BxH0BxH0BxH0BxH0BxH2DkLgvgLgvgLgvgLgvgLgvgLgvgLgvgLgvgLgvgLgvgLgvgLgvgLgvgLgvgLgvgLhvEBL3BRD3BRD3BRD3BRD3BRD3BRD3BRD3BRD3BRD3BRD3BRD3BRD3BRD3BRD3BRD3DULivgDivgDivgDivgDivgDivgDivgDivgDivgDivgDivgDivgDivgDivgDi/gsB3sj4A9iR2AAAAABJRU5ErkJggg=="));
}

QByteArray onePixelPng()
{
    return QByteArray::fromBase64(QByteArrayLiteral(
        "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR4nGP4z8DwHwAFAAH/iZk9HQAAAABJRU5ErkJggg=="));
}

QByteArray blueOnePixelPng()
{
    return QByteArray::fromBase64(QByteArrayLiteral(
        "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR4nGNgYPj/HwADAgH/5ncLrgAAAABJRU5ErkJggg=="));
}

} // namespace

TEST(ImageAssetMetadataContractTest, RefreshesDimensionsAndBasicImageMetadata)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("checker.png"));
    ASSERT_TRUE(writeFile(path, checkerPng()));

    ImageAssetFile image(path);
    ASSERT_FALSE(image.isLoaded());
    ASSERT_TRUE(image.load());
    EXPECT_TRUE(image.isLoaded());
    EXPECT_TRUE(image.isImageMetadataValid());
    EXPECT_EQ(image.imageMeta().width, 128);
    EXPECT_EQ(image.imageMeta().height, 128);
    EXPECT_EQ(image.imageMeta().channelCount, 4);
    EXPECT_GT(image.imageMeta().bitDepth, 0);
    EXPECT_FALSE(image.imageMeta().pixelType.isEmpty());
    EXPECT_EQ(image.meta().getValue(UniString("width")).toQString(), QStringLiteral("128"));
    EXPECT_EQ(image.meta().getValue(UniString("height")).toQString(), QStringLiteral("128"));
    EXPECT_EQ(image.meta().getValue(UniString("channels")).toQString(), QStringLiteral("4"));

    image.unload();
    EXPECT_FALSE(image.isLoaded());
}

TEST(ImageAssetMetadataContractTest, RejectsMissingAndUndecodableImages)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString invalidPath = directory.filePath(QStringLiteral("broken.png"));
    ASSERT_TRUE(writeFile(invalidPath, QByteArrayLiteral("not an image")));

    ImageAssetFile missing(directory.filePath(QStringLiteral("missing.png")));
    EXPECT_FALSE(missing.refreshMetadata());
    EXPECT_FALSE(missing.isImageMetadataValid());

    ImageAssetFile invalid(invalidPath);
    EXPECT_FALSE(invalid.load());
    EXPECT_FALSE(invalid.isLoaded());
    EXPECT_FALSE(invalid.isImageMetadataValid());
}

TEST(ImageImporterContractTest, ReadsImageAndResetsAfterClose)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("checker.png"));
    ASSERT_TRUE(writeFile(path, checkerPng()));

    ImageImporter importer;
    EXPECT_FALSE(importer.readImage().isValid());
    ASSERT_TRUE(importer.open(path));
    const RawImage decoded = importer.readImage();
    ASSERT_TRUE(decoded.isValid());
    EXPECT_EQ(decoded.width, 128);
    EXPECT_EQ(decoded.height, 128);
    EXPECT_EQ(decoded.channels, 4);
    const int bytesPerChannel = decoded.getPixelTypeSizeInBytes();
    ASSERT_GT(bytesPerChannel, 0);
    EXPECT_EQ(decoded.data.size(),
              decoded.width * decoded.height * decoded.channels * bytesPerChannel);

    importer.close();
    EXPECT_FALSE(importer.readImage().isValid());

    ASSERT_TRUE(importer.open(path));
    ASSERT_TRUE(importer.readImage().isValid());
    EXPECT_FALSE(importer.open(directory.filePath(QStringLiteral("missing.png"))));
    EXPECT_FALSE(importer.readImage().isValid());
}

TEST(ImageImporterContractTest, OpenRejectsMissingFile)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());

    ImageImporter importer;
    EXPECT_FALSE(importer.open(directory.filePath(QStringLiteral("missing.png"))));
    EXPECT_FALSE(importer.readImage().isValid());
}

TEST(ImageImporterContractTest, DefersDecodeFailureUntilRead)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("broken.png"));
    ASSERT_TRUE(writeFile(path, QByteArrayLiteral("not an image")));

    ImageImporter importer;
    EXPECT_TRUE(importer.open(path));
    EXPECT_FALSE(importer.readImage().isValid());
}

TEST(ImageSequenceSourceContractTest, PreservesExplicitMissingFrameSlots)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString firstPath = directory.filePath(QStringLiteral("shot_0001.png"));
    const QString missingPath = directory.filePath(QStringLiteral("shot_0002.png"));
    const QString thirdPath = directory.filePath(QStringLiteral("shot_0003.png"));
    ASSERT_TRUE(writeFile(firstPath, checkerPng()));
    ASSERT_TRUE(writeFile(thirdPath, checkerPng()));

    ImageSequenceSource source;
    source.setFrameRate(12.0);
    ASSERT_TRUE(source.openFramePaths({firstPath, missingPath, thirdPath}));

    EXPECT_TRUE(source.isOpen());
    EXPECT_EQ(source.frameCount(), 3);
    EXPECT_EQ(source.frameRate(), 12.0);
    EXPECT_EQ(source.sourceFrameNumberAt(0), 0);
    EXPECT_EQ(source.sourceFrameNumberAt(1), 1);
    EXPECT_EQ(source.sourceFrameNumberAt(2), 2);
    EXPECT_EQ(source.sequenceIndexForSourceFrame(1), 1);
    EXPECT_EQ(source.sequenceIndexForSourceFrame(3), -1);
    const SourceMetadata metadata = source.metadata();
    EXPECT_EQ(metadata.frameCount, 3);
    EXPECT_EQ(metadata.frameStart, 0);
    EXPECT_EQ(metadata.frameEnd, 2);
    EXPECT_EQ(metadata.missingFrameCount, 1);
    EXPECT_TRUE(metadata.hasVideo);
    EXPECT_TRUE(metadata.isSequence);
    EXPECT_EQ(metadata.uri, QFileInfo(firstPath).absoluteFilePath());
    EXPECT_EQ(source.kind(), SourceKind::ImageSequence);
    EXPECT_EQ(metadata.frameSize.width(), 128);
    EXPECT_EQ(metadata.frameSize.height(), 128);
    EXPECT_TRUE(source.frameAt(1).isNull());
    EXPECT_FALSE(source.frameAt(2).isNull());
    EXPECT_EQ(source.metadata().missingFrameCount, 1);

    ASSERT_TRUE(writeFile(missingPath, checkerPng()));
    EXPECT_FALSE(source.frameAt(1).isNull());
    EXPECT_EQ(source.metadata().missingFrameCount, 0);
    ASSERT_TRUE(QFile::remove(missingPath));
    EXPECT_TRUE(source.frameAt(1).isNull());
    EXPECT_EQ(source.metadata().missingFrameCount, 1);

    source.close();
    EXPECT_FALSE(source.isOpen());
    EXPECT_EQ(source.frameCount(), 0);
    ASSERT_TRUE(source.openFramePaths({firstPath, thirdPath}));
    EXPECT_DOUBLE_EQ(source.frameRate(), 12.0);
}

TEST(ImageSequenceSourceContractTest, MapsNumberedFilesAcrossGaps)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString firstPath = directory.filePath(QStringLiteral("take_0001.png"));
    const QString thirdPath = directory.filePath(QStringLiteral("take_0003.png"));
    ASSERT_TRUE(writeFile(firstPath, checkerPng()));
    ASSERT_TRUE(writeFile(thirdPath, checkerPng()));

    ImageSequenceSource source;
    ASSERT_TRUE(source.open(firstPath));
    const SourceMetadata metadata = source.metadata();
    EXPECT_EQ(source.frameCount(), 2);
    EXPECT_EQ(metadata.frameStart, 1);
    EXPECT_EQ(metadata.frameEnd, 3);
    EXPECT_EQ(metadata.missingFrameCount, 1);
    EXPECT_TRUE(metadata.isSequence);
    EXPECT_EQ(source.sourceFrameNumberAt(0), 1);
    EXPECT_EQ(source.sourceFrameNumberAt(1), 3);
    EXPECT_EQ(source.sequenceIndexForSourceFrame(3), 1);
    EXPECT_EQ(source.sequenceIndexForSourceFrame(2), -1);
    EXPECT_FALSE(source.seek(-1));
    EXPECT_FALSE(source.seek(source.frameCount()));
    EXPECT_EQ(source.currentFrameIndex(), 0);

    EXPECT_TRUE(source.seekSourceFrame(3));
    EXPECT_EQ(source.currentFrameIndex(), 1);
    EXPECT_FALSE(source.seekSourceFrame(2));
    EXPECT_EQ(source.frameIndexAtTime(1, 24.0), 1);
    EXPECT_EQ(source.frameIndexAtTime(100, 24.0), 1);
}

TEST(ImageSequenceSourceContractTest, CachesDecodedFramesAndExplicitlyClearsCache)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("frame.png"));
    ASSERT_TRUE(writeFile(path, checkerPng()));

    ImageSequenceSource source;
    ASSERT_TRUE(source.openFramePaths({path}));
    EXPECT_EQ(source.frameCacheHitCount(), 0u);
    EXPECT_EQ(source.frameCacheMissCount(), 0u);
    EXPECT_TRUE(source.frameAt(-1).isNull());
    EXPECT_TRUE(source.frameAt(source.frameCount()).isNull());
    EXPECT_EQ(source.frameCacheEntryCount(), 0);
    EXPECT_EQ(source.frameCacheMissCount(), 0u);

    ASSERT_FALSE(source.frameAt(0).isNull());
    EXPECT_EQ(source.frameCacheMissCount(), 1u);
    EXPECT_EQ(source.frameCacheHitCount(), 0u);
    ASSERT_FALSE(source.frameAt(0).isNull());
    EXPECT_EQ(source.frameCacheMissCount(), 1u);
    EXPECT_EQ(source.frameCacheHitCount(), 1u);
    EXPECT_EQ(source.frameCacheEntryCount(), 1);
    EXPECT_GT(source.frameCacheBytes(), 0u);

    source.clearFrameCache();
    EXPECT_EQ(source.frameCacheEntryCount(), 0);
    EXPECT_EQ(source.frameCacheBytes(), 0u);
    EXPECT_EQ(source.frameCacheHitCount(), 0u);
    EXPECT_EQ(source.frameCacheMissCount(), 0u);
}

TEST(ImageSequenceSourceContractTest, KeepsDecodedFrameCacheWithinCapacity)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    QStringList framePaths;
    for (int index = 0; index < 10; ++index) {
        const QString path = directory.filePath(
            QStringLiteral("frame_%1.png").arg(index, 4, 10, QLatin1Char('0')));
        ASSERT_TRUE(writeFile(path, checkerPng()));
        framePaths.push_back(path);
    }

    ImageSequenceSource source;
    ASSERT_TRUE(source.openFramePaths(framePaths));
    for (qint64 index = 0; index < source.frameCount(); ++index) {
        ASSERT_FALSE(source.frameAt(index).isNull());
    }

    EXPECT_EQ(source.frameCacheCapacity(), 8);
    EXPECT_LE(source.frameCacheEntryCount(), source.frameCacheCapacity());
    EXPECT_LE(source.frameCacheBytes(), source.frameCacheByteCapacity());
    EXPECT_EQ(source.frameCacheMissCount(), 10u);
    ASSERT_FALSE(source.frameAt(0).isNull());
    EXPECT_EQ(source.frameCacheMissCount(), 11u);
    ASSERT_FALSE(source.frameAt(9).isNull());
    EXPECT_EQ(source.frameCacheHitCount(), 1u);
}

TEST(ImageSequenceSourceContractTest, InvalidatesCachedFrameWhenSourceFileChanges)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("replaceable.png"));
    ASSERT_TRUE(writeFile(path, checkerPng()));

    ImageSequenceSource source;
    ASSERT_TRUE(source.openFramePaths({path}));
    ASSERT_FALSE(source.frameAt(0).isNull());
    EXPECT_EQ(source.frameCacheMissCount(), 1u);

    ASSERT_TRUE(writeFile(path, QByteArrayLiteral("invalid replacement")));
    EXPECT_TRUE(source.frameAt(0).isNull());
    EXPECT_EQ(source.frameCacheMissCount(), 2u);
    EXPECT_EQ(source.frameCacheBytes(), 0u);
    EXPECT_TRUE(source.frameAt(0).isNull());
    EXPECT_EQ(source.frameCacheMissCount(), 2u);
    EXPECT_EQ(source.frameCacheHitCount(), 1u);

    ASSERT_TRUE(writeFile(path, checkerPng()));
    EXPECT_FALSE(source.frameAt(0).isNull());
    EXPECT_EQ(source.frameCacheMissCount(), 3u);
}

TEST(ImageSequenceSourceContractTest, InvalidatesSameSizeReplacementByModificationTime)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("same_size.png"));
    const QByteArray firstImage = onePixelPng();
    const QByteArray replacementImage = blueOnePixelPng();
    ASSERT_EQ(firstImage.size(), replacementImage.size());
    ASSERT_TRUE(writeFile(path, firstImage));

    const auto setModifiedTime = [&path](qint64 millisecondsSinceEpoch) {
        QFile file(path);
        return file.open(QIODevice::ReadWrite) &&
               file.setFileTime(QDateTime::fromMSecsSinceEpoch(millisecondsSinceEpoch),
                                QFileDevice::FileModificationTime);
    };
    ASSERT_TRUE(setModifiedTime(1700000000000LL));

    ImageSequenceSource source;
    ASSERT_TRUE(source.openFramePaths({path}));
    const QImage firstFrame = source.frameAt(0);
    ASSERT_FALSE(firstFrame.isNull());
    EXPECT_EQ(firstFrame.pixelColor(0, 0).red(), 255);
    EXPECT_EQ(firstFrame.pixelColor(0, 0).blue(), 0);

    ASSERT_TRUE(writeFile(path, replacementImage));
    ASSERT_TRUE(setModifiedTime(1700000010000LL));
    const QImage replacementFrame = source.frameAt(0);
    ASSERT_FALSE(replacementFrame.isNull());
    EXPECT_EQ(replacementFrame.pixelColor(0, 0).red(), 0);
    EXPECT_EQ(replacementFrame.pixelColor(0, 0).blue(), 255);
    EXPECT_EQ(source.frameCacheMissCount(), 2u);
}

TEST(ImageSequenceSourceContractTest, ConvertsTimelineFramesUsingSourceAndTimelineRates)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    QStringList framePaths;
    for (int index = 0; index < 6; ++index) {
        const QString path = directory.filePath(
            QStringLiteral("rate_%1.png").arg(index));
        ASSERT_TRUE(writeFile(path, checkerPng()));
        framePaths.push_back(path);
    }

    ImageSequenceSource source;
    source.setFrameRate(12.0);
    ASSERT_TRUE(source.openFramePaths(framePaths));
    EXPECT_DOUBLE_EQ(source.frameRate(), 12.0);
    EXPECT_EQ(source.frameIndexAtTime(0, 24.0), 0);
    EXPECT_EQ(source.frameIndexAtTime(1, 24.0), 0);
    EXPECT_EQ(source.frameIndexAtTime(2, 24.0), 1);
    EXPECT_EQ(source.frameIndexAtTime(3, 24.0), 1);
    EXPECT_EQ(source.frameIndexAtTime(10, 24.0), 5);
}

TEST(ImageSequenceSourceContractTest, FailedReopenClearsPreviouslyOpenedSequence)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("frame.png"));
    ASSERT_TRUE(writeFile(path, checkerPng()));

    ImageSequenceSource source;
    ASSERT_TRUE(source.openFramePaths({path}));
    ASSERT_TRUE(source.isOpen());
    ASSERT_EQ(source.frameCount(), 1);

    EXPECT_FALSE(source.openFramePaths({}));
    EXPECT_FALSE(source.isOpen());
    EXPECT_EQ(source.frameCount(), 0);
    EXPECT_TRUE(source.uri().isEmpty());
    EXPECT_TRUE(source.frameAt(0).isNull());
    EXPECT_EQ(source.frameIndexAtTime(1, 24.0), -1);
}

TEST(ImageSequenceSourceContractTest, DirectoryOpenListsSupportedImagesAtTopLevel)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    ASSERT_TRUE(writeFile(directory.filePath(QStringLiteral("b.png")), checkerPng()));
    ASSERT_TRUE(writeFile(directory.filePath(QStringLiteral("a.png")), onePixelPng()));
    ASSERT_TRUE(writeFile(directory.filePath(QStringLiteral("notes.txt")),
                          QByteArrayLiteral("not an image")));
    ASSERT_TRUE(QDir().mkdir(directory.filePath(QStringLiteral("nested"))));
    ASSERT_TRUE(writeFile(directory.filePath(QStringLiteral("nested/c.png")), onePixelPng()));

    ImageSequenceSource source;
    ASSERT_TRUE(source.open(directory.path()));
    EXPECT_EQ(source.frameCount(), 2);
    EXPECT_EQ(source.sourceFrameNumberAt(0), 0);
    EXPECT_EQ(source.sourceFrameNumberAt(1), 1);
    EXPECT_EQ(source.frameSize().width(), 1);
    EXPECT_EQ(source.frameSize().height(), 1);
    EXPECT_EQ(source.metadata().missingFrameCount, 0);
    EXPECT_TRUE(source.metadata().isSequence);
    EXPECT_EQ(source.frameAt(0).size(), QSize(1, 1));
    EXPECT_EQ(source.frameAt(1).size(), QSize(128, 128));
}
