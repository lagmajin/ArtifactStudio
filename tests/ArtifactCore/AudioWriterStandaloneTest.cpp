#include <gtest/gtest.h>

#include <QFile>
#include <QTemporaryDir>
#include <QVector>
#include <QByteArray>
#include <QString>
#include <QtGlobal>

#include <cmath>
#include <cstdint>
#include <limits>
#include <utility>

import Audio.Render.Writer;
import Audio.Segment;

using namespace ArtifactCore;

namespace {
quint16 readU16(const QByteArray& bytes, qsizetype offset)
{
    const auto* data = reinterpret_cast<const unsigned char*>(bytes.constData());
    return static_cast<quint16>(data[offset]) |
           static_cast<quint16>(static_cast<quint16>(data[offset + 1]) << 8);
}

quint32 readU32(const QByteArray& bytes, qsizetype offset)
{
    const auto* data = reinterpret_cast<const unsigned char*>(bytes.constData());
    return static_cast<quint32>(data[offset]) |
           (static_cast<quint32>(data[offset + 1]) << 8) |
           (static_cast<quint32>(data[offset + 2]) << 16) |
           (static_cast<quint32>(data[offset + 3]) << 24);
}

QByteArray readFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    return file.readAll();
}

AudioSegment mono(QVector<float> samples, int sampleRate = 48000)
{
    AudioSegment segment;
    segment.sampleRate = sampleRate;
    segment.layout = AudioChannelLayout::Mono;
    segment.channelData.push_back(std::move(samples));
    return segment;
}

bool hasRiffHeader(const QByteArray& bytes)
{
    return bytes.size() >= 44 && bytes.startsWith("RIFF") &&
           bytes.mid(8, 4) == "WAVE" && bytes.mid(12, 4) == "fmt " &&
           bytes.mid(36, 4) == "data";
}
}

TEST(AudioWriterStandaloneTest, EmptyPathDoesNotCreateOutput)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString whitespacePath = dir.path() + QStringLiteral("/   ");
    AudioWriter writer;
    writer.openFile(whitespacePath);
    writer.write(mono(QVector<float>{1.0f}));
    writer.closeFile();
    EXPECT_FALSE(QFile::exists(whitespacePath));
}

TEST(AudioWriterStandaloneTest, WritesValidSixteenBitPcmHeaderAndPayload)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString path = dir.filePath("s16.wav");
    AudioWriter writer;
    writer.setBitDepth(16);
    writer.openFile(path);
    writer.write(mono(QVector<float>{-1.0f, 0.0f, 1.0f}, 44100));
    writer.closeFile();
    const QByteArray bytes = readFile(path);

    ASSERT_TRUE(hasRiffHeader(bytes));
    EXPECT_EQ(readU32(bytes, 4), 42u);
    EXPECT_EQ(readU16(bytes, 20), 1u);
    EXPECT_EQ(readU16(bytes, 22), 1u);
    EXPECT_EQ(readU32(bytes, 24), 44100u);
    EXPECT_EQ(readU16(bytes, 34), 16u);
    EXPECT_EQ(readU32(bytes, 40), 6u);
    EXPECT_EQ(static_cast<qint16>(readU16(bytes, 44)), -32767);
    EXPECT_EQ(static_cast<qint16>(readU16(bytes, 46)), 0);
    EXPECT_EQ(static_cast<qint16>(readU16(bytes, 48)), 32767);
}

TEST(AudioWriterStandaloneTest, WritesTwentyFourBitPcmWithSignExtensionLayout)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString path = dir.filePath("s24.wav");
    AudioWriter writer;
    writer.setBitDepth(24);
    writer.openFile(path);
    writer.write(mono(QVector<float>{-1.0f, 0.0f, 1.0f}));
    writer.closeFile();
    const QByteArray bytes = readFile(path);

    ASSERT_TRUE(hasRiffHeader(bytes));
    EXPECT_EQ(readU16(bytes, 34), 24u);
    EXPECT_EQ(readU32(bytes, 40), 9u);
    EXPECT_EQ(static_cast<unsigned char>(bytes[44]), 0x01);
    EXPECT_EQ(static_cast<unsigned char>(bytes[45]), 0x00);
    EXPECT_EQ(static_cast<unsigned char>(bytes[46]), 0x80);
    EXPECT_EQ(static_cast<unsigned char>(bytes[50]), 0xff);
    EXPECT_EQ(static_cast<unsigned char>(bytes[51]), 0xff);
    EXPECT_EQ(static_cast<unsigned char>(bytes[52]), 0x7f);
}

TEST(AudioWriterStandaloneTest, InvalidBitDepthLeavesDefaultSixteenBitFormat)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString path = dir.filePath("default.wav");
    AudioWriter writer;
    writer.setBitDepth(32);
    writer.openFile(path);
    writer.write(mono(QVector<float>{0.0f}));
    writer.closeFile();

    const QByteArray bytes = readFile(path);
    ASSERT_TRUE(hasRiffHeader(bytes));
    EXPECT_EQ(readU16(bytes, 34), 16u);
}

TEST(AudioWriterStandaloneTest, BitDepthCannotChangeAfterWritingStarts)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString path = dir.filePath("fixed.wav");
    AudioWriter writer;
    writer.openFile(path);
    writer.write(mono(QVector<float>{0.5f}));
    writer.setBitDepth(24);
    writer.closeFile();

    EXPECT_EQ(readU16(readFile(path), 34), 16u);
}

