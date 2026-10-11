#include <gtest/gtest.h>

#include <QString>

#include <type_traits>

import Audio.Backend;

using namespace ArtifactCore;

TEST(AudioBackendStandaloneTest, EmptyDeviceDescriptionIsInvalid)
{
    const AudioDeviceInfo device{};
    EXPECT_FALSE(device.isValid());
}

TEST(AudioBackendStandaloneTest, NonEmptyDeviceDescriptionIsValid)
{
    AudioDeviceInfo device;
    device.description = QStringLiteral("Test Output");
    EXPECT_TRUE(device.isValid());
}

TEST(AudioBackendStandaloneTest, WhitespaceDescriptionIsNonEmptyAndValid)
{
    AudioDeviceInfo device;
    device.description = QStringLiteral(" ");
    EXPECT_TRUE(device.isValid());
}

TEST(AudioBackendStandaloneTest, DefaultFormatIsInvalidUntilDimensionsArePositive)
{
    AudioBackendFormat format;
    EXPECT_FALSE(format.isValid());
    EXPECT_EQ(format.sampleRate, 0);
    EXPECT_EQ(format.channelCount, 0);
    EXPECT_EQ(format.sampleFormat, AudioBackendSampleFormat::Float32);
}

TEST(AudioBackendStandaloneTest, PositiveSampleRateAndChannelCountMakeFormatValid)
{
    AudioBackendFormat format{48000, 2, AudioBackendSampleFormat::Float32};
    EXPECT_TRUE(format.isValid());
    format.channelCount = 0;
    EXPECT_FALSE(format.isValid());
    format.channelCount = 1;
    format.sampleRate = -1;
    EXPECT_FALSE(format.isValid());
}

TEST(AudioBackendStandaloneTest, SupportsBothDeclaredSampleFormats)
{
    AudioBackendFormat format{44100, 1, AudioBackendSampleFormat::Float32};
    EXPECT_TRUE(format.isValid());
    format.sampleFormat = AudioBackendSampleFormat::Int16;
    EXPECT_TRUE(format.isValid());
    EXPECT_EQ(format.sampleFormat, AudioBackendSampleFormat::Int16);
}

TEST(AudioBackendStandaloneTest, AudioCallbackAcceptsMutableInterleavedBuffer)
{
    float samples[6]{};
    int receivedFrames = 0;
    int receivedChannels = 0;
    AudioCallback callback = [&](float* buffer, int frames, int channels) {
        receivedFrames = frames;
        receivedChannels = channels;
        for (int i = 0; i < frames * channels; ++i)
            buffer[i] = static_cast<float>(i + 1);
    };

    callback(samples, 3, 2);

    EXPECT_EQ(receivedFrames, 3);
    EXPECT_EQ(receivedChannels, 2);
    EXPECT_FLOAT_EQ(samples[0], 1.0f);
    EXPECT_FLOAT_EQ(samples[5], 6.0f);
}

TEST(AudioBackendStandaloneTest, BackendRemainsAnAbstractInterface)
{
    EXPECT_TRUE(std::is_abstract_v<AudioBackend>);
    EXPECT_TRUE(std::has_virtual_destructor_v<AudioBackend>);
}

