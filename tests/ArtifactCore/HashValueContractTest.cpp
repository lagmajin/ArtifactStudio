#include <gtest/gtest.h>

#include <QCryptographicHash>
#include <QFile>
#include <QString>
#include <QTemporaryDir>

#include <array>

import Utils.HashValue;

using namespace ArtifactCore;

namespace {
QCryptographicHash::Algorithm qtAlgorithm(HashValue::Algorithm algorithm)
{
    switch (algorithm) {
    case HashValue::Algorithm::SHA256: return QCryptographicHash::Sha256;
    case HashValue::Algorithm::SHA1: return QCryptographicHash::Sha1;
    case HashValue::Algorithm::MD5: return QCryptographicHash::Md5;
    case HashValue::Algorithm::Unknown: return QCryptographicHash::Md5;
    }
    return QCryptographicHash::Md5;
}
}

TEST(HashValueContractTest, DefaultValueIsInvalidAndEmpty)
{
    const HashValue value;
    EXPECT_FALSE(value.isValid());
    EXPECT_EQ(value.algorithm(), HashValue::Algorithm::Unknown);
    EXPECT_TRUE(value.raw().isEmpty());
    EXPECT_TRUE(value.toHexString().isEmpty());
}

TEST(HashValueContractTest, FromDataMatchesQtForEverySupportedAlgorithm)
{
    const QByteArray input("ArtifactStudio hash input");
    constexpr std::array algorithms = {
        HashValue::Algorithm::SHA256,
        HashValue::Algorithm::SHA1,
        HashValue::Algorithm::MD5};
    for (const auto algorithm : algorithms) {
        const auto value = HashValue::fromData(input, algorithm);
        const auto expected = QCryptographicHash::hash(input, qtAlgorithm(algorithm));
        EXPECT_TRUE(value.isValid());
        EXPECT_EQ(value.algorithm(), algorithm);
        EXPECT_EQ(value.raw(), expected);
        EXPECT_EQ(value.toHexString(), expected.toHex());
    }
}

TEST(HashValueContractTest, SupportedAlgorithmsMatchFixedAbcDigestVectors)
{
    const QByteArray input = QByteArrayLiteral("abc");
    const struct {
        HashValue::Algorithm algorithm;
        QString expectedHex;
    } cases[] = {
        {HashValue::Algorithm::SHA256,
         QStringLiteral("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")},
        {HashValue::Algorithm::SHA1,
         QStringLiteral("a9993e364706816aba3e25717850c26c9cd0d89d")},
        {HashValue::Algorithm::MD5,
         QStringLiteral("900150983cd24fb0d6963f7d28e17f72")},
    };

    for (const auto& testCase : cases) {
        EXPECT_EQ(HashValue::fromData(input, testCase.algorithm).toHexString(),
                  testCase.expectedHex);
    }
}

TEST(HashValueContractTest, EmptyInputProducesTheAlgorithmDigest)
{
    const QByteArray empty;
    const auto value = HashValue::fromData(empty, HashValue::Algorithm::SHA256);
    EXPECT_TRUE(value.isValid());
    EXPECT_EQ(value.raw(), QCryptographicHash::hash(empty, QCryptographicHash::Sha256));
}

TEST(HashValueContractTest, FromDataHashesBinaryPayloadIncludingEmbeddedNullBytes)
{
    const QByteArray input("prefix\0middle\xffsuffix", 20);
    const auto value = HashValue::fromData(input, HashValue::Algorithm::SHA256);

    EXPECT_TRUE(value.isValid());
    EXPECT_EQ(value.raw(), QCryptographicHash::hash(input, QCryptographicHash::Sha256));
}

TEST(HashValueContractTest, HexRepresentationIsLowercaseAndMatchesRawBytes)
{
    const QByteArray bytes("\x00\xab\xcd\xef", 4);
    const HashValue value(bytes, HashValue::Algorithm::SHA1);
    EXPECT_TRUE(value.isValid());
    EXPECT_EQ(value.raw(), bytes);
    EXPECT_EQ(value.toHexString(), QStringLiteral("00abcdef"));
}

