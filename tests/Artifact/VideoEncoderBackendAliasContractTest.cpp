#include <gtest/gtest.h>

#include <QString>

#include <array>
#include <utility>

import Artifact.Render.Queue.Encoder;

TEST(VideoEncoderBackendAliasContractTest,
     AliasesNormalizeToStablePublicBackendNames)
{
    const std::array<std::pair<QString, QString>, 19> aliases{{
        {QStringLiteral(" AUTO "), QStringLiteral("auto")},
        {QStringLiteral("unknown"), QStringLiteral("auto")},
        {QStringLiteral("pipe"), QStringLiteral("pipe")},
        {QStringLiteral("ffmpeg.exe"), QStringLiteral("pipe")},
        {QStringLiteral("FFMPEG"), QStringLiteral("pipe")},
        {QStringLiteral("pipe-hw"), QStringLiteral("pipe-hw")},
        {QStringLiteral("pipe-hw (nvenc)"), QStringLiteral("pipe-hw")},
        {QStringLiteral("ffmpeg-hw"), QStringLiteral("pipe-hw")},
        {QStringLiteral("hardware"), QStringLiteral("pipe-hw")},
        {QStringLiteral("hw"), QStringLiteral("pipe-hw")},
        {QStringLiteral("pipe-vulkan"), QStringLiteral("pipe-vulkan")},
        {QStringLiteral("ffmpeg-vulkan"), QStringLiteral("pipe-vulkan")},
        {QStringLiteral("vulkan-hw"), QStringLiteral("pipe-vulkan")},
        {QStringLiteral("VULKAN"), QStringLiteral("pipe-vulkan")},
        {QStringLiteral("native"), QStringLiteral("native")},
        {QStringLiteral("api"), QStringLiteral("native")},
        {QStringLiteral("FFmpegAPI"), QStringLiteral("native")},
        {QStringLiteral("gpu"), QStringLiteral("gpu")},
        {QStringLiteral("Diligent"), QStringLiteral("auto")},
    }};

    for (const auto& [alias, expected] : aliases) {
        EXPECT_EQ(Artifact::normalizeVideoEncodeBackendName(alias), expected)
            << "alias=" << alias.toStdString();
    }
}
