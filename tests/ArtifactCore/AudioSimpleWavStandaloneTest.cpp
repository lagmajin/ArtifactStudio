#include <gtest/gtest.h>

#include <QDataStream>
#include <QFile>
#include <QTemporaryDir>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <limits>

import Audio.SimpleWav;
import Utils.String.UniString;

using namespace ArtifactCore;

namespace {
void appendU16(QByteArray& bytes, quint16 value)
{
    bytes.append(static_cast<char>(value & 0xff));
    bytes.append(static_cast<char>((value >> 8) & 0xff));
}

void appendU32(QByteArray& bytes, quint32 value)
{
    for (int shift = 0; shift < 32; shift += 8)
        bytes.append(static_cast<char>((value >> shift) & 0xff));
}

QByteArray makeWave(quint16 format, quint16 channels, quint32 sampleRate,
                    quint16 bitsPerSample, const QByteArray& pcm)
{
    const quint16 blockAlign = static_cast<quint16>(channels * (bitsPerSample / 8));
    QByteArray bytes;
    bytes.append("RIFF", 4);
    appendU32(bytes, static_cast<quint32>(36 + pcm.size()));
    bytes.append("WAVEfmt ", 8);
    appendU32(bytes, 16);
    appendU16(bytes, format);
    appendU16(bytes, channels);
    appendU32(bytes, sampleRate);
    appendU32(bytes, sampleRate * blockAlign);
    appendU16(bytes, blockAlign);
    appendU16(bytes, bitsPerSample);
    bytes.append("data", 4);
    appendU32(bytes, static_cast<quint32>(pcm.size()));
    bytes.append(pcm);
    return bytes;
}

QByteArray makeWaveWithOddJunkChunk(quint16 format, quint16 channels,
                                    quint32 sampleRate, quint16 bitsPerSample,
                                    const QByteArray& pcm)
{
    QByteArray bytes = makeWave(format, channels, sampleRate, bitsPerSample, pcm);
    const int dataOffset = bytes.indexOf("data");
    if (dataOffset < 0) return {};

    QByteArray junk;
    junk.append("JUNK", 4);
    appendU32(junk, 3);
    junk.append("abc", 3);
    junk.append(char(0));
    bytes.insert(dataOffset, junk);

    const quint32 riffSize = static_cast<quint32>(bytes.size() - 8);
    for (int shift = 0; shift < 32; shift += 8)
        bytes[4 + shift / 8] = static_cast<char>((riffSize >> shift) & 0xff);
    return bytes;
}

bool writeFile(const QString& path, const QByteArray& data)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return false;
    return file.write(data) == data.size();
}

QByteArray pcm16(std::initializer_list<qint16> samples)
{
    QByteArray bytes;
    for (const qint16 sample : samples)
        appendU16(bytes, static_cast<quint16>(sample));
    return bytes;
}

QByteArray pcm24(std::initializer_list<qint32> samples)
{
    QByteArray bytes;
    for (qint32 sample : samples) {
        const quint32 bits = static_cast<quint32>(sample);
        bytes.append(static_cast<char>(bits & 0xff));
        bytes.append(static_cast<char>((bits >> 8) & 0xff));
        bytes.append(static_cast<char>((bits >> 16) & 0xff));
    }
    return bytes;
}

QByteArray pcmFloat(std::initializer_list<float> samples)
{
    QByteArray bytes;
    for (float sample : samples) {
        quint32 bits = 0;
        std::memcpy(&bits, &sample, sizeof(bits));
        appendU32(bytes, bits);
    }
    return bytes;
}
}

TEST(AudioSimpleWavStandaloneTest, NewReaderStartsWithEmptyMetadata)
{
    const SimpleWav wav;
    EXPECT_EQ(wav.sampleRate(), 0);
    EXPECT_EQ(wav.bitDepth(), 0);
    EXPECT_EQ(wav.channelCount(), 0);
    EXPECT_EQ(wav.frameCount(), 0);
    EXPECT_TRUE(wav.getAudioData().isEmpty());
}

