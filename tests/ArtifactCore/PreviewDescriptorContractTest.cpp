#include <gtest/gtest.h>

#include <QByteArray>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QString>
#include <QTemporaryDir>

#include <array>

import File.Preview;
import File.TypeDetector;

using ArtifactCore::FileType;
using ArtifactCore::PreviewDescriptor;
using ArtifactCore::isInternalPreviewType;
using ArtifactCore::makePreviewDescriptor;

TEST(PreviewDescriptorContractTest,
     ExistingReadableTextFileGetsAbsolutePathAndInternalPreviewCapability)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("notes.txt"));
    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    ASSERT_NE(file.write(QByteArrayLiteral("preview contract")), -1);
    file.close();

    const PreviewDescriptor descriptor = makePreviewDescriptor(path);

    EXPECT_TRUE(descriptor.exists);
    EXPECT_FALSE(descriptor.isDirectory);
    EXPECT_TRUE(descriptor.readable);
    EXPECT_EQ(descriptor.fileType, FileType::Text);
    EXPECT_EQ(descriptor.displayName, QStringLiteral("notes.txt"));
    EXPECT_EQ(descriptor.filePath, QFileInfo(path).absoluteFilePath());
    EXPECT_TRUE(descriptor.canPreviewInternally);
    EXPECT_TRUE(descriptor.canPreviewWithSystem);
    EXPECT_TRUE(descriptor.isPreviewable());
}

TEST(PreviewDescriptorContractTest,
     DirectoriesAndMissingPathsDoNotAdvertiseFilePreview)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const auto directoryDescriptor = makePreviewDescriptor(directory.path());
    EXPECT_TRUE(directoryDescriptor.exists);
    EXPECT_TRUE(directoryDescriptor.isDirectory);
    EXPECT_EQ(directoryDescriptor.fileType, FileType::Unknown);
    EXPECT_FALSE(directoryDescriptor.canPreviewInternally);
    EXPECT_FALSE(directoryDescriptor.canPreviewWithSystem);
    EXPECT_FALSE(directoryDescriptor.isPreviewable());

    const QString missingPath = directory.filePath(QStringLiteral("missing.png"));
    const auto missing = makePreviewDescriptor(missingPath);
    EXPECT_FALSE(missing.exists);
    EXPECT_FALSE(missing.isDirectory);
    EXPECT_FALSE(missing.readable);
    EXPECT_EQ(missing.displayName, QStringLiteral("missing.png"));
    EXPECT_EQ(missing.fileType, FileType::Unknown);
    EXPECT_FALSE(missing.canPreviewInternally);
    EXPECT_FALSE(missing.canPreviewWithSystem);
    EXPECT_FALSE(missing.isPreviewable());
}

TEST(PreviewDescriptorContractTest,
     InternalPreviewPolicySeparatesImportableMediaFromOpaqueContainers)
{
    const std::array<FileType, 7> internallyPreviewable{
        FileType::Image, FileType::Video, FileType::Audio, FileType::Text,
        FileType::Font, FileType::Document, FileType::Model3D};
    for (const FileType type : internallyPreviewable) {
        SCOPED_TRACE(static_cast<int>(type));
        EXPECT_TRUE(isInternalPreviewType(type));
    }

    const std::array<FileType, 3> externalOnly{
        FileType::Unknown, FileType::Binary, FileType::Archive};
    for (const FileType type : externalOnly) {
        SCOPED_TRACE(static_cast<int>(type));
        EXPECT_FALSE(isInternalPreviewType(type));
    }

    PreviewDescriptor descriptor;
    EXPECT_FALSE(descriptor.isPreviewable());
    descriptor.canPreviewWithSystem = true;
    EXPECT_TRUE(descriptor.isPreviewable());
}
