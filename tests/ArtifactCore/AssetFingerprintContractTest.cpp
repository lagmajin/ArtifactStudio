#include <gtest/gtest.h>

#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QString>
#include <QTemporaryDir>

#include <initializer_list>

import Utils.Fingerprint;

using namespace ArtifactCore;

namespace {
bool writeFile(const QString& path, const QByteArray& data)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
}

QByteArray patternedData(qsizetype size)
{
    QByteArray data;
    data.resize(size);
    for (qsizetype index = 0; index < size; ++index) {
        data[index] = static_cast<char>((index * 17 + index / 1024) & 0xff);
    }
    return data;
}
}

TEST(AssetFingerprintContractTest, Md5AndSha256MatchQtKnownDataDigests)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("content.bin"));
    const QByteArray data("fingerprint contract\0with binary", 32);
    ASSERT_TRUE(writeFile(path, data));

    EXPECT_EQ(AssetFingerprint::calculateMd5(path),
              QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Md5).toHex()));
    EXPECT_EQ(AssetFingerprint::calculateSha256(path),
              QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex()));
}

TEST(AssetFingerprintContractTest, StandardAbcDigestVectorsMatchFixedValues)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("abc.bin"));
    ASSERT_TRUE(writeFile(path, QByteArrayLiteral("abc")));

    EXPECT_EQ(AssetFingerprint::calculateMd5(path),
              QStringLiteral("900150983cd24fb0d6963f7d28e17f72"));
    EXPECT_EQ(AssetFingerprint::calculateSha256(path),
              QStringLiteral("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
    EXPECT_EQ(AssetFingerprint::calculateFastHash(path),
              QStringLiteral("900150983cd24fb0d6963f7d28e17f72"));
}

TEST(AssetFingerprintContractTest, Sha256StreamsLargeFileToStandardDigest)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("large-sha256.bin"));
    const QByteArray data = patternedData(5 * 1024 * 1024 + 37);
    ASSERT_TRUE(writeFile(path, data));

    const QString expected = QString::fromLatin1(
        QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
    EXPECT_EQ(AssetFingerprint::calculateSha256(path), expected);
}

TEST(AssetFingerprintContractTest, EmptyFileHashesMatchStandardEmptyDigests)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("empty.bin"));
    ASSERT_TRUE(writeFile(path, {}));
    EXPECT_EQ(AssetFingerprint::calculateMd5(path),
              QStringLiteral("d41d8cd98f00b204e9800998ecf8427e"));
    EXPECT_EQ(AssetFingerprint::calculateSha256(path),
              QStringLiteral("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
}

TEST(AssetFingerprintContractTest, MissingFileReturnsEmptyForEveryFingerprintOperation)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString missing = directory.filePath(QStringLiteral("missing.bin"));
    EXPECT_TRUE(AssetFingerprint::calculateMd5(missing).isEmpty());
    EXPECT_TRUE(AssetFingerprint::calculateSha256(missing).isEmpty());
    EXPECT_TRUE(AssetFingerprint::calculateFastHash(missing).isEmpty());
}

TEST(AssetFingerprintContractTest, FastHashUsesFullMd5BelowOneMebibyte)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("small.bin"));
    const QByteArray data = patternedData(1024 * 1024 - 1);
    ASSERT_TRUE(writeFile(path, data));
    EXPECT_EQ(AssetFingerprint::calculateFastHash(path),
              AssetFingerprint::calculateMd5(path));
}

TEST(AssetFingerprintContractTest, FastHashUsesSampledModeAtOneMebibyteBoundary)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("boundary.bin"));
    const QByteArray data = patternedData(1024 * 1024);
    ASSERT_TRUE(writeFile(path, data));

    QByteArray sampled = data;
    sampled.append(data);
    sampled.append(QByteArray::number(data.size()));
    const QString expected = QString::fromLatin1(
        QCryptographicHash::hash(sampled, QCryptographicHash::Md5).toHex());
    EXPECT_EQ(AssetFingerprint::calculateFastHash(path), expected);
}

