#include <gtest/gtest.h>

#include <QString>
#include <QVector>

#include <limits>

import Codec.Encoder.Setting;
import Utils.String.UniString;

using ArtifactCore::AudioCodec;
using ArtifactCore::EncoderSettings;
using ArtifactCore::ImageFormat;
using ArtifactCore::OutputMode;
using ArtifactCore::VideoCodec;
using ArtifactCore::VideoProfile;

TEST(EncoderSettingsContractTest, DefaultsDescribeAValidVideoAndAudioExport)
{
    const EncoderSettings settings;

    EXPECT_EQ(settings.getOutputMode(), OutputMode::VideoAndAudio);
    EXPECT_EQ(settings.getVideoCodec(), VideoCodec::H264);
    EXPECT_EQ(settings.getAudioCodec(), AudioCodec::AAC);
    EXPECT_EQ(settings.getVideoProfile(), VideoProfile::H264_High);
    EXPECT_EQ(settings.getWidth(), 1920);
    EXPECT_EQ(settings.getHeight(), 1080);
    EXPECT_DOUBLE_EQ(settings.getFrameRate(), 60.0);
    EXPECT_TRUE(settings.isValid());
    EXPECT_TRUE(settings.isProfileValidForCodec(
        settings.getVideoCodec(), settings.getVideoProfile()));
}

TEST(EncoderSettingsContractTest, NumericSettersClampToDocumentedRanges)
{
    EncoderSettings settings;
    settings.setWidth(1);
    settings.setHeight(10000);
    settings.setFrameRate(0.0);
    settings.setBitrate(1000);
    settings.setQuality(-5);
    settings.setAudioBitrate(1);
    settings.setAudioSampleRate(12345);
    settings.setStartTimeCodeFrame(-4);
    settings.setImageSequencePadding(20);

    EXPECT_EQ(settings.getWidth(), 320);
    EXPECT_EQ(settings.getHeight(), 4320);
    EXPECT_DOUBLE_EQ(settings.getFrameRate(), 1.0);
    EXPECT_EQ(settings.getBitrate(), 500);
    EXPECT_EQ(settings.getQuality(), 0);
    EXPECT_EQ(settings.getAudioBitrate(), 32);
    EXPECT_EQ(settings.getAudioSampleRate(), 48000);
    EXPECT_EQ(settings.getStartTimeCodeFrame(), 0);
    EXPECT_EQ(settings.getImageSequencePadding(), 8);

    settings.setWidth(9000);
    settings.setHeight(100);
    settings.setFrameRate(240.0);
    settings.setBitrate(0);
    settings.setQuality(105);
    settings.setAudioBitrate(500);
    settings.setAudioSampleRate(96000);
    settings.setStartTimeCodeFrame(120);
    settings.setImageSequencePadding(0);

    EXPECT_EQ(settings.getWidth(), 7680);
    EXPECT_EQ(settings.getHeight(), 240);
    EXPECT_DOUBLE_EQ(settings.getFrameRate(), 120.0);
    EXPECT_EQ(settings.getBitrate(), 1);
    EXPECT_EQ(settings.getQuality(), 100);
    EXPECT_EQ(settings.getAudioBitrate(), 320);
    EXPECT_EQ(settings.getAudioSampleRate(), 96000);
    EXPECT_EQ(settings.getStartTimeCodeFrame(), 120);
    EXPECT_EQ(settings.getImageSequencePadding(), 1);

    settings.setFrameRate(37.5);
    settings.setFrameRate(std::numeric_limits<double>::quiet_NaN());
    EXPECT_DOUBLE_EQ(settings.getFrameRate(), 37.5);
    settings.setFrameRate(std::numeric_limits<double>::infinity());
    EXPECT_DOUBLE_EQ(settings.getFrameRate(), 37.5);
    settings.setFrameRate(-std::numeric_limits<double>::infinity());
    EXPECT_DOUBLE_EQ(settings.getFrameRate(), 37.5);
}

TEST(EncoderSettingsContractTest,
     PresetsSetTheirCodecDimensionsRateAndRecommendedBitrate)
{
    EncoderSettings settings;
    settings.applyPreset(EncoderSettings::Preset::YouTube_4K_60fps);

    EXPECT_EQ(settings.getVideoCodec(), VideoCodec::H265);
    EXPECT_EQ(settings.getWidth(), 3840);
    EXPECT_EQ(settings.getHeight(), 2160);
    EXPECT_DOUBLE_EQ(settings.getFrameRate(), 60.0);
    EXPECT_EQ(settings.getBitrate(), 50);
    EXPECT_EQ(settings.getAudioCodec(), AudioCodec::AAC);
    EXPECT_EQ(settings.getAudioBitrate(), 256);
    EXPECT_EQ(settings.getRecommendedBitrate(), 500);
    EXPECT_EQ(settings.getRecommendedAudioBitrate(), 128);

    settings.applyPreset(EncoderSettings::Preset::ProRes_422_FullHD);
    EXPECT_EQ(settings.getVideoCodec(), VideoCodec::AppleProRes);
    EXPECT_EQ(settings.getVideoProfile(), VideoProfile::ProRes_422);
    EXPECT_EQ(settings.getWidth(), 1920);
    EXPECT_EQ(settings.getHeight(), 1080);
    EXPECT_DOUBLE_EQ(settings.getFrameRate(), 24.0);
    EXPECT_TRUE(settings.isValid());

    settings.applyPreset(EncoderSettings::Preset::WebH265_High);
    EXPECT_EQ(settings.getVideoCodec(), VideoCodec::H265);
    EXPECT_EQ(settings.getWidth(), 1920);
    EXPECT_EQ(settings.getHeight(), 1080);
    EXPECT_DOUBLE_EQ(settings.getFrameRate(), 30.0);
    EXPECT_EQ(settings.getRecommendedBitrate(), 248);
}

