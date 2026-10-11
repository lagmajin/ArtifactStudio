#include <gtest/gtest.h>

#include <QByteArray>
#include <QFile>
#include <QIODevice>
#include <QTemporaryDir>
#include <QString>

#include <initializer_list>

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

TEST(FileTypeDetectorContractTest, MagicDetectionStartsAtFourByteHeaderBoundary)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString threeBytePath = directory.filePath(QStringLiteral("three.bin"));
    const QString fourBytePath = directory.filePath(QStringLiteral("four.bin"));
    ASSERT_TRUE(writeBytes(threeBytePath, QByteArrayLiteral("abc")));
    ASSERT_TRUE(writeBytes(fourBytePath, QByteArrayLiteral("abcd")));

    const FileTypeDetector detector;
    EXPECT_EQ(detector.detectByMagicNumber(threeBytePath), FileType::Unknown);
    EXPECT_EQ(detector.detectByMagicNumber(fourBytePath), FileType::Text);
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

TEST(FileTypeDetectorContractTest, ExistingEmptyFileFallsBackToKnownExtension)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("empty.png"));
    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    file.close();

    const FileTypeDetector detector;
    EXPECT_EQ(detector.detectByMagicNumber(path), FileType::Unknown);
    EXPECT_EQ(detector.detectByExtension(path), FileType::Image);
    EXPECT_EQ(detector.detect(path), FileType::Image);
}

TEST(FileTypeDetectorContractTest, ExtensionLookupClassifiesRepresentativeMediaFamilies)
{
    const FileTypeDetector detector;
    const struct {
        const char* path;
        FileType expected;
    } cases[] = {
        {"still.png", FileType::Image},
        {"clip.mkv", FileType::Video},
        {"track.flac", FileType::Audio},
        {"font.woff2", FileType::Font},
        {"notes.yaml", FileType::Text},
        {"report.docx", FileType::Document},
        {"bundle.tar", FileType::Archive},
        {"model.usdz", FileType::Model3D},
        {"no-extension", FileType::Unknown},
    };

    for (const auto& testCase : cases) {
        SCOPED_TRACE(testCase.path);
        EXPECT_EQ(detector.detectByExtension(QString::fromLatin1(testCase.path)),
                  testCase.expected);
    }
}

TEST(FileTypeDetectorContractTest, ExtensionLookupIgnoresCaseAndUsesFinalSuffix)
{
    const FileTypeDetector detector;
    EXPECT_EQ(detector.detectByExtension(QStringLiteral("PHOTO.PnG")), FileType::Image);
    EXPECT_EQ(detector.detectByExtension(QStringLiteral("archive.tar.GZ")), FileType::Archive);
    EXPECT_EQ(detector.detectByExtension(QStringLiteral(".hidden")), FileType::Unknown);
}