TEST(AudioSimpleWavStandaloneTest, LoadsUnsignedEightBitPcmAsNormalizedFloats)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    QByteArray pcm;
    pcm.append(char(0));
    pcm.append(char(128));
    pcm.append(char(255));
    ASSERT_TRUE(writeFile(dir.filePath("u8.wav"), makeWave(1, 1, 48000, 8, pcm)));
    SimpleWav wav;

    ASSERT_TRUE(wav.loadFromFile(dir.filePath("u8.wav")));

    EXPECT_EQ(wav.sampleRate(), 48000);
    EXPECT_EQ(wav.bitDepth(), 16);
    EXPECT_EQ(wav.channelCount(), 1);
    EXPECT_EQ(wav.frameCount(), 3);
    EXPECT_FLOAT_EQ(wav.getAudioData()[0], -1.0f);
    EXPECT_FLOAT_EQ(wav.getAudioData()[1], 0.0f);
    EXPECT_FLOAT_EQ(wav.getAudioData()[2], 127.0f / 128.0f);
}

TEST(AudioSimpleWavStandaloneTest, LoadsSignedSixteenBitPcmIncludingExtrema)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    ASSERT_TRUE(writeFile(dir.filePath("s16.wav"),
                          makeWave(1, 1, 44100, 16, pcm16({-32768, 0, 32767}))));
    SimpleWav wav;

    ASSERT_TRUE(wav.loadFromFile(dir.filePath("s16.wav")));

    EXPECT_FLOAT_EQ(wav.getAudioData()[0], -1.0f);
    EXPECT_FLOAT_EQ(wav.getAudioData()[1], 0.0f);
    EXPECT_NEAR(wav.getAudioData()[2], 32767.0f / 32768.0f, 1e-7f);
}

TEST(AudioSimpleWavStandaloneTest, LoadsDataChunkBeforeFormatChunk)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QByteArray canonical = makeWave(1, 1, 48000, 16, pcm16({1234, -2345}));
    ASSERT_EQ(canonical.size(), 48);
    QByteArray reordered = canonical.left(12);
    reordered.append(canonical.mid(36));
    reordered.append(canonical.mid(12, 24));
    const QString path = dir.filePath("data_before_fmt.wav");
    ASSERT_TRUE(writeFile(path, reordered));
    SimpleWav wav;

    ASSERT_TRUE(wav.loadFromFile(path));

    EXPECT_EQ(wav.sampleRate(), 48000);
    EXPECT_EQ(wav.channelCount(), 1);
    EXPECT_EQ(wav.bitDepth(), 16);
    EXPECT_EQ(wav.frameCount(), 2);
    ASSERT_EQ(wav.getAudioData().size(), 2);
    EXPECT_FLOAT_EQ(wav.getAudioData()[0], 1234.0f / 32768.0f);
    EXPECT_FLOAT_EQ(wav.getAudioData()[1], -2345.0f / 32768.0f);
}

TEST(AudioSimpleWavStandaloneTest, SkipsOddSizedUnknownChunkAndItsPadding)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QByteArray pcm = pcm16({-1234, 2345});
    ASSERT_TRUE(writeFile(dir.filePath("odd_chunk.wav"), makeWaveWithOddJunkChunk(
        1, 1, 48000, 16, pcm)));
    SimpleWav wav;

    ASSERT_TRUE(wav.loadFromFile(dir.filePath("odd_chunk.wav")));

    EXPECT_EQ(wav.sampleRate(), 48000);
    EXPECT_EQ(wav.frameCount(), 2);
    ASSERT_EQ(wav.getAudioData().size(), 2);
    EXPECT_FLOAT_EQ(wav.getAudioData()[0], -1234.0f / 32768.0f);
    EXPECT_FLOAT_EQ(wav.getAudioData()[1], 2345.0f / 32768.0f);
}