TEST(AssetFingerprintContractTest, FastHashSwitchesToSamplingImmediatelyAboveOneMebibyte)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    constexpr qsizetype size = 1024 * 1024 + 1;
    const QByteArray data = patternedData(size);
    const QString path = directory.filePath(QStringLiteral("above-threshold.bin"));
    ASSERT_TRUE(writeFile(path, data));

    QByteArray sampled = data.left(1024 * 1024);
    sampled.append(data.mid(size - 1024 * 1024));
    sampled.append(QByteArray::number(size));
    const QString expected = QString::fromLatin1(
        QCryptographicHash::hash(sampled, QCryptographicHash::Md5).toHex());
    EXPECT_EQ(AssetFingerprint::calculateFastHash(path), expected);
}

TEST(AssetFingerprintContractTest, FastHashUsesFullMd5ForSmallAndEmptyFiles)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString smallPath = directory.filePath(QStringLiteral("tiny.bin"));
    const QString emptyPath = directory.filePath(QStringLiteral("empty.bin"));
    ASSERT_TRUE(writeFile(smallPath, QByteArray("tiny")));
    ASSERT_TRUE(writeFile(emptyPath, {}));
    EXPECT_EQ(AssetFingerprint::calculateFastHash(smallPath),
              AssetFingerprint::calculateMd5(smallPath));
    EXPECT_EQ(AssetFingerprint::calculateFastHash(emptyPath),
              AssetFingerprint::calculateMd5(emptyPath));
}

TEST(AssetFingerprintContractTest, FastHashDetectsContentChangeInSameSizeSmallFile)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString firstPath = directory.filePath(QStringLiteral("first.bin"));
    const QString secondPath = directory.filePath(QStringLiteral("second.bin"));
    ASSERT_TRUE(writeFile(firstPath, QByteArrayLiteral("frame-a")));
    ASSERT_TRUE(writeFile(secondPath, QByteArrayLiteral("frame-b")));

    EXPECT_EQ(QFileInfo(firstPath).size(), QFileInfo(secondPath).size());
    EXPECT_NE(AssetFingerprint::calculateFastHash(firstPath),
              AssetFingerprint::calculateFastHash(secondPath));
}

TEST(AssetFingerprintContractTest, MiddleSampleStartsOnlyAboveThreeMebibytes)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    constexpr qsizetype threeMiB = 3 * 1024 * 1024;

    for (const qsizetype size : {threeMiB, threeMiB + 1}) {
        const QByteArray data = patternedData(size);
        const QString path = directory.filePath(
            QStringLiteral("boundary-%1.bin").arg(size));
        ASSERT_TRUE(writeFile(path, data));

        QByteArray sampled = data.left(1024 * 1024);
        if (size > threeMiB)
            sampled.append(data.mid(size / 2, 1024 * 1024));
        sampled.append(data.mid(size - 1024 * 1024));
        sampled.append(QByteArray::number(size));
        const QString expected = QString::fromLatin1(
            QCryptographicHash::hash(sampled, QCryptographicHash::Md5).toHex());
        EXPECT_EQ(AssetFingerprint::calculateFastHash(path), expected) << size;
    }
}

TEST(AssetFingerprintContractTest, FastHashIsStableForUnchangedLargeFile)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("large.bin"));
    ASSERT_TRUE(writeFile(path, patternedData(4 * 1024 * 1024 + 128)));
    const QString first = AssetFingerprint::calculateFastHash(path);
    ASSERT_FALSE(first.isEmpty());
    EXPECT_EQ(AssetFingerprint::calculateFastHash(path), first);
}