TEST(HashValueContractTest, EqualityIncludesAlgorithmAndDigestBytes)
{
    const QByteArray bytes("digest");
    const HashValue sha256(bytes, HashValue::Algorithm::SHA256);
    const HashValue same(bytes, HashValue::Algorithm::SHA256);
    const HashValue sha1(bytes, HashValue::Algorithm::SHA1);
    const HashValue different(QByteArray("other"), HashValue::Algorithm::SHA256);
    EXPECT_EQ(sha256, same);
    EXPECT_NE(sha256, sha1);
    EXPECT_NE(sha256, different);
    EXPECT_FALSE(sha256 != same);
}

TEST(HashValueContractTest, EqualityIsStructuralForInvalidEmptyValues)
{
    const HashValue defaultValue;
    const HashValue explicitUnknown(QByteArray{}, HashValue::Algorithm::Unknown);
    const HashValue emptySha256(QByteArray{}, HashValue::Algorithm::SHA256);
    const HashValue sameEmptySha256(QByteArray{}, HashValue::Algorithm::SHA256);

    EXPECT_EQ(defaultValue, explicitUnknown);
    EXPECT_EQ(emptySha256, sameEmptySha256);
    EXPECT_NE(defaultValue, emptySha256);
    EXPECT_FALSE(emptySha256.isValid());
}

TEST(HashValueContractTest, ConstructorPreservesSuppliedAlgorithmAndBytes)
{
    const QByteArray bytes("manual digest");
    const HashValue value(bytes, HashValue::Algorithm::MD5);
    EXPECT_EQ(value.algorithm(), HashValue::Algorithm::MD5);
    EXPECT_EQ(value.raw(), bytes);
    EXPECT_TRUE(value.isValid());
}

TEST(HashValueContractTest, EmptySuppliedBytesAreInvalidEvenWithKnownAlgorithm)
{
    const HashValue value(QByteArray{}, HashValue::Algorithm::SHA1);
    EXPECT_FALSE(value.isValid());
    EXPECT_EQ(value.algorithm(), HashValue::Algorithm::SHA1);
}

TEST(HashValueContractTest, FromFileMatchesFromDataForContentLargerThanReadChunk)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("large.bin"));
    QByteArray contents;
    contents.resize(20000);
    for (qsizetype index = 0; index < contents.size(); ++index) {
        contents[index] = static_cast<char>((index * 37) & 0xff);
    }
    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    ASSERT_EQ(file.write(contents), contents.size());
    file.close();

    const auto fromFile = HashValue::fromFile(path, HashValue::Algorithm::SHA256);
    const auto fromData = HashValue::fromData(contents, HashValue::Algorithm::SHA256);
    EXPECT_TRUE(fromFile.isValid());
    EXPECT_EQ(fromFile, fromData);
}

TEST(HashValueContractTest, FromFileAndFromDataAgreeForEverySupportedAlgorithm)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("all-algorithms.bin"));
    const QByteArray contents("same bytes across algorithms");
    QFile writer(path);
    ASSERT_TRUE(writer.open(QIODevice::WriteOnly));
    ASSERT_EQ(writer.write(contents), contents.size());
    writer.close();

    constexpr HashValue::Algorithm algorithms[] = {
        HashValue::Algorithm::SHA256,
        HashValue::Algorithm::SHA1,
        HashValue::Algorithm::MD5};
    for (const auto algorithm : algorithms) {
        EXPECT_EQ(HashValue::fromFile(path, algorithm),
                  HashValue::fromData(contents, algorithm));
    }
}

TEST(HashValueContractTest, FromFileHashesAnEmptyFileAsAValidDigest)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("empty.bin"));
    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    file.close();

    const auto value = HashValue::fromFile(path);
    EXPECT_TRUE(value.isValid());
    EXPECT_EQ(value, HashValue::fromData(QByteArray{}));
}