TEST(FileTypeDetectorContractTest, RecognizesRepresentativeMagicSignatures)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const struct {
        const char* name;
        QByteArray bytes;
        FileType expected;
    } cases[] = {
        {"png", QByteArray::fromHex("89504e470d0a1a0a"), FileType::Image},
        {"psd", QByteArrayLiteral("8BPSxxxx"), FileType::Image},
        {"jpeg", QByteArray::fromHex("ffd8ff00"), FileType::Image},
        {"gif", QByteArrayLiteral("GIF89a"), FileType::Image},
        {"bmp", QByteArrayLiteral("BM00"), FileType::Image},
        {"mp4", QByteArrayLiteral("xxxxftyp"), FileType::Video},
        {"avi", QByteArrayLiteral("RIFFxxxxAVI "), FileType::Video},
        {"mp3", QByteArrayLiteral("ID3xxxx"), FileType::Audio},
        {"wav", QByteArrayLiteral("RIFFxxxxWAVE"), FileType::Audio},
        {"ttf", QByteArray::fromHex("0001000000000000"), FileType::Font},
        {"otf", QByteArrayLiteral("OTTOxxxx"), FileType::Font},
        {"woff", QByteArrayLiteral("wOFFxxxx"), FileType::Font},
        {"woff2", QByteArrayLiteral("wOF2xxxx"), FileType::Font},
        {"zip", QByteArray::fromHex("504b0304"), FileType::Archive},
        {"rar", QByteArrayLiteral("Rar!xxxx"), FileType::Archive},
        {"pdf", QByteArrayLiteral("%PDF-1.7"), FileType::Document},
        {"ps", QByteArrayLiteral("%!PS-Adobe"), FileType::Document},
        {"glb", QByteArrayLiteral("glTFxxxx"), FileType::Model3D},
        {"pmd", QByteArrayLiteral("Pmdxxxxx"), FileType::Model3D},
    };

    const FileTypeDetector detector;
    for (const auto& testCase : cases) {
        SCOPED_TRACE(testCase.name);
        const QString path = directory.filePath(QString::fromLatin1(testCase.name));
        ASSERT_TRUE(writeBytes(path, testCase.bytes));
        EXPECT_EQ(detector.detectByMagicNumber(path), testCase.expected);
    }
}

TEST(FileTypeDetectorContractTest, MagicClassificationRequiresAtLeastFourBytes)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const FileTypeDetector detector;
    for (const QByteArray bytes : {QByteArray{}, QByteArrayLiteral("a"),
                                   QByteArrayLiteral("ab"), QByteArrayLiteral("abc")}) {
        const QString path = directory.filePath(QString::number(bytes.size()));
        ASSERT_TRUE(writeBytes(path, bytes));
        EXPECT_EQ(detector.detectByMagicNumber(path), FileType::Unknown);
    }

    const QString fourBytePath = directory.filePath(QStringLiteral("four"));
    ASSERT_TRUE(writeBytes(fourBytePath, QByteArrayLiteral("abcd")));
    EXPECT_EQ(detector.detectByMagicNumber(fourBytePath), FileType::Text);
}

TEST(FileTypeDetectorContractTest, TextHeaderAllowsOnlyTabLineFeedAndCarriageReturnControls)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString allowedPath = directory.filePath(QStringLiteral("allowed.bin"));
    const QString forbiddenPath = directory.filePath(QStringLiteral("forbidden.bin"));
    ASSERT_TRUE(writeBytes(allowedPath, QByteArray("a\t\r\n", 4)));
    ASSERT_TRUE(writeBytes(forbiddenPath, QByteArray::fromHex("61016263")));

    const FileTypeDetector detector;
    EXPECT_EQ(detector.detectByMagicNumber(allowedPath), FileType::Text);
    EXPECT_EQ(detector.detectByMagicNumber(forbiddenPath), FileType::Binary);
}

TEST(FileTypeDetectorContractTest, Utf8BomAndMultibyteTextAreNotMisclassifiedAsBinary)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("japanese.bin"));
    const QByteArray utf8Text = QByteArray::fromHex("efbbbfe697a5e69cace8aa9e0a");
    ASSERT_TRUE(writeBytes(path, utf8Text));

    const FileTypeDetector detector;
    EXPECT_EQ(detector.detectByMagicNumber(path), FileType::Text);
    EXPECT_EQ(detector.detect(path), FileType::Text);
}

TEST(FileTypeDetectorContractTest, SupplementaryUtf8TextIsNotMisclassifiedAsBinary)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("supplementary.bin"));
    ASSERT_TRUE(writeBytes(path, QByteArray::fromHex("61f09f8ea862")));

    const FileTypeDetector detector;
    EXPECT_EQ(detector.detectByMagicNumber(path), FileType::Text);
    EXPECT_EQ(detector.detect(path), FileType::Text);
}

