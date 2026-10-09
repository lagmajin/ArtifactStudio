#include <gtest/gtest.h>

#include <QByteArray>
#include <QFile>
#include <QIODevice>
#include <QTemporaryDir>
#include <QString>

import File.TypeDetector;

using namespace ArtifactCore;

namespace {

bool writeBytes(const QString& path, const QByteArray& bytes)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate) &&
           file.write(bytes) == bytes.size();
}

} // namespace

TEST(FileTypeDetectorContractTest, RecognizesMagicNumberWhenExtensionIsUnknown)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("image.bin"));
    ASSERT_TRUE(writeBytes(path, QByteArray::fromHex("89504e470d0a1a0a")));

    const FileTypeDetector detector;
    EXPECT_EQ(detector.detectByExtension(path), FileType::Unknown);
    EXPECT_EQ(detector.detectByMagicNumber(path), FileType::Image);
    EXPECT_EQ(detector.detect(path), FileType::Image);
}

TEST(FileTypeDetectorContractTest, PreservesKnownMediaExtensionOverTextHeader)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("clip.mp4"));
    ASSERT_TRUE(writeBytes(path, QByteArrayLiteral("plain text header")));

    const FileTypeDetector detector;
    EXPECT_EQ(detector.detectByExtension(path), FileType::Video);
    EXPECT_EQ(detector.detectByMagicNumber(path), FileType::Text);
    EXPECT_EQ(detector.detect(path), FileType::Video);
}

TEST(FileTypeDetectorContractTest, DistinguishesShortUnknownTextAndBinaryHeaders)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString shortPath = directory.filePath(QStringLiteral("short.dat"));
    const QString textPath = directory.filePath(QStringLiteral("text.dat"));
    const QString binaryPath = directory.filePath(QStringLiteral("binary.dat"));
    ASSERT_TRUE(writeBytes(shortPath, QByteArrayLiteral("abc")));
    ASSERT_TRUE(writeBytes(textPath, QByteArrayLiteral("text\n")));
    ASSERT_TRUE(writeBytes(binaryPath, QByteArray::fromHex("00010203")));

    const FileTypeDetector detector;
    EXPECT_EQ(detector.detectByMagicNumber(shortPath), FileType::Unknown);
    EXPECT_EQ(detector.detect(shortPath), FileType::Unknown);
    EXPECT_EQ(detector.detectByMagicNumber(textPath), FileType::Text);
    EXPECT_EQ(detector.detect(textPath), FileType::Text);
    EXPECT_EQ(detector.detectByMagicNumber(binaryPath), FileType::Binary);
    EXPECT_EQ(detector.detect(binaryPath), FileType::Binary);
}

TEST(FileTypeDetectorContractTest, MissingFileFallsBackToKnownExtensionOnly)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString imagePath = directory.filePath(QStringLiteral("missing.png"));
    const QString unknownPath = directory.filePath(QStringLiteral("missing.bin"));

    const FileTypeDetector detector;
    EXPECT_EQ(detector.detectByMagicNumber(imagePath), FileType::Unknown);
    EXPECT_EQ(detector.detect(imagePath), FileType::Image);
    EXPECT_EQ(detector.detectByMagicNumber(unknownPath), FileType::Unknown);
    EXPECT_EQ(detector.detect(unknownPath), FileType::Unknown);
}