TEST(HashValueContractTest, FromFileReturnsInvalidValueWhenPathCannotBeOpened)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const auto value = HashValue::fromFile(directory.filePath(QStringLiteral("missing.bin")));
    EXPECT_FALSE(value.isValid());
    EXPECT_EQ(value.algorithm(), HashValue::Algorithm::Unknown);
}

TEST(HashValueContractTest, FromFileHandlesReadChunkBoundaries)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    constexpr std::array<qsizetype, 4> sizes = {8191, 8192, 8193, 16384};
    for (const qsizetype size : sizes) {
        QByteArray contents(size, '\0');
        for (qsizetype index = 0; index < size; ++index) {
            contents[index] = static_cast<char>((index * 13 + size) & 0xff);
        }

        const QString path = directory.filePath(QStringLiteral("boundary-%1.bin").arg(size));
        QFile file(path);
        ASSERT_TRUE(file.open(QIODevice::WriteOnly));
        ASSERT_EQ(file.write(contents), size);
        file.close();

        EXPECT_EQ(HashValue::fromFile(path), HashValue::fromData(contents));
    }
}

TEST(HashValueContractTest, EveryAlgorithmHandlesFileReadChunkBoundaries)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    constexpr std::array<qsizetype, 6> sizes = {0, 8191, 8192, 8193, 16384, 16385};
    constexpr std::array algorithms = {
        HashValue::Algorithm::SHA256,
        HashValue::Algorithm::SHA1,
        HashValue::Algorithm::MD5};

    for (const qsizetype size : sizes) {
        QByteArray contents(size, '\0');
        for (qsizetype index = 0; index < size; ++index)
            contents[index] = static_cast<char>((index * 29 + size) & 0xff);

        const QString path = directory.filePath(QStringLiteral("all-algorithms-%1.bin").arg(size));
        QFile file(path);
        ASSERT_TRUE(file.open(QIODevice::WriteOnly));
        ASSERT_EQ(file.write(contents), size);
        file.close();

        for (const auto algorithm : algorithms) {
            const auto expected = QCryptographicHash::hash(contents, qtAlgorithm(algorithm));
            const auto actual = HashValue::fromFile(path, algorithm);
            EXPECT_TRUE(actual.isValid()) << size;
            EXPECT_EQ(actual.raw(), expected) << size;
            EXPECT_EQ(actual, HashValue::fromData(contents, algorithm)) << size;
        }
    }
}

TEST(HashValueContractTest, UnknownAlgorithmWithSuppliedBytesRemainsInvalid)
{
    const QByteArray bytes("opaque digest");
    const HashValue value(bytes, HashValue::Algorithm::Unknown);
    EXPECT_FALSE(value.isValid());
    EXPECT_EQ(value.algorithm(), HashValue::Algorithm::Unknown);
    EXPECT_EQ(value.raw(), bytes);
    EXPECT_EQ(value.toHexString(), bytes.toHex());
}

TEST(HashValueContractTest, FromDataRejectsUnknownAlgorithmWithoutReturningFallbackDigest)
{
    const auto value = HashValue::fromData(
        QByteArrayLiteral("payload"), HashValue::Algorithm::Unknown);

    EXPECT_FALSE(value.isValid());
    EXPECT_EQ(value.algorithm(), HashValue::Algorithm::Unknown);
    EXPECT_TRUE(value.raw().isEmpty());
    EXPECT_TRUE(value.toHexString().isEmpty());
}

TEST(HashValueContractTest, FromFileRejectsUnknownAlgorithmWithoutReturningFallbackDigest)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("unknown-algorithm.bin"));
    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    ASSERT_EQ(file.write("payload"), 7);
    file.close();

    const auto value = HashValue::fromFile(path, HashValue::Algorithm::Unknown);

    EXPECT_FALSE(value.isValid());
    EXPECT_EQ(value.algorithm(), HashValue::Algorithm::Unknown);
    EXPECT_TRUE(value.raw().isEmpty());
    EXPECT_TRUE(value.toHexString().isEmpty());
}