TEST(FileTypeDetectorContractTest, InvalidUtf8HeadersAreClassifiedAsBinary)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString invalidLeadPath = directory.filePath(QStringLiteral("invalid-lead.bin"));
    const QString invalidContinuationPath =
        directory.filePath(QStringLiteral("invalid-continuation.bin"));
    ASSERT_TRUE(writeBytes(invalidLeadPath, QByteArray::fromHex("61ff6263")));
    ASSERT_TRUE(writeBytes(invalidContinuationPath, QByteArray::fromHex("41e328a1")));

    const FileTypeDetector detector;
    EXPECT_EQ(detector.detectByMagicNumber(invalidLeadPath), FileType::Binary);
    EXPECT_EQ(detector.detectByMagicNumber(invalidContinuationPath), FileType::Binary);
}

TEST(FileTypeDetectorContractTest, InvalidUtf8ScalarEncodingsAreClassifiedAsBinary)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const struct {
        const char* name;
        QByteArray bytes;
    } cases[] = {
        {"overlong", QByteArray::fromHex("c0af4142")},
        {"surrogate", QByteArray::fromHex("eda08041")},
        {"above-unicode-range", QByteArray::fromHex("f4908080")},
        {"truncated-continuation", QByteArray::fromHex("61e28262")},
    };

    const FileTypeDetector detector;
    for (const auto& testCase : cases) {
        SCOPED_TRACE(testCase.name);
        const QString path = directory.filePath(QString::fromLatin1(testCase.name));
        ASSERT_TRUE(writeBytes(path, testCase.bytes));
        EXPECT_EQ(detector.detectByMagicNumber(path), FileType::Binary);
    }
}

TEST(FileTypeDetectorContractTest, SemanticMediaExtensionsWinOverTextMagic)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QByteArray text = QByteArrayLiteral("plain text header");
    const FileTypeDetector detector;
    const struct {
        const char* fileName;
        FileType expected;
    } cases[] = {
        {"clip.obj", FileType::Model3D},
        {"track.mp3", FileType::Audio},
        {"bundle.zip", FileType::Archive},
        {"clip.mp4", FileType::Video},
    };
    for (const auto& testCase : cases) {
        SCOPED_TRACE(testCase.fileName);
        const QString path = directory.filePath(QString::fromLatin1(testCase.fileName));
        ASSERT_TRUE(writeBytes(path, text));
        EXPECT_EQ(detector.detect(path), testCase.expected);
    }
}

TEST(FileTypeDetectorContractTest, SemanticMediaExtensionsWinOverStrongMagic)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QByteArray pngSignature = QByteArray::fromHex("89504e470d0a1a0a");
    const FileTypeDetector detector;
    const struct {
        const char* fileName;
        FileType expected;
    } cases[] = {
        {"scene.obj", FileType::Model3D},
        {"music.mp3", FileType::Audio},
        {"package.zip", FileType::Archive},
        {"clip.mp4", FileType::Video},
    };

    for (const auto& testCase : cases) {
        SCOPED_TRACE(testCase.fileName);
        const QString path = directory.filePath(QString::fromLatin1(testCase.fileName));
        ASSERT_TRUE(writeBytes(path, pngSignature));
        EXPECT_EQ(detector.detectByMagicNumber(path), FileType::Image);
        EXPECT_EQ(detector.detect(path), testCase.expected);
    }
}

TEST(FileTypeDetectorContractTest, StrongMagicOverridesOtherKnownExtensions)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("notes.txt"));
    ASSERT_TRUE(writeBytes(path, QByteArrayLiteral("%PDF-1.7")));
    EXPECT_EQ(FileTypeDetector().detect(path), FileType::Document);
}

TEST(FileTypeDetectorContractTest, BinaryMagicFallsBackToKnownImageExtension)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("opaque.png"));
    ASSERT_TRUE(writeBytes(path, QByteArray::fromHex("00010203")));
    const FileTypeDetector detector;
    EXPECT_EQ(detector.detectByMagicNumber(path), FileType::Binary);
    EXPECT_EQ(detector.detect(path), FileType::Image);
}