TEST(EncoderSettingsContractTest,
     DropFrameValidationDependsOnRateAndOutputModeCodecRequirements)
{
    EncoderSettings settings;
    settings.setDropFrame(true);
    settings.setFrameRate(30.0);
    EXPECT_FALSE(settings.isValid());
    EXPECT_EQ(settings.getValidationErrors().size(), 1);

    settings.setFrameRate(29.97);
    EXPECT_TRUE(settings.isValid());

    settings.setOutputMode(OutputMode::AudioOnly);
    settings.setAudioCodec(AudioCodec::None);
    EXPECT_FALSE(settings.isValid());
    settings.setAudioCodec(AudioCodec::AAC);
    EXPECT_TRUE(settings.isValid());

    settings.setOutputMode(OutputMode::VideoOnly);
    settings.setVideoCodec(VideoCodec::None);
    EXPECT_FALSE(settings.isValid());
    settings.setVideoCodec(VideoCodec::H264);
    EXPECT_TRUE(settings.isValid());
}

TEST(EncoderSettingsContractTest,
     ProResProfileValidationAndImageSequenceNamingAreConsistent)
{
    EncoderSettings settings;
    settings.setVideoCodec(VideoCodec::AppleProRes);
    settings.setVideoProfile(VideoProfile::H264_High);
    EXPECT_FALSE(settings.isValid());
    EXPECT_FALSE(settings.isProfileValidForCodec(
        VideoCodec::AppleProRes, VideoProfile::H264_High));

    settings.setVideoProfile(VideoProfile::ProRes_4444);
    EXPECT_TRUE(settings.isValid());
    EXPECT_TRUE(settings.isProfileValidForCodec(
        VideoCodec::AppleProRes, VideoProfile::ProRes_4444));

    settings.setImageSequencePrefix(QStringLiteral("render"));
    settings.setImageSequencePadding(5);
    settings.setImageFormat(ImageFormat::PNG);
    EXPECT_EQ(settings.generateImageSequencePath(7),
              QStringLiteral("render_00007.png"));
    settings.setImageFormat(ImageFormat::JPEG);
    EXPECT_EQ(settings.generateImageSequencePath(42),
              QStringLiteral("render_00042.jpg"));
    settings.setImageFormat(ImageFormat::EXR);
    EXPECT_EQ(settings.getImageFormatExtension(), QStringLiteral(".exr"));
}

TEST(EncoderSettingsContractTest,
     SerializedFieldsRoundTripAndCopyMutationsRemainIndependent)
{
    EncoderSettings source;
    source.setWidth(1280);
    source.setHeight(720);
    source.setFrameRate(29.97);
    source.setBitrate(18);
    source.setQuality(63);
    source.setAudioCodec(AudioCodec::MP3);
    source.setStartTimeCodeFrame(2400);
    source.setDropFrame(true);

    EncoderSettings copy(source);
    source.setWidth(1920);
    EXPECT_EQ(copy.getWidth(), 1280);

    EncoderSettings assigned;
    assigned = copy;
    copy.setQuality(10);
    EXPECT_EQ(assigned.getQuality(), 63);

    EncoderSettings restored;
    ASSERT_TRUE(restored.deserialize(assigned.serialize()));
    EXPECT_EQ(restored.getWidth(), 1280);
    EXPECT_EQ(restored.getHeight(), 720);
    EXPECT_DOUBLE_EQ(restored.getFrameRate(), 29.97);
    EXPECT_EQ(restored.getBitrate(), 18);
    EXPECT_EQ(restored.getQuality(), 63);
    EXPECT_EQ(restored.getAudioCodec(), AudioCodec::MP3);
    EXPECT_EQ(restored.getStartTimeCodeFrame(), 2400);
    EXPECT_TRUE(restored.isDropFrame());
}

TEST(EncoderSettingsContractTest,
     InvalidSerializedDocumentIsRejectedWithoutChangingExistingSettings)
{
    EncoderSettings settings;
    settings.setWidth(1600);
    settings.setHeight(900);
    settings.setFrameRate(25.0);
    const auto original = settings.serialize();

    EXPECT_FALSE(settings.deserialize(
        ArtifactCore::UniString(QStringLiteral("not json"))));
    EXPECT_EQ(settings.serialize().toQString(), original.toQString());
}
