#include <gtest/gtest.h>

#include <QSize>
#include <QString>

#include <utility>

import Media.MetaData;

using namespace ArtifactCore;

TEST(MediaMetaDataContractTest, ValidityRequiresFilePathAndPositiveDuration)
{
    MediaMetaData metadata;
    EXPECT_FALSE(metadata.isValid());
    metadata.filePath = QStringLiteral("clip.mov");
    EXPECT_FALSE(metadata.isValid());
    metadata.duration = 0.001;
    EXPECT_TRUE(metadata.isValid());
    metadata.filePath.clear();
    EXPECT_FALSE(metadata.isValid());
    metadata.filePath = QStringLiteral("clip.mov");
    metadata.duration = -1.0;
    EXPECT_FALSE(metadata.isValid());
}

TEST(MediaMetaDataContractTest, StreamQueriesReportPresenceAndReturnFirstMatchingStream)
{
    MediaMetaData metadata;
    EXPECT_FALSE(metadata.hasVideo());
    EXPECT_FALSE(metadata.hasAudio());
    EXPECT_FALSE(metadata.hasSubtitles());
    EXPECT_EQ(metadata.getFirstVideoStream(), nullptr);
    EXPECT_EQ(metadata.getFirstAudioStream(), nullptr);

    StreamInfo firstAudio;
    firstAudio.index = 0;
    firstAudio.type = MediaType::Audio;
    StreamInfo video;
    video.index = 1;
    video.type = MediaType::Video;
    StreamInfo secondVideo = video;
    secondVideo.index = 2;
    StreamInfo subtitle;
    subtitle.type = MediaType::Subtitle;
    metadata.streams = {firstAudio, video, secondVideo, subtitle};

    EXPECT_TRUE(metadata.hasVideo());
    EXPECT_TRUE(metadata.hasAudio());
    EXPECT_TRUE(metadata.hasSubtitles());
    ASSERT_NE(metadata.getFirstVideoStream(), nullptr);
    ASSERT_NE(metadata.getFirstAudioStream(), nullptr);
    EXPECT_EQ(metadata.getFirstVideoStream()->index, 1);
    EXPECT_EQ(metadata.getFirstAudioStream()->index, 0);

    const MediaMetaData& constMetadata = metadata;
    EXPECT_EQ(constMetadata.getFirstVideoStream()->index, 1);
    EXPECT_EQ(constMetadata.getFirstAudioStream()->index, 0);
}

TEST(MediaMetaDataContractTest, DurationFormattingUsesMinuteAndHourLayouts)
{
    MediaMetaData metadata;
    metadata.duration = 59.9;
    EXPECT_EQ(metadata.getFormattedDuration(), QStringLiteral("0:59"));
    metadata.duration = 60.0;
    EXPECT_EQ(metadata.getFormattedDuration(), QStringLiteral("1:00"));
    metadata.duration = 3599.0;
    EXPECT_EQ(metadata.getFormattedDuration(), QStringLiteral("59:59"));
    metadata.duration = 3600.0;
    EXPECT_EQ(metadata.getFormattedDuration(), QStringLiteral("1:00:00"));
    metadata.duration = 90061.0;
    EXPECT_EQ(metadata.getFormattedDuration(), QStringLiteral("25:01:01"));
}

TEST(MediaMetaDataContractTest, FileSizeFormattingUsesBinaryThresholds)
{
    MediaMetaData metadata;
    metadata.fileSize = 1023;
    EXPECT_EQ(metadata.getFormattedFileSize(), QStringLiteral("1023 B"));
    metadata.fileSize = 1024;
    EXPECT_EQ(metadata.getFormattedFileSize(), QStringLiteral("1.00 KB"));
    metadata.fileSize = 1024 * 1024;
    EXPECT_EQ(metadata.getFormattedFileSize(), QStringLiteral("1.00 MB"));
    metadata.fileSize = 1024LL * 1024 * 1024;
    EXPECT_EQ(metadata.getFormattedFileSize(), QStringLiteral("1.00 GB"));
}

TEST(MediaMetaDataContractTest, BitrateFormattingChangesUnitsAtExactThresholds)
{
    MediaMetaData metadata;
    metadata.bitrate = 999;
    EXPECT_EQ(metadata.getFormattedBitrate(), QStringLiteral("999 bps"));
    metadata.bitrate = 1000;
    EXPECT_EQ(metadata.getFormattedBitrate(), QStringLiteral("1 kbps"));
    metadata.bitrate = 999999;
    EXPECT_EQ(metadata.getFormattedBitrate(), QStringLiteral("1000 kbps"));
    metadata.bitrate = 1000000;
    EXPECT_EQ(metadata.getFormattedBitrate(), QStringLiteral("1.00 Mbps"));
}

TEST(MediaMetaDataContractTest, VideoDisplayHelpersUseFirstVideoStreamOnly)
{
    MediaMetaData metadata;
    StreamInfo audio;
    audio.type = MediaType::Audio;
    audio.resolution = QSize(1920, 1080);
    audio.frameRate = 60.0;
    metadata.streams.push_back(audio);
    EXPECT_TRUE(metadata.getResolutionString().isEmpty());
    EXPECT_TRUE(metadata.getFrameRateString().isEmpty());

    StreamInfo video;
    video.type = MediaType::Video;
    video.resolution = QSize(1920, 1080);
    video.frameRate = 29.97;
    metadata.streams.push_back(video);
    EXPECT_EQ(metadata.getResolutionString(), QStringLiteral("1920x1080"));
    EXPECT_EQ(metadata.getFrameRateString(), QStringLiteral("29.97 fps"));

    metadata.streams[1].resolution = QSize();
    metadata.streams[1].frameRate = 0.0;
    EXPECT_TRUE(metadata.getResolutionString().isEmpty());
    EXPECT_TRUE(metadata.getFrameRateString().isEmpty());
}

TEST(MediaMetaDataContractTest, EqualityUsesFilePathAndDuration)
{
    MediaMetaData first;
    first.filePath = QStringLiteral("same.mov");
    first.duration = 12.5;
    first.title = QStringLiteral("First title");
    MediaMetaData second = first;
    second.title = QStringLiteral("Different title");
    EXPECT_EQ(first, second);

    second.duration += 0.001;
    EXPECT_NE(first, second);
    second = first;
    second.filePath = QStringLiteral("other.mov");
    EXPECT_NE(first, second);
}

TEST(MediaMetaDataContractTest, TextSummaryIncludesCoreFieldsAndVideoDetailsOnlyWhenPresent)
{
    MediaMetaData metadata;
    metadata.fileName = QStringLiteral("scene.mov");
    metadata.duration = 65.0;
    metadata.fileSize = 2048;
    metadata.bitrate = 1500;
    const QString audioOnly = metadata.toString();
    EXPECT_TRUE(audioOnly.contains(QStringLiteral("File: scene.mov\n")));
    EXPECT_TRUE(audioOnly.contains(QStringLiteral("Duration: 1:05\n")));
    EXPECT_FALSE(audioOnly.contains(QStringLiteral("Resolution:")));

    StreamInfo video;
    video.type = MediaType::Video;
    video.resolution = QSize(640, 480);
    video.frameRate = 24.0;
    metadata.streams.push_back(video);
    const QString withVideo = metadata.toString();
    EXPECT_TRUE(withVideo.contains(QStringLiteral("Resolution: 640x480\n")));
    EXPECT_TRUE(withVideo.contains(QStringLiteral("Frame Rate: 24.00 fps\n")));
}