TEST(AudioWriterStandaloneTest, NonFiniteAndOutOfRangeSamplesAreSanitized)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString path = dir.filePath("sanitize.wav");
    AudioWriter writer;
    writer.openFile(path);
    writer.write(mono(QVector<float>{
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(), 2.0f, -2.0f}));
    writer.closeFile();
    const QByteArray bytes = readFile(path);

    ASSERT_TRUE(hasRiffHeader(bytes));
    EXPECT_EQ(static_cast<qint16>(readU16(bytes, 44)), 0);
    EXPECT_EQ(static_cast<qint16>(readU16(bytes, 46)), 0);
    EXPECT_EQ(static_cast<qint16>(readU16(bytes, 48)), 32767);
    EXPECT_EQ(static_cast<qint16>(readU16(bytes, 50)), -32767);
}

TEST(AudioWriterStandaloneTest, AppendsSegmentsAtTheOriginalSampleRate)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString path = dir.filePath("append.wav");
    AudioWriter writer;
    writer.openFile(path);
    writer.write(mono(QVector<float>{0.25f}));
    writer.write(mono(QVector<float>{0.5f, 0.75f}));
    writer.closeFile();
    const QByteArray bytes = readFile(path);

    ASSERT_TRUE(hasRiffHeader(bytes));
    EXPECT_EQ(readU32(bytes, 40), 6u);
    EXPECT_EQ(bytes.size(), 50);
}

TEST(AudioWriterStandaloneTest, IgnoresSegmentsAtDifferentSampleRate)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString path = dir.filePath("rate.wav");
    AudioWriter writer;
    writer.openFile(path);
    writer.write(mono(QVector<float>{0.25f}, 48000));
    writer.write(mono(QVector<float>{0.75f, 0.5f}, 44100));
    writer.closeFile();
    const QByteArray bytes = readFile(path);

    ASSERT_TRUE(hasRiffHeader(bytes));
    EXPECT_EQ(readU32(bytes, 24), 48000u);
    EXPECT_EQ(readU32(bytes, 40), 2u);
    EXPECT_EQ(bytes.size(), 46);
}

TEST(AudioWriterStandaloneTest, PadsMissingChannelsWithSilence)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString path = dir.filePath("pad.wav");
    AudioSegment stereo;
    stereo.sampleRate = 48000;
    stereo.layout = AudioChannelLayout::Stereo;
    stereo.channelData.push_back(QVector<float>{0.0f});
    stereo.channelData.push_back(QVector<float>{0.0f});
    AudioWriter writer;
    writer.openFile(path);
    writer.write(stereo);
    writer.write(mono(QVector<float>{0.5f, -0.5f}));
    writer.closeFile();
    const QByteArray bytes = readFile(path);

    ASSERT_TRUE(hasRiffHeader(bytes));
    EXPECT_EQ(readU16(bytes, 22), 2u);
    EXPECT_EQ(readU32(bytes, 40), 12u);
    EXPECT_EQ(static_cast<qint16>(readU16(bytes, 48)), 16384);
    EXPECT_EQ(static_cast<qint16>(readU16(bytes, 50)), 0);
    EXPECT_EQ(static_cast<qint16>(readU16(bytes, 52)), -16384);
    EXPECT_EQ(static_cast<qint16>(readU16(bytes, 54)), 0);
}

TEST(AudioWriterStandaloneTest, IgnoresExtraChannelsAfterHeaderIsEstablished)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString path = dir.filePath("extra.wav");
    AudioWriter writer;
    writer.openFile(path);
    writer.write(mono(QVector<float>{0.25f}));
    AudioSegment stereo;
    stereo.sampleRate = 48000;
    stereo.layout = AudioChannelLayout::Stereo;
    stereo.channelData.push_back(QVector<float>{0.5f});
    stereo.channelData.push_back(QVector<float>{1.0f});
    writer.write(stereo);
    writer.closeFile();

    const QByteArray bytes = readFile(path);
    ASSERT_TRUE(hasRiffHeader(bytes));
    EXPECT_EQ(readU16(bytes, 22), 1u);
    EXPECT_EQ(readU32(bytes, 40), 4u);
    EXPECT_EQ(bytes.size(), 48);
}

TEST(AudioWriterStandaloneTest, CloseAndDestructorBothFinalizeRiffHeader)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString explicitPath = dir.filePath("explicit.wav");
    const QString destructorPath = dir.filePath("destructor.wav");
    {
        AudioWriter writer;
        writer.openFile(explicitPath);
        writer.write(mono(QVector<float>{0.25f}));
        writer.closeFile();
    }
    {
        AudioWriter writer;
        writer.openFile(destructorPath);
        writer.write(mono(QVector<float>{0.25f}));
    }

    EXPECT_TRUE(hasRiffHeader(readFile(explicitPath)));
    EXPECT_TRUE(hasRiffHeader(readFile(destructorPath)));
    EXPECT_EQ(readU32(readFile(destructorPath), 40), 2u);
}

TEST(AudioWriterStandaloneTest, EmptyOrInvalidSegmentsDoNotCreateDataPayload)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString path = dir.filePath("empty.wav");
    AudioWriter writer;
    writer.openFile(path);
    writer.write(mono(QVector<float>{}));
    writer.write(mono(QVector<float>{1.0f}, 0));
    writer.closeFile();

    EXPECT_TRUE(readFile(path).isEmpty());
}