TEST(AssetFingerprintContractTest, SampledFastHashIncludesFileSize)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    constexpr qsizetype baseSize = 4 * 1024 * 1024;
    const QString basePath = directory.filePath(QStringLiteral("base.bin"));
    const QString largerPath = directory.filePath(QStringLiteral("larger.bin"));
    ASSERT_TRUE(writeFile(basePath, QByteArray(baseSize, '\0')));
    ASSERT_TRUE(writeFile(largerPath, QByteArray(baseSize + 1, '\0')));

    EXPECT_NE(AssetFingerprint::calculateFastHash(basePath),
              AssetFingerprint::calculateFastHash(largerPath));
}

TEST(AssetFingerprintContractTest, LargeFastHashChangesWhenHeadMiddleOrTailSampleChanges)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    constexpr qsizetype size = 4 * 1024 * 1024 + 128;
    const QByteArray original = patternedData(size);
    const QString originalPath = directory.filePath(QStringLiteral("original.bin"));
    ASSERT_TRUE(writeFile(originalPath, original));
    const QString baseline = AssetFingerprint::calculateFastHash(originalPath);

    const qsizetype sampleOffsets[] = {17, size / 2 + 17, size - 17};
    const QString names[] = {QStringLiteral("head.bin"), QStringLiteral("middle.bin"), QStringLiteral("tail.bin")};
    for (int index = 0; index < 3; ++index) {
        QByteArray changed = original;
        changed[sampleOffsets[index]] = static_cast<char>(changed.at(sampleOffsets[index]) ^ 0x5a);
        const QString path = directory.filePath(names[index]);
        ASSERT_TRUE(writeFile(path, changed));
        EXPECT_NE(AssetFingerprint::calculateFastHash(path), baseline) << index;
    }
}

TEST(AssetFingerprintContractTest, LargeFastHashIncludesBothEdgesOfEverySampleWindow)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    constexpr qsizetype mebibyte = 1024 * 1024;
    constexpr qsizetype size = 4 * mebibyte + 128;
    const QByteArray original = patternedData(size);
    const QString originalPath = directory.filePath(QStringLiteral("original.bin"));
    ASSERT_TRUE(writeFile(originalPath, original));
    const QString baseline = AssetFingerprint::calculateFastHash(originalPath);
    ASSERT_FALSE(baseline.isEmpty());

    const qsizetype sampleEdges[] = {
        0, mebibyte - 1,
        size / 2, size / 2 + mebibyte - 1,
        size - mebibyte, size - 1,
    };
    for (int index = 0; index < 6; ++index) {
        QByteArray changed = original;
        const qsizetype offset = sampleEdges[index];
        changed[offset] = static_cast<char>(changed.at(offset) ^ 0x5a);
        const QString path = directory.filePath(
            QStringLiteral("sample-edge-%1.bin").arg(index));
        ASSERT_TRUE(writeFile(path, changed));
        EXPECT_NE(AssetFingerprint::calculateFastHash(path), baseline) << offset;
    }
}

TEST(AssetFingerprintContractTest, LargeFastHashIgnoresChangesOutsideSampledBlocks)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    constexpr qsizetype size = 4 * 1024 * 1024 + 128;
    const QByteArray original = patternedData(size);
    const QString originalPath = directory.filePath(QStringLiteral("original.bin"));
    const QString changedPath = directory.filePath(QStringLiteral("unsampled.bin"));
    ASSERT_TRUE(writeFile(originalPath, original));

    QByteArray changed = original;
    const qsizetype unsampledOffset = 3 * 1024 * 1024 / 2;
    changed[unsampledOffset] = static_cast<char>(changed.at(unsampledOffset) ^ 0x5a);
    ASSERT_TRUE(writeFile(changedPath, changed));
    EXPECT_EQ(AssetFingerprint::calculateFastHash(originalPath),
              AssetFingerprint::calculateFastHash(changedPath));
}

TEST(AssetFingerprintContractTest, FastHashOutputIsLowercaseHex)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("small.bin"));
    ASSERT_TRUE(writeFile(path, QByteArray("hex")));
    const QString hash = AssetFingerprint::calculateFastHash(path);
    EXPECT_TRUE(hash.contains(QRegularExpression(QStringLiteral("^[0-9a-f]+$"))));
}