TEST(AudioSimpleWavStandaloneTest, LoadsAndSignExtendsTwentyFourBitPcm)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    ASSERT_TRUE(writeFile(dir.filePath("s24.wav"), makeWave(
        1, 1, 48000, 24, pcm24({-8388608, 0, 8388607}))));
    SimpleWav wav;

    ASSERT_TRUE(wav.loadFromFile(dir.filePath("s24.wav")));

    EXPECT_FLOAT_EQ(wav.getAudioData()[0], -1.0f);
    EXPECT_FLOAT_EQ(wav.getAudioData()[1], 0.0f);
    EXPECT_NEAR(wav.getAudioData()[2], 8388607.0f / 8388608.0f, 1e-7f);
}

TEST(AudioSimpleWavStandaloneTest, LoadsFloatPcmAndSanitizesOutOfRangeAndNonFiniteSamples)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    ASSERT_TRUE(writeFile(dir.filePath("f32.wav"), makeWave(
        3, 1, 48000, 32,
        pcmFloat({-1.25f, std::numeric_limits<float>::quiet_NaN(), 0.5f, 1.5f}))));
    SimpleWav wav;

    ASSERT_TRUE(wav.loadFromFile(dir.filePath("f32.wav")));

    EXPECT_EQ(wav.bitDepth(), 32);
    EXPECT_EQ(wav.frameCount(), 4);
    EXPECT_FLOAT_EQ(wav.getAudioData()[0], -1.0f);
    EXPECT_FLOAT_EQ(wav.getAudioData()[1], 0.0f);
    EXPECT_FLOAT_EQ(wav.getAudioData()[2], 0.5f);
    EXPECT_FLOAT_EQ(wav.getAudioData()[3], 1.0f);
}

TEST(AudioSimpleWavStandaloneTest, MaxFramesTruncatesAtCompleteInterleavedFrames)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QByteArray pcm = pcm16({100, -100, 200, -200, 300, -300});
    ASSERT_TRUE(writeFile(dir.filePath("stereo.wav"), makeWave(1, 2, 48000, 16, pcm)));
    SimpleWav wav;

    ASSERT_TRUE(wav.loadFromFile(dir.filePath("stereo.wav"), 2));

    EXPECT_EQ(wav.channelCount(), 2);
    EXPECT_EQ(wav.frameCount(), 2);
    EXPECT_EQ(wav.getAudioData().size(), 4);
}

TEST(AudioSimpleWavStandaloneTest, ZeroMaxFramesLeavesInputUntruncated)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QByteArray pcm = pcm16({100, -100, 200, -200, 300, -300});
    const QString path = dir.filePath("stereo.wav");
    ASSERT_TRUE(writeFile(path, makeWave(1, 2, 48000, 16, pcm)));
    SimpleWav wav;

    ASSERT_TRUE(wav.loadFromFile(path, 0));

    EXPECT_EQ(wav.channelCount(), 2);
    EXPECT_EQ(wav.frameCount(), 3);
    EXPECT_EQ(wav.getAudioData().size(), 6);
    EXPECT_FLOAT_EQ(wav.getAudioData()[4], 300.0f / 32768.0f);
    EXPECT_FLOAT_EQ(wav.getAudioData()[5], -300.0f / 32768.0f);
}

TEST(AudioSimpleWavStandaloneTest, NegativeMaxFramesLeavesInputUntruncated)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString path = dir.filePath("stereo.wav");
    ASSERT_TRUE(writeFile(path, makeWave(
        1, 2, 48000, 16, pcm16({100, -100, 200, -200, 300, -300}))));
    SimpleWav wav;

    ASSERT_TRUE(wav.loadFromFile(path, -2));

    EXPECT_EQ(wav.channelCount(), 2);
    EXPECT_EQ(wav.frameCount(), 3);
    EXPECT_EQ(wav.getAudioData().size(), 6);
    EXPECT_FLOAT_EQ(wav.getAudioData()[4], 300.0f / 32768.0f);
    EXPECT_FLOAT_EQ(wav.getAudioData()[5], -300.0f / 32768.0f);
}

TEST(AudioSimpleWavStandaloneTest, RoundTripsSavedFloatWaveData)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString sourcePath = dir.filePath("source.wav");
    const QString savedPath = dir.filePath("saved.wav");
    ASSERT_TRUE(writeFile(sourcePath, makeWave(
        3, 1, 32000, 32, pcmFloat({-0.75f, 0.0f, 0.875f}))));
    SimpleWav original;
    ASSERT_TRUE(original.loadFromFile(sourcePath));
    ASSERT_TRUE(original.saveToFile(savedPath));

    SimpleWav restored;
    ASSERT_TRUE(restored.loadFromFile(savedPath));

    EXPECT_EQ(restored.sampleRate(), 32000);
    EXPECT_EQ(restored.channelCount(), 1);
    EXPECT_EQ(restored.bitDepth(), 32);
    EXPECT_EQ(restored.frameCount(), 3);
    EXPECT_EQ(restored.getAudioData(), original.getAudioData());
}

TEST(AudioSimpleWavStandaloneTest, UniStringPathOverloadsLoadAndSaveUnicodePaths)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString sourcePath = dir.filePath(QStringLiteral("素材.wav"));
    const QString savedPath = dir.filePath(QStringLiteral("保存.wav"));
    ASSERT_TRUE(writeFile(sourcePath, makeWave(
        1, 1, 48000, 16, pcm16({1234, -2345}))));

    SimpleWav original;
    ASSERT_TRUE(original.loadFromFile(UniString(sourcePath)));
    ASSERT_TRUE(original.saveToFile(UniString(savedPath)));

    SimpleWav restored;
    ASSERT_TRUE(restored.loadFromFile(savedPath));
    EXPECT_EQ(restored.sampleRate(), 48000);
    EXPECT_EQ(restored.frameCount(), 2);
    EXPECT_EQ(restored.getAudioData(), original.getAudioData());
}

TEST(AudioSimpleWavStandaloneTest, SavingIntegerPcmWritesFloatWaveWithoutChangingNormalizedSamples)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString sourcePath = dir.filePath("source_s16.wav");
    const QString savedPath = dir.filePath("saved_float.wav");
    ASSERT_TRUE(writeFile(sourcePath, makeWave(
        1, 2, 44100, 16, pcm16({-32768, 16384, 12345, -23456}))));
    SimpleWav original;
    ASSERT_TRUE(original.loadFromFile(sourcePath));
    ASSERT_EQ(original.bitDepth(), 16);
    const QVector<float> normalizedSamples = original.getAudioData();

    ASSERT_TRUE(original.saveToFile(savedPath));

    SimpleWav restored;
    ASSERT_TRUE(restored.loadFromFile(savedPath));
    EXPECT_EQ(restored.sampleRate(), 44100);
    EXPECT_EQ(restored.channelCount(), 2);
    EXPECT_EQ(restored.bitDepth(), 32);
    EXPECT_EQ(restored.frameCount(), 2);
    EXPECT_EQ(restored.getAudioData(), normalizedSamples);
}

TEST(AudioSimpleWavStandaloneTest, ReturnedAudioDataCanBeEditedWithoutChangingReaderState)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString path = dir.filePath("samples.wav");
    ASSERT_TRUE(writeFile(path, makeWave(1, 1, 48000, 16, pcm16({100, -200}))));
    SimpleWav wav;
    ASSERT_TRUE(wav.loadFromFile(path));

    QVector<float> returnedSamples = wav.getAudioData();
    returnedSamples[0] = 0.75f;
    returnedSamples.push_back(0.5f);

    const QVector<float> storedSamples = wav.getAudioData();
    ASSERT_EQ(storedSamples.size(), 2);
    EXPECT_FLOAT_EQ(storedSamples[0], 100.0f / 32768.0f);
    EXPECT_FLOAT_EQ(storedSamples[1], -200.0f / 32768.0f);
    EXPECT_EQ(wav.frameCount(), 2);
}

TEST(AudioSimpleWavStandaloneTest, RejectsMissingAndInvalidRiffWaveFiles)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    ASSERT_TRUE(writeFile(dir.filePath("invalid.wav"), QByteArray("NOPEbad!", 8)));
    SimpleWav wav;

    EXPECT_FALSE(wav.loadFromFile(dir.filePath("missing.wav")));
    EXPECT_FALSE(wav.loadFromFile(dir.filePath("invalid.wav")));
    EXPECT_EQ(wav.frameCount(), 0);
}

TEST(AudioSimpleWavStandaloneTest, FailedReloadClearsPreviouslyLoadedAudio)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString validPath = dir.filePath("valid.wav");
    const QString invalidPath = dir.filePath("invalid.wav");
    ASSERT_TRUE(writeFile(validPath, makeWave(1, 1, 48000, 16, pcm16({100, -200}))));
    ASSERT_TRUE(writeFile(invalidPath, QByteArray("NOPEbad!", 8)));
    SimpleWav wav;
    ASSERT_TRUE(wav.loadFromFile(validPath));
    ASSERT_EQ(wav.frameCount(), 2);

    EXPECT_FALSE(wav.loadFromFile(invalidPath));

    EXPECT_EQ(wav.sampleRate(), 0);
    EXPECT_EQ(wav.bitDepth(), 0);
    EXPECT_EQ(wav.channelCount(), 0);
    EXPECT_EQ(wav.frameCount(), 0);
    EXPECT_TRUE(wav.getAudioData().isEmpty());
}

TEST(AudioSimpleWavStandaloneTest, RejectsTruncatedDataChunk)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    QByteArray truncated = makeWave(1, 1, 48000, 16, pcm16({1}));
    truncated.chop(2);
    ASSERT_TRUE(writeFile(dir.filePath("truncated.wav"), truncated));
    SimpleWav wav;

    EXPECT_FALSE(wav.loadFromFile(dir.filePath("truncated.wav")));
    EXPECT_EQ(wav.frameCount(), 0);
}

TEST(AudioSimpleWavStandaloneTest, RejectsUnsupportedEncoding)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    ASSERT_TRUE(writeFile(dir.filePath("unsupported.wav"), makeWave(
        6, 1, 48000, 16, pcm16({1, 2}))));
    SimpleWav wav;

    EXPECT_FALSE(wav.loadFromFile(dir.filePath("unsupported.wav")));
    EXPECT_EQ(wav.frameCount(), 0);
}

TEST(AudioSimpleWavStandaloneTest, SaveRejectsEmptyPathOrUninitializedAudio)
{
    SimpleWav wav;
    EXPECT_FALSE(wav.saveToFile(QString{}));
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    EXPECT_FALSE(wav.saveToFile(dir.filePath("empty.wav")));
}

TEST(AudioSimpleWavStandaloneTest, FailedSaveLeavesLoadedReaderStateUntouched)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString sourcePath = dir.filePath("source.wav");
    ASSERT_TRUE(writeFile(sourcePath, makeWave(
        1, 1, 48000, 16, pcm16({1234, -2345}))));
    SimpleWav wav;
    ASSERT_TRUE(wav.loadFromFile(sourcePath));
    const QVector<float> originalSamples = wav.getAudioData();
    const QString invalidOutputPath = dir.filePath("missing/output.wav");

    EXPECT_FALSE(wav.saveToFile(invalidOutputPath));

    EXPECT_EQ(wav.sampleRate(), 48000);
    EXPECT_EQ(wav.channelCount(), 1);
    EXPECT_EQ(wav.bitDepth(), 16);
    EXPECT_EQ(wav.frameCount(), 2);
    EXPECT_EQ(wav.getAudioData(), originalSamples);
}

